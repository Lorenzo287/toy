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
	if !ok {
		return "", false
	}

	var path string
	switch reference.Kind {
	case analysis.SourceReferenceModule:
		path, ok = s.docs.resolveModulePath(doc, reference.Target)
	case analysis.SourceReferenceLoad:
		path, ok = s.docs.resolveLoadPath(doc, reference.Target)
	default:
		return "", false
	}
	if !ok {
		return "", false
	}
	target, ok := s.docs.getPath(path)
	if !ok {
		return "", false
	}
	return target.URI, true
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
	if symbol, ok := doc.Index.Definitions[word]; ok {
		return definitionTarget{URI: doc.URI, Symbol: symbol}, true
	}
	if strings.Contains(word, ".") {
		return s.resolveQualifiedWord(doc, word)
	}
	return s.resolveLoadedWord(doc, word, make(map[string]bool))
}

func (s *Server) resolveQualifiedWord(origin *document, word string) (definitionTarget, bool) {
	for _, imported := range origin.Index.Imports {
		if imported.Alias == "" || !strings.HasPrefix(word, imported.Alias+".") {
			continue
		}
		localName := strings.TrimPrefix(word, imported.Alias+".")
		if localName == "" || strings.Contains(localName, ".") {
			return definitionTarget{}, false
		}
		return s.resolveImportedWord(origin, imported.Module, localName)
	}

	modules := make(map[string]*document)
	s.collectReachableModules(origin, modules, make(map[string]bool))
	moduleName := ""
	for candidate := range modules {
		if !strings.HasPrefix(word, candidate+".") || len(candidate) <= len(moduleName) {
			continue
		}
		localName := strings.TrimPrefix(word, candidate+".")
		if localName == "" || strings.Contains(localName, ".") {
			continue
		}
		moduleName = candidate
	}
	if moduleName == "" {
		return definitionTarget{}, false
	}

	return s.exportedDefinition(modules[moduleName], strings.TrimPrefix(word, moduleName+"."))
}

func (s *Server) resolveImportedWord(importer *document, module, localName string) (definitionTarget, bool) {
	path, ok := s.docs.resolveModulePath(importer, module)
	if !ok {
		return definitionTarget{}, false
	}
	target, ok := s.docs.getPath(path)
	if !ok {
		return definitionTarget{}, false
	}
	return s.exportedDefinition(target, localName)
}

func (s *Server) exportedDefinition(doc *document, localName string) (definitionTarget, bool) {
	if doc == nil || !doc.Index.IsExported(localName) {
		return definitionTarget{}, false
	}
	symbol, ok := doc.Index.Definitions[localName]
	if ok {
		return definitionTarget{URI: doc.URI, Symbol: symbol}, true
	}
	return s.resolveLoadedWord(doc, localName, make(map[string]bool))
}

func (s *Server) collectReachableModules(origin *document, modules map[string]*document, visiting map[string]bool) {
	if origin == nil {
		return
	}
	key := origin.URI
	if visiting[key] {
		return
	}
	visiting[key] = true
	defer delete(visiting, key)

	for _, imported := range origin.Index.Imports {
		path, ok := s.docs.resolveModulePath(origin, imported.Module)
		if !ok {
			continue
		}
		target, ok := s.docs.getPath(path)
		if !ok {
			continue
		}
		if _, exists := modules[imported.Module]; !exists {
			modules[imported.Module] = target
		}
		s.collectReachableModules(target, modules, visiting)
	}
}

func (s *Server) resolveLoadedWord(origin *document, name string, visiting map[string]bool) (definitionTarget, bool) {
	if origin == nil || visiting[origin.URI] {
		return definitionTarget{}, false
	}
	visiting[origin.URI] = true
	defer delete(visiting, origin.URI)

	for i := len(origin.Index.Loads) - 1; i >= 0; i-- {
		loaded := origin.Index.Loads[i]
		path, ok := s.docs.resolveLoadPath(origin, loaded.Path)
		if !ok {
			continue
		}
		target, ok := s.docs.getPath(filepath.Clean(path))
		if !ok {
			continue
		}
		if symbol, ok := target.Index.Definitions[name]; ok {
			return definitionTarget{URI: target.URI, Symbol: symbol}, true
		}
		if nested, ok := s.resolveLoadedWord(target, name, visiting); ok {
			return nested, true
		}
	}
	return definitionTarget{}, false
}
