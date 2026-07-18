# Toy LSP

Minimal standalone Language Server Protocol implementation for Toy, written in Go. 
It uses the Tree-sitter parser for indexing and providing IDE features.

## Features

### Supported LSP Methods

- **Navigation**: workspace-aware `textDocument/definition` and
  `textDocument/references`.
- **Introspection**: `textDocument/hover` (shows builtin docs and stack effects), `textDocument/documentSymbol`.
- **Refactoring**: `textDocument/rename` for locals and user words, including
  qualified uses in other files.
- **Formatting**: `textDocument/formatting`, backed by the same formatter as
  the `toyfmt` CLI.
- **Lifecycle**: `initialize`, `shutdown`, `exit`, `didOpen`, `didChange`, `didClose`.

### Analysis Scope

- **Top-level definitions**: `'name [ ... ] def`.
- **Locals**: Bindings from `| a b |`, fetches like `$a`, and nested block shadowing.
- **Documentation**: Leading `\ comment` lines are extracted for hovers.
- **Workspace files**: Toy sources under the initialized workspace folders are
  indexed from disk; unsaved open buffers override their disk copies.
- **Packages**: Directory peers share definitions and privacy declarations.
  Literal `import` and `import-as` forms resolve exact relative or absolute
  directories, local aliases, public visibility, and qualified calls. Go to
  definition on an import string opens package source. `core:` navigation uses
  a `core` directory beneath an initialized workspace root when source exists.

Imports assembled dynamically in root evaluation cannot be inferred
statically. C-backed packages have no Toy source definition to jump to. Completion,
diagnostics, semantic tokens, and workspace-symbol search are not implemented
yet.

## Getting Started

`toy-lsp` is prebuilt in every Toy SDK. After installing Toy, verify that the
server is available with:

```powershell
Get-Command toy-lsp
```

## Editor Setup

### Neovim

Register the installed server directly:

```lua
vim.lsp.config('toyls', {
  cmd = { 'toy-lsp' },
  filetypes = { 'toy' },
  root_markers = { '.git', 'README.md' },
})

vim.lsp.enable('toyls')
```

With the server attached, format the current buffer through Neovim's built-in
LSP client:

```lua
vim.lsp.buf.format({ async = false })
```

Formatting behavior and `.toyfmt` configuration are documented in the
[formatter guide](formatter.md).

## Development

The source server relies on the generated Tree-sitter parser. From a clean
checkout, and again after grammar changes, install the pinned generator and
regenerate the parser before running or building the LSP:

```powershell
Set-Location tools\tree-sitter-toy
npm ci --ignore-scripts
npm rebuild tree-sitter-cli
npm run generate
Set-Location ..\toy-lsp
go run ./cmd/toy-lsp
```

### Verification

From `tools/toy-lsp`:

```powershell
go fmt ./...
go test -count=1 ./...
```

### Example Fixture

The sample file in `tools/toy-lsp/testdata/symbols.toy` exercises the current server behavior.
