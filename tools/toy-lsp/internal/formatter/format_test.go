package formatter

import (
	"bytes"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestFormatFixturesAndIdempotence(t *testing.T) {
	input := readFixture(t, "layout.input.toy")
	tests := []struct {
		name    string
		spacing DelimiterSpacing
		want    string
	}{
		{name: "spaced", spacing: DelimiterSpaced, want: "layout.spaced.toy"},
		{name: "compact", spacing: DelimiterCompact, want: "layout.compact.toy"},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			options := DefaultOptions()
			options.DelimiterSpacing = test.spacing
			want := readFixture(t, test.want)

			got, err := Format(input, options)
			if err != nil {
				t.Fatalf("Format() error = %v", err)
			}
			if !bytes.Equal(got, want) {
				t.Fatalf("Format() mismatch\n--- got ---\n%s--- want ---\n%s", got, want)
			}

			again, err := Format(got, options)
			if err != nil {
				t.Fatalf("Format(formatted) error = %v", err)
			}
			if !bytes.Equal(again, got) {
				t.Fatalf("formatter is not idempotent\nfirst:\n%ssecond:\n%s", got, again)
			}
		})
	}
}

func TestFormatPreservesLineLayoutAndLiteralContents(t *testing.T) {
	source := []byte("[   \"line  one\nline [ two ]\"   /* first\n  [ exact ]\n*/\n\n[]   ()   {}   #{}   ]")
	want := "[ \"line  one\nline [ two ]\" /* first\n  [ exact ]\n*/\n\n    [] () {} #{} ]\n"

	got, err := Format(source, DefaultOptions())
	if err != nil {
		t.Fatalf("Format() error = %v", err)
	}
	if string(got) != want {
		t.Fatalf("Format() = %q, want %q", got, want)
	}
	if !bytes.Contains(got, []byte("\"line  one\nline [ two ]\"")) {
		t.Fatal("string contents changed")
	}
	if !bytes.Contains(got, []byte("/* first\n  [ exact ]\n*/")) {
		t.Fatal("block comment contents changed")
	}
}

func TestFormatTabsAndFinalNewline(t *testing.T) {
	options := DefaultOptions()
	options.IndentStyle = IndentTabs
	options.DelimiterSpacing = DelimiterCompact
	source := []byte("[\r\n [\r\n1\r\n ]\r\n]")
	want := "[\r\n\t[\r\n\t\t1\r\n\t]\r\n]\r\n"

	got, err := Format(source, options)
	if err != nil {
		t.Fatalf("Format() error = %v", err)
	}
	if string(got) != want {
		t.Fatalf("Format() = %q, want %q", got, want)
	}
}

func TestFormatRejectsMalformedInput(t *testing.T) {
	_, err := Format([]byte("[ 1 2"), DefaultOptions())
	if err == nil {
		t.Fatal("Format() unexpectedly accepted malformed input")
	}
	var syntaxErr *SyntaxError
	if !errors.As(err, &syntaxErr) {
		t.Fatalf("Format() error type = %T, want *SyntaxError", err)
	}
	if !strings.Contains(err.Error(), "line 1") {
		t.Fatalf("Format() error = %q, want source location", err)
	}
}

func TestFormatUsesRuntimeLexerTokenBoundaries(t *testing.T) {
	tests := []struct {
		name   string
		source string
		want   string
	}{
		{
			name:   "operator adjacent to numbers",
			source: "10 [1-2] exec 1+2",
			want:   "10 [ 1 - 2 ] exec 1 + 2\n",
		},
		{
			name:   "namespace remains part of symbols",
			source: "foo.bar 'foo.bar . .s .S '. '.s '.S",
			want:   "foo.bar 'foo.bar . .s .S '. '.s '.S\n",
		},
		{
			name:   "slash is always an operator",
			source: "1/2 foo/bar 1*2",
			want:   "1 / 2 foo / bar 1 *2\n",
		},
		{
			name:   "comments terminate symbols",
			source: "foo/* exact */bar",
			want:   "foo /* exact */ bar\n",
		},
		{
			name:   "empty capture",
			source: "[||]",
			want:   "[ || ]\n",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got, err := Format([]byte(test.source), DefaultOptions())
			if err != nil {
				t.Fatalf("Format() error = %v", err)
			}
			if string(got) != test.want {
				t.Fatalf("Format() = %q, want %q", got, test.want)
			}

			again, err := Format(got, DefaultOptions())
			if err != nil {
				t.Fatalf("Format(formatted) error = %v", err)
			}
			if !bytes.Equal(again, got) {
				t.Fatalf("formatter is not idempotent: first %q, second %q", got, again)
			}
		})
	}
}

func TestFormatRejectsRuntimeMalformedTokenAdjacency(t *testing.T) {
	for _, source := range []string{"1foo", "1..2"} {
		t.Run(source, func(t *testing.T) {
			_, err := Format([]byte(source), DefaultOptions())
			if err == nil {
				t.Fatalf("Format(%q) unexpectedly succeeded", source)
			}
			var syntaxErr *SyntaxError
			if !errors.As(err, &syntaxErr) {
				t.Fatalf("Format(%q) error type = %T, want *SyntaxError", source, err)
			}
			if !strings.Contains(err.Error(), "malformed number literal") {
				t.Fatalf("Format(%q) error = %q, want malformed-number diagnostic", source, err)
			}
		})
	}
}

func TestFormatRejectsMalformedNamespaceNames(t *testing.T) {
	for _, source := range []string{
		"foo:bar", "$foo.bar", "foo..bar", ".foo", "foo.", ".5",
	} {
		t.Run(source, func(t *testing.T) {
			_, err := Format([]byte(source), DefaultOptions())
			if err == nil {
				t.Fatalf("Format(%q) unexpectedly succeeded", source)
			}
		})
	}
}

func TestDisabledFormatLeavesSourceUntouched(t *testing.T) {
	source := []byte("[   malformed")
	options := DefaultOptions()
	options.Disabled = true

	got, err := Format(source, options)
	if err != nil {
		t.Fatalf("Format() error = %v", err)
	}
	if !bytes.Equal(got, source) {
		t.Fatalf("Format() = %q, want unchanged %q", got, source)
	}
}

func TestDiscoverConfigParentToChildPrecedence(t *testing.T) {
	root := t.TempDir()
	child := filepath.Join(root, "examples")
	grandchild := filepath.Join(child, "generated")
	if err := os.MkdirAll(grandchild, 0o755); err != nil {
		t.Fatal(err)
	}
	writeTestFile(t, filepath.Join(root, ConfigFileName), `
indent_width = 2
indent_style = "tabs"
delimiter_spacing = "spaced"
disable = true
`)
	writeTestFile(t, filepath.Join(child, ConfigFileName), `
indent_width = 6
delimiter_spacing = "compact"
`)
	writeTestFile(t, filepath.Join(grandchild, ConfigFileName), "disable = false # allow generated files\n")

	base := Options{
		IndentWidth:      8,
		IndentStyle:      IndentSpaces,
		DelimiterSpacing: DelimiterSpaced,
	}
	got, err := OptionsForPath(filepath.Join(grandchild, "program.toy"), base)
	if err != nil {
		t.Fatalf("OptionsForPath() error = %v", err)
	}
	if got.IndentWidth != 6 || got.IndentStyle != IndentTabs || got.DelimiterSpacing != DelimiterCompact || got.Disabled {
		t.Fatalf("OptionsForPath() = %+v, want child overrides with inherited tab style", got)
	}
}

func TestParseConfigReportsInvalidSettings(t *testing.T) {
	tests := []string{
		"indent_width = 0\n",
		"indent_style = \"elastic\"\n",
		"delimiter_spacing = \"wide\"\n",
		"unknown = true\n",
	}
	for _, source := range tests {
		if _, err := ParseConfig(strings.NewReader(source)); err == nil {
			t.Errorf("ParseConfig(%q) unexpectedly succeeded", source)
		}
	}
}

func TestParseConfigAcceptsSingularTabStyle(t *testing.T) {
	config, err := ParseConfig(strings.NewReader("indent_style = \"tab\"\n"))
	if err != nil {
		t.Fatalf("ParseConfig() error = %v", err)
	}
	options := config.Apply(DefaultOptions())
	if options.IndentStyle != IndentTabs {
		t.Fatalf("IndentStyle = %q, want %q", options.IndentStyle, IndentTabs)
	}
}

func readFixture(t *testing.T, name string) []byte {
	t.Helper()
	content, err := os.ReadFile(filepath.Join("testdata", name))
	if err != nil {
		t.Fatal(err)
	}
	return content
}

func writeTestFile(t *testing.T, path, content string) {
	t.Helper()
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
}
