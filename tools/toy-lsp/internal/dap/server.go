package dap

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
)

const toyThreadID = 1
const dataStackReference = 1

type Server struct {
	logger *log.Logger
	writer *messageWriter

	stateMu       sync.Mutex
	session       *debugSession
	snapshot      *machineEvent
	breakpointSet map[int]bool
	stopOnEntry   bool
	configured    bool
	terminated    bool
}

func NewServer(logger *log.Logger) *Server {
	return &Server{logger: logger, breakpointSet: make(map[int]bool)}
}

func (s *Server) Serve(input io.Reader, output io.Writer) error {
	s.writer = newMessageWriter(output)
	reader := bufio.NewReader(input)
	defer func() {
		s.stateMu.Lock()
		session := s.session
		s.stateMu.Unlock()
		if session != nil {
			_ = session.kill()
		}
	}()

	for {
		payload, err := readMessage(reader)
		if err != nil {
			if err == io.EOF {
				return nil
			}
			return err
		}
		var req request
		if err := json.Unmarshal(payload, &req); err != nil {
			return fmt.Errorf("decode DAP request: %w", err)
		}
		if req.Type != "request" {
			continue
		}
		if err := s.handle(req); err != nil {
			return err
		}
		if req.Command == "disconnect" {
			return nil
		}
	}
}

func (s *Server) handle(req request) error {
	switch req.Command {
	case "initialize":
		return s.writer.respond(req, map[string]any{
			"supportsConfigurationDoneRequest": true,
			"supportsTerminateRequest":         true,
		})
	case "launch":
		return s.handleLaunch(req)
	case "setBreakpoints":
		return s.handleSetBreakpoints(req)
	case "setExceptionBreakpoints":
		return s.writer.respond(req, map[string]any{"breakpoints": []any{}})
	case "configurationDone":
		return s.handleConfigurationDone(req)
	case "threads":
		return s.writer.respond(req, map[string]any{
			"threads": []any{map[string]any{"id": toyThreadID, "name": "Toy VM"}},
		})
	case "stackTrace":
		return s.handleStackTrace(req)
	case "scopes":
		return s.handleScopes(req)
	case "variables":
		return s.handleVariables(req)
	case "continue":
		return s.handleResume(req, "continue")
	case "next":
		return s.handleResume(req, "next")
	case "stepIn":
		return s.handleResume(req, "step")
	case "stepOut":
		return s.handleResume(req, "step-out")
	case "terminate":
		return s.handleTerminate(req)
	case "disconnect":
		return s.handleDisconnect(req)
	case "pause":
		return s.writer.fail(req, "pausing a running Toy program is not supported yet")
	default:
		return s.writer.fail(req, fmt.Sprintf("unsupported request: %s", req.Command))
	}
}

type launchArguments struct {
	Program           string   `json:"program"`
	RuntimeExecutable string   `json:"runtimeExecutable"`
	Cwd               string   `json:"cwd"`
	Args              []string `json:"args"`
	StopOnEntry       bool     `json:"stopOnEntry"`
}

func (s *Server) handleLaunch(req request) error {
	var arguments launchArguments
	if err := json.Unmarshal(req.Arguments, &arguments); err != nil {
		return s.writer.fail(req, fmt.Sprintf("invalid launch arguments: %v", err))
	}
	if arguments.Program == "" {
		return s.writer.fail(req, "launch requires a program path")
	}
	if arguments.RuntimeExecutable == "" {
		arguments.RuntimeExecutable = defaultRuntimeExecutable()
	}

	session, initial, err := startDebugSession(sessionConfig{
		RuntimeExecutable: arguments.RuntimeExecutable,
		Program:           arguments.Program,
		Cwd:               arguments.Cwd,
		Args:              arguments.Args,
	})
	if err != nil {
		return s.writer.fail(req, fmt.Sprintf("launch Toy: %v", err))
	}

	s.stateMu.Lock()
	if s.session != nil {
		s.stateMu.Unlock()
		_ = session.kill()
		return s.writer.fail(req, "a Toy debug session is already active")
	}
	s.session = session
	snapshot := initial
	s.snapshot = &snapshot
	s.stopOnEntry = arguments.StopOnEntry
	s.configured = false
	s.terminated = false
	s.stateMu.Unlock()

	go s.consumeMachineEvents(session)
	go s.consumeProcessOutput(session)
	if err := s.writer.respond(req, nil); err != nil {
		return err
	}
	return s.writer.sendEvent("initialized", nil)
}

type source struct {
	Name string `json:"name,omitempty"`
	Path string `json:"path,omitempty"`
}

type sourceBreakpoint struct {
	Line         int    `json:"line"`
	Column       int    `json:"column,omitempty"`
	Condition    string `json:"condition,omitempty"`
	HitCondition string `json:"hitCondition,omitempty"`
	LogMessage   string `json:"logMessage,omitempty"`
}

type setBreakpointsArguments struct {
	Source      source             `json:"source"`
	Breakpoints []sourceBreakpoint `json:"breakpoints"`
}

func (s *Server) handleSetBreakpoints(req request) error {
	var arguments setBreakpointsArguments
	if err := json.Unmarshal(req.Arguments, &arguments); err != nil {
		return s.writer.fail(req, fmt.Sprintf("invalid breakpoint arguments: %v", err))
	}

	s.stateMu.Lock()
	defer s.stateMu.Unlock()
	if s.session == nil {
		return s.writer.fail(req, "no active Toy debug session")
	}
	if s.snapshot == nil {
		return s.writer.fail(req, "breakpoints can currently be changed only while paused")
	}

	programSource := samePath(arguments.Source.Path, s.session.program)
	breakpoints := make([]any, 0, len(arguments.Breakpoints))
	if programSource {
		if err := s.session.command("clear-breakpoints"); err != nil {
			return s.writer.fail(req, err.Error())
		}
		clear(s.breakpointSet)
	}
	for index, requested := range arguments.Breakpoints {
		verified := programSource && requested.Line > 0 && requested.Column == 0 &&
			requested.Condition == "" && requested.HitCondition == "" &&
			requested.LogMessage == ""
		breakpoint := map[string]any{
			"id":       index + 1,
			"verified": verified,
			"line":     requested.Line,
			"source":   arguments.Source,
		}
		if !programSource {
			breakpoint["message"] = "this first adapter slice supports the launched source only"
		} else if !verified {
			breakpoint["message"] = "conditional, hit-count, column, and log breakpoints are not supported"
		} else {
			s.breakpointSet[requested.Line] = true
			if err := s.session.command(fmt.Sprintf("break %d", requested.Line)); err != nil {
				return s.writer.fail(req, err.Error())
			}
		}
		breakpoints = append(breakpoints, breakpoint)
	}
	return s.writer.respond(req, map[string]any{"breakpoints": breakpoints})
}

func (s *Server) handleConfigurationDone(req request) error {
	s.stateMu.Lock()
	if s.session == nil || s.snapshot == nil {
		s.stateMu.Unlock()
		return s.writer.fail(req, "no paused Toy debug session")
	}
	if s.configured {
		s.stateMu.Unlock()
		return s.writer.respond(req, nil)
	}
	s.configured = true
	snapshot := *s.snapshot
	stopOnEntry := s.stopOnEntry
	stopOnBreakpoint := s.breakpointSet[snapshot.Line]
	if !stopOnEntry && !stopOnBreakpoint {
		s.snapshot = nil
	}
	session := s.session
	s.stateMu.Unlock()

	if err := s.writer.respond(req, nil); err != nil {
		return err
	}
	if stopOnEntry {
		return s.sendStopped("entry")
	}
	if stopOnBreakpoint {
		s.stateMu.Lock()
		if s.snapshot != nil {
			s.snapshot.Reason = "breakpoint"
		}
		s.stateMu.Unlock()
		return s.sendStopped("breakpoint")
	}
	return session.command("continue")
}

func (s *Server) handleStackTrace(req request) error {
	var arguments struct {
		StartFrame int `json:"startFrame"`
		Levels     int `json:"levels"`
	}
	_ = json.Unmarshal(req.Arguments, &arguments)
	snapshot, ok := s.currentSnapshot()
	if !ok {
		return s.writer.fail(req, "Toy is not paused")
	}

	start := arguments.StartFrame
	if start < 0 {
		start = 0
	}
	end := len(snapshot.Frames)
	if arguments.Levels > 0 && start+arguments.Levels < end {
		end = start + arguments.Levels
	}
	if start > end {
		start = end
	}
	frames := make([]any, 0, end-start)
	for _, frame := range snapshot.Frames[start:end] {
		frames = append(frames, map[string]any{
			"id":     frame.ID,
			"name":   frame.Name,
			"source": source{Name: filepath.Base(frame.Source), Path: frame.Source},
			"line":   frame.Line,
			"column": max(frame.Column, 1),
		})
	}
	return s.writer.respond(req, map[string]any{
		"stackFrames": frames,
		"totalFrames": len(snapshot.Frames),
	})
}

func (s *Server) handleScopes(req request) error {
	snapshot, ok := s.currentSnapshot()
	if !ok {
		return s.writer.fail(req, "Toy is not paused")
	}
	return s.writer.respond(req, map[string]any{
		"scopes": []any{map[string]any{
			"name":               "Data stack",
			"presentationHint":   "locals",
			"variablesReference": dataStackReference,
			"namedVariables":     len(snapshot.Stack),
			"expensive":          false,
		}},
	})
}

func (s *Server) handleVariables(req request) error {
	var arguments struct {
		VariablesReference int `json:"variablesReference"`
		Start              int `json:"start"`
		Count              int `json:"count"`
	}
	if err := json.Unmarshal(req.Arguments, &arguments); err != nil {
		return s.writer.fail(req, fmt.Sprintf("invalid variables arguments: %v", err))
	}
	if arguments.VariablesReference != dataStackReference {
		return s.writer.respond(req, map[string]any{"variables": []any{}})
	}
	snapshot, ok := s.currentSnapshot()
	if !ok {
		return s.writer.fail(req, "Toy is not paused")
	}
	start := max(arguments.Start, 0)
	end := len(snapshot.Stack)
	if arguments.Count > 0 && start+arguments.Count < end {
		end = start + arguments.Count
	}
	if start > end {
		start = end
	}
	variables := make([]any, 0, end-start)
	for _, value := range snapshot.Stack[start:end] {
		variables = append(variables, map[string]any{
			"name":               value.Name,
			"value":              value.Value,
			"type":               value.Type,
			"variablesReference": 0,
		})
	}
	return s.writer.respond(req, map[string]any{"variables": variables})
}

func (s *Server) handleResume(req request, command string) error {
	s.stateMu.Lock()
	if s.session == nil || s.snapshot == nil {
		s.stateMu.Unlock()
		return s.writer.fail(req, "Toy is not paused")
	}
	session := s.session
	s.snapshot = nil
	s.stateMu.Unlock()

	body := any(nil)
	if command == "continue" {
		body = map[string]any{"allThreadsContinued": true}
	}
	if err := s.writer.respond(req, body); err != nil {
		return err
	}
	if err := s.writer.sendEvent("continued", map[string]any{
		"threadId":            toyThreadID,
		"allThreadsContinued": true,
	}); err != nil {
		return err
	}
	return session.command(command)
}

func (s *Server) handleTerminate(req request) error {
	s.stateMu.Lock()
	session := s.session
	s.stateMu.Unlock()
	if session != nil {
		_ = session.kill()
	}
	return s.writer.respond(req, nil)
}

func (s *Server) handleDisconnect(req request) error {
	s.stateMu.Lock()
	session := s.session
	s.stateMu.Unlock()
	if session != nil {
		_ = session.kill()
	}
	return s.writer.respond(req, nil)
}

func (s *Server) consumeMachineEvents(session *debugSession) {
	for event := range session.events {
		switch event.Event {
		case "stopped":
			s.stateMu.Lock()
			snapshot := event
			s.snapshot = &snapshot
			configured := s.configured
			s.stateMu.Unlock()
			if configured {
				_ = s.sendStopped(event.Reason)
			}
		case "terminated":
			s.finishSession(event.ExitCode)
		case "error":
			_ = s.writer.sendEvent("output", map[string]any{
				"category": "stderr",
				"output":   "toy debug protocol: " + event.Message + "\n",
			})
		case "output":
			_ = s.writer.sendEvent("output", map[string]any{
				"category": "stdout",
				"output":   event.Message,
			})
		}
	}
}

func (s *Server) consumeProcessOutput(session *debugSession) {
	for output := range session.outputs {
		_ = s.writer.sendEvent("output", map[string]any{
			"category": output.Category,
			"output":   output.Text,
		})
	}
}

func (s *Server) sendStopped(reason string) error {
	if reason == "" {
		reason = "step"
	}
	return s.writer.sendEvent("stopped", map[string]any{
		"reason":            reason,
		"threadId":          toyThreadID,
		"allThreadsStopped": true,
	})
}

func (s *Server) finishSession(exitCode int) {
	s.stateMu.Lock()
	if s.terminated {
		s.stateMu.Unlock()
		return
	}
	s.terminated = true
	s.snapshot = nil
	s.stateMu.Unlock()
	_ = s.writer.sendEvent("exited", map[string]any{"exitCode": exitCode})
	_ = s.writer.sendEvent("terminated", nil)
}

func (s *Server) currentSnapshot() (machineEvent, bool) {
	s.stateMu.Lock()
	defer s.stateMu.Unlock()
	if s.snapshot == nil {
		return machineEvent{}, false
	}
	return *s.snapshot, true
}

func defaultRuntimeExecutable() string {
	executable, err := os.Executable()
	if err == nil {
		name := "toy"
		if runtime.GOOS == "windows" {
			name += ".exe"
		}
		candidate := filepath.Join(filepath.Dir(executable), name)
		if _, err := os.Stat(candidate); err == nil {
			return candidate
		}
	}
	return "toy"
}

func samePath(left, right string) bool {
	leftPath, leftErr := filepath.Abs(left)
	rightPath, rightErr := filepath.Abs(right)
	if leftErr != nil || rightErr != nil {
		return false
	}
	leftPath = filepath.Clean(leftPath)
	rightPath = filepath.Clean(rightPath)
	if runtime.GOOS == "windows" {
		return strings.EqualFold(leftPath, rightPath)
	}
	return leftPath == rightPath
}
