# Toy LSP

Minimal standalone Language Server Protocol implementation for Toy, written in Go. 
It uses the Tree-sitter parser for indexing and providing IDE features.

## Features

### Supported LSP Methods

- **Navigation**: `textDocument/definition`, `textDocument/references`.
- **Introspection**: `textDocument/hover` (shows builtin docs and stack effects), `textDocument/documentSymbol`.
- **Refactoring**: `textDocument/rename` (works for both top-level words and locals).
- **Formatting**: `textDocument/formatting`, backed by the same formatter as
  the `toyfmt` CLI.
- **Lifecycle**: `initialize`, `shutdown`, `exit`, `didOpen`, `didChange`, `didClose`.

### Analysis Scope

- **Top-level definitions**: `'name [ ... ] def`.
- **Locals**: Bindings from `| a b |`, fetches like `$a`, and nested block shadowing.
- **Documentation**: Leading `\ comment` lines are extracted for hovers.

## Getting Started

> [!IMPORTANT]
> The LSP relies on the Tree-sitter parser for code analysis. Generate and
> synchronize it from `tools/tree-sitter-toy` with `npm run generate`
> before running or building the LSP.

### Run from Source

From `tools/toy-lsp`:

```powershell
go run ./cmd/toy-lsp
```

### Build Executable

From `tools/toy-lsp`:

```powershell
go build -o toy-lsp.exe ./cmd/toy-lsp
./toy-lsp.exe
```

## Editor Setup

### Neovim

You can use the automated installation script to generate the Tree-sitter
parser, build and install the LSP and `toyfmt`, install the local grammar, and
remove stale generated Neovim parser/query artifacts:

- **Windows**: `.\tools\install-nvim.ps1`
- **Linux/macOS**: `bash tools/install-nvim.sh`

Follow the instructions printed by the script to update your Neovim config. After adding the printed Tree-sitter snippet, restart Neovim and run `:TSInstall! toy`. See [Toy Tree-sitter](tree-sitter.md) for the manual parser setup.

Alternatively, register the LSP manually:

```lua
vim.lsp.config('toyls', {
  cmd = { 'path/to/toy-lsp.exe' },
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

### Verification

From `tools/toy-lsp`:

```powershell
go fmt ./...
go test -count=1 ./...
```

### Example Fixture

The sample file in `tools/toy-lsp/testdata/symbols.toy` exercises the current server behavior.
