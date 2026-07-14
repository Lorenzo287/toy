package lsp

import (
	"io/fs"
	"net/url"
	"os"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
	"time"

	"toy-lsp/internal/analysis"
)

type document struct {
	URI         string
	Path        string
	Text        string
	Version     int
	Index       analysis.DocumentIndex
	Open        bool
	diskModTime time.Time
	diskSize    int64
}

type documentStore struct {
	docs  map[string]*document
	roots []string
}

func newDocumentStore() *documentStore {
	return &documentStore{
		docs: make(map[string]*document),
	}
}

func (s *documentStore) configureWorkspace(params initializeParams) []error {
	roots := make([]string, 0, len(params.WorkspaceFolders)+1)
	if len(params.WorkspaceFolders) > 0 {
		for _, folder := range params.WorkspaceFolders {
			path, isFile, err := filePathFromURI(folder.URI)
			if err != nil {
				return append([]error(nil), err)
			}
			if isFile {
				roots = append(roots, path)
			}
		}
	} else if params.RootURI != "" {
		path, isFile, err := filePathFromURI(params.RootURI)
		if err != nil {
			return append([]error(nil), err)
		}
		if isFile {
			roots = append(roots, path)
		}
	} else if params.RootPath != "" {
		roots = append(roots, params.RootPath)
	}

	s.roots = uniqueAbsolutePaths(roots)
	errors := make([]error, 0)
	for _, root := range s.roots {
		errors = append(errors, s.indexWorkspaceRoot(root)...)
	}
	return errors
}

func (s *documentStore) open(uri, text string, version int) {
	key, path := normalizedDocumentURI(uri)
	s.docs[key] = &document{
		URI:     key,
		Path:    path,
		Text:    text,
		Version: version,
		Index:   analysis.IndexDocument(text),
		Open:    true,
	}
}

func (s *documentStore) update(uri, text string, version int) {
	key, _ := normalizedDocumentURI(uri)
	doc, ok := s.docs[key]
	if !ok {
		s.open(uri, text, version)
		return
	}

	doc.Text = text
	doc.Version = version
	doc.Index = analysis.IndexDocument(text)
	doc.Open = true
}

func (s *documentStore) close(uri string) {
	key, _ := normalizedDocumentURI(uri)
	doc, ok := s.docs[key]
	if !ok {
		return
	}
	if doc.Path == "" {
		delete(s.docs, key)
		return
	}
	if _, err := os.Stat(doc.Path); err != nil {
		delete(s.docs, key)
		return
	}
	if _, err := s.loadDiskPath(doc.Path, true); err != nil {
		delete(s.docs, key)
	}
}

func (s *documentStore) get(uri string) (*document, bool) {
	key, _ := normalizedDocumentURI(uri)
	doc, ok := s.docs[key]
	if !ok {
		return nil, false
	}
	s.refresh(doc)
	doc, ok = s.docs[key]
	return doc, ok
}

func (s *documentStore) getPath(path string) (*document, bool) {
	absolute, err := filepath.Abs(path)
	if err != nil {
		return nil, false
	}
	absolute = filepath.Clean(absolute)
	uri := fileURIFromPath(absolute)
	if doc, ok := s.docs[uri]; ok {
		s.refresh(doc)
		doc, ok = s.docs[uri]
		return doc, ok
	}

	doc, err := s.loadDiskPath(absolute, false)
	return doc, err == nil
}

func (s *documentStore) all() []*document {
	keys := make([]string, 0, len(s.docs))
	for uri := range s.docs {
		keys = append(keys, uri)
	}
	sort.Strings(keys)
	documents := make([]*document, 0, len(keys))
	for _, uri := range keys {
		if doc, ok := s.get(uri); ok {
			documents = append(documents, doc)
		}
	}
	return documents
}

func (s *documentStore) indexWorkspaceRoot(root string) []error {
	errors := make([]error, 0)
	err := filepath.WalkDir(root, func(path string, entry fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			errors = append(errors, walkErr)
			if entry != nil && entry.IsDir() {
				return filepath.SkipDir
			}
			return nil
		}
		if entry.IsDir() {
			if path != root && ignoredWorkspaceDirectory(entry.Name()) {
				return filepath.SkipDir
			}
			return nil
		}
		if !isToySourcePath(path) {
			return nil
		}
		if _, err := s.loadDiskPath(path, false); err != nil {
			errors = append(errors, err)
		}
		return nil
	})
	if err != nil {
		errors = append(errors, err)
	}
	return errors
}

func (s *documentStore) loadDiskPath(path string, replaceOpen bool) (*document, error) {
	absolute, err := filepath.Abs(path)
	if err != nil {
		return nil, err
	}
	absolute = filepath.Clean(absolute)
	uri := fileURIFromPath(absolute)
	if existing, ok := s.docs[uri]; ok && existing.Open && !replaceOpen {
		return existing, nil
	}

	info, err := os.Stat(absolute)
	if err != nil {
		return nil, err
	}
	source, err := os.ReadFile(absolute)
	if err != nil {
		return nil, err
	}
	doc := &document{
		URI:         uri,
		Path:        absolute,
		Text:        string(source),
		Version:     -1,
		Index:       analysis.IndexDocument(string(source)),
		diskModTime: info.ModTime(),
		diskSize:    info.Size(),
	}
	s.docs[uri] = doc
	return doc, nil
}

func (s *documentStore) refresh(doc *document) {
	if doc == nil || doc.Open || doc.Path == "" {
		return
	}
	info, err := os.Stat(doc.Path)
	if err != nil {
		delete(s.docs, doc.URI)
		return
	}
	if info.Size() == doc.diskSize && info.ModTime().Equal(doc.diskModTime) {
		return
	}
	_, _ = s.loadDiskPath(doc.Path, true)
}

func uniqueAbsolutePaths(paths []string) []string {
	seen := make(map[string]bool)
	result := make([]string, 0, len(paths))
	for _, path := range paths {
		absolute, err := filepath.Abs(path)
		if err != nil {
			continue
		}
		absolute = filepath.Clean(absolute)
		key := absolute
		if runtime.GOOS == "windows" {
			key = strings.ToLower(key)
		}
		if seen[key] {
			continue
		}
		seen[key] = true
		result = append(result, absolute)
	}
	return result
}

func normalizedDocumentURI(rawURI string) (string, string) {
	path, isFile, err := filePathFromURI(rawURI)
	if err != nil || !isFile {
		return rawURI, ""
	}
	absolute, err := filepath.Abs(path)
	if err != nil {
		return rawURI, filepath.Clean(path)
	}
	absolute = filepath.Clean(absolute)
	return fileURIFromPath(absolute), absolute
}

func fileURIFromPath(path string) string {
	slashPath := filepath.ToSlash(path)
	if runtime.GOOS == "windows" && !strings.HasPrefix(slashPath, "/") {
		slashPath = "/" + slashPath
	}
	return (&url.URL{Scheme: "file", Path: slashPath}).String()
}

func ignoredWorkspaceDirectory(name string) bool {
	switch strings.ToLower(name) {
	case ".git", ".hg", ".svn", ".cache", "node_modules", "build":
		return true
	}
	return strings.HasPrefix(strings.ToLower(name), "build-")
}

func isToySourcePath(path string) bool {
	switch strings.ToLower(filepath.Ext(path)) {
	case ".toy", ".tf", ".fth":
		return true
	default:
		return false
	}
}

func (s *documentStore) resolveModulePath(importer *document, module string) (string, bool) {
	if !validModuleName(module) {
		return "", false
	}
	relative := filepath.FromSlash(strings.ReplaceAll(module, ".", "/") + ".toy")
	candidates := make([]string, 0, len(s.roots)+1)
	if importer != nil && importer.Path != "" {
		candidates = append(candidates, filepath.Join(filepath.Dir(importer.Path), relative))
	}
	for _, root := range s.roots {
		candidates = append(candidates, filepath.Join(root, relative))
	}

	seen := make(map[string]bool)
	for _, candidate := range candidates {
		absolute, err := filepath.Abs(candidate)
		if err != nil {
			continue
		}
		absolute = filepath.Clean(absolute)
		key := absolute
		if runtime.GOOS == "windows" {
			key = strings.ToLower(key)
		}
		if seen[key] {
			continue
		}
		seen[key] = true
		if doc, ok := s.getPath(absolute); ok {
			return doc.Path, true
		}
	}
	return "", false
}

func validModuleName(name string) bool {
	segmentStart := true
	for i := 0; i < len(name); i++ {
		c := name[i]
		if c == '.' {
			if segmentStart {
				return false
			}
			segmentStart = true
			continue
		}
		if segmentStart {
			if !isASCIILetter(c) && c != '_' {
				return false
			}
			segmentStart = false
			continue
		}
		if !isASCIILetter(c) && (c < '0' || c > '9') && c != '_' && c != '-' {
			return false
		}
	}
	return !segmentStart
}

func isASCIILetter(c byte) bool {
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'
}

func (s *documentStore) resolveLoadPath(importer *document, requested string) (string, bool) {
	if requested == "" {
		return "", false
	}
	if filepath.IsAbs(requested) {
		if doc, ok := s.getPath(requested); ok {
			return doc.Path, true
		}
		return "", false
	}

	candidates := make([]string, 0, len(s.roots)+1)
	if importer != nil && importer.Path != "" {
		candidates = append(candidates, filepath.Join(filepath.Dir(importer.Path), requested))
	}
	for _, root := range s.roots {
		candidates = append(candidates, filepath.Join(root, requested))
	}
	for _, candidate := range candidates {
		if doc, ok := s.getPath(candidate); ok {
			return doc.Path, true
		}
	}
	return "", false
}
