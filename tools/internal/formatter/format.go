package formatter

import (
	"bytes"
	"fmt"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"

	"toy-tools/internal/parser"
)

// IndentStyle controls how one structural indentation level is written.
type IndentStyle string

const (
	IndentSpaces IndentStyle = "spaces"
	IndentTabs   IndentStyle = "tabs"
	indentTab    IndentStyle = "tab"
)

// DelimiterSpacing controls spaces just inside non-empty delimiters.
type DelimiterSpacing string

const (
	DelimiterSpaced  DelimiterSpacing = "spaced"
	DelimiterCompact DelimiterSpacing = "compact"
)

// Options controls formatting. Zero-valued fields use DefaultOptions.
type Options struct {
	IndentWidth      int
	IndentStyle      IndentStyle
	DelimiterSpacing DelimiterSpacing
	Disabled         bool
}

// DefaultOptions returns the built-in formatting policy.
func DefaultOptions() Options {
	return Options{
		IndentWidth:      4,
		IndentStyle:      IndentSpaces,
		DelimiterSpacing: DelimiterSpaced,
	}
}

// SyntaxError reports a location at which Tree-sitter recovered from invalid
// Toy source. Locations are one-based for human-facing diagnostics.
type SyntaxError struct {
	Line   int
	Column int
	Kind   string
}

func (e *SyntaxError) Error() string {
	return fmt.Sprintf("invalid Toy syntax at line %d, column %d (%s)", e.Line, e.Column, e.Kind)
}

type tokenRole uint8

const (
	roleAtom tokenRole = iota
	roleOpen
	roleClose
)

type sourceToken struct {
	start       int
	end         int
	role        tokenRole
	containerID int
	nonEmpty    bool
	// forceLineBreak is set for a multiline vector whose opener is followed
	// by a newline. Inline vectors retain their original line layout.
	forceLineBreak bool
}

// Format formats Toy source without changing its line layout. It preserves
// every existing line break and the contents of strings and comments, while
// normalizing indentation and horizontal whitespace between source tokens.
func Format(source []byte, options Options) ([]byte, error) {
	options, err := normalizeOptions(options)
	if err != nil {
		return nil, err
	}
	if options.Disabled {
		return bytes.Clone(source), nil
	}

	tree, err := parser.Parse(source)
	if err != nil {
		return nil, fmt.Errorf("parse Toy source: %w", err)
	}
	defer tree.Close()

	root := tree.RootNode()
	if bad := firstInvalidNode(root); bad != nil {
		position := bad.StartPosition()
		kind := bad.Kind()
		if bad.IsMissing() {
			kind = "missing " + kind
		}
		return nil, &SyntaxError{
			Line:   int(position.Row) + 1,
			Column: int(position.Column) + 1,
			Kind:   kind,
		}
	}

	tokens, err := lexSourceTokens(source)
	if err != nil {
		return nil, err
	}
	markMultilineVectorClosers(source, tokens)
	formatted := render(source, tokens, options)
	if err := verifyTokenSequence(source, tokens, formatted); err != nil {
		return nil, err
	}
	return formatted, nil
}

func normalizeOptions(options Options) (Options, error) {
	defaults := DefaultOptions()
	if options.IndentWidth == 0 {
		options.IndentWidth = defaults.IndentWidth
	}
	if options.IndentStyle == "" {
		options.IndentStyle = defaults.IndentStyle
	}
	if options.IndentStyle == indentTab {
		options.IndentStyle = IndentTabs
	}
	if options.DelimiterSpacing == "" {
		options.DelimiterSpacing = defaults.DelimiterSpacing
	}

	if options.IndentWidth < 1 {
		return Options{}, fmt.Errorf("indent width must be at least 1")
	}
	if options.IndentStyle != IndentSpaces && options.IndentStyle != IndentTabs {
		return Options{}, fmt.Errorf("invalid indent style %q (want %q or %q)", options.IndentStyle, IndentSpaces, IndentTabs)
	}
	if options.DelimiterSpacing != DelimiterSpaced && options.DelimiterSpacing != DelimiterCompact {
		return Options{}, fmt.Errorf("invalid delimiter spacing %q (want %q or %q)", options.DelimiterSpacing, DelimiterSpaced, DelimiterCompact)
	}
	return options, nil
}

func firstInvalidNode(node *tree_sitter.Node) *tree_sitter.Node {
	if node.IsError() || node.IsMissing() {
		return node
	}
	for i := uint(0); i < node.ChildCount(); i++ {
		if bad := firstInvalidNode(node.Child(i)); bad != nil {
			return bad
		}
	}
	return nil
}

func render(source []byte, tokens []sourceToken, options Options) []byte {
	var output bytes.Buffer
	depth := 0
	previousEnd := 0
	var previous *sourceToken

	for i := range tokens {
		token := &tokens[i]
		gap := source[previousEnd:token.start]
		lineBreaks := appendLineBreaks(&output, gap)
		if token.forceLineBreak && lineBreaks == 0 {
			output.Write(preferredLineBreak(source))
			lineBreaks = 1
		}

		if token.role == roleClose && depth > 0 {
			depth--
		}
		if lineBreaks > 0 {
			writeIndent(&output, depth, options)
		} else if previous != nil && needsHorizontalSpace(*previous, *token, options) {
			output.WriteByte(' ')
		}

		output.Write(source[token.start:token.end])
		if token.role == roleOpen {
			depth++
		}
		previousEnd = token.end
		previous = token
	}

	trailingBreaks := appendLineBreaks(&output, source[previousEnd:])
	if trailingBreaks == 0 {
		output.Write(preferredLineBreak(source))
	}
	return output.Bytes()
}

func needsHorizontalSpace(previous, current sourceToken, options Options) bool {
	if previous.role == roleOpen && previous.containerID == current.containerID && current.role == roleClose {
		return false
	}
	if previous.role == roleOpen {
		return previous.nonEmpty && options.DelimiterSpacing == DelimiterSpaced
	}
	if current.role == roleClose {
		return current.nonEmpty && options.DelimiterSpacing == DelimiterSpaced
	}
	return true
}

func appendLineBreaks(output *bytes.Buffer, whitespace []byte) int {
	count := 0
	for i := 0; i < len(whitespace); i++ {
		switch whitespace[i] {
		case '\r':
			output.WriteByte('\r')
			if i+1 < len(whitespace) && whitespace[i+1] == '\n' {
				i++
				output.WriteByte('\n')
			}
			count++
		case '\n':
			output.WriteByte('\n')
			count++
		}
	}
	return count
}

func writeIndent(output *bytes.Buffer, depth int, options Options) {
	if options.IndentStyle == IndentTabs {
		output.Write(bytes.Repeat([]byte{'\t'}, depth))
		return
	}
	output.Write(bytes.Repeat([]byte{' '}, depth*options.IndentWidth))
}

func preferredLineBreak(source []byte) []byte {
	for i, b := range source {
		switch b {
		case '\r':
			if i+1 < len(source) && source[i+1] == '\n' {
				return []byte("\r\n")
			}
			return []byte("\r")
		case '\n':
			return []byte("\n")
		}
	}
	return []byte("\n")
}
