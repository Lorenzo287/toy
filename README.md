# Toy

Toy is a minimalist, stack-based interpreter written in C. It started from a traditional Forth shape, but is evolving into a quotation-first concatenative language inspired by Joy, the language designed by Manfred von Thun.

The core idea is simple: code blocks are data. Quotations (`[ ... ]`) and quoted symbols (`'name`) are first-class values that can be stored, passed, composed, and executed.

Based on the original [toyforth](https://github.com/antirez/toyforth) project by **Salvatore Sanfilippo (antirez)**.
This version includes an interactive REPL, using **antirez**'s `linenoise` library for history and tab completion, along with a Tree-sitter grammar, a standalone language server written in Go, and a VS Code extension.

## Key Features

### Language Features

- **Dynamic Object System**: Native support for **Integers, Floats, Booleans, Strings, Symbols,** and **Lists**.
- **Quotations & Blocks**: First-class code blocks `[ ... ]` and quoted symbols `'name` allow for deferred execution and higher-order programming.
- **Variable Capturing**: Named local variables with dynamic scoping using `{ a b }` and `$a` syntax.
- **First-class Control Flow**: Branches, loops, combinators, and recursion schemes are simple words over quotations, with Joy-style predicate inspection for quoted conditions.

### Engine & Performance

- **Iterative Execution**: An explicit return stack of frames eliminates C recursion for user-defined words, preventing stack overflows.
- **Automatic Memory Management**: A uniform reference counting model (`retain_obj`/`release_obj`) handles all heap-allocated objects.
- **$O(1)$ Word Lookup**: A high-performance hash table dictionary ensures fast dispatch.
- **Type Promotion**: Automatic mixed-type arithmetic (e.g., `1 2.5 +` -> `3.5`).

## Showcase

### Definitions

Define new words by binding a quoted symbol to a quotation:

```toy
'square [ dup * ] def
'cube [ dup square * ] def

5 square print  \ Prints 25
3 cube .        \ Prints 27, leaves 27
```

Classic Forth-style definitions are still accepted as compatibility sugar:

```toy
: square dup * ;
```

For code that needs names for intermediate values, use frame-local captures:

```toy
'hypot2 [ { x y } $x $x * $y $y * + ] def
3 4 hypot2 print  \ Prints 25

\ Captured variables are visible to inner quotations while the frame is active.
10 { x }
[ $x 5 + . ] exec  \ Prints 15
```

### Deferred Execution (Quotations)

Code is data. You can defer execution by "quoting" a symbol or wrapping code in a block:

```toy
'dup           \ Leaves 'dup

[ 1 2 + ]      \ Leaves [1 2 +]
exec           \ Leaves 3

[ 1 2 + ] i    \ Leaves 3
```

### Combinators

Toy favors small words that combine quotations instead of special syntax:

```toy
1 2 4 [ + ] dip
\ Leaves 3 4

5 [ 1 + ] keep
\ Leaves 5 6

5 [ 1 + ] [ 2 * ] bi
\ Leaves 6 10

[ 1 2 3 4 5 ] [ 3 < ] split
\ Leaves [1 2] [3 4 5]
```

### Control Flow

Branches (`if`, `ifelse`) support predicates that inspect the stack without permanently consuming it:

```toy
5 [ 0 > ] [ "Positive" print ] if
\ Prints Positive

5 [ 0 > ] [ "Positive" print ] [ "Non-positive" print ] ifelse
\ Prints Positive

\ 'while' re-evaluates its predicate each iteration without consuming loop state.
10 [ 0 > ] [ . pred ] while
\ Prints 10 down to 1

\ Iterators
5 [ "Hello " printf ] times
\ Prints Hello Hello Hello Hello Hello

[ 1 2 3 ] [ . ] each
\ Prints 1 2 3

[ 1 2 3 ] [ succ ] map .s
\ Prints <1> [2 3 4]
```

### Recursion Schemes

Linear recursion can express factorial without naming a recursive helper:

```toy
5 [ 0 == ] [ succ ] [ dup pred ] [ * ] linrec
\ Leaves 120
```

Binary recursion can express quicksort in the Joy style:

```toy
'qsort [
    [ len nip 2 < ]
    []
    [ uncons [ > ] split ]
    [ swapd cons concat ]
    binrec
] def

[ 3 1 4 1 5 2 ] qsort
\ Leaves [1 1 2 3 4 5]
```

### List Words

Toy includes basic list inspection, construction, and update words:

- `len`, `geth`, `first`, `rest`, and `empty?` inspect lists without consuming
  the list
- `uncons`, `cons`, and `concat` construct or destructure lists and consume
  their list inputs
- `seth` updates list contents in $O(1)$ time

```toy
[ 1 2 3 ] len print        \ Prints 3, list stays on the stack
[ 1 2 3 ] 1 geth nip print \ Prints 2, list is dropped explicitly by nip
[ 1 2 3 ] uncons           \ Leaves 1 [2 3]
0 [ 1 2 3 ] cons           \ Leaves [0 1 2 3]

[ 1 2 3 ] { list }
$list 0 99 seth           \ Updates index 0
$list print               \ Prints [99 2 3]
```

### System & Utility Words

Toy also provides words for runtime interaction and introspection:

- `words` prints the dictionary and `see` shows a source-like definition
- `rand`, `sleep`, and `time` provide simple runtime utilities
- `bye` and `exit` terminate the interpreter

```toy
words
'square [ dup * ] def
'square see
time print
```

## Standard Library

Toy includes a compact native vocabulary plus Forth standard-library words
that can be loaded from `toy/std/*.toy`:

| Category         | Words                                                                                                                                                   |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Stack**        | `dup`, `drop`, `swap`, `over`, `rot`, `swapd`, `nip`, `tuck`, `pick`, `roll`, `empty`                                                                   |
| **Math**         | `+`, `-`, `*`, `/`, `%`, `mod`, `abs`, `neg`, `succ`, `pred`, `max`, `min`                                                                              |
| **Logic**        | `and`, `or`, `xor`, `not`, `shl`, `shr`                                                                                                                 |
| **Comparison**   | `==`, `!=`, `<`, `>`, `<=`, `>=`                                                                                                                        |
| **Control**      | `if`, `ifelse`, `while`, `times`, `each`, `map`, `fold`, `exec`, `i`, `app2`, `dip`, `keep`, `bi`, `split`, `linrec`, `binrec`                          |
| **I/O**          | `print`, `printf`, `.`, `.s`, `cr`, `key`, `input`, `clear`, `page`                                                                                     |
| **List/String**  | `geth`, `seth`, `len`, `first`, `rest`, `uncons`, `cons`, `append`, `concat`, `join`, `trim`, `upper`, `lower`, `splits`, `splitmid`, `range`, `empty?` |
| **File I/O**     | `readf`, `writef`, `delf`, `readl`, `exists?`                                                                                                           |
| **System/Utils** | `rand`, `sleep`, `time`, `words`, `see`, `load`, `bye`, `exit`                                                                                          |
| **Definition**   | `def`, `:`                                                                                                                                              |
| **Stdlib**       | Load `toy/std/std.toy` or pass `--std`; includes `core.toy` (`square`, `succ`, `pred`, `inc`) and `list.toy` (`null`, `small`, `filter`)                |

Output convention:

- `print` and `printf` consume what they print
- `.` prints the top value without consuming it
- `.s` prints the whole stack without consuming it
- `cr` prints a newline

Aggregate observer convention:

- `len` leaves the list in place and pushes its length
- `geth` leaves the list in place, consumes the index, and pushes the selected
  element
- `first`, `rest`, and `empty?` inspect a list without consuming it
- `uncons`, `cons`, and `concat` construct or destructure lists and consume
  their list inputs
- `seth` remains an updating word and consumes the list, index, and new value

## Ecosystem & Tooling

Toy comes with a suite of tools to provide a modern development experience:

- [**Tree-sitter Grammar**](./docs/tree-sitter.md): High-performance incremental parser for syntax highlighting, indentation, and folding.
- [**Language Server (LSP)**](./docs/lsp.md): A standalone server written in Go that provides go-to-definition, hover documentation, and renaming.
- [**VS Code Extension**](./docs/vscode.md): Official VS Code support integrating the TextMate grammar and LSP client.
- [**REPL Guide**](./docs/repl.md): Interactive usage, multiline input, history/completion, interrupts, and REPL-specific behavior.

## Architecture

- **Lexer**: A recursive-descent tokenizer that supports nested blocks, strings, quoted symbols, and multiple comment styles (`\` and `(...)`).
- **Engine**: An iterative execution engine using a frame-based call stack. User-defined word recursion is safe from C stack limits; some native quotation runners still call `exec()` synchronously and are marked internally with `_r`.
- **Context**: Maintains the data stack, the global function hash table, and the active execution frames.
- **Memory**: Every object is a tagged union with an internal reference count. The system is designed to be leak-free (verifiable with `stb_leakcheck`).

## Getting Started

### Build (see [Build Instructions](./docs/build.md))

```powershell
cmake --build build
```

### Usage

```powershell
# Start the interactive REPL
.\build\toy.exe

# Basic run
.\build\toy.exe toy\program.toy

# Run with the standard library preloaded
.\build\toy.exe --std toy\program.toy

# Debug mode (prints tokenized program and final stack state)
.\build\toy.exe --debug toy\program.toy
```

## License

MIT
