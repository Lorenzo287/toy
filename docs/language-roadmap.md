# Toy Forth Language Roadmap

## Summary

Toy Forth should evolve toward a quotation-first model.

The intended semantics are:

- bare symbol: resolve and invoke
- quoted symbol: symbol value as data
- list / quotation: first-class executable value
- `def`: bind a symbol to a quotation
- `exec`: apply a quotation or symbol

This keeps the current stack-based feel while making the existing "code as data"
facilities the primary conceptual model.

## Architectural Direction

### Named Quotations

User-defined words should be treated as named quotations rather than as a
separate compile-mode artifact.

```forth
'square [ dup * ] def
```

The quotation `[ dup * ]` is the function value. The symbol `square` is the
binding used to retrieve it later.

This is the preferred long-term mental model for:

- documentation
- examples
- new core features
- future library design

### Compatibility Syntax

The old definition form should remain available:

```forth
: square dup * ;
```

But it should be treated as compatibility and ergonomic sugar over the same
underlying model. Toy Forth should not grow a larger Forth-style compile mode
unless a later feature explicitly requires staged behavior.

### Non-Goals

This roadmap does not aim to:

- turn Toy Forth into a Lisp with macros and reader-level quoting forms
- expand compile-time semantics around `:` and `;`
- prioritize new syntax over stronger quotation/list semantics

## Execution Plan

### Phase 1: Semantic Cleanup

Clarify the language model in code-facing docs and examples.

- Prefer `'name [ ... ] def` in documentation and new examples.
- Describe `:` `;` as sugar, not as the primary definition mechanism.
- Keep the current behavior of `exec`, but choose and document the canonical
  apply word if aliases are added later.
- Keep native words and user-defined words under one dictionary model.

Acceptance criteria:

- core docs describe quotations as first-class code values
- examples consistently present quotation-based definition first
- compatibility wording around `:` `;` is explicit

### Phase 2: Foundational Aggregate and Quotation Primitives

Add the minimum vocabulary needed for functional, concatenative composition.

Control-flow semantics now in place:

- `if` and `ifelse` accept either a direct boolean or a quoted predicate
- direct booleans are consumed as condition values
- quoted predicates run against a temporary view of the current data stack and
  must leave a boolean result
- the inspected stack seen by quoted predicates is preserved for the selected
  branch, so predicate stack effects do not leak into branch execution
- `while` remains quotation-only and reevaluates its predicate this same way on
  each iteration

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
- one small branching/application family such as `bi`

Design constraints:

- preserve iterative execution behavior for user quotations
- preserve current frame-local variable capture semantics
- keep stack effects explicit and testable

Acceptance criteria:

- basic quotation-manipulating examples do not require syntax changes
- list construction and destructuring cover simple higher-order use cases
- tests document stack behavior and failure modes
- control-flow docs and examples reflect non-destructive quoted predicates

### Phase 3: Explicit Higher-Order Style

Use the new primitive vocabulary to validate the model before adding advanced
recursion combinators.

- Add examples that pass quotations explicitly.
- Prefer direct recursive named quotations before abstract recursion schemes.
- Add small standard-library style demonstrations only after the primitive set
  feels coherent.

Acceptance criteria:

- at least one nontrivial example is readable using named and anonymous
  quotations
- the missing primitive set is small and concrete

### Phase 4: Recursion Combinators

Only after the primitive algebra is stable, evaluate recursion combinators.

- start with `linrec`
- add `binrec` only if motivating examples remain awkward without it
- treat recursion combinators as semantic library features, not syntax features

Acceptance criteria:

- there are multiple concrete motivating examples
- the combinators can be specified with clear stack effects
- they fit the established quotation-first model without special parser rules

## Implementation Guidelines

Future implementation work should follow these priorities:

1. semantics first
2. primitive vocabulary second
3. examples third
4. syntax last

When a design choice is ambiguous, prefer:

- quotation-first semantics over compile-mode expansion
- reusable combinators over one-off special forms
- explicit stack effects over hidden evaluator behavior
- compatibility shims over breaking syntax removal

## Test Expectations

Any implementation turn derived from this roadmap should add or update tests
for:

- quoted symbols staying inert until explicitly applied
- `def` binding symbols to quotations correctly
- `exec` applying both quotations and symbols consistently
- `:` `;` remaining behaviorally equivalent to quotation-based definition
- control-flow words documenting and testing that quoted predicates preserve
  inspected values while direct booleans remain consumed
- list primitive stack effects, ownership, and error cases
- combinator behavior under nested quotation execution
- unchanged REPL persistence and frame-local variable semantics
