package main

import (
	"bytes"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"toy-lsp/internal/formatter"
)

func TestRunFormatsStdin(t *testing.T) {
	var stdout, stderr bytes.Buffer
	exitCode := run(nil, strings.NewReader("[1   2]"), &stdout, &stderr)
	if exitCode != 0 {
		t.Fatalf("run() exit = %d, stderr = %q", exitCode, stderr.String())
	}
	if got, want := stdout.String(), "[ 1 2 ]\n"; got != want {
		t.Fatalf("stdout = %q, want %q", got, want)
	}
}

func TestRunCLIOverridesProjectConfig(t *testing.T) {
	directory := t.TempDir()
	if err := os.WriteFile(filepath.Join(directory, formatter.ConfigFileName), []byte(`
indent_width = 2
delimiter_spacing = "compact"
`), 0o644); err != nil {
		t.Fatal(err)
	}
	path := filepath.Join(directory, "program.toy")
	if err := os.WriteFile(path, []byte("[\n[1   2]\n]"), 0o644); err != nil {
		t.Fatal(err)
	}

	var stdout, stderr bytes.Buffer
	exitCode := run([]string{
		"--indent-width", "3",
		"--delimiter-spacing", "spaced",
		path,
	}, strings.NewReader(""), &stdout, &stderr)
	if exitCode != 0 {
		t.Fatalf("run() exit = %d, stderr = %q", exitCode, stderr.String())
	}
	if got, want := stdout.String(), "[\n   [ 1 2 ]\n]\n"; got != want {
		t.Fatalf("stdout = %q, want %q", got, want)
	}
}

func TestRunCheckAndWrite(t *testing.T) {
	path := filepath.Join(t.TempDir(), "program.toy")
	if err := os.WriteFile(path, []byte("[1   2]"), 0o644); err != nil {
		t.Fatal(err)
	}

	var stdout, stderr bytes.Buffer
	if exitCode := run([]string{"--check", path}, strings.NewReader(""), &stdout, &stderr); exitCode != 1 {
		t.Fatalf("check exit = %d, want 1; stderr = %q", exitCode, stderr.String())
	}
	if !strings.Contains(stderr.String(), "needs formatting") {
		t.Fatalf("check stderr = %q, want needs-formatting report", stderr.String())
	}

	stdout.Reset()
	stderr.Reset()
	if exitCode := run([]string{"--write", path}, strings.NewReader(""), &stdout, &stderr); exitCode != 0 {
		t.Fatalf("write exit = %d, stderr = %q", exitCode, stderr.String())
	}
	content, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if got, want := string(content), "[ 1 2 ]\n"; got != want {
		t.Fatalf("written file = %q, want %q", got, want)
	}

	stderr.Reset()
	if exitCode := run([]string{"--check", path}, strings.NewReader(""), &stdout, &stderr); exitCode != 0 {
		t.Fatalf("check formatted exit = %d, stderr = %q", exitCode, stderr.String())
	}
}

func TestRunRejectsExplicitZeroIndentWidth(t *testing.T) {
	var stdout, stderr bytes.Buffer
	exitCode := run([]string{"--indent-width", "0"}, strings.NewReader("1"), &stdout, &stderr)
	if exitCode != 2 || !strings.Contains(stderr.String(), "at least 1") {
		t.Fatalf("run() exit = %d, stderr = %q", exitCode, stderr.String())
	}
}
