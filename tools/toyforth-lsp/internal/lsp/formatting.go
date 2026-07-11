package lsp

import (
	"encoding/json"
	"fmt"
	"io"
	"net/url"
	"path/filepath"
	"runtime"
	"unicode/utf16"
	"unicode/utf8"

	"toyforth-lsp/internal/formatter"
)

const lspRequestFailed = -32803

func (s *Server) handleFormatting(w io.Writer, req request) error {
	var params documentFormattingParams
	if err := json.Unmarshal(req.Params, &params); err != nil {
		return writeResponse(w, response{
			JSONRPC: "2.0",
			ID:      decodeID(req.ID),
			Error: &respError{
				Code:    -32602,
				Message: fmt.Sprintf("invalid formatting parameters: %v", err),
			},
		})
	}

	doc, ok := s.docs.get(params.TextDocument.URI)
	if !ok {
		return writeResponse(w, response{
			JSONRPC: "2.0",
			ID:      decodeID(req.ID),
			Result:  newResult([]textEdit{}),
		})
	}

	options := formatter.DefaultOptions()
	if params.Options.TabSize > 0 {
		options.IndentWidth = params.Options.TabSize
	}
	if !params.Options.InsertSpaces {
		options.IndentStyle = formatter.IndentTabs
	}

	path, isFile, err := filePathFromURI(params.TextDocument.URI)
	if err != nil {
		return s.writeFormattingError(w, req, err)
	}
	if isFile {
		options, err = formatter.OptionsForPath(path, options)
		if err != nil {
			return s.writeFormattingError(w, req, err)
		}
	}

	formatted, err := formatter.Format([]byte(doc.Text), options)
	if err != nil {
		return s.writeFormattingError(w, req, err)
	}
	if string(formatted) == doc.Text {
		return writeResponse(w, response{
			JSONRPC: "2.0",
			ID:      decodeID(req.ID),
			Result:  newResult([]textEdit{}),
		})
	}

	return writeResponse(w, response{
		JSONRPC: "2.0",
		ID:      decodeID(req.ID),
		Result: newResult([]textEdit{{
			Range: lspRange{
				Start: lspPosition{},
				End:   documentEndPosition(doc.Text),
			},
			NewText: string(formatted),
		}}),
	})
}

func (s *Server) writeFormattingError(w io.Writer, req request, err error) error {
	s.logger.Printf("formatting failed: %v", err)
	return writeResponse(w, response{
		JSONRPC: "2.0",
		ID:      decodeID(req.ID),
		Error: &respError{
			Code:    lspRequestFailed,
			Message: fmt.Sprintf("formatting failed: %v", err),
		},
	})
}

func filePathFromURI(rawURI string) (string, bool, error) {
	parsed, err := url.Parse(rawURI)
	if err != nil {
		return "", false, fmt.Errorf("parse document URI: %w", err)
	}
	if parsed.Scheme != "file" {
		return "", false, nil
	}

	path := parsed.Path
	if parsed.Host != "" && parsed.Host != "localhost" {
		path = "//" + parsed.Host + path
	}
	if runtime.GOOS == "windows" && len(path) >= 3 && path[0] == '/' && path[2] == ':' {
		path = path[1:]
	}
	path = filepath.FromSlash(path)
	if path == "" {
		return "", false, fmt.Errorf("file document URI has no path")
	}
	return path, true, nil
}

func documentEndPosition(text string) lspPosition {
	position := lspPosition{}
	for len(text) > 0 {
		if text[0] == '\r' {
			text = text[1:]
			if len(text) > 0 && text[0] == '\n' {
				text = text[1:]
			}
			position.Line++
			position.Character = 0
			continue
		}
		if text[0] == '\n' {
			text = text[1:]
			position.Line++
			position.Character = 0
			continue
		}

		r, size := utf8.DecodeRuneInString(text)
		text = text[size:]
		position.Character += utf16.RuneLen(r)
	}
	return position
}
