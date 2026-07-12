# Toy Formatter

`toyfmt` is a layout-preserving formatter shared by the standalone CLI and the
Toy language server. It normalizes indentation and horizontal whitespace but
does not decide whether a quotation should be inline or multiline.

Tree-sitter validates the source structure. Before rendering, a lossless
lexical pass follows the runtime lexer's token-boundary rules so formatting
cannot turn operators into signed numbers or split symbols that contain `/`.

## Formatting Policy

The formatter:

- preserves existing line breaks and blank lines;
- indents nested quotations, lists, maps, sets, and captures;
- aligns closing delimiters;
- normalizes spaces between tokens and just inside delimiters;
- removes trailing whitespace and adds a final newline;
- preserves string and comment contents;
- rejects malformed syntax instead of guessing.

Empty forms remain compact: `[]`, `()`, `{}`, and `#{}`.

## Configuration

Configuration lives in `.toyfmt` files using `key = value` entries:

```toml
indent_width = 4
indent_style = "spaces"
delimiter_spacing = "spaced"
```

Supported values are:

- `indent_width`: a positive integer;
- `indent_style`: `"spaces"`, or `"tab"`/`"tabs"` for tab indentation;
- `delimiter_spacing`: `"spaced"` or `"compact"`;
- `disable`: `true` or `false`.

`"spaced"` produces `[ 1 2 3 ]`; `"compact"` produces `[1 2 3]`.
The setting also applies to lists, maps, sets, and capture bars.

Configuration is discovered from the filesystem root down to the formatted
file. A nearer `.toyfmt` overrides only the settings it specifies. The
`toy/examples/quines/` directory uses `disable = true` because rewriting a
quine's source would change what it reproduces.

## Command Line

The formatter uses the same generated Tree-sitter parser as the LSP. Generate
`tools/tree-sitter-toy/src/parser.c` first by following the
[Tree-sitter build instructions](tree-sitter.md#build--test), then build from
`tools/toy-lsp`:

```powershell
go build -o toyfmt.exe ./cmd/toyfmt
```

Format one file to standard output, check files, or update them in place:

```powershell
.\toyfmt.exe ..\..\toy\examples\factorial.toy
.\toyfmt.exe --check ..\..\toy\examples\factorial.toy
.\toyfmt.exe --write ..\..\toy\examples\factorial.toy
```

Use `--stdin-filepath` when reading standard input so configuration discovery
has a source path. CLI overrides are available through `--indent-width`,
`--indent-style`, and `--delimiter-spacing`.

## LSP and Neovim

The language server exposes `textDocument/formatting` through the same formatter
library. With the Toy LSP attached, Neovim can format the current buffer without
an adapter plugin:

```lua
vim.lsp.buf.format({ async = false })
```

For optional format-on-save behavior:

```lua
vim.api.nvim_create_autocmd("BufWritePre", {
  pattern = "*.toy",
  callback = function()
    vim.lsp.buf.format({ async = false })
  end,
})
```

Project configuration takes precedence over indentation preferences supplied by
an LSP client. Explicit CLI flags take precedence over project configuration.
