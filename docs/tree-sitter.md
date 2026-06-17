# Toy Tree-sitter

Tree-sitter grammar for Toy. It provides a robust, incremental parser that powers syntax highlighting, indentation, and code folding for modern editors.

## Features

- **Syntax Highlighting**: Precise, scope-based highlighting via Tree-sitter queries.
- **Indentation**: Automatic indentation rules for vectors `[ ]`, lists `( )`, maps/sets `{ }`, definitions `: ;`, and local capture lists `| |`.
- **Folding**: Logical folding ranges for definitions and blocks.

## Editor Setup

### Neovim

The easiest way to set up Tree-sitter and the LSP in Neovim is to use the automated script:

- **Windows**: `.\tools\install-nvim.ps1`
- **Linux/macOS**: `bash tools/install-nvim.sh`

Follow the printed instructions to update your `init.lua`.

Alternatively, you can register the parser manually:

```lua
local toy_path = "~/path/to/toy/tools/tree-sitter-toyforth"

-- 1. Register the parser
require("nvim-treesitter.parsers").get_parser_configs().toyforth = {
    install_info = {
        url = toy_path,
        files = { "src/parser.c" },
        branch = "main",
    },
    filetype = "toy",
}

-- 2. Add queries to runtimepath (so highlighting works)
vim.opt.rtp:append(toy_path)

-- 3. Register filetypes
vim.filetype.add({
    extension = {
        fth = "toy",
        tf = "toy",
        toy = "toy",
    },
})
```

After adding the configuration, run `:TSInstall toyforth` inside Neovim.

> [!NOTE]
> The configuration above uses a local path and requires that you have already generated the parser code. Run `tree-sitter generate` inside the `tools/tree-sitter-toyforth` directory before installing.
>
> Alternatively, you can configure `nvim-treesitter` to fetch and build the parser automatically from a remote GitHub repository. For details on advanced setup without local cloning, refer to the [nvim-treesitter documentation](https://github.com/nvim-treesitter/nvim-treesitter).

> [!IMPORTANT]
> On Windows, `TSInstall` and `TSUpdate` can fail with `Access is denied` if
> Neovim has already loaded `toyforth.so` from the `nvim-treesitter` parser
> directory.
>
> The automated installation script (`tools/install-nvim.ps1`) handles this by 
> attempting to delete the old binary for you. If you are installing manually:
>
> - close all Neovim instances
> - delete `toyforth.so` from
>   `~/AppData/Local/nvim-data/lazy/nvim-treesitter/parser/`
> - restart Neovim and let `auto_install` or `:TSInstall toyforth` rebuild it
>
> This is a file-locking issue in the current workflow, not a Toy grammar
> issue.

## Development

Requires the [tree-sitter CLI](https://github.com/tree-sitter/tree-sitter/blob/master/crates/cli/README.md).

### Build & Test

From `tools/tree-sitter-toyforth`:

```powershell
# Generate the parser from grammar.js
tree-sitter generate

# Run the test suite
tree-sitter test

# Open the interactive playground
npm run start
```
