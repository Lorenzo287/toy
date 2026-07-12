# Toy for VS Code

Visual Studio Code extension for Toy, providing syntax highlighting and integration with the Toy Language Server (LSP).

## Features

- **Syntax Highlighting**: Comprehensive TextMate-based highlighting for words, symbols, variables, and control flow.
- **LSP Client**: Seamless integration with `toy-lsp` for definitions, hovers, and symbols.
- **Language Configuration**: Support for comments, brackets, and auto-closing pairs.

## Setup & Installation

> [!IMPORTANT]
> Although the VS Code extension uses TextMate for highlighting, the
> underlying **LSP depends on Tree-sitter** for code analysis. Generate and
> synchronize the parser before building the LSP binary.

The extension expects the LSP executable to be located in a `bin/` subdirectory within the extension folder (`tools/vscode-toy/bin/`).

### 1. Prepare the LSP Binary
First, ensure the Tree-sitter parser is generated in `tools/tree-sitter-toy`:
```powershell
cd tools/tree-sitter-toy
npm run generate
cd ../..
```

Then, build the LSP and copy it to the extension's `bin` folder:
```powershell
cd tools/toy-lsp
go build -o ../vscode-toy/bin/toy-lsp.exe ./cmd/toy-lsp
```

### 2. Package & Install
From `tools/vscode-toy`:

1. Package the extension (requires `npm install -g @vscode/vsce`):
   ```powershell
   vsce package
   ```
2. Install the generated `.vsix` file:
   - Open VS Code Command Palette (`Ctrl+Shift+P`).
   - Search for **Extensions: Install from VSIX...**.
   - Select the `vscode-toy-0.2.0.vsix` file.

## Development

The extension source is in `tools/vscode-toy/extension.js`.

To set up the development environment, run from `tools/vscode-toy`:
```powershell
npm install
```
