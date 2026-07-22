package lsp

import (
	"path/filepath"
	"runtime"
	"strings"

	"toy-tools/internal/analysis"
)

type wordOccurrence struct {
	URI         string
	Range       analysis.Range
	RenameRange analysis.Range
}

func (s *Server) wordOccurrences(target definitionTarget, includeDeclaration bool) []wordOccurrence {
	return s.wordOccurrencesWithResolver(target, includeDeclaration, newResolutionCache(s.docs))
}

func (s *Server) wordOccurrencesWithResolver(target definitionTarget, includeDeclaration bool, resolver *resolutionCache) []wordOccurrence {
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
			resolved, ok := resolver.resolveWord(doc, token.Text)
			if !ok || !sameDefinition(resolved, target) {
				continue
			}
			appendOccurrence(wordOccurrence{
				URI:         doc.URI,
				Range:       token.Range,
				RenameRange: localNameRange(token, target.Symbol.Name),
			})
		}

		for _, private := range doc.Index.Privates {
			resolved, ok := resolver.packageDefinition(doc, private.Name)
			if !ok || !sameDefinition(resolved, target) {
				continue
			}
			appendOccurrence(wordOccurrence{
				URI:         doc.URI,
				Range:       private.Range,
				RenameRange: private.Range,
			})
		}
	}

	return occurrences
}

// resolutionCache memoizes the directory/package view used by one references
// or rename request.  Package resolution is directory-scoped, but the old
// implementation re-read every package directory for every token.  A large
// workspace therefore turned a rename into repeated directory scans and file
// stats, delaying the workspace edit on the client.
type resolutionCache struct {
	store         *documentStore
	packages      map[string][]*document
	names         map[string]string
	definitions   map[string]map[string]definitionTarget
	privateNames  map[string]map[string]bool
	packageImport map[string]map[string]string
}

func newResolutionCache(store *documentStore) *resolutionCache {
	return &resolutionCache{
		store:         store,
		packages:      make(map[string][]*document),
		names:         make(map[string]string),
		definitions:   make(map[string]map[string]definitionTarget),
		privateNames:  make(map[string]map[string]bool),
		packageImport: make(map[string]map[string]string),
	}
}

func (r *resolutionCache) packageDocuments(directory string) []*document {
	key := packageDirectoryKey(directory)
	if key == "" {
		return nil
	}
	if documents, ok := r.packages[key]; ok {
		return documents
	}

	documents := r.store.packageDocuments(directory)
	r.packages[key] = documents

	definitions := make(map[string]definitionTarget)
	privateNames := make(map[string]bool)
	for _, doc := range documents {
		for name, symbol := range doc.Index.Definitions {
			if _, exists := definitions[name]; !exists {
				definitions[name] = definitionTarget{URI: doc.URI, Symbol: symbol}
			}
		}
		for _, private := range doc.Index.Privates {
			privateNames[private.Name] = true
		}
	}
	r.definitions[key] = definitions
	r.privateNames[key] = privateNames
	return documents
}

func (r *resolutionCache) packageName(directory string) (string, bool) {
	key := packageDirectoryKey(directory)
	if key == "" {
		return "", false
	}
	if name, ok := r.names[key]; ok {
		return name, name != ""
	}
	name := ""
	for _, doc := range r.packageDocuments(directory) {
		if doc.Index.PackageName != "" {
			name = doc.Index.PackageName
			break
		}
	}
	r.names[key] = name
	return name, name != ""
}

func (r *resolutionCache) packageDefinition(origin *document, name string) (definitionTarget, bool) {
	if origin == nil || origin.Path == "" {
		return definitionTarget{}, false
	}
	directory := filepath.Dir(origin.Path)
	key := packageDirectoryKey(directory)
	if key == "" {
		return definitionTarget{}, false
	}
	definition, ok := r.definitionsFor(directory)[name]
	return definition, ok
}

func (r *resolutionCache) definitionsFor(directory string) map[string]definitionTarget {
	r.packageDocuments(directory)
	return r.definitions[packageDirectoryKey(directory)]
}

func (r *resolutionCache) resolveWord(doc *document, word string) (definitionTarget, bool) {
	if doc == nil || doc.Path == "" {
		return definitionTarget{}, false
	}
	if target, ok := r.packageDefinition(doc, word); ok {
		return target, true
	}
	if !strings.Contains(word, ".") {
		return definitionTarget{}, false
	}
	return r.resolveQualifiedWord(doc, word)
}

func (r *resolutionCache) resolveQualifiedWord(origin *document, word string) (definitionTarget, bool) {
	if origin == nil || origin.Path == "" {
		return definitionTarget{}, false
	}
	imports := r.importsForDirectory(filepath.Dir(origin.Path))
	for alias, directory := range imports {
		prefix := alias + "."
		if !strings.HasPrefix(word, prefix) {
			continue
		}
		localName := strings.TrimPrefix(word, prefix)
		if localName == "" || strings.Contains(localName, ".") {
			return definitionTarget{}, false
		}
		return r.publicPackageDefinition(directory, localName)
	}
	return definitionTarget{}, false
}

func (r *resolutionCache) importsForDirectory(directory string) map[string]string {
	key := packageDirectoryKey(directory)
	if key == "" {
		return nil
	}
	if imports, ok := r.packageImport[key]; ok {
		return imports
	}

	imports := make(map[string]string)
	for _, importer := range r.packageDocuments(directory) {
		for _, imported := range importer.Index.Imports {
			targetDirectory, ok := r.store.resolvePackageDirectory(importer, imported.Path)
			if !ok {
				continue
			}
			alias := imported.Alias
			if alias == "" {
				alias, ok = r.packageName(targetDirectory)
				if !ok {
					continue
				}
			}
			if _, exists := imports[alias]; !exists {
				imports[alias] = packageDirectoryKey(targetDirectory)
			}
		}
	}
	r.packageImport[key] = imports
	return imports
}

func (r *resolutionCache) publicPackageDefinition(directory, name string) (definitionTarget, bool) {
	key := packageDirectoryKey(directory)
	if key == "" {
		return definitionTarget{}, false
	}
	r.packageDocuments(directory)
	if r.privateNames[key][name] {
		return definitionTarget{}, false
	}
	target, ok := r.definitions[key][name]
	return target, ok
}

func packageDirectoryKey(directory string) string {
	if directory == "" {
		return ""
	}
	absolute, err := filepath.Abs(directory)
	if err != nil {
		return ""
	}
	absolute = filepath.Clean(absolute)
	if runtime.GOOS == "windows" {
		return strings.ToLower(absolute)
	}
	return absolute
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
