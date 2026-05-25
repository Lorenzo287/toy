# Toy Language Roadmap

Toy is evolving from a Forth-like interpreter into a quotation-first
concatenative language inspired by Joy. Quotations (`[ ... ]`) and symbols
(`'name`) are the main units of definition, composition, and execution.

## Baseline

- Preferred definition style: `'name [ ... ] def`; `: name ... ;` remains
  supported for existing Forth-style code.
- Quotations/lists are first-class values; `exec`/`i` apply them.
- User-defined words run on the explicit frame stack.
- Some native quotation runners still call `exec()` synchronously and keep the
  `_r` suffix.
- Native word source of truth: grouped native tables in `src/tf_exec.c`.

## Roadmap Tracks

These tracks are not a strict priority order. Prefer work that clarifies
semantics, improves tests/examples, and keeps the interpreter educational.

### Callable Consistency

Introduce an explicit callable abstraction so higher-order words accept both
atomic quoted symbols (`'word`) and compound quotations (`[ ... ]`) where they
consume deferred code. Keep symbols and lists distinct as data types, and keep
sequence dispatch for lists/strings separate from callable dispatch unless a
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

Convert `_r` native words to continuation-style frame scheduling so nested user
code no longer grows the C call stack. Preserve existing stack effects and
predicate-inspection behavior. Add regression scripts and run leak checks for
each conversion.

### High-Level Data Structures

Add maps/dictionaries as first-class values, then consider JSON support. Decide
early whether maps are mutable or persistent. Start with a small API: create,
get, set, delete, keys, values, predicates.

### Debugger

Use the frame stack to expose stepping, current word/program counter, data
stack, and call stack. Support REPL usage first; editor integration can come
later.

### Formatter

Build on Tree-sitter. Define deterministic, idempotent formatting for
quotations, captures, colon definitions, comments, and long pipelines. Start
with fixtures before editor integration.

### Performance Lab

Profile first, change one technique at a time, and record results. Topics:
dispatch, dictionary lookup, allocation, stack/list growth, hot-path
specialization, bytecode, threaded code, cache behavior.

### Compiler / LLVM

Treat compilation as a later learning track. Start with an IR for quotations,
then bytecode for the existing VM, then LLVM for a constrained subset.

## Design Rules

- Semantics before syntax.
- Prefer reusable combinators over special forms.
- Keep stack effects explicit and testable.
- Overload existing words when the language concept is the same across types
  (`split` for list partitioning and string splitting); avoid aliases that only
  encode implementation categories.
- Treat strings as byte sequences for shared sequence words. A string item is a
  one-byte string; `append` adds one item, while `concat` combines two
  sequences.
- Introspection words should push data rather than print directly. Word names
  are symbols: `words` pushes a list of symbols, while `see` pushes source text
  as a string.
- Absence should be represented explicitly with a predicate word or a runtime
  error, not by returning an unrelated sentinel value such as `[]`.
- Ordinary words consume their declared stack inputs. Use `dup`, `keep`, `bi`,
  and related stack combinators when a caller wants preservation.
- Predicate quotations used by control and predicate combinators observe the
  surrounding data stack by sandboxing stack changes and reading one boolean
  result. Side effects inside predicates are still real effects.
- Diagnostic display words such as `.`, `.s`, and `.S` may observe the stack
  without consuming values.
- Update-style data words such as `seth` should return updated values rather
  than mutating shared objects in place.
- New native words need focused tests and lightweight tooling metadata updates.
