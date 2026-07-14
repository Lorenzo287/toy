# Toy Tree-sitter

Tree-sitter grammar for Toy. It provides a robust, incremental parser that powers syntax highlighting, indentation, and code folding for modern editors.

## Features

- **Syntax Highlighting**: User words and symbols use a neutral baseline;
  captures are parameter-colored, definition symbols use the function color,
  and native words, constants, control words, and literals remain
  distinguishable.
- **Indentation**: Automatic indentation rules for vectors `[ ]`, lists `( )`, maps/sets `{ }`, and local capture lists `| |`.
- **Folding**: Logical folding ranges for compound forms.

## Editor Setup

### Neovim

The easiest way to set up Tree-sitter and the LSP in Neovim is to use the automated script:

- **Windows**: `.\tools\install-nvim.ps1`
- **Linux/macOS**: `bash tools/install-nvim.sh`

The script generates the Tree-sitter parser with ABI 15, builds the LSP and
formatter, installs the local tools and grammar files, removes stale generated
Neovim parser/query artifacts, and prints the Neovim configuration snippet to
add to your config.

Alternatively, you can register the parser manually:

```lua
local toy_path = "C:/toy/tree-sitter-toy"
-- Linux/macOS example:
-- local toy_path = vim.fn.expand("~/.local/share/toy/tree-sitter-toy")

-- Register filetypes
vim.filetype.add({
    extension = {
        fth = "toy",
        tf = "toy",
        toy = "toy",
    },
})

-- Register the local parser for nvim-treesitter main
local function add_toy_parser()
    require("nvim-treesitter.parsers").toy = {
        install_info = {
            path = toy_path,
            queries = "queries/toy",
        },
    }
end

add_toy_parser()
vim.api.nvim_create_autocmd("User", {
    pattern = "TSUpdate",
    callback = add_toy_parser,
})

vim.treesitter.language.register("toy", "toy")
```

After adding the configuration, restart Neovim and run `:TSInstall! toy`.

> [!NOTE]
> The configuration above uses a local path and requires generated parser
> code. Run `npm run generate` inside `tools/tree-sitter-toy` before
> installing manually.
>
> `queries = "queries/toy"` lets `nvim-treesitter` install the Toy query files into its site query directory. You do not need to append the grammar folder to `runtimepath`.

> [!IMPORTANT]
> On Windows, `TSInstall` and `TSUpdate` can fail with `Access is denied` if
> Neovim has already loaded `toy.so`.
>
> The automated installation script (`tools/install-nvim.ps1`) handles this by attempting to delete stale parser/query artifacts. If you are installing manually:
>
> - close all Neovim instances
> - delete stale Toy parsers from `%LOCALAPPDATA%\nvim-data\site\parser\` and `%LOCALAPPDATA%\nvim-data\lazy\nvim-treesitter\parser\`
> - delete `%LOCALAPPDATA%\nvim-data\site\queries\toy` if it exists
> - restart Neovim and run `:TSInstall! toy`
>
> This is a Windows file-locking issue, not a Toy grammar issue.

## Development

Requires the [tree-sitter CLI](https://github.com/tree-sitter/tree-sitter/blob/master/crates/cli/README.md).

### Build & Test

From `tools/tree-sitter-toy`:

```powershell
# Install the pinned local CLI
npm ci --ignore-scripts
npm rebuild tree-sitter-cli

# Generate the parser and synchronize the Go binding
npm run generate

# Run the test suite
npm test

# Open the interactive playground
npm run start
```
