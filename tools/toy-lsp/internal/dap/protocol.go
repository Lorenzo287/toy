package dap

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"strconv"
	"strings"
	"sync"
)

type request struct {
	Seq       int             `json:"seq"`
	Type      string          `json:"type"`
	Command   string          `json:"command"`
	Arguments json.RawMessage `json:"arguments,omitempty"`
}

type response struct {
	Seq        int    `json:"seq"`
	Type       string `json:"type"`
	RequestSeq int    `json:"request_seq"`
	Success    bool   `json:"success"`
	Command    string `json:"command"`
	Message    string `json:"message,omitempty"`
	Body       any    `json:"body,omitempty"`
}

type event struct {
	Seq   int    `json:"seq"`
	Type  string `json:"type"`
	Event string `json:"event"`
	Body  any    `json:"body,omitempty"`
}

type messageWriter struct {
	mu  sync.Mutex
	out io.Writer
	seq int
}

func newMessageWriter(out io.Writer) *messageWriter {
	return &messageWriter{out: out, seq: 1}
}

func (w *messageWriter) respond(req request, body any) error {
	return w.write(response{
		Type:       "response",
		RequestSeq: req.Seq,
		Success:    true,
		Command:    req.Command,
		Body:       body,
	})
}

func (w *messageWriter) fail(req request, message string) error {
	return w.write(response{
		Type:       "response",
		RequestSeq: req.Seq,
		Success:    false,
		Command:    req.Command,
		Message:    message,
	})
}

func (w *messageWriter) sendEvent(name string, body any) error {
	return w.write(event{Type: "event", Event: name, Body: body})
}

func (w *messageWriter) write(message any) error {
	w.mu.Lock()
	defer w.mu.Unlock()

	switch value := message.(type) {
	case response:
		value.Seq = w.seq
		message = value
	case event:
		value.Seq = w.seq
		message = value
	}
	w.seq++

	payload, err := json.Marshal(message)
	if err != nil {
		return err
	}
	var framed bytes.Buffer
	fmt.Fprintf(&framed, "Content-Length: %d\r\n\r\n", len(payload))
	framed.Write(payload)
	_, err = w.out.Write(framed.Bytes())
	return err
}

func readMessage(reader *bufio.Reader) ([]byte, error) {
	contentLength := -1
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			return nil, err
		}
		line = strings.TrimRight(line, "\r\n")
		if line == "" {
			break
		}
		if strings.HasPrefix(strings.ToLower(line), "content-length:") {
			value := strings.TrimSpace(line[len("content-length:"):])
			length, err := strconv.Atoi(value)
			if err != nil {
				return nil, fmt.Errorf("invalid content length %q: %w", value, err)
			}
			contentLength = length
		}
	}
	if contentLength < 0 {
		return nil, fmt.Errorf("missing Content-Length header")
	}
	payload := make([]byte, contentLength)
	if _, err := io.ReadFull(reader, payload); err != nil {
		return nil, err
	}
	return payload, nil
}
