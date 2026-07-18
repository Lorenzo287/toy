# Toy for VS Code

Visual Studio Code extension for Toy, providing syntax highlighting and integration with the Toy Language Server (LSP).

## Features

- **Syntax Highlighting**: Comprehensive TextMate-based highlighting for words, symbols, variables, and control flow.
- **LSP Client**: Seamless integration with `toy-lsp` for definitions, hovers, and symbols.
- **Language Configuration**: Support for comments, brackets, and auto-closing pairs.

## Setup & Installation

Install the Toy SDK first so `toy-lsp` is on `PATH`. The extension uses that
installed server by default; the `toy.lsp.path` setting can select another
executable.

### Package and Install the Extension
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
