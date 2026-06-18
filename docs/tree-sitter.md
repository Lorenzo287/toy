# Toy Tree-sitter

Tree-sitter grammar for Toy. It provides a robust, incremental parser that powers syntax highlighting, indentation, and code folding for modern editors.

## Features

- **Syntax Highlighting**: Precise, scope-based highlighting via Tree-sitter queries.
- **Indentation**: Automatic indentation rules for vectors `[ ]`, lists `( )`, maps/sets `{ }`, and local capture lists `| |`.
- **Folding**: Logical folding ranges for compound forms.

## Editor Setup

### Neovim

The easiest way to set up Tree-sitter and the LSP in Neovim is to use the automated script:

- **Windows**: `.\tools\install-nvim.ps1`
- **Linux/macOS**: `bash tools/install-nvim.sh`

The script builds the LSP, generates the Tree-sitter parser with ABI 15, installs the local grammar files, removes stale generated Neovim parser/query artifacts, and prints the Neovim configuration snippet to add to your config.

Alternatively, you can register the parser manually:

```lua
local toy_path = "C:/toy/tree-sitter-toyforth"
-- Linux/macOS example:
-- local toy_path = vim.fn.expand("~/.local/share/toy/tree-sitter-toyforth")

-- Register filetypes
vim.filetype.add({
    extension = {
        fth = "toy",
        tf = "toy",
        toy = "toy",
    },
})

-- Register the local parser for nvim-treesitter main
local function add_toyforth_parser()
    require("nvim-treesitter.parsers").toyforth = {
        install_info = {
            path = toy_path,
            queries = "queries/toyforth",
        },
    }
end

add_toyforth_parser()
vim.api.nvim_create_autocmd("User", {
    pattern = "TSUpdate",
    callback = add_toyforth_parser,
})

vim.treesitter.language.register("toyforth", "toy")
```

After adding the configuration, restart Neovim and run `:TSInstall! toyforth`.

> [!NOTE]
> The configuration above uses a local path and requires generated parser code. Run `tree-sitter generate --abi 15` inside `tools/tree-sitter-toyforth` before installing manually.
>
> `queries = "queries/toyforth"` lets `nvim-treesitter` install the Toy query files into its site query directory. You do not need to append the grammar folder to `runtimepath`.

> [!IMPORTANT]
> On Windows, `TSInstall` and `TSUpdate` can fail with `Access is denied` if
> Neovim has already loaded `toyforth.so`.
>
> The automated installation script (`tools/install-nvim.ps1`) handles this by attempting to delete stale parser/query artifacts. If you are installing manually:
>
> - close all Neovim instances
> - delete stale Toy parsers from `%LOCALAPPDATA%\nvim-data\site\parser\` and `%LOCALAPPDATA%\nvim-data\lazy\nvim-treesitter\parser\`
> - delete `%LOCALAPPDATA%\nvim-data\site\queries\toyforth` if it exists
> - restart Neovim and run `:TSInstall! toyforth`
>
> This is a Windows file-locking issue, not a Toy grammar issue.

## Development

Requires the [tree-sitter CLI](https://github.com/tree-sitter/tree-sitter/blob/master/crates/cli/README.md).

### Build & Test

From `tools/tree-sitter-toyforth`:

```powershell
# Generate the parser from grammar.js
tree-sitter generate --abi 15

# Run the test suite
tree-sitter test

# Open the interactive playground
npm run start
```
