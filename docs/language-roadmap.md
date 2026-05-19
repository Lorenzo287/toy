# Toy Language Roadmap

## Vision: Quotation-First Programming
Toy is evolving from a traditional Forth clone into a quotation-first concatenative language. The goal is to make code blocks (`[ ... ]`) and symbols (`'sym`) the primary units of definition and composition, in the spirit of Joy, the concatenative language designed by Manfred von Thun.

### Core Model
- **Bare Symbol**: Resolve in dictionary and invoke.
- **Quoted Symbol**: Push symbol value as data (inert).
- **List / Quotation**: First-class executable value.
- **`def`**: Bind a symbol to a quotation (the preferred definition style).
- **`exec` / `i`**: Apply a quotation or symbol.

### Non-Goals
- **Lisp-style macros** or reader-level quoting.
- **Expanded compile-time semantics** around `:` and `;`.
- **New syntax** before the quotation/list model is mature enough to support it.

---

## Project Status

### 1. Completed Foundations
- **Iterative Engine**: Frame-based execution prevents C stack overflows for user words.
- **Native Object System**: Refcounted Integers, Floats, Strings, Lists, Symbols, and Booleans.
- **Core Primitives**: 
    - **Stack**: `dup`, `drop`, `swap`, `over`, `rot`, `swapd`, `nip`, `tuck`, `pick`, `roll`, `empty`.
    - **Math**: arithmetic plus small numeric helpers such as `succ` and `pred`.
    - **List Algebra**: `first`, `rest`, `uncons`, `cons`, `append`, `concat`, `range`, `empty?`, `geth`, `seth`, `len`.
    - **Combinators**: `dip`, `keep`, `bi`, `split`, `map`, `fold`, `linrec`, `binrec`; `each` remains the stack-producing iterator.
- **Tooling Baseline**: Tree-sitter, LSP, and VS Code support exist for native words. Keep metadata in sync when adding words, but default implementation work remains C/Forth-first.

### 2. Current Expressiveness Check
The quotation/list algebra is now expressive enough to write a compact quicksort through `binrec`:
```toy
'qsort [
    [ len nip 2 < ]
    []
    [ uncons [ > ] split ]
    [ swapd cons concat ]
    binrec
] def
```

### 3. Active Development (Phase 1 & 3)
- **Documentation Migration**: README examples now prioritize `'name [ ... ] def`. Continue migrating older docs and tests away from colon-first style when touching them.
- **Legacy Sugar**: Treat `:` and `;` as compatibility syntax for definitions, not the primary language model.
- **Standard Library Shape**: Use `load` to factor reusable Toy words into `toy/std/` modules as examples mature.

### 4. Future Goals (Phase 4)
- **Standard-Library Factoring**: Move common examples and utilities from native C or tests into reusable Toy quotations.
- **Integrated Debugging**: Step-by-step execution and stack visualization.

---

## Design Principles

### Control Flow & Inspection
- **Predicate Inspection**: `if`, `ifelse`, and `while` should support quoted predicates that inspect the stack without permanently consuming values (using temporary stack views).
- **Aggregate Preservation**: 
    - Observational words (`len`, `geth`, `first`, `rest`, `empty?`) should preserve the inspected list.
    - Structural words (`uncons`, `cons`, `concat`) should consume their list inputs.
    - Update words (`seth`) may remain consuming/mutating.

### Implementation Guidelines
1. **Semantics First**: Focus on how words behave and compose before adding new syntax.
2. **Quotation-First**: Prefer reusable combinators over specialized syntax or compiler modes.
3. **Explicit Stack Effects**: Avoid "hidden" evaluator state; ensure stack changes are predictable and testable.
4. **Tooling Parity**: Every new native word should have obvious metadata updates for LSP, Tree-sitter, and editor support, but full tooling test runs are optional unless the task is tooling-focused.

---

## Test Expectations
- **Inertness**: Quoted symbols must stay inert until `exec` or `i`.
- **Equivalence**: `: name ... ;` must remain behaviorally identical to `'name [ ... ] def`.
- **Persistence**: REPL state and frame-local variables must persist correctly across execution boundaries.
- **Verification Focus**: Prefer C builds, targeted `toy/` scripts, and leak checks for interpreter changes. Tooling verification can be run manually or when explicitly requested.
