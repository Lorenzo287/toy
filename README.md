# Toy

Toy is a minimalist, stack-based interpreter written in C. It started from a
traditional Forth shape, but is evolving into a quotation-first concatenative
language inspired by Joy, the language designed by Manfred von Thun.

The core idea: code is data. Quotations (`[ ... ]`) and quoted symbols (`'name`)
are callable values that can be stored, passed, composed, and executed.

Based on the original [toyforth](https://github.com/antirez/toyforth) project by
**Salvatore Sanfilippo (antirez)**. Toy adds a persistent REPL using **antirez**'s
`linenoise` library for history and tab completion, file/string/list
words, Tree-sitter grammar, Go LSP, and VS Code extension.

## What It Supports

- Integers, floats, booleans, strings, symbols, and lists.
- First-class quotations for deferred execution and higher-order code.
- Stack combinators such as `dip`, `keep`, `bi`, `map`, `filter`, `fold`,
  `linrec`, `binrec`, and `genrec`.
- Shared sequence words for lists and strings when the result type is clear.
  String items are one-byte strings.
- Representation predicates such as `list?` and `symbol?`, plus capability
  predicates such as `sequence?` and `callable?`.
- Local captures with `{ name }` and `$name` when stack-only code gets too hard
  to read.
- File I/O, string manipulation, dictionary introspection, process helpers, and
  an interactive REPL with history/completion.
- An iterative execution engine, so user-defined recursion does not depend on
  the C call stack.

## Language Tour

```toy
\ Define words by binding a symbol to a quotation.
'square [ dup * ] def
'cube [ dup square * ] def
5 square print       \ 25
3 cube .             \ prints 27, leaves it on the stack

\ Local captures give names to stack values inside a frame.
'hypot2 [ { x y } $x $x * $y $y * + ] def
3 4 hypot2 print     \ 25
```

```toy
\ Quotations and quoted symbols are first-class callable values.
[ 1 2 + ] exec       \ leaves 3
[ 1 2 + ] i          \ same
'dup exec            \ executes dup
'dup                 \ leaves the symbol 'dup as data

\ Combinators apply callables in different stack contexts.
1 2 4 [ + ] dip      \ leaves 3 4
5 [ 1 + ] keep       \ leaves 5 6
5 'succ 'square bi   \ leaves 6 25
```

```toy
\ Conditions can be callables that inspect the stack.
5 [ 0 > ] [ "Positive" print ] if

\ Iteration and mapping are words over callables.
10 [ 0 > ] [ . 1 - ] while
[ 1 2 3 ] 'succ map .s

\ Recursion schemes can express algorithms without naming recursive helpers.
'qsort [
    [ len 2 < ]
    []
    [ uncons [ > ] split ]
    [ swapd cons concat ]
    binrec
] def

[ 3 1 4 1 5 2 ] qsort
```

```toy
\ Lists, strings, and files are available for small scripts.
[ 1 2 3 ] uncons     \ leaves 1 [2 3]
0 [ 1 2 3 ] cons     \ leaves [0 1 2 3]
"abc" first          \ leaves "a"
"ab" "c" append      \ leaves "abc"
"ab" "cd" concat     \ leaves "abcd"
"  alpha,beta,gamma  " trim "," split [ upper ] map "-" join print

"notes.txt" "hello from Toy" writef
"notes.txt" readf print
```

## Stack Effects

Toy words consume their declared inputs by default:

```toy
"hello" len          \ leaves 5
[ 1 2 3 ] first      \ leaves 1
10 int?              \ leaves true
```

Use stack words or combinators when a value should be preserved:

```toy
"hello" dup len      \ leaves "hello" 5
[ 1 2 3 ] [ len ] keep
```

Predicate callables used by control and predicate combinators are the main
exception. Words such as `if`, `while`, `linrec`, `filter`, `split`, and
`merge` run predicates in a stack sandbox: they read the boolean result and
restore the surrounding data stack afterward. Side effects performed inside the
predicate are not undone.

Diagnostic display words are also observers: `.`, `.s`, and `.S` print without
changing the data stack.

## Built-in Words

| Category      | Words                                                                                                                                                                                                                                                              |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Stack         | `dup`, `drop`, `swap`, `over`, `rot`, `swapd`, `nip`, `tuck`, `pick`, `roll`, `empty`                                                                                                                                                                              |
| Math          | `+`, `-`, `*`, `/`, `%`, `mod`, `abs`, `neg`, `max`, `min`, `sqrt`, `pow`, `exp`, `log`, `log10`, `sin`, `cos`, `tan`, `floor`, `ceil`, `round`, `pred`, `succ`, `square`, `cube`, `pi`, `e`, `tau`                                                                |
| Logic         | `and`, `or`, `xor`, `not`, `shl`, `shr`                                                                                                                                                                                                                            |
| Comparison    | `==`, `!=`, `<`, `>`, `<=`, `>=`                                                                                                                                                                                                                                   |
| Control       | `if`, `ifelse`, `while`, `try`, `error`, `exec`, `i`, `app2`, `infra`, `cond`, `cleave`, `construct`, `replicate`, `times`, `dip`, `keep`, `bi`, `linrec`, `binrec`, `genrec`, `treerec`                                                                           |
| Combinators   | `each`, `map`, `fold`, `filter`, `some`, `all`, `split`, `merge`                                                                                                                                                                                                   |
| List/String   | `geth`, `seth`, `slice`, `take`, `dropn`, `len`, `first`, `rest`, `uncons`, `cons`, `append`, `concat`, `reverse`, `join`, `trim`, `upper`, `lower`, `splitmid`, `range`, `empty?`, `char?`, `letter?`, `digit?`, `alnum?`, `space?`, `upper?`, `lower?`, `punct?` |
| Introspection | `typeof`, `bool?`, `int?`, `float?`, `str?`, `symbol?`, `list?`, `number?`, `sequence?`, `callable?`, `nan?`, `inf?`, `word?`, `var?`, `inf`, `nan`, `body`, `intern`, `name`, `words`, `see`                                                                      |
| I/O           | `print`, `printf`, `.`, `.s`, `.S`, `cr`, `key`, `input`, `load`, `readf`, `writef`, `delf`, `readl`, `exists?`, `clear`, `page`                                                                                                                                   |
| System        | `rand`, `sleep`, `argc`, `argv`, `env?`, `getenv`, `setenv`, `pwd`, `shell`, `time`, `clock`, `bye`, `exit`                                                                                                                                                        |
| Definition    | `def`, `:`                                                                                                                                                                                                                                                         |

## Tooling

- [REPL](./docs/repl.md)
- [Tree-sitter](./docs/tree-sitter.md)
- [LSP](./docs/lsp.md)
- [VS Code](./docs/vscode.md)

## Extra

- [Combinator Examples](./docs/combinators.md)
- [Roadmap](./docs/language-roadmap.md)

## Getting Started

- [Build](./docs/build.md)

```powershell
cmake --build build
.\build\toy.exe
.\build\toy.exe toy\program.toy
.\build\toy.exe --debug toy\program.toy
```

## License

MIT
