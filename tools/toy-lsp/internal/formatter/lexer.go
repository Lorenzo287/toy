package formatter

import (
	"bytes"
	"fmt"
)

// sourceLexer mirrors the token-boundary rules in src/tf_lexer.c. Tree-sitter
// remains the formatter's structural parser, but its lexical choices are not
// lossless for adjacency such as 1-2 or foo.bar. Rendering from these spans
// ensures that inserting whitespace cannot split or merge runtime tokens.
type sourceLexer struct {
	source          []byte
	pos             int
	tokens          []sourceToken
	nextContainerID int
}

func lexSourceTokens(source []byte) ([]sourceToken, error) {
	lexer := sourceLexer{
		source: source,
		tokens: make([]sourceToken, 0, 32),
	}
	if _, err := lexer.scanUntil(0); err != nil {
		return nil, err
	}
	return lexer.tokens, nil
}

func verifyTokenSequence(source []byte, tokens []sourceToken, formatted []byte) error {
	formattedTokens, err := lexSourceTokens(formatted)
	if err != nil {
		return fmt.Errorf("formatter produced invalid Toy source: %w", err)
	}
	if len(formattedTokens) != len(tokens) {
		return fmt.Errorf("formatter changed Toy token count from %d to %d", len(tokens), len(formattedTokens))
	}
	for index := range tokens {
		before := tokens[index]
		after := formattedTokens[index]
		if before.role != after.role || before.containerID != after.containerID || before.nonEmpty != after.nonEmpty ||
			!bytes.Equal(source[before.start:before.end], formatted[after.start:after.end]) {
			return fmt.Errorf("formatter changed Toy token %d", index+1)
		}
	}
	return nil
}

// scanUntil scans a top-level program or the contents of one collection. It
// returns the number of runtime values read at this level; comments do not
// count, which lets map literals enforce their key/value shape like C does.
func (lexer *sourceLexer) scanUntil(terminator byte) (int, error) {
	itemCount := 0
	for {
		lexer.skipWhitespace()
		if lexer.pos == len(lexer.source) {
			if terminator != 0 {
				return 0, lexer.errorAt(lexer.pos, fmt.Sprintf("expected %q but reached end of program", terminator))
			}
			return itemCount, nil
		}

		current := lexer.source[lexer.pos]
		if current == 0 {
			return 0, lexer.errorAt(lexer.pos, "unexpected NUL byte")
		}
		if current == '\\' {
			lexer.scanLineComment()
			continue
		}
		if current == '/' && lexer.peek(1) == '*' {
			if err := lexer.scanBlockComment(); err != nil {
				return 0, err
			}
			continue
		}
		if terminator != 0 && current == terminator {
			return itemCount, nil
		}

		if terminator == 0 && (current == ']' || current == ')' || current == '}') {
			return 0, lexer.errorAt(lexer.pos, fmt.Sprintf("unexpected %q", current))
		}

		var err error
		switch {
		case isASCIIDigit(current) || lexer.startsSignedNumber():
			err = lexer.scanNumber()
		case current == '[':
			err = lexer.scanContainer(1, ']', false)
		case current == '(':
			err = lexer.scanContainer(1, ')', false)
		case current == '{':
			err = lexer.scanContainer(1, '}', true)
		case current == '#':
			if lexer.peek(1) != '{' {
				err = lexer.errorAt(lexer.pos, "expected '{' after '#'")
			} else {
				err = lexer.scanContainer(2, '}', false)
			}
		case current == '|':
			err = lexer.scanContainer(1, '|', false)
		case current == '$':
			err = lexer.scanPrefixedSymbol('$')
		case current == '\'':
			err = lexer.scanPrefixedSymbol('\'')
		case current == '/':
			lexer.appendAtom(lexer.pos, lexer.pos+1)
			lexer.pos++
		case (current == '-' || current == '+') &&
			(isASCIIDigit(lexer.peek(1)) ||
				(lexer.peek(1) == '.' && isASCIIDigit(lexer.peek(2)))):
			lexer.appendAtom(lexer.pos, lexer.pos+1)
			lexer.pos++
		case isSymbolByte(current):
			err = lexer.scanSymbol()
		case current == '"':
			err = lexer.scanString()
		default:
			err = lexer.errorAt(lexer.pos, fmt.Sprintf("unexpected input byte %q", current))
		}
		if err != nil {
			return 0, err
		}
		itemCount++
	}
}

func (lexer *sourceLexer) scanContainer(openLength int, terminator byte, requirePairs bool) error {
	start := lexer.pos
	containerID := lexer.nextContainerID
	lexer.nextContainerID++
	openIndex := len(lexer.tokens)
	lexer.tokens = append(lexer.tokens, sourceToken{
		start:       start,
		end:         start + openLength,
		role:        roleOpen,
		containerID: containerID,
	})
	lexer.pos += openLength
	contentTokenStart := len(lexer.tokens)

	itemCount, err := lexer.scanUntil(terminator)
	if err != nil {
		return err
	}
	if requirePairs && itemCount%2 != 0 {
		return lexer.errorAt(start, "map literal expected key/value pairs")
	}

	closeStart := lexer.pos
	lexer.pos++
	nonEmpty := len(lexer.tokens) != contentTokenStart
	lexer.tokens[openIndex].nonEmpty = nonEmpty
	lexer.tokens = append(lexer.tokens, sourceToken{
		start:       closeStart,
		end:         lexer.pos,
		role:        roleClose,
		containerID: containerID,
		nonEmpty:    nonEmpty,
	})
	return nil
}

func (lexer *sourceLexer) scanNumber() error {
	start := lexer.pos
	if lexer.peek(0) == '-' || lexer.peek(0) == '+' {
		lexer.pos++
	}

	digitSeen := false
	for isASCIIDigit(lexer.peek(0)) {
		digitSeen = true
		lexer.pos++
	}
	if lexer.peek(0) == '.' {
		lexer.pos++
		for isASCIIDigit(lexer.peek(0)) {
			digitSeen = true
			lexer.pos++
		}
	}
	if !digitSeen {
		return lexer.errorAt(start, "malformed number literal")
	}

	if lexer.peek(0) == 'e' || lexer.peek(0) == 'E' {
		lexer.pos++
		if lexer.peek(0) == '-' || lexer.peek(0) == '+' {
			lexer.pos++
		}
		if !isASCIIDigit(lexer.peek(0)) {
			return lexer.errorAt(start, "malformed number literal")
		}
		for isASCIIDigit(lexer.peek(0)) {
			lexer.pos++
		}
	}

	next := lexer.peek(0)
	if isASCIIAlpha(next) || next == '_' || next == '.' {
		return lexer.errorAt(start, "malformed number literal")
	}
	lexer.appendAtom(start, lexer.pos)
	return nil
}

func (lexer *sourceLexer) scanSymbol() error {
	start := lexer.pos
	for isSymbolByte(lexer.peek(0)) {
		lexer.pos++
	}
	if lexer.pos == start {
		return lexer.errorAt(start, "expected symbol")
	}
	if err := lexer.validateNamespaceName(start, lexer.pos); err != nil {
		return err
	}
	lexer.appendAtom(start, lexer.pos)
	return nil
}

func (lexer *sourceLexer) scanPrefixedSymbol(prefix byte) error {
	start := lexer.pos
	lexer.pos++
	symbolStart := lexer.pos
	if lexer.peek(0) == '/' {
		lexer.pos++
	} else {
		for isSymbolByte(lexer.peek(0)) {
			lexer.pos++
		}
	}
	if lexer.pos == symbolStart {
		return lexer.errorAt(start, fmt.Sprintf("expected symbol name after %q", prefix))
	}
	if err := lexer.validateNamespaceName(symbolStart, lexer.pos); err != nil {
		return err
	}
	if prefix == '$' {
		name := string(lexer.source[symbolStart:lexer.pos])
		if name == "true" || name == "false" {
			return lexer.errorAt(start, "expected variable name after '$'")
		}
		if name == "/" || bytes.Contains(lexer.source[symbolStart:lexer.pos], []byte(".")) {
			return lexer.errorAt(start, "capture names cannot be namespace-qualified")
		}
	}
	lexer.appendAtom(start, lexer.pos)
	return nil
}

func (lexer *sourceLexer) validateNamespaceName(start, end int) error {
	name := lexer.source[start:end]
	if len(name) == 1 && name[0] == '/' {
		return nil
	}
	if len(name) == 1 && name[0] == '.' ||
		len(name) == 2 && name[0] == '.' && (name[1] == 's' || name[1] == 'S') {
		return nil
	}
	for index := 0; index < len(name); index++ {
		if name[index] != '.' {
			continue
		}
		if index == 0 || index+1 >= len(name) || name[index+1] == '.' {
			return lexer.errorAt(start+index, "namespace separator '.' must appear between names")
		}
	}
	return nil
}

func (lexer *sourceLexer) scanString() error {
	start := lexer.pos
	lexer.pos++
	for lexer.pos < len(lexer.source) {
		current := lexer.source[lexer.pos]
		if current == 0 {
			return lexer.errorAt(start, "unterminated string literal")
		}
		if current == '"' {
			lexer.pos++
			lexer.appendAtom(start, lexer.pos)
			return nil
		}
		if current != '\\' {
			lexer.pos++
			continue
		}

		lexer.pos++
		if lexer.pos == len(lexer.source) {
			return lexer.errorAt(start, "unterminated string literal")
		}
		escape := lexer.source[lexer.pos]
		switch escape {
		case 'n', 'r', 't', '"', '\\':
			lexer.pos++
		case 'x':
			if !isASCIIHex(lexer.peek(1)) || !isASCIIHex(lexer.peek(2)) {
				return lexer.errorAt(lexer.pos-1, "invalid hexadecimal escape")
			}
			lexer.pos += 3
		default:
			return lexer.errorAt(lexer.pos-1, fmt.Sprintf("unknown string escape \\%c", escape))
		}
	}
	return lexer.errorAt(start, "unterminated string literal")
}

func (lexer *sourceLexer) scanLineComment() {
	start := lexer.pos
	for lexer.pos < len(lexer.source) && lexer.source[lexer.pos] != '\n' {
		lexer.pos++
	}
	lexer.appendAtom(start, lexer.pos)
}

func (lexer *sourceLexer) scanBlockComment() error {
	start := lexer.pos
	lexer.pos += 2
	for lexer.pos < len(lexer.source) {
		if lexer.source[lexer.pos] == '*' && lexer.peek(1) == '/' {
			lexer.pos += 2
			lexer.appendAtom(start, lexer.pos)
			return nil
		}
		lexer.pos++
	}
	return lexer.errorAt(start, "unterminated block comment")
}

func (lexer *sourceLexer) startsSignedNumber() bool {
	current := lexer.peek(0)
	if current != '-' && current != '+' {
		return false
	}
	if !lexer.atTokenBoundary() {
		return false
	}
	return isASCIIDigit(lexer.peek(1))
}

func (lexer *sourceLexer) atTokenBoundary() bool {
	if lexer.pos == 0 {
		return true
	}
	previous := lexer.source[lexer.pos-1]
	if previous == '/' && lexer.pos >= 2 && lexer.source[lexer.pos-2] == '*' {
		return true
	}
	return isASCIIWhitespace(previous) || isStructuralByte(previous) || previous == '\\'
}

func (lexer *sourceLexer) skipWhitespace() {
	for isASCIIWhitespace(lexer.peek(0)) {
		lexer.pos++
	}
}

func (lexer *sourceLexer) appendAtom(start, end int) {
	lexer.tokens = append(lexer.tokens, sourceToken{
		start: start,
		end:   end,
		role:  roleAtom,
	})
}

func (lexer *sourceLexer) peek(offset int) byte {
	position := lexer.pos + offset
	if position < 0 || position >= len(lexer.source) {
		return 0
	}
	return lexer.source[position]
}

func (lexer *sourceLexer) errorAt(offset int, kind string) error {
	line := 1
	column := 1
	if offset > len(lexer.source) {
		offset = len(lexer.source)
	}
	for _, current := range lexer.source[:offset] {
		if current == '\n' {
			line++
			column = 1
		} else {
			column++
		}
	}
	return &SyntaxError{Line: line, Column: column, Kind: kind}
}

func isASCIIWhitespace(value byte) bool {
	switch value {
	case ' ', '\t', '\n', '\v', '\f', '\r':
		return true
	default:
		return false
	}
}

func isStructuralByte(value byte) bool {
	switch value {
	case '[', ']', '{', '}', '|', '(', ')':
		return true
	default:
		return false
	}
}

func isASCIIDigit(value byte) bool {
	return value >= '0' && value <= '9'
}

func isASCIIAlpha(value byte) bool {
	return value >= 'a' && value <= 'z' || value >= 'A' && value <= 'Z'
}

func isASCIIHex(value byte) bool {
	return isASCIIDigit(value) || value >= 'a' && value <= 'f' || value >= 'A' && value <= 'F'
}

func isSymbolByte(value byte) bool {
	return isASCIIAlpha(value) || isASCIIDigit(value) || value == '_' ||
		value == '+' || value == '-' || value == '*' ||
		value == '%' || value == '<' || value == '>' || value == '=' ||
		value == '!' || value == '.' || value == '?'
}
