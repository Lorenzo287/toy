package lsp

import (
	"bytes"
	"encoding/json"
	"io"
	"log"
	"net/url"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"

	"toy-lsp/internal/formatter"
)

func TestInitializeAdvertisesDocumentFormatting(t *testing.T) {
	server := NewServer(log.New(io.Discard, "", 0))
	var output bytes.Buffer
	if err := server.handleInitialize(&output, request{
		ID: json.RawMessage("1"),
	}); err != nil {
		t.Fatal(err)
	}

	response := decodeTestResponse(t, output.Bytes())
	var result initializeResult
	if err := json.Unmarshal(response.Result, &result); err != nil {
		t.Fatal(err)
	}
	if !result.Capabilities.DocumentFormattingProvider {
		t.Fatal("initialize result does not advertise document formatting")
	}
}

func TestDocumentFormattingUsesProjectConfigOverLSPDefaults(t *testing.T) {
	directory := t.TempDir()
	if err := os.WriteFile(filepath.Join(directory, formatter.ConfigFileName), []byte(`
indent_width = 2
indent_style = "spaces"
delimiter_spacing = "compact"
`), 0o644); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(directory, "format-me.toy")
	uri := testFileURI(path)
	source := "[\n [   1   2 ]\n] \"😀\"   print"

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.open(uri, source, 1)
	params, err := json.Marshal(documentFormattingParams{
		TextDocument: textDocumentIdentifier{URI: uri},
		Options: formattingOptions{
			TabSize:      8,
			InsertSpaces: false,
		},
	})
	if err != nil {
		t.Fatal(err)
	}

	var output bytes.Buffer
	if err := server.handleFormatting(&output, request{
		ID:     json.RawMessage("2"),
		Params: params,
	}); err != nil {
		t.Fatal(err)
	}

	response := decodeTestResponse(t, output.Bytes())
	if response.Error != nil {
		t.Fatalf("formatting response error = %+v", response.Error)
	}
	var edits []textEdit
	if err := json.Unmarshal(response.Result, &edits); err != nil {
		t.Fatal(err)
	}
	if len(edits) != 1 {
		t.Fatalf("formatting edits = %d, want 1", len(edits))
	}
	want := "[\n  [1 2]\n] \"😀\" print\n"
	if edits[0].NewText != want {
		t.Fatalf("formatted text = %q, want %q", edits[0].NewText, want)
	}
	if edits[0].Range.Start != (lspPosition{}) {
		t.Fatalf("edit start = %+v, want document start", edits[0].Range.Start)
	}
	if edits[0].Range.End != (lspPosition{Line: 2, Character: 14}) {
		t.Fatalf("edit end = %+v, want UTF-16 document end {2 14}", edits[0].Range.End)
	}
}

func TestDocumentFormattingReturnsRequestErrorForInvalidSyntax(t *testing.T) {
	server := NewServer(log.New(io.Discard, "", 0))
	uri := "untitled:broken"
	server.docs.open(uri, "[ 1", 1)
	params, err := json.Marshal(documentFormattingParams{
		TextDocument: textDocumentIdentifier{URI: uri},
		Options: formattingOptions{
			TabSize:      4,
			InsertSpaces: true,
		},
	})
	if err != nil {
		t.Fatal(err)
	}

	var output bytes.Buffer
	if err := server.handleFormatting(&output, request{
		ID:     json.RawMessage("3"),
		Params: params,
	}); err != nil {
		t.Fatal(err)
	}
	response := decodeTestResponse(t, output.Bytes())
	if response.Error == nil {
		t.Fatal("invalid syntax unexpectedly produced a successful formatting response")
	}
	if response.Error.Code != lspRequestFailed || !strings.Contains(response.Error.Message, "invalid Toy syntax") {
		t.Fatalf("formatting error = %+v, want clear request-failed syntax diagnostic", response.Error)
	}
}

func TestDocumentEndPositionUsesUTF16AndLineEndings(t *testing.T) {
	got := documentEndPosition("a😀\r\nβ")
	want := lspPosition{Line: 1, Character: 1}
	if got != want {
		t.Fatalf("documentEndPosition() = %+v, want %+v", got, want)
	}
}

type testResponse struct {
	Result json.RawMessage `json:"result"`
	Error  *respError      `json:"error"`
}

func decodeTestResponse(t *testing.T, framed []byte) testResponse {
	t.Helper()
	separator := bytes.Index(framed, []byte("\r\n\r\n"))
	if separator < 0 {
		t.Fatalf("response has no header separator: %q", framed)
	}
	var response testResponse
	if err := json.Unmarshal(framed[separator+4:], &response); err != nil {
		t.Fatal(err)
	}
	return response
}

func testFileURI(path string) string {
	slashPath := filepath.ToSlash(path)
	if runtime.GOOS == "windows" && !strings.HasPrefix(slashPath, "/") {
		slashPath = "/" + slashPath
	}
	return (&url.URL{Scheme: "file", Path: slashPath}).String()
}
