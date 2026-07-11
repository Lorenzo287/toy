# Toy Language Roadmap

This roadmap records the current development priorities for Toy and the order
in which larger changes should be approached. It is not a language reference,
an agent manual, or a changelog:

## Status Labels

- **Completed**: stable enough to serve as a foundation for later work.
- **In progress**: an active or ongoing development track.
- **Future**: an accepted direction that has not started or is intentionally
  deferred.

## Completed Foundations

### Language and Execution Model

**Status: Completed**

Toy has a quotation-first language model with first-class callable quotations
and symbols, dynamic captures, higher-order combinators, predicate stack
sandboxing, and an iterative VM frame stack. Current behavior is documented in
the [README](../README.md), [combinator reference](./combinators.md), and
[runtime internals](./runtime-internals.md).

### Collection and Value Model

**Status: Completed**

The value model covers vectors, lists, strings, maps, sets, deques, and priority
queues with public value semantics and representation-aware implementations.
The current contracts and complexity expectations live in the
[data-model reference](./data-model.md).

### Tooling and Measurement Foundation

**Status: Completed**

Builtin metadata generates the runtime registry, help and documentation data,
LSP data, Tree-sitter word lists, and VS Code grammar data. The repository also
has focused regression scripts, a Tree-sitter grammar, a Go LSP, editor support,
benchmark workloads, and allocation-counting builds. These foundations may be
extended by the active tracks below.

## Work in Progress

### Validation and Release Confidence

**Status: In progress**

Create one repeatable validation path from the existing language and tooling
checks. Completion means:

- self-checking `toy/test_*.toy` scripts have a runner;
- interactive, environment-dependent, and output-only examples are separated
  from portable regressions;
- `tools/generate-builtins.js --check` is part of validation;
- LSP and Tree-sitter tests run when their toolchains are available;
- release builds depend on the portable correctness checks;
- leak-sensitive runtime changes have a documented validation path.

### Performance Work

**Status: In progress**

Keep optimization work benchmark-driven and record durable experiments under
`benchmarks/results/`. Current investigation candidates are:

- dictionary lookup and word dispatch;
- allocation and object-lifetime hot spots;
- call-frame specialization;
- cache behavior for list, vector, and string workloads;
- structural hashes if map/set key policy expands.

## Future Work

### Debugger

**Status: Future**

Use the explicit frame stack to expose stepping, the current word and program
counter, the data stack, and the call stack. Start in the REPL before adding
editor integration.

### Formatter

**Status: Future**

Build on Tree-sitter. Define deterministic, idempotent formatting for
quotations, captures, comments, and long pipelines. Start with fixtures before
adding editor integration.

### Runtime API and Foreign Function Interface

**Status: Future**

Treat the [foreign function interface](https://en.wikipedia.org/wiki/Foreign_function_interface)
as a staged runtime-boundary project, not a single general-purpose `call-c`
word. Toy has an internal native-word convention, but its context, values,
ownership rules, and registration functions are not a stable external API.

The proposed stages are:

1. separate the CLI from a reusable Toy runtime library;
2. expose opaque context and value handles with explicit ownership and error
   rules;
3. support host applications registering native words;
4. define a versioned native-module interface;
5. use [libffi](https://github.com/libffi/libffi) to experiment with explicit
   scalar and string signatures resolved at runtime;
6. consider foreign pointers, output parameters, structs, variadic functions,
   and callbacks only after the basic ownership and VM-boundary rules are
   settled.

The resulting boundary should be reusable by embedders, native modules, and
compiled Toy code. Native calls that schedule Toy code must preserve the
iterative VM execution model.

### Compiler Backends

**Status: Future**

Treat compilation as a learning track with explicit semantic checkpoints:

1. define an IR for quotations, calls, stack effects, captures, and source
   spans;
2. compile a constrained subset to bytecode for the current VM, retaining an
   interpreter fallback for dynamic behavior;
3. define the runtime ABI used by compiled code for values, errors, dictionary
   lookup, and native calls;
4. experiment with the compact [QBE compiler backend](https://c9x.me/compile/)
   after bytecode semantics are stable;
5. consider LLVM if Toy later needs broader target support or a more ambitious
   optimization pipeline.

Compilation must preserve runtime `def`, first-class quotations and symbols,
dynamic captures, predicate stack sandboxing, error behavior, and value
ownership. Early native compilation should target an explicit subset rather
than silently changing those semantics.

### Design Comparisons

**Status: Future**

Use other small stack languages as focused experiments, not as specifications
Toy must copy.

- [Aocla](https://github.com/antirez/aocla/) is a useful comparison for
  readable captures, code-as-data, copy-on-write values, and an understandable
  C interpreter.
- [Porth](https://gitlab.com/tsoding/porth) is a useful comparison for direct
  native lowering, compile-time stack reasoning, system interfaces, examples,
  and test workflow.

Port representative small programs and record what each comparison teaches
about Toy's semantics, implementation size, compiler boundary, or vocabulary.
