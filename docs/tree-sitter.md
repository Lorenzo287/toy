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

The Toy SDK contains generated parser source and queries under
`share/toy/tree-sitter-toy`. Set `toy_path` to that installed directory and
register it with Neovim:

```lua
local toy_path = vim.fn.expand("$LOCALAPPDATA/Toy/share/toy/tree-sitter-toy")
-- Linux/macOS default:
-- local toy_path = vim.fn.expand("~/.local/share/toy/share/toy/tree-sitter-toy")

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
> `queries = "queries/toy"` lets `nvim-treesitter` install the Toy query files
> into its site query directory. You do not need to append the grammar folder
> to `runtimepath` or install npm to use the generated SDK assets.

> [!IMPORTANT]
> On Windows, `TSInstall` and `TSUpdate` can fail with `Access is denied` if
> Neovim has already loaded `toy.so`.
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
