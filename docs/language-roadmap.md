# Toy Language Roadmap

Toy is evolving from a Forth-like interpreter into a quotation-first
concatenative language inspired by Joy. Quotations (`[ ... ]`) and symbols
(`'name`) are the main units of definition, composition, and execution.

## Baseline

- Definitions bind quoted symbols to quotations with `'name [ ... ] def`.
- Vector quotations are first-class values; `exec`/`i` apply vectors or quoted
  symbols. Linked lists are data sequences and are not callable.
- User-defined words and native callable runners use the explicit frame stack.
- Native continuations represent words that need to resume after user code,
  including predicate sandboxes and error boundaries.
- Native word source of truth: grouped native tables in `src/tf_exec.c`.

## Roadmap Tracks

These tracks are not a strict priority order. Prefer work that clarifies
semantics, improves tests/examples, and keeps the interpreter educational.

### Callable Consistency

Introduce an explicit callable abstraction so higher-order words accept both
atomic quoted symbols (`'word`) and compound quotations (`[ ... ]`) where they
consume deferred code. Keep symbols, vectors, and lists distinct as data types,
and keep sequence dispatch for vectors/lists/strings separate from callable dispatch unless a
cleaner language model replaces those distinctions. Backwards compatibility is
secondary to semantic clarity while the language is still being designed. See
[Callable Refactor Plan](./callables-refactor.md).

### Vocabulary and Resilience

Continue auditing Joy builtins and add only words with clear stack effects and
real value. The first vocabulary expansion now covers numeric constants,
numeric predicates, dictionary introspection, higher-order collection words,
error handling experiments, and external interop (`argv`, `env?`, `getenv`,
`setenv`, `shell`). Future vocabulary work should emphasize semantic cleanup, edge-case
tests, and consistency over breadth.

Prefer Toy definitions for convenience words. Prefer C natives for direct object
access, frame scheduling, platform I/O, or measured performance.

### Explicit Execution Boundary

Completed: native words no longer synchronously re-enter `tf_vm_exec()` to wait for
callable results. Keep new callable-running natives on continuation-style frame
scheduling so nested user code does not grow the C call stack. Preserve stack
effects, predicate-inspection behavior, and error-boundary semantics.

### Data Model and Collections

Rethink collections around boxed `tf_obj` values, capabilities, and explicit
conversion between data structures. Keep `[ ... ]` as the ordered
vector/quotation form, use `( ... )` for linked lists, `{ ... }` for maps,
`#{ ... }` for sets, and explicit constructor words such as `>map`, `>set`,
`>deque`, and `>pqueue` when converting runtime data or building secondary
structures. See
[Data Model Plan](./data-model.md).

### Debugger

Use the frame stack to expose stepping, current word/program counter, data
stack, and call stack. Support REPL usage first; editor integration can come
later.

### Formatter

Build on Tree-sitter. Define deterministic, idempotent formatting for
quotations, captures, comments, and long pipelines. Start
with fixtures before editor integration.

### Performance Lab

The reproducible benchmark suite lives in [`benchmarks/`](../benchmarks/README.md).
Keep benchmarks separate from regression tests, compare optimized builds in the
same environment, change one technique at a time, and record the commit,
compiler, configuration, and machine with results. Initial coverage includes
dispatch, vector growth/index/endpoint/concatenation paths, and persistent-list
front/traversal/concatenation paths. Sequence-algorithm workloads cover sort
crossovers and duplicate-density effects in `unique`. String workloads cover
inline storage, byte extraction and traversal, transforms, splitting, and
incremental flat-string growth.

Future topics: dictionary lookup, allocation, list growth, hot-path
specialization, bytecode, threaded code, and cache behavior.

### Compiler / LLVM

Treat compilation as a later learning track. Start with an IR for quotations,
then bytecode for the existing VM, then LLVM for a constrained subset.

## Design Rules

- Semantics before syntax.
- Prefer reusable combinators over special forms.
- Captures use `| name ... |`; vectors use `[ ... ]`, lists use `( ... )`, maps
  use `{ ... }`, and sets use `#{ ... }`.
- Keep stack effects explicit and testable.
- Overload existing words when the language concept is the same across types
  (`split` for sequence partitioning and string splitting); avoid aliases that
  only encode implementation categories.
- Shared words use representation-aware implementations. Accept intrinsic
  complexity differences such as list versus vector `uncons`, but require
  one-pass sequence operations to remain O(n) for every finite sequence.
- Capability names may carry complexity contracts: indexed access and
  persistent-list front operations are O(1). Update capabilities must document
  both their unique-update cost and the copying required when a value is shared.
- Treat strings as byte sequences for shared sequence words. A Toy character is
  a one-byte string with an unsigned code from 0 through 255; `push-back` adds
  one character, while `concat` combines two sequences. String `contains?` and
  `indexof` search substrings because their operand is itself a string.
- Introspection words should push data rather than print directly. Word names
  are symbols: `words` and `apropos` push vectors of symbols, while `see` and
  `doc` push strings.
- Callable equivalence applies only where a word consumes deferred code.
  Name/introspection words consume symbols as names, not single-symbol
  quotations.
- Absence should be represented explicitly with a predicate word or a runtime
  error, not by returning an unrelated sentinel value such as `[]`.
- Ordinary words consume their declared stack inputs. Use `dup`, `keep`, `bi`,
  and related stack combinators when a caller wants preservation.
- Endpoint destructors return reconstruction inputs in constructor order:
  `pop-back` leaves `collection item` for `push-back`, while `uncons` leaves
  `item sequence` for `cons`.
- Predicate quotations used by control and predicate combinators observe the
  surrounding data stack by sandboxing stack changes and reading one boolean
  result. Side effects inside predicates are still real effects.
- Diagnostic display words such as `.`, `.s`, and `.S` may observe the stack
  without consuming values.
- `print` emits one literal value and a newline. Formatting is explicit through
  `printf`; source-style conversion is data-producing through `repr`.
- Update-style data words such as `set-at` should return updated values rather
  than mutating shared objects in place.
- New native words need focused tests and lightweight tooling metadata updates.
