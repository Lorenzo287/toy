# Toy Forth Language Roadmap

## Summary

Toy Forth should continue moving toward a quotation-first model:

- bare symbol: resolve and invoke
- quoted symbol: symbol value as data
- list / quotation: first-class executable value
- `def`: bind a symbol to a quotation
- `exec`: apply a quotation or symbol

Preferred definition style:

```forth
'square [ dup * ] def
```

Compatibility syntax stays available:

```forth
: square dup * ;
```

But `:` `;` should remain sugar over the same underlying model, not grow into a
larger compile mode.

Non-goals:

- Lisp-style macros or reader-level quoting
- expanded compile-time semantics around `:` and `;`
- adding syntax before the quotation/list model is strong enough

## Current Direction

### Control Flow

Current control-flow semantics:

- `if` and `ifelse` accept either a direct boolean or a quoted predicate
- direct booleans are consumed as condition values
- quoted predicates inspect a temporary view of the current data stack and must
  leave a boolean
- quoted predicate stack effects do not leak into the selected branch
- `while` remains quotation-only and reevaluates its predicate this same way on
  each iteration

Current aggregate-observer direction:

- observational words should preserve inspected aggregates when practical
- `len` preserves the list and pushes its length
- `geth` preserves the list, consumes the index, and pushes the selected value
- update words such as `seth` may remain consuming/mutating

### Near-Term Primitive Vocabulary

Target style to eventually support:

```forth
[small] [] [uncons [>] split] [swapd cons concat] binrec
```

This is not a near-term implementation target by itself, but it is a useful
end-state check for whether the quotation/list algebra is expressive enough.

Target list words:

- `first`
- `rest`
- `uncons`
- `cons`
- `concat`
- one emptiness predicate such as `empty?`

Target quotation combinators:

- `dip`
- `keep`
- one small application/branching family such as `bi`

Likely prerequisites for `linrec`/`binrec`-style programs:

- `uncons`
- `cons`
- `concat`
- `split`
- one simple size/emptiness predicate family such as `small` or `empty?`
- deeper stack combinators such as `swapd`
- enough quotation combinators that recursive branch bodies remain readable

Design constraints:

- preserve iterative execution for user quotations
- preserve current frame-local variable semantics
- keep stack effects explicit and testable

## Planned Progression

### Phase 1: Semantic Cleanup

- Prefer `'name [ ... ] def` in docs and examples
- Describe `:` `;` as compatibility sugar
- Keep `exec` behavior stable
- Keep native and user-defined words under one dictionary model
- Keep builtin vocabulary, docs, and tooling metadata in sync when words are
  added or repurposed

### Phase 2: Foundational Primitives

- Add the minimum aggregate and quotation vocabulary needed for higher-order
  composition
- Use tests and examples to validate the control-flow and stack-effect model

### Phase 3: Explicit Higher-Order Style

- Add examples that pass quotations explicitly
- Prefer direct named recursive quotations before abstract recursion schemes
- Add standard-library-style demonstrations only after the primitive set feels
  coherent

### Phase 4: Recursion Combinators

- Start with `linrec`
- Add `binrec` only if real examples still justify it
- Treat recursion combinators as semantic library features, not syntax features

## Implementation Guidelines

Priorities:

1. semantics first
2. primitive vocabulary second
3. examples third
4. syntax last

When a design choice is ambiguous, prefer:

- quotation-first semantics over compile-mode expansion
- reusable combinators over one-off special forms
- explicit stack effects over hidden evaluator behavior
- compatibility shims over breaking syntax removal
- updating docs and editor tooling metadata alongside builtin vocabulary changes

## Test Expectations

Implementation work derived from this roadmap should add or update tests for:

- quoted symbols staying inert until explicitly applied
- `def` binding symbols to quotations correctly
- `exec` applying both quotations and symbols consistently
- `:` `;` remaining behaviorally equivalent to quotation-based definition
- control-flow words preserving inspected values for quoted predicates while
  consuming direct booleans
- list primitive stack effects, ownership, and error cases
- combinator behavior under nested quotation execution
- unchanged REPL persistence and frame-local variable semantics
