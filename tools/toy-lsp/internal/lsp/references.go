package lsp

import (
	"strings"

	"toy-lsp/internal/analysis"
)

type wordOccurrence struct {
	URI         string
	Range       analysis.Range
	RenameRange analysis.Range
}

func (s *Server) wordOccurrences(target definitionTarget, includeDeclaration bool) []wordOccurrence {
	occurrences := make([]wordOccurrence, 0)
	seen := make(map[occurrenceKey]bool)
	appendOccurrence := func(occurrence wordOccurrence) {
		key := occurrenceKey{
			URI:   occurrence.URI,
			Range: occurrence.Range,
		}
		if seen[key] {
			return
		}
		seen[key] = true
		occurrences = append(occurrences, occurrence)
	}

	if includeDeclaration {
		appendOccurrence(wordOccurrence{
			URI:         target.URI,
			Range:       target.Symbol.SelectionRange,
			RenameRange: target.Symbol.SelectionRange,
		})
	}

	for _, doc := range s.docs.all() {
		for _, token := range doc.Index.WordTokens {
			if !token.IsWord() {
				continue
			}
			resolved, ok := s.resolveWord(doc, token.Text)
			if !ok || !sameDefinition(resolved, target) {
				continue
			}
			appendOccurrence(wordOccurrence{
				URI:         doc.URI,
				Range:       token.Range,
				RenameRange: localNameRange(token, target.Symbol.Name),
			})
		}

		for _, exported := range doc.Index.Exports {
			resolved, ok := s.exportedDefinition(doc, exported.Name)
			if !ok || !sameDefinition(resolved, target) {
				continue
			}
			appendOccurrence(wordOccurrence{
				URI:         doc.URI,
				Range:       exported.Range,
				RenameRange: exported.Range,
			})
		}
	}

	return occurrences
}

func sameDefinition(a, b definitionTarget) bool {
	return a.URI == b.URI && a.Symbol.SelectionRange == b.Symbol.SelectionRange
}

func localNameRange(token analysis.Token, localName string) analysis.Range {
	if token.Text == localName || !strings.HasSuffix(token.Text, "."+localName) {
		return token.Range
	}
	rng := token.Range
	rng.Start = rng.End
	rng.Start.Character -= len(localName)
	return rng
}

type occurrenceKey struct {
	URI   string
	Range analysis.Range
}
