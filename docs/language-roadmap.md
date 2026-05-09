# Toy Forth Language Roadmap

## Vision: Quotation-First Programming
Toy Forth is evolving from a traditional Forth clone into a quotation-first concatenative language. The goal is to make code blocks (`[ ... ]`) and symbols (`'sym`) the primary units of definition and composition, similar to Joy or Factor.

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
    - **Stack**: `dup`, `drop`, `swap`, `over`, `rot`, `nip`, `tuck`, `pick`, `roll`, `empty`.
    - **List Algebra**: `first`, `rest`, `uncons`, `cons`, `concat`, `empty?`, `geth`, `seth`, `len`.
    - **Combinators**: `dip`, `keep`.
- **Tooling Baseline**: Synchronized Tree-sitter, LSP, and VS Code support for all native words.

### 2. End-state Check (Technical Benchmark)
A useful end-state check for whether the quotation/list algebra is expressive enough is the ability to write readable recursion schemes:
```forth
[small] [] [uncons [>] split] [swapd cons concat] binrec
```

### 3. Active Development (Phase 1 & 3)
- **Documentation Migration**: (Active) Updating docs and examples to prioritize `'name [ ... ] def` over `: name ... ;`.
- **Legacy Sugar**: Formally treating `:` and `;` as simple syntactic sugar for `def`, with no unique compile-time semantics.
- **Higher-Order Primitives**: Adding `bi`, `split`, and other "clever" combinators to reduce stack shuffling.

### 4. Future Goals (Phase 4)
- **Recursion Schemes**: Implement `linrec` (linear recursion) and `binrec` (binary recursion).
- **Standard Library**: Porting common utilities from C to Toy Forth quotations.
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
4. **Tooling Parity**: Every new native word must be supported by the LSP and Tree-sitter.

---

## Test Expectations
- **Inertness**: Quoted symbols must stay inert until `exec` or `i`.
- **Equivalence**: `: name ... ;` must remain behaviorally identical to `'name [ ... ] def`.
- **Persistence**: REPL state and frame-local variables must persist correctly across execution boundaries.
