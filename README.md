# Toy

Toy is a minimalist, stack-based language written in C. It started from a
traditional Forth shape, but is evolving into a quotation-first concatenative
language inspired by Joy, the language designed by Manfred von Thun.

The core idea is that code is data. Quotations `[ ... ]` and quoted symbols
`'name` are callable values, so they can be stored, passed to other words,
composed, inspected, and executed.

Based on the original [toyforth](https://github.com/antirez/toyforth) project by
**Salvatore Sanfilippo (antirez)**.
It has since grown into its own language with first-class quotations,
refcounted types and data structures, higher-order combinators, an iterative VM,
a persistent REPL (built with **antirez**'s `linenoise`), Tree-sitter grammar,
Go LSP, and VS Code extension.

## A Quick Look

```toy
\ A quotation is code-as-data. This word keeps the even numbers,
\ squares them, and folds the result into one value.
'evens-squared-sum [
    [ 2 % 0 == ] filter
    [ square ] map
    0 swap [ + ] fold
] def

[ 1 2 3 4 5 6 ] evens-squared-sum print   \ 56
```

## Getting Started

- [Build instructions](./docs/build.md)
- [REPL guide](./docs/repl.md)
- [Pre-built release binaries](https://github.com/Lorenzo287/toy/releases)

```powershell
cmake -S . -B build
cmake --build build

.\build\toy.exe # REPL
.\build\toy.exe program.toy
.\build\toy.exe --eval "1 2 + print"
.\build\toy.exe --debug program.toy
```

## Stack Syntax

Every expression of a programming language is a 
[tree](https://en.wikipedia.org/wiki/Abstract_syntax_tree) in disguise, 
and syntax is just determined by the 
[traversal](https://en.wikipedia.org/wiki/Tree_traversal) order. 
Take `1 + (2 * 3)`: it has `+` at the root, `1` as the left child, 
and `* 2 3` as the right subtree.
Infix, prefix and postfix syntax stem from different traversals,
namely in-order, pre-order, post-order.

| Style | Traversal | Example |
| ----- | --------- | ------- |
| Infix | left child, operator, right child | `1 + (2 * 3)` |
| Prefix (Lisp) | operator before children | `(+ 1 (* 2 3))` |
| postfix (Forth/Toy) | children before operator | `1 2 3 * +` |

In Toy, values are pushed as they are read. When `*` appears, it consumes
`2 3` and leaves `6`; when `+` appears, it consumes `1 6` and leaves `7`.
That is the whole syntax: data first, then the word that transforms it.

## Language Tour

### Words Transform the Stack

```toy
2 3 + print          \ 5

\ Define words by binding symbols to quotations.
'square [ dup * ] def
'cube [ dup square * ] def

5 square print       \ 25
3 cube print         \ 27

\ Local captures give names to stack values when pure stack code gets dense.
'hypot2 [ | x y | $x $x * $y $y * + ] def
3 4 hypot2 print     \ 25
```

### Code Is Data

```toy
\ Quotations and quoted symbols are first-class callable values.
[ 1 2 + ] exec       \ leaves 3
[ 1 2 + ] i          \ same
'dup                 \ leaves the symbol 'dup as data

\ Combinators apply callables in different stack contexts.
1 2 4 [ + ] dip      \ leaves 3 4
5 [ 1 + ] keep       \ leaves 5 6
5 'succ 'square bi   \ leaves 6 25
```

Words that consume deferred code accept both kinds of quotations,
meaning that `'` and `[]` are functionally equivalent (even though the second is
a collection and can contain more than one object).
Dictionary introspection words such as `see`, `doc`, `body`, `word?`, `var?`,
and `name` consume symbols as names, so use `'` rather than `[]` for those positions.
Vectors are the compound quotation type; linked lists created with `( ... )`
are data sequences, not callables.

### Control Is Built from Words

```toy
5 [ 0 > ] [ "positive" print ] if

10 [ 0 > ] [ dup print 1 - ] while drop

\ Recursion schemes can express algorithms without naming recursive helpers.
'qsort [
    [ len 2 < ]
    []
    [ uncons [ > ] split ]
    [ swapd cons concat ]
    binrec
] def

'merge-sort [
    [ len 2 < ]
    []
    [ split-mid ]
    [ [ < ] merge ]
    binrec
] def
```

Predicate callables used by words such as `if`, `while`, `filter`, `split`,
and `merge` run in a stack sandbox: the boolean result is read, then the
surrounding stack is restored (for example code inside the conditions does not
consume the stack). Side effects inside the predicate still happen.

### Collections Are Native Values

```toy
\ Vectors are ordered arrays and also the quotation representation.
[ 1 2 3 ] 4 push-back       \ leaves [1 2 3 4]
[ 1 2 3 ] pop-back          \ leaves [1 2] 3

\ Lists are persistent, front-oriented sequences.
( 1 2 3 ) uncons            \ leaves 1 (2 3)
0 ( 1 2 3 ) cons            \ leaves (0 1 2 3)

\ Strings are byte sequences; one string item is a one-byte string.
"abc" first                 \ leaves "a"
"abc" last                  \ leaves "c"
"ab" "c" push-back          \ leaves "abc"
"abcabc" "bc" index-of      \ leaves 1
"abc" "bc" contains?        \ leaves true
63 >char char-code          \ leaves 63

"  alpha,beta,gamma  " trim "," split [ upper ] map "-" join print

\ Toy also supports maps, sets, deques and pqueues
{ 'name "Ada" 'age 36 } 'name get print
#{ "red" "green" } #{ "green" "blue" } union items sort print
[ 1 2 3 ] >deque 0 push-front print
[ [ 10 "low" ] [ 1 "urgent" ] ] >pqueue pairs print
```

Vector are indexed and optimized for the back, which makes the perfect
to represent Toy's data stack itself.

Lists are optimized for the front. `cons`, `rest`, and `uncons` are
constant-time and may share tails. To build a list in forward order without
repeated linear `push-back`, prepend each item and reverse once:

```toy
( ) [ 1 2 3 4 ] [ swap cons ] fold reverse   \ leaves (1 2 3 4)
```

### Files and Introspection

Toy has a number of words that cover file managment, introspection
(you can easily find documentation inside the lang itself and the REPL),
types utilities, system calls and many others. Explore them all
in the table below or by typing `help` and later `'name doc print` in the REPL.


```toy
"notes.txt" "hello from Toy" write-file
"notes.txt" read-file print

'square [ dup * ] def
'square see print
```

Comments use `\` to the end of a line or `/* ... */` for block comments.

## What It Supports

- Runtime values: 64-bit integers, double-precision floats, booleans, strings,
  symbols, vectors, lists, maps, sets, deques, and priority queues.
- Literal syntax: `[ ... ]` for vectors/quotations, `( ... )` for lists,
  `{ key value ... }` for maps, and `#{ ... }` for sets.
- Explicit constructors for secondary structures such as `>deque` and
  `>pqueue`.
- First-class quotations and symbols for deferred execution, higher-order code,
  and dictionary introspection.
- Stack combinators such as `dip`, `keep`, `bi`, `app2`, `linrec`, `binrec`,
  `genrec`, and `treerec`.
- Sequence combinators such as `each`, `map`, `fold`, `filter`, `some`, `all`,
  `split`, and `merge`.
- Shared sequence words for vectors, lists, and strings when the result type is
  clear.
- Representation predicates such as `vector?`, `list?`, and `symbol?`, plus
  capability predicates such as `sequence?` and `callable?`.
- Local captures with `| name |` and `$name`.
- File I/O, string manipulation, dictionary introspection, process helpers,
  time words, and an interactive REPL with history/completion.
- Numeric literals include 64-bit integers and double-precision floats; decimal
  float literals also accept exponent notation such as `1e6` and `2.5e-3`.
- Strings are byte sequences, not Unicode strings. A Toy character is exactly
  a one-byte string.

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

Use `repr` to obtain a source-style string with bytes escaped (makes me think 
about [quines](toy/quines/quine.toy)). `print` always prints one value literally with a newline, 
while `printf` explicitly interprets `{}` placeholders and does not append a newline.

## Built-in Words

<!-- BEGIN GENERATED BUILTIN TABLE -->
| Category                    | Words |
| --------------------------- | ----- |
| Stack                       | `dup`, `drop`, `swap`, `over`, `rot`, `swapd`, `nip`, `tuck`, `pick`, `roll`, `empty` |
| Math                        | `+`, `-`, `*`, `/`, `%`, `mod`, `neg`, `abs`, `max`, `min`, `sqrt`, `pow`, `exp`, `log`, `log10`, `sin`, `cos`, `tan`, `floor`, `ceil`, `round`, `pred`, `succ`, `square`, `cube`, `pi`, `e`, `tau`, `inf`, `nan`, `inf?`, `nan?`, `rand` |
| Logic / Bitwise             | `and`, `or`, `xor`, `not`, `shl`, `shr` |
| Comparison                  | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Control                     | `exec`, `i`, `if`, `ifelse`, `while`, `cond`, `try`, `error` |
| Combinators                 | `app2`, `infra`, `cleave`, `construct`, `replicate`, `times`, `dip`, `keep`, `bi`, `linrec`, `binrec`, `genrec`, `treerec` |
| Sequence Combinators        | `each`, `map`, `fold`, `filter`, `some`, `all`, `split`, `merge` |
| Ordered Collections         | `>vector`, `>list`, `>string`, `>deque`, `at`, `set-at`, `slice`, `take`, `skip`, `len`, `concat`, `reverse`, `split-mid`, `range`, `empty?` |
| Collection Endpoints        | `first`, `last`, `rest`, `uncons`, `cons`, `push-front`, `push-back`, `pop-front`, `pop-back` |
| Sequence Search / Ordering  | `contains?`, `index-of`, `unique`, `sort` |
| Strings / Characters        | `join`, `trim`, `upper`, `lower`, `char?`, `>char`, `char-code`, `letter?`, `digit?`, `alnum?`, `space?`, `upper?`, `lower?`, `punct?` |
| Maps / Sets                 | `>map`, `>set`, `has?`, `get`, `get-or`, `assoc`, `dissoc`, `keys`, `values`, `pairs`, `items`, `insert`, `remove` |
| Set Algebra                 | `union`, `intersection`, `difference`, `symmetric-difference`, `subset?`, `proper-subset?`, `superset?`, `proper-superset?`, `disjoint?` |
| Priority Queues             | `>pqueue`, `pq-push`, `pq-peek`, `pq-pop` |
| Types                       | `type-of`, `bool?`, `int?`, `float?`, `string?`, `symbol?`, `vector?`, `list?`, `map?`, `set?`, `deque?`, `pqueue?`, `number?`, `sequence?`, `callable?` |
| Definitions / Introspection | `def`, `word?`, `var?`, `body`, `intern`, `name`, `words`, `see`, `doc`, `search-words`, `repr` |
| Console                     | `printf`, `print`, `.`, `.s`, `.S`, `read-key`, `read-line`, `clear` |
| Files                       | `load`, `read-file`, `write-file`, `delete-file`, `read-lines`, `file-exists?` |
| Environment / Processes     | `argc`, `argv`, `env?`, `get-env`, `set-env`, `pwd`, `shell`, `exit` |
| Time                        | `sleep`, `unix-time`, `local-time`, `utc-time`, `cpu-time`, `monotonic-ns` |
<!-- END GENERATED BUILTIN TABLE -->

Words are listed under their primary concept even when they support more than
one representation. For example, `push-back` works on sequences and deques,
while `pairs` works on maps and priority queues.

Builtin registration and metadata are canonical in `builtins.json`. After
editing it, run `node tools/generate-builtins.js`; use `--check` to verify that
the committed generated files are current.

## Tooling

- [REPL](./docs/repl.md)
- [Tree-sitter](./docs/tree-sitter.md)
- [LSP](./docs/lsp.md)
- [VS Code](./docs/vscode.md)

## Extra

- [Combinator Reference](./docs/combinators.md)
- [Benchmarks](./benchmarks/README.md)
- [Data Model Reference](./docs/data-model.md)
- [Runtime Internals](./docs/runtime-internals.md)
- [Roadmap](./docs/language-roadmap.md)

## License

MIT
