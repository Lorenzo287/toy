package lsp

import (
	"bytes"
	"encoding/json"
	"io"
	"log"
	"os"
	"path/filepath"
	"testing"

	"toy-lsp/internal/analysis"
)

func TestResolveDefinitionAcrossModulesAndAliases(t *testing.T) {
	root := t.TempDir()
	mathPath := writeToyFile(t, root, "math.toy", `\ math words
'double [ 2 * ] def
'private [ 1 + ] def
'double export
`)
	appPath := writeToyFile(t, root, "app.toy", `"math" require
21 math.double
math.private
"math" 'm require-as
21 m.double
`)

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})
	appURI := testFileURI(appPath)
	appSource, err := os.ReadFile(appPath)
	if err != nil {
		t.Fatal(err)
	}
	server.docs.open(appURI, string(appSource), 1)

	canonical, ok := server.resolveDefinition(appURI, analysis.Position{Line: 1, Character: 9})
	if !ok {
		t.Fatal("canonical module call did not resolve")
	}
	if canonical.URI != testFileURI(mathPath) || canonical.Symbol.Name != "double" {
		t.Fatalf("canonical target = %+v", canonical)
	}
	if canonical.Symbol.SelectionRange.Start != (analysis.Position{Line: 1, Character: 1}) {
		t.Fatalf("canonical target range = %+v", canonical.Symbol.SelectionRange)
	}

	if _, ok := server.resolveDefinition(appURI, analysis.Position{Line: 2, Character: 6}); ok {
		t.Fatal("private module word unexpectedly resolved")
	}

	alias, ok := server.resolveDefinition(appURI, analysis.Position{Line: 4, Character: 6})
	if !ok || alias.URI != canonical.URI || alias.Symbol.Name != "double" {
		t.Fatalf("alias target = %+v, %v", alias, ok)
	}
}

func TestInitializeIndexesWorkspaceFiles(t *testing.T) {
	root := t.TempDir()
	path := writeToyFile(t, root, "indexed.toy", `'indexed [ 1 ] def`)
	params, err := json.Marshal(initializeParams{RootURI: testFileURI(root)})
	if err != nil {
		t.Fatal(err)
	}

	server := NewServer(log.New(io.Discard, "", 0))
	var output bytes.Buffer
	if err := server.handleInitialize(&output, request{ID: json.RawMessage("1"), Params: params}); err != nil {
		t.Fatal(err)
	}
	if _, ok := server.docs.get(testFileURI(path)); !ok {
		t.Fatal("workspace source was not indexed during initialize")
	}
}

func TestOpenModuleBufferOverridesDiskUntilClose(t *testing.T) {
	root := t.TempDir()
	mathPath := writeToyFile(t, root, "math.toy", `'disk-word [ 1 ] def 'disk-word export`)
	appPath := writeToyFile(t, root, "app.toy", `"math" require math.buffer-word`)
	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})

	mathURI := testFileURI(mathPath)
	server.docs.open(mathURI, `'buffer-word [ 2 ] def 'buffer-word export`, 1)
	if _, ok := server.resolveDefinition(testFileURI(appPath), analysis.Position{Line: 0, Character: 24}); !ok {
		t.Fatal("unsaved module definition did not override disk index")
	}
	server.docs.close(mathURI)
	if _, ok := server.resolveDefinition(testFileURI(appPath), analysis.Position{Line: 0, Character: 24}); ok {
		t.Fatal("closed module buffer did not fall back to disk index")
	}
}

func TestResolveDefinitionThroughTransitiveModuleAndLoad(t *testing.T) {
	root := t.TempDir()
	dependencyPath := writeToyFile(t, root, "dependency.toy", `'value [ 5 ] def
'value export
`)
	writeToyFile(t, root, "consumer.toy", `"dependency" 'dep require-as
'combined [ dep.value 2 * ] def
'combined export
`)
	loadedPath := writeToyFile(t, root, "shared.toy", `'loaded [ 9 ] def
`)
	appPath := writeToyFile(t, root, "app.toy", `"consumer" require
consumer.combined
dependency.value
"shared.toy" load
loaded
`)

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})
	appURI := testFileURI(appPath)

	transitive, ok := server.resolveDefinition(appURI, analysis.Position{Line: 2, Character: 12})
	if !ok || transitive.URI != testFileURI(dependencyPath) || transitive.Symbol.Name != "value" {
		t.Fatalf("transitive target = %+v, %v", transitive, ok)
	}

	loaded, ok := server.resolveDefinition(appURI, analysis.Position{Line: 4, Character: 2})
	if !ok || loaded.URI != testFileURI(loadedPath) || loaded.Symbol.Name != "loaded" {
		t.Fatalf("loaded target = %+v, %v", loaded, ok)
	}
}

func TestResolveDefinitionExportedAfterModuleLoad(t *testing.T) {
	root := t.TempDir()
	extensionPath := writeToyFile(t, root, "extension.toy", `'loaded-word [ 12 ] def`)
	writeToyFile(t, root, "loader.toy", `"extension.toy" load
'loaded-word export`)
	appPath := writeToyFile(t, root, "app.toy", `"loader" require loader.loaded-word`)

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})
	target, ok := server.resolveDefinition(testFileURI(appPath), analysis.Position{Line: 0, Character: 25})
	if !ok || target.URI != testFileURI(extensionPath) || target.Symbol.Name != "loaded-word" {
		t.Fatalf("loaded module export target = %+v, %v", target, ok)
	}
	if occurrences := server.wordOccurrences(target, true); len(occurrences) != 3 {
		t.Fatalf("loaded module export occurrences = %+v, want declaration, export, and call", occurrences)
	}
}

func TestResolveModulePrefersImporterDirectory(t *testing.T) {
	root := t.TempDir()
	writeToyFile(t, root, "math.toy", `'value [ 1 ] def 'value export`)
	nestedMath := writeToyFile(t, root, filepath.Join("nested", "math.toy"), `'value [ 2 ] def 'value export`)
	appPath := writeToyFile(t, root, filepath.Join("nested", "app.toy"), `"math" require math.value`)

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})
	target, ok := server.resolveDefinition(testFileURI(appPath), analysis.Position{Line: 0, Character: 18})
	if !ok || target.URI != testFileURI(nestedMath) {
		t.Fatalf("relative module target = %+v, %v", target, ok)
	}
}

func TestDefinitionResponseReturnsTargetDocumentURI(t *testing.T) {
	root := t.TempDir()
	mathPath := writeToyFile(t, root, "math.toy", `'double [ 2 * ] def 'double export`)
	appPath := writeToyFile(t, root, "app.toy", `"math" require math.double`)

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})
	params, err := json.Marshal(definitionParams{
		TextDocument: textDocumentIdentifier{URI: testFileURI(appPath)},
		Position:     lspPosition{Line: 0, Character: 20},
	})
	if err != nil {
		t.Fatal(err)
	}
	var output bytes.Buffer
	if err := server.handleDefinition(&output, request{ID: json.RawMessage("1"), Params: params}); err != nil {
		t.Fatal(err)
	}
	response := decodeTestResponse(t, output.Bytes())
	var target location
	if err := json.Unmarshal(response.Result, &target); err != nil {
		t.Fatal(err)
	}
	if target.URI != testFileURI(mathPath) || target.Range.Start != (lspPosition{Line: 0, Character: 1}) {
		t.Fatalf("definition response = %+v", target)
	}
}

func TestDefinitionResponseFollowsLoadAndModuleSourceStrings(t *testing.T) {
	root := t.TempDir()
	mathPath := writeToyFile(t, root, "math.toy", `'value [ 1 ] def 'value export`)
	sharedPath := writeToyFile(t, root, "shared.toy", `'shared [ 2 ] def`)
	appPath := writeToyFile(t, root, "app.toy", `"math" require
"math" 'm require-as
"shared.toy" load`)

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})
	tests := []struct {
		name     string
		position lspPosition
		wantURI  string
	}{
		{name: "require", position: lspPosition{Line: 0, Character: 2}, wantURI: testFileURI(mathPath)},
		{name: "require-as", position: lspPosition{Line: 1, Character: 2}, wantURI: testFileURI(mathPath)},
		{name: "load", position: lspPosition{Line: 2, Character: 3}, wantURI: testFileURI(sharedPath)},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			params, err := json.Marshal(definitionParams{
				TextDocument: textDocumentIdentifier{URI: testFileURI(appPath)},
				Position:     test.position,
			})
			if err != nil {
				t.Fatal(err)
			}
			var output bytes.Buffer
			if err := server.handleDefinition(&output, request{ID: json.RawMessage("1"), Params: params}); err != nil {
				t.Fatal(err)
			}
			response := decodeTestResponse(t, output.Bytes())
			var target location
			if err := json.Unmarshal(response.Result, &target); err != nil {
				t.Fatal(err)
			}
			if target.URI != test.wantURI || target.Range != (lspRange{}) {
				t.Fatalf("source definition response = %+v", target)
			}
		})
	}
}

func TestReferencesAndRenameFollowModuleIdentity(t *testing.T) {
	root := t.TempDir()
	mathPath := writeToyFile(t, root, "math.toy", `'double [ 2 * ] def
'use-double [ double ] def
'double export
`)
	appPath := writeToyFile(t, root, "app.toy", `"math" require
math.double
"math" 'm require-as
m.double
`)

	server := NewServer(log.New(io.Discard, "", 0))
	server.docs.configureWorkspace(initializeParams{RootURI: testFileURI(root)})
	appURI := testFileURI(appPath)
	target, ok := server.resolveDefinition(appURI, analysis.Position{Line: 1, Character: 7})
	if !ok {
		t.Fatal("module call did not resolve")
	}
	occurrences := server.wordOccurrences(target, true)
	if len(occurrences) != 5 {
		t.Fatalf("occurrences = %+v, want declaration, internal call, export, and two module calls", occurrences)
	}

	params, err := json.Marshal(renameParams{
		TextDocument: textDocumentIdentifier{URI: appURI},
		Position:     lspPosition{Line: 3, Character: 4},
		NewName:      "twice",
	})
	if err != nil {
		t.Fatal(err)
	}
	var output bytes.Buffer
	if err := server.handleRename(&output, request{ID: json.RawMessage("2"), Params: params}); err != nil {
		t.Fatal(err)
	}
	response := decodeTestResponse(t, output.Bytes())
	var edit workspaceEdit
	if err := json.Unmarshal(response.Result, &edit); err != nil {
		t.Fatal(err)
	}
	mathEdits := edit.Changes[testFileURI(mathPath)]
	appEdits := edit.Changes[appURI]
	if len(mathEdits) != 3 || len(appEdits) != 2 {
		t.Fatalf("rename changes = %+v", edit.Changes)
	}
	for _, change := range append(append([]textEdit(nil), mathEdits...), appEdits...) {
		if change.NewText != "twice" {
			t.Fatalf("rename edit = %+v", change)
		}
	}
	if appEdits[0].Range != (lspRange{Start: lspPosition{Line: 1, Character: 5}, End: lspPosition{Line: 1, Character: 11}}) {
		t.Fatalf("canonical rename range = %+v", appEdits[0].Range)
	}
	if appEdits[1].Range != (lspRange{Start: lspPosition{Line: 3, Character: 2}, End: lspPosition{Line: 3, Character: 8}}) {
		t.Fatalf("alias rename range = %+v", appEdits[1].Range)
	}
}

func writeToyFile(t *testing.T, root, relative, contents string) string {
	t.Helper()
	path := filepath.Join(root, relative)
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte(contents), 0o644); err != nil {
		t.Fatal(err)
	}
	return path
}
