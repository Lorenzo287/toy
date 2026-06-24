# Toy

Toy is a minimalist, stack-based interpreter written in C. It started from a
traditional Forth shape, but is evolving into a quotation-first concatenative
language inspired by Joy, the language designed by Manfred von Thun.

The core idea: code is data. Quotations (`[ ... ]`) and quoted symbols (`'name`)
are callable values that can be stored, passed, composed, and executed.

Based on the original [toyforth](https://github.com/antirez/toyforth) project by
**Salvatore Sanfilippo (antirez)**. Toy adds a persistent REPL using **antirez**'s
`linenoise` library for history and tab completion, file/string/collection
words, Tree-sitter grammar, Go LSP, and VS Code extension.

## What It Supports

- 64-bit integers, double-precision floats, booleans, strings, symbols,
  vectors, lists, maps, sets, deques, and priority queues.
- Decimal float literals accept exponent notation such as `1e6` and `2.5e-3`.
- First-class quotations for deferred execution and higher-order code.
- Stack combinators such as `dip`, `keep`, `bi`, `map`, `filter`, `fold`,
  `linrec`, `binrec`, and `genrec`.
- Shared sequence words for vectors, lists, and strings when the result type is
  clear. A Toy character is exactly a one-byte string; strings are not
  Unicode-aware.
- Dedicated collection syntax: `[ ... ]` for ordered vectors/quotations,
  `( ... )` for linked lists, `{ key value ... }` for maps, and `#{ ... }` for sets.
- Comments use `\` to the end of a line or `/* ... */` for block comments.
- Explicit constructors for secondary structures such as `>deque` and `>pqueue`.
- Value output tags secondary structures as `deque[...]` and `pqueue[...]` so
  each reads as one value. These are display forms, not literal syntax;
  `repr` and `.S` use reconstructable `[...] >deque`/`[...] >pqueue` source.
- Representation predicates such as `vector?`, `list?`, and `symbol?`, plus
  capability predicates such as `sequence?` and `callable?`.
- Local captures with `| name |` and `$name` when stack-only code gets too hard
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
'hypot2 [ | x y | $x $x * $y $y * + ] def
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

Callable equivalence applies where a word consumes deferred code. Dictionary
introspection words such as `see`, `doc`, `body`, `word?`, `var?`, and `name`
consume symbols as names, so use `'name` rather than `[ name ]` for those
positions.
Vectors are the compound quotation type; linked lists created with `( ... )`
are data sequences, not callables.

Lists are persistent front-oriented sequences. `cons`, `rest`, and `uncons`
are constant-time and may share tails. To construct a list in forward order
without repeated linear `push-back`, prepend each item and reverse once:

```toy
( ) [ 1 2 3 4 ] [ swap cons ] fold reverse   \ leaves (1 2 3 4)
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
\ Vectors, lists, strings, and files are available
( 1 2 3 ) uncons     \ leaves 1 (2 3)
0 ( 1 2 3 ) cons     \ leaves (0 1 2 3)
"abc" first          \ leaves "a"
"abc" last           \ leaves "c"
"ab" "c" push-back   \ leaves "abc"
"ab" "cd" concat     \ leaves "abcd"
"abcabc" "bc" index-of \ leaves 1
"abc" "bc" contains? \ leaves true
[ "a" "b" "c" ] >string \ leaves "abc"
63 >char char-code     \ leaves 63
[ 1 2 3 ] pop-back    \ leaves [ 1 2 ] 3
"  alpha,beta,gamma  " trim "," split [ upper ] map "-" join print

{ 'name "Ada" 'age 36 } 'name get print
{ 'name "Ada" } 'missing "unknown" get-or print
[ [ 'name "Ada" ] [ 'age 36 ] ] >map pairs print
#{ "red" "green" "blue" } "green" has? print
[ 1 2 2 3 ] >set items print
#{ "red" "green" } #{ "green" "blue" } union items print
[ 1 2 3 ] >deque 0 push-front items print
[ [ 10 "low" ] [ 1 "urgent" ] ] >pqueue pairs print

"notes.txt" "hello from Toy" write-file
"notes.txt" read-file print
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

String escapes are strict. `\n`, `\r`, `\t`, `\"`, and `\\` cover common
text escapes; `\xHH` constructs any byte from exactly two hexadecimal digits.
Use `repr` to obtain a source-style string with bytes escaped. `print` always
prints one value literally with a newline, while `printf` explicitly interprets
`{}` placeholders and does not append a newline.

`unix-time` returns whole seconds since 1970-01-01 00:00:00 UTC.
`local-time` and `utc-time` return maps with `year`, `month`, `day`, `hour`,
`minute`, and `second` fields. `cpu-time` reports process CPU seconds without
discarding the underlying timer resolution. `monotonic-ns` returns an integer
reading with an unspecified origin; subtract two readings to measure elapsed
wall time. Its unit is one nanosecond, while actual resolution depends on the
platform clock.

## Built-in Words

| Category                 | Words |
| ------------------------ | ----- |
| Stack                    | `dup`, `drop`, `swap`, `over`, `rot`, `swapd`, `nip`, `tuck`, `pick`, `roll`, `empty` |
| Math                     | `+`, `-`, `*`, `/`, `%`, `mod`, `abs`, `neg`, `max`, `min`, `sqrt`, `pow`, `exp`, `log`, `log10`, `sin`, `cos`, `tan`, `floor`, `ceil`, `round`, `pred`, `succ`, `square`, `cube`, `pi`, `e`, `tau`, `inf`, `nan`, `inf?`, `nan?`, `rand` |
| Logic / Bitwise          | `and`, `or`, `xor`, `not`, `shl`, `shr` |
| Comparison               | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Control                  | `exec`, `i`, `if`, `ifelse`, `while`, `cond`, `try`, `error` |
| Combinators              | `app2`, `infra`, `cleave`, `construct`, `replicate`, `times`, `dip`, `keep`, `bi`, `linrec`, `binrec`, `genrec`, `treerec` |
| Sequence Combinators     | `each`, `map`, `fold`, `filter`, `some`, `all`, `split`, `merge` |
| Ordered Collections      | `>vector`, `>list`, `>string`, `>deque`, `at`, `set-at`, `slice`, `take`, `skip`, `len`, `concat`, `reverse`, `split-mid`, `range`, `empty?` |
| Collection Endpoints     | `first`, `last`, `rest`, `uncons`, `cons`, `push-front`, `push-back`, `pop-front`, `pop-back` |
| Sequence Search / Ordering | `contains?`, `index-of`, `unique`, `sort` |
| Strings / Characters    | `join`, `trim`, `upper`, `lower`, `char?`, `>char`, `char-code`, `letter?`, `digit?`, `alnum?`, `space?`, `upper?`, `lower?`, `punct?` |
| Maps / Sets              | `>map`, `>set`, `has?`, `get`, `get-or`, `assoc`, `dissoc`, `keys`, `values`, `pairs`, `items`, `insert`, `remove` |
| Set Algebra              | `union`, `intersection`, `difference`, `symmetric-difference`, `subset?`, `proper-subset?`, `superset?`, `proper-superset?`, `disjoint?` |
| Priority Queues          | `>pqueue`, `pq-push`, `pq-peek`, `pq-pop` |
| Types                    | `type-of`, `bool?`, `int?`, `float?`, `string?`, `symbol?`, `vector?`, `list?`, `map?`, `set?`, `deque?`, `pqueue?`, `number?`, `sequence?`, `callable?` |
| Definitions / Introspection | `def`, `word?`, `var?`, `body`, `intern`, `name`, `words`, `see`, `doc`, `search-words`, `repr` |
| Console                  | `printf`, `print`, `.`, `.s`, `.S`, `read-key`, `read-line`, `clear` |
| Files                    | `load`, `read-file`, `write-file`, `delete-file`, `read-lines`, `file-exists?` |
| Environment / Processes | `argc`, `argv`, `env?`, `get-env`, `set-env`, `pwd`, `shell`, `exit` |
| Time                     | `sleep`, `unix-time`, `local-time`, `utc-time`, `cpu-time`, `monotonic-ns` |

Words are listed under their primary concept even when they support more than
one representation. For example, `push-back` works on sequences and deques,
while `pairs` works on maps and priority queues.

## Tooling

- [REPL](./docs/repl.md)
- [Tree-sitter](./docs/tree-sitter.md)
- [LSP](./docs/lsp.md)
- [VS Code](./docs/vscode.md)

## Extra

- [Combinator Examples](./docs/combinators.md)
- [Benchmarks](./benchmarks/README.md)
- [Data Model Plan](./docs/data-model.md)
- [Runtime Internals](./docs/runtime-internals.md)
- [Roadmap](./docs/language-roadmap.md)

## Releases

Download pre-built binaries for Windows, Linux, and macOS from the [Releases](https://github.com/Lorenzo287/toy/releases) page.

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
