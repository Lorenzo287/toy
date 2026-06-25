# Toy Language Roadmap

Toy is now a quotation-first concatenative language with a small C runtime,
first-class callable values, refcounted collections, and generated builtin
metadata. This roadmap records what is settled, what is active, and what is
intentionally future work.

## Current Baseline

Implemented and treated as the current language model:

- Definitions bind quoted symbols to quotations with `'name [ ... ] def`.
- Callable values are vector quotations (`[ ... ]`) and symbols naming words.
  Lists are data sequences, not callables.
- Higher-order words accept quoted symbols and vector quotations where they
  consume deferred code.
- User-defined words and native callable runners use the explicit VM frame
  stack; new native words should not synchronously re-enter `tf_vm_exec()`.
- Predicate callables used by control and predicate combinators observe the
  surrounding stack through sandboxing, then leave one boolean result.
- Collections are boxed `tf_obj` values with explicit representation types:
  vector, list, map, set, deque, priority queue, string, symbol, bool, int, and
  float.
- Builtin metadata is canonical in `builtins.json` and generates native
  registry tables, runtime docs, README tables, LSP docs, Tree-sitter word
  lists, and VS Code grammar metadata.

## Completed Tracks

### Callable Consistency

Complete. The old callable-refactor plan has been removed.

- `exec`/`i` and higher-order words use the callable abstraction.
- Quoted symbols work as atomic callables.
- Vector quotations work as compound callables.
- Name/introspection words still consume symbols as names, not single-symbol
  quotations.
- Regression coverage lives in `toy/test_callables.toy` and related
  combinator/stack-effect tests.

### Explicit Execution Boundary

Complete. Callable-running native words schedule program frames or native
continuations and return to the VM loop. This keeps nested user code off the C
call stack and preserves error-boundary and predicate-sandbox semantics.

### Collection/Data Model Round

The main collection audit is complete for vectors, lists, strings, maps, sets,
deques, and priority queues. The public reference is
[`docs/data-model.md`](./data-model.md).

Settled points:

- vectors are indexed arrays and the quotation representation;
- lists are persistent front-oriented sequences;
- strings are byte sequences with one-byte string characters;
- maps and sets are insertion-ordered hash tables over hashable scalar keys;
- deques expose efficient front/back endpoints;
- priority queues expose stable minimum-priority access;
- update-style data words return updated values and may optimize with
  copy-on-write.

### Vocabulary Cleanup

The current vocabulary favors explicit names, hyphen-separated multiword names,
and capability-oriented sharing. Examples:

- `push-back`, `pop-back`, `push-front`, `pop-front`
- `first` / `last`
- `index-of`, `split-mid`, `type-of`
- `read-file`, `write-file`, `delete-file`
- `search-words`
- `unix-time`, `local-time`, `utc-time`, `cpu-time`, `monotonic-ns`

Continue adding words only when they have clear stack effects, tested behavior,
and a real use case.

### Performance Lab Foundation

Complete as infrastructure; ongoing as practice. Benchmarks live in
[`benchmarks/`](../benchmarks/README.md), with recorded experiments in
`benchmarks/results/`. Current coverage includes dispatch, vectors, lists,
strings, maps, sets, deques, priority queues, sequence algorithms, and runtime
internals. `BUILD_MODE=AllocationStats` gives reproducible allocation counts for
identical workloads.

## Active / Future Tracks

### Debugger

Use the explicit frame stack to expose stepping, current word/program counter,
data stack, and call stack. Start in the REPL before editor integration.

### Formatter

Build on Tree-sitter. Define deterministic, idempotent formatting for
quotations, captures, comments, and long pipelines. Start with fixtures before
editor integration.

### Performance Work

Keep performance changes benchmark-driven. Useful future topics:

- dictionary lookup and word dispatch;
- allocation and object lifetime hot spots;
- call-frame specialization;
- bytecode or threaded-code experiments;
- cache behavior for list/vector/string workloads;
- structural hashes if map/set key policy expands.

### Compiler / LLVM

Treat compilation as a later learning track. A plausible path is:

1. define an IR for quotations;
2. compile a constrained subset to bytecode for the current VM;
3. experiment with LLVM after the bytecode semantics are stable.

## Design Rules

- Semantics before syntax.
- Prefer reusable words and combinators over new syntax.
- Keep stack effects explicit and testable.
- Captures use `| name ... |`; vectors use `[ ... ]`, lists use `( ... )`,
  maps use `{ ... }`, and sets use `#{ ... }`.
- Integers are signed 64-bit values and floats use double precision. Mixed
  comparisons must preserve exact integer ordering where possible.
- Overload existing words when the language concept is the same across types;
  avoid aliases that only encode implementation categories.
- Shared words use representation-aware implementations. Accept intrinsic
  complexity differences, but keep one-pass sequence operations O(n) for every
  finite sequence.
- Capability names may carry complexity contracts: indexed access, persistent
  list-front operations, and priority-queue access should document their costs.
- Strings are byte sequences. A Toy character is a one-byte string with an
  unsigned code from 0 through 255.
- Introspection words should push data rather than print directly. Word names
  are symbols; `see` and `doc` push strings.
- Callable equivalence applies only where a word consumes deferred code.
- Absence should be explicit: use a predicate word, a caller-provided default,
  or a runtime error, not an unrelated sentinel value.
- Ordinary words consume their declared stack inputs. Use `dup`, `keep`, `bi`,
  and related combinators when a caller wants preservation.
- Endpoint destructors return reconstruction inputs in constructor order:
  `pop-back` leaves `collection item` for `push-back`, while `uncons` leaves
  `item sequence` for `cons`.
- Predicate callables observe the stack through sandboxing; side effects inside
  predicates remain real effects.
- Diagnostic display words such as `.`, `.s`, and `.S` may observe the stack
  without consuming values.
- `print` emits one literal value and a newline. Formatting is explicit through
  `printf`; source-style conversion is data-producing through `repr`.
- Update-style data words such as `set-at`, `assoc`, `insert`, and `push-back`
  return updated values rather than exposing shared in-place mutation.
- New native words need focused tests and generated metadata updates through
  `builtins.json`.
