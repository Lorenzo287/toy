package lsp

import (
	"path/filepath"
	"strings"

	"toy-lsp/internal/analysis"
)

type definitionTarget struct {
	URI    string
	Symbol analysis.Symbol
}

func (s *Server) resolveSourceReference(uri string, position analysis.Position) (string, bool) {
	doc, ok := s.docs.get(uri)
	if !ok {
		return "", false
	}
	reference, ok := analysis.LookupSourceReferenceAt(doc.Index, position)
	if !ok || reference.Kind != analysis.SourceReferencePackage {
		return "", false
	}
	directory, ok := s.docs.resolvePackageDirectory(doc, reference.Target)
	if !ok {
		return "", false
	}
	for _, target := range s.docs.packageDocuments(directory) {
		if target.Index.PackageName != "" {
			return target.URI, true
		}
	}
	return "", false
}

func (s *Server) resolveDefinition(uri string, position analysis.Position) (definitionTarget, bool) {
	doc, ok := s.docs.get(uri)
	if !ok {
		return definitionTarget{}, false
	}

	if symbol, ok := analysis.LookupDefinition(doc.Index, position); ok {
		return definitionTarget{URI: doc.URI, Symbol: symbol}, true
	}

	token, ok := analysis.LookupTokenAt(doc.Index, position)
	if !ok {
		return definitionTarget{}, false
	}
	return s.resolveWord(doc, token.Text)
}

func (s *Server) resolveWord(doc *document, word string) (definitionTarget, bool) {
	if doc == nil {
		return definitionTarget{}, false
	}
	if target, ok := s.packageDefinition(doc, word); ok {
		return target, true
	}
	if strings.Contains(word, ".") {
		return s.resolveQualifiedWord(doc, word)
	}
	return definitionTarget{}, false
}

func (s *Server) packageDefinition(origin *document, name string) (definitionTarget, bool) {
	if origin == nil || origin.Path == "" {
		return definitionTarget{}, false
	}
	directory := filepath.Dir(origin.Path)
	for _, doc := range s.docs.packageDocuments(directory) {
		if symbol, ok := doc.Index.Definitions[name]; ok {
			return definitionTarget{URI: doc.URI, Symbol: symbol}, true
		}
	}
	return definitionTarget{}, false
}

func (s *Server) resolveQualifiedWord(origin *document, word string) (definitionTarget, bool) {
	for _, importer := range s.docs.packageDocuments(filepath.Dir(origin.Path)) {
		for _, imported := range importer.Index.Imports {
			directory, ok := s.docs.resolvePackageDirectory(importer, imported.Path)
			if !ok {
				continue
			}
			name := imported.Alias
			if name == "" {
				name, ok = s.docs.packageName(directory)
				if !ok {
					continue
				}
			}
			prefix := name + "."
			if !strings.HasPrefix(word, prefix) {
				continue
			}
			localName := strings.TrimPrefix(word, prefix)
			if localName == "" || strings.Contains(localName, ".") {
				return definitionTarget{}, false
			}
			return s.publicPackageDefinition(directory, localName)
		}
	}
	return definitionTarget{}, false
}

func (s *Server) publicPackageDefinition(directory, localName string) (definitionTarget, bool) {
	documents := s.docs.packageDocuments(directory)
	for _, doc := range documents {
		if !doc.Index.IsPublic(localName) {
			return definitionTarget{}, false
		}
	}
	for _, doc := range documents {
		if symbol, ok := doc.Index.Definitions[localName]; ok {
			return definitionTarget{URI: doc.URI, Symbol: symbol}, true
		}
	}
	return definitionTarget{}, false
}
