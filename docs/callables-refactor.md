# Callable Refactor Plan

Toy currently has two executable-looking value forms:

```toy
'upper
[ upper ]
```

They are equivalent for `exec`/`i`, but many higher-order words only accept the
list quotation form. This is an implementation leak: the C object model stores
quoted symbols and lists differently, while the language-level idea is that both
can represent deferred code.

This plan introduces `callable` as an explicit runtime capability. It should
make the language rule consistent without erasing the useful data distinction
between an atomic symbol and a compound quotation/list.

Toy is still in active language design, so this plan does not require full
backwards compatibility. Existing behavior is evidence, not a constraint. Keep
it when it matches the cleaner model; change or remove it when preserving it
would make the language harder to explain.

## Target Model

Toy has three relevant source-level cases:

```toy
upper      \ resolve and execute the word now
'upper     \ push an atomic deferred word/name
[ ... ]    \ push a compound deferred program/list
```

At the language level:

- A **symbol** is an atomic name value.
- A **quotation** is a list value whose contents can be executed as a program.
- A **callable** is either a symbol or a quotation in a word position that
  explicitly consumes deferred code.
- A **sequence** is either a list or a string in a word position that explicitly
  consumes iterable data.

Symbols and quotations are not the same data type. They share one capability:
they can be called.

This gives the user-facing rule:

> Any word documented as consuming a callable accepts either `'word` or
> `[ ... ]`.

Examples that should eventually work:

```toy
"abc" 'upper map
"abc" [ upper ] map

[ 1 2 3 ] 'succ map
[ 1 2 3 ] [ 1 + ] map

5 'square keep
5 [ square ] keep
```

## Compatibility Stance

Prioritize the model over historical compatibility. In particular:

- It is acceptable to change stack effects, type names, predicates, or
  introspection words if the new behavior is simpler and documented.
- It is acceptable to remove or rename words whose meaning becomes misleading
  after `callable` is introduced.
- It is acceptable to change tests and examples that encode accidental
  implementation details.
- It is not acceptable to keep two behaviors merely because both already exist.
  If both survive, they need distinct language-level purposes.

The implementation plan may still stage changes conservatively to keep the
interpreter working after each commit. That is an engineering tactic, not a
semantic promise.

## Current Implementation Leak

The current implementation exposes representation details in behavior:

- `TF_OBJ_TYPE_SYMBOL` stores a `str.quoted` flag.
- `TF_OBJ_TYPE_LIST` represents both data lists and executable quotations.
- The frame evaluator resolves unquoted symbols and pushes quoted symbols.
- `exec`/`i` accept both lists and symbols.
- Most control and collection combinators hard-check `TF_OBJ_TYPE_LIST`.

That means `'upper exec` works, while `"abc" 'upper map` does not. The
interpreter already has enough machinery to call a symbol; that machinery is
not yet used consistently by higher-order words.

## Design Principles

- Prefer keeping `'` as the compact atomic form of deferred code and the
  natural representation for word names. Revisit this only if a better syntax
  covers definition names, symbolic data, and atomic callables more clearly.
- Prefer keeping symbols and lists as distinct object representations. Collapse
  them only if there is a stronger language model for symbolic names,
  quotations, and list data.
- Do not make symbols auto-execute everywhere. A symbol is executed only by the
  frame evaluator when it appears as an unquoted word token, or by a word whose
  contract explicitly consumes a callable.
- Keep `sequence` separate from `callable`. Lists can be both executable
  quotations and iterable data, so the consuming word's stack effect must decide
  which capability is being requested.
- Prefer one central callable API over local checks such as
  `o->type == TF_OBJ_TYPE_LIST || o->type == TF_OBJ_TYPE_SYMBOL`.
- Do not preserve public introspection behavior by default. Decide which
  predicates and type names belong to the new model.

## Runtime Abstraction

Add a small execution-level interface, probably in `include/tf_exec.h` and
`src/tf_exec.c`:

```c
bool tf_is_callable(tf_obj *o);
tf_ret tf_call_callable(tf_ctx *ctx, tf_obj *callable);
tf_ret tf_call_callable_sync(tf_ctx *ctx, tf_obj *callable);
tf_obj *stack_pop_callable(tf_ctx *ctx);
```

Initial behavior:

```c
TF_OBJ_TYPE_LIST   -> frame_push(ctx, callable)
TF_OBJ_TYPE_SYMBOL -> call_symbol(ctx, callable)
otherwise          -> TF_ERR
```

`tf_call_callable()` should be the only place, outside the main frame evaluator,
that dispatches callable values. Native words should stop calling `exec()` or
`call_symbol()` directly when what they mean is "run deferred code".

`exec(tf_ctx *, tf_obj *)` can remain list-only as the low-level "execute a
quotation/list frame" primitive. The native word `exec`/`i` should become a thin
wrapper around `tf_call_callable()`.

There are two execution modes:

- `tf_call_callable()` schedules the callable in the current frame context. This
  is appropriate for the native `exec`/`i` word.
- `tf_call_callable_sync()` runs the callable and waits for completion. This is
  required by `_r` natives such as `map`, `filter`, `while`, and recursion
  combinators, because they need to inspect stack results before returning.

## Quotedness and Symbols

The current `str.quoted` flag mixes two concepts:

- source/evaluation mode: should this symbol token be resolved now or pushed?
- runtime value identity: this object is a symbol with a name

The callable refactor does not need to solve this immediately, but the design
should avoid depending on quotedness outside the frame evaluator and source
printing.

Short-term rule:

- `tf_is_callable()` accepts `TF_OBJ_TYPE_SYMBOL` regardless of `str.quoted`.
- The frame evaluator still uses `str.quoted` to decide whether a symbol token
  in a program is pushed or resolved.
- `call_symbol()` ignores `str.quoted`; when a symbol has been passed to a word
  that consumes a callable, the caller has explicitly requested execution.
- Lexer-created bare word tokens use `create_symbol_obj()`. Words that
  manufacture symbol values as data should use `create_quoted_symbol_obj()` so
  source-style printing reconstructs literal symbols rather than executable word
  tokens.

Long-term cleanup to consider:

- Treat quotedness as token metadata rather than symbol identity.
- Introduce an internal literal wrapper or token kind if the interpreter ever
  separates parsed programs from runtime values.

This is also where backwards-incompatible cleanup may be worth it. For example,
if quotedness remains an object flag, `symbol?`, printing, equality, and
dictionary lookup should be audited so quoted and unquoted symbols do not
accidentally behave differently as runtime data.

## Word Resolution

Keep word lookup as a separate layer:

```c
tf_func *get_func(tf_ctx *ctx, tf_obj *name);
tf_ret call_symbol(tf_ctx *ctx, tf_obj *symb);
tf_ret tf_call_callable(tf_ctx *ctx, tf_obj *callable);
```

Responsibilities:

- `get_func()` only resolves a symbol name to a dictionary entry. Strings are
  not accepted as alternate word-name values.
- `call_symbol()` executes a resolved word name, using frame scheduling for user
  words and direct calls for native words.
- `tf_call_callable()` dispatches lists versus symbols and reports
  callable-level type errors.

This keeps dictionary lookup, callable dispatch, and frame execution separate.

Resolved for the current design:

- Public `callable?` means executable now: lists are callable, and symbols are
  callable only when they name a defined word.
- `word?` and `var?` are symbol-only. Strings are byte sequences, not alternate
  spellings for names.

Open question for a later pass:

- Should calling an undefined symbol fail at `tf_call_callable()` type/lookup
  time with a callable-specific diagnostic, or should it reuse the ordinary
  undefined-word path?

## Introspection and Type Predicates

The current predicates expose raw object representation:

```toy
typeof
symbol?
list?
str?
sequence?
callable?
word?
var?
body
intern
name
see
```

The chosen model is to keep representation predicates and add capability
predicates:

```toy
'upper typeof           \ "symbol"
[ upper ] typeof        \ "list"
[ upper ] callable?     \ true
'upper callable?        \ true, when upper is defined
"upper" callable?       \ false
[ 1 2 3 ] sequence?     \ true
"abc" sequence?         \ true
'upper sequence?        \ false
```

`typeof`, `symbol?`, `list?`, and `str?` describe storage representation.
`sequence?` and `callable?` describe behavior that words may consume.

Related decisions:

- `body` and `see` remain name/dictionary introspection words and require
  symbols.
- `intern` and `name` remain useful if symbols stay as first-class names.
- `list?` should not be overloaded to mean quotation; list-as-sequence and
  list-as-quotation are contextual capabilities.

## Native Word Contracts

Update higher-order words from "quotation" contracts to "callable" contracts
where a symbol shorthand is meaningful.

First pass, low risk:

```toy
exec        ( callable -- ... )
i           ( callable -- ... )
app2        ( x y callable -- ... )
dip         ( x callable -- ... x )
keep        ( x callable -- x ... )
bi          ( x callable callable -- ... )
times       ( n callable -- ... )
each        ( seq callable -- ... )
map         ( seq callable -- seq )
fold        ( acc seq callable -- acc )
filter      ( seq callable -- seq )
some        ( seq callable -- bool )
all         ( seq callable -- bool )
split       ( seq callable -- true-part false-part )
merge       ( seq seq callable -- seq )
```

Second pass, more care because of predicates, stack sandboxes, branching, or
synthetic continuations:

```toy
if          ( bool|callable callable -- ... )
ifelse      ( bool|callable callable callable -- ... )
while       ( callable callable -- ... )
try         ( callable callable -- ... )
cond        ( clauses -- ... )
cleave      ( x [ callable... ] -- ... )
construct   ( x [ callable... ] -- list )
replicate   ( n callable -- list )
linrec      ( callable callable callable callable -- ... )
binrec      ( callable callable callable callable -- ... )
genrec      ( callable callable callable callable -- ... )
treerec     ( tree callable callable -- result )
infra       ( list callable -- list )
```

Some words consume lists as structural containers of callables. For those,
do not simply replace the outer list with callable:

- `cond` consumes a list of two-item clauses. The clause predicates and bodies
  should become callables, but the clause list remains structural data.
- `cleave` and `construct` consume a list of branches. Each branch should become
  callable, but the branch container remains a list.
- `genrec` currently supports either four quotations on the stack or a list of
  four quotations. The four parts should become callables; the grouped form
  remains a list of four parts.

## Predicate Sandboxing

Predicate runners should use the callable API too, but must preserve the
existing stack rule:

> Predicate callables observe the surrounding data stack by sandboxing stack
> changes and reading one boolean result. Side effects inside predicates remain
> real effects.

The current helper:

```c
tf_eval_predicate_sandbox_r(...)
```

should accept a callable instead of only `TF_OBJ_TYPE_LIST`, then call
`tf_call_callable()` internally.

Careful regression cases:

```toy
[ 1 2 3 ] 'int? all
"abc" 'str? all
[ 1 "x" 3 ] 'number? filter
5 'positive? [ "yes" ] if
```

## Sequence Overloads

Do not conflate sequence dispatch with callable dispatch.

Current sequence rules should remain:

- Lists and strings are sequences when a word asks for a sequence.
- String items are one-byte strings.
- A list can be data even though lists are also executable quotations.

For overloaded words, dispatch should be explicit and ordered by the word's
language contract, not by incidental object representation.

`split` is the main case:

```toy
"a,b,c" "," split       \ string separator form
"abc" 'letter? split    \ sequence predicate form
"abc" [ "b" == ] split  \ sequence predicate form
```

Recommended dispatch:

1. If the top two values are strings, use the string-separator form.
2. Otherwise, if the lower value is a sequence and the top value is callable,
   use the sequence predicate form.
3. Otherwise, report a type error.

This preserves the existing string split behavior while allowing quoted-symbol
predicates.

## Parser and Source Printing

No syntax change is required for the first implementation pass.

The lexer should continue to parse:

- `word` as a symbol token that the frame evaluator resolves.
- `'word` as a symbol token that the frame evaluator pushes.
- `[ ... ]` as a list/quotation.
- `{ ... }` as capture binding syntax.
- `$name` as variable fetch syntax.

Source printers (`print_source_obj`, `see`) should keep preserving the current
surface syntax. A quoted symbol should still print as `'word`; a quotation
should still print as `[ ... ]`.

Do not normalize `'word` to `[ word ]` during parsing. That would lose the
atomic symbol value and make introspection/source round-tripping worse.

## Implementation Order

1. Confirm the public model.
   - Decide whether `'` remains the atomic callable syntax.
   - Decide whether existing type predicates are kept, renamed, reduced, or
     supplemented with capability predicates.
   - Treat symbols as word names in introspection helpers; strings remain byte
     sequences.

2. Add callable helpers in `tf_exec`.
   - Add declarations to `include/tf_exec.h`.
   - Implement `tf_is_callable()`, `stack_pop_callable()`, and
     `tf_call_callable()`.
   - Convert native `exec`/`i` to use the helper.

3. Add focused tests for callable equivalence.
   - `'upper exec` and `[ upper ] exec`.
   - User-defined symbols as callables.
   - Undefined symbols produce the same error path through the callable helper.

4. Convert sequence combinators.
   - Replace local list-only checks in `each`, `map`, `fold`, `filter`, `some`,
     `all`, `split`, and `merge`.
   - Preserve list/string sequence result rules.
   - Preserve predicate sandbox behavior.

5. Convert stack/control combinators that execute ordinary bodies.
   - `dip`, `keep`, `bi`, `app2`, `times`, `replicate`, `try`, and `infra`.
   - Keep `_r` suffixes until each native no longer calls synchronous
     `exec()`/callable execution directly.

6. Convert predicate and recursion combinators.
   - Update `if`, `ifelse`, `while`, `linrec`, `binrec`, `genrec`, `treerec`,
     `cond`, `cleave`, and `construct`.
   - Pay special attention to synthetic continuations that currently insert
     `exec` symbols; those can often continue to work, but the constructed
     program should not accidentally quote or resolve symbols in the wrong
     phase.

7. Update introspection and metadata.
   - Implement the chosen `typeof`/predicate/name-resolution changes.
   - Remove or rename misleading words rather than preserving them with
     confusing semantics.

8. Update documentation and tooling metadata.
   - README stack effects and examples.
   - `docs/combinators.md`.
   - REPL hints in `src/tf_repl.c`.
   - LSP builtins in `tools/toyforth-lsp/internal/analysis/builtins.go`.
   - Syntax docs only if syntax changes, which this plan does not require.

9. Run verification.
   - `cmake --build build`
   - Relevant `toy/` regression scripts.
   - `build-leak` for ownership, stack-effect, and execution-flow changes.

## Regression Test Matrix

Add tests that pair `[ word ]` with `'word` for every converted word.

Basic execution:

```toy
"abc" 'upper exec
"abc" [ upper ] exec
```

Sequence combinators:

```toy
"abc" 'upper map
[ 1 2 3 ] 'succ map
[ 1 2 3 ] 'number? all
[ 1 "x" 3 ] 'number? filter
0 [ 1 2 3 ] '+ fold
```

Stack combinators:

```toy
5 'square keep
5 'succ 'square bi
1 2 10 '+ dip
```

Overloads:

```toy
"a,b,c" "," split
"abc" 'str? split
"abc" [ "b" == ] split
```

User words:

```toy
'twice [ 2 * ] def
[ 1 2 3 ] 'twice map
```

Predicate sandboxing:

```toy
5 'number? [ "yes" ] if
[ 1 2 3 ] 'number? all
```

## Non-Goals

- Do not redesign syntax and callable dispatch in the same implementation step
  unless the syntax decision is already settled.
- Do not convert strings into executable word names unless the language model
  explicitly chooses strings as names. Symbols are the preferred candidate for
  word names.
- Do not make every list context executable.
- Do not change list/string sequence semantics accidentally. If they change,
  handle that as an explicit sequence-design decision.
- Do not remove `_r` suffixes merely because a word accepts symbols; `_r` is
  about synchronous execution and C call stack behavior, not about accepted
  argument types.

## Success Criteria

The refactor is complete when:

- Every word documented as consuming a callable accepts both `'word` and
  `[ word ]` where their stack effects are equivalent.
- Words that consume structural lists still reject atomic symbols in the
  structural position.
- Sequence overloads keep their existing string/list behavior.
- Error messages and REPL hints use "callable" consistently for executable
  arguments.
- Public introspection words either match the new model or are renamed/removed.
  Any backwards-incompatible changes are documented in README and regression
  tests.
