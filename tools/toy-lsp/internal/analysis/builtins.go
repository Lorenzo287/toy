package analysis

import "strings"

type Hover struct {
	Contents string
	Range    Range
}

type builtinDoc struct {
	StackEffect string
	Syntax      string
	Description string
}

func LookupHover(index DocumentIndex, pos Position) (Hover, bool) {
	word, tok, ok := lookupTokenAt(index, pos)
	if !ok {
		return Hover{}, false
	}

	if tok.Kind == tokenKindVariable {
		local, ok := lookupLocalBinding(index, word, pos)
		if !ok {
			return Hover{}, false
		}
		return Hover{
			Contents: "```toy\n$" + local.Name + "\n```\nLocal binding from `| " + local.Name + " |`.",
			Range:    tok.Range,
		}, true
	}
	if tok.Kind == tokenKindSymbol {
		sym, ok := index.Definitions[word]
		if !ok || (!sameRange(tok.Range, sym.SelectionRange) && !isPrivacyRange(index, word, tok.Range)) {
			return Hover{}, false
		}
		return DefinitionHover(word, sym, tok.Range), true
	}

	if doc, ok := builtinDocs[word]; ok {
		body := doc.Description
		switch {
		case doc.StackEffect != "":
			body = "stack: `" + doc.StackEffect + "`\n\n" + body
		case doc.Syntax != "":
			body = "syntax: `" + doc.Syntax + "`\n\n" + body
		}

		return Hover{
			Contents: strings.TrimSpace("```toy\n" + word + "\n```\n" + body),
			Range:    tok.Range,
		}, true
	}

	if sym, ok := index.Definitions[word]; ok {
		return DefinitionHover(word, sym, tok.Range), true
	}

	return Hover{}, false
}

func DefinitionHover(displayName string, sym Symbol, rng Range) Hover {
	body := sym.Detail
	switch {
	case sym.Doc != "" && sym.StackEffect != "":
		body = sym.Doc + "\n\nstack: `" + sym.StackEffect + "`"
	case sym.Doc != "":
		body = sym.Doc
	case sym.StackEffect != "":
		body = "stack: `" + sym.StackEffect + "`"
	}

	return Hover{
		Contents: strings.TrimSpace("```toy\n" + displayName + "\n```\n" + body),
		Range:    rng,
	}
}
