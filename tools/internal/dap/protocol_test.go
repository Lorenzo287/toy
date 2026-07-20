package dap

import (
	"bufio"
	"bytes"
	"encoding/json"
	"strconv"
	"testing"
)

func frameMessage(t *testing.T, value any) []byte {
	t.Helper()
	payload, err := json.Marshal(value)
	if err != nil {
		t.Fatal(err)
	}
	return []byte("Content-Length: " + strconv.Itoa(len(payload)) + "\r\n\r\n" + string(payload))
}

func TestReadMessage(t *testing.T) {
	want := map[string]any{
		"seq":     1,
		"type":    "request",
		"command": "initialize",
	}
	payload, err := readMessage(bufio.NewReader(bytes.NewReader(frameMessage(t, want))))
	if err != nil {
		t.Fatal(err)
	}
	var got map[string]any
	if err := json.Unmarshal(payload, &got); err != nil {
		t.Fatal(err)
	}
	if got["command"] != "initialize" {
		t.Fatalf("command = %v, want initialize", got["command"])
	}
}

func TestServerInitialize(t *testing.T) {
	input := frameMessage(t, map[string]any{
		"seq":     7,
		"type":    "request",
		"command": "initialize",
	})
	var output bytes.Buffer
	server := NewServer(nil)
	if err := server.Serve(bytes.NewReader(input), &output); err != nil {
		t.Fatal(err)
	}
	payload, err := readMessage(bufio.NewReader(&output))
	if err != nil {
		t.Fatal(err)
	}
	var got struct {
		Type       string `json:"type"`
		RequestSeq int    `json:"request_seq"`
		Success    bool   `json:"success"`
		Body       struct {
			SupportsConfigurationDoneRequest bool `json:"supportsConfigurationDoneRequest"`
			SupportsTerminateRequest         bool `json:"supportsTerminateRequest"`
		} `json:"body"`
	}
	if err := json.Unmarshal(payload, &got); err != nil {
		t.Fatal(err)
	}
	if got.Type != "response" || got.RequestSeq != 7 || !got.Success {
		t.Fatalf("unexpected initialize response: %s", payload)
	}
	if !got.Body.SupportsConfigurationDoneRequest || !got.Body.SupportsTerminateRequest {
		t.Fatalf("missing capabilities: %s", payload)
	}
}
