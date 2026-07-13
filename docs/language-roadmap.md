# Toy Language Roadmap

This roadmap records Toy's current development status and larger active or
future tracks. It is not a language reference, an agent manual, or a changelog.

## Status Labels

- **Completed**: stable enough to serve as a foundation for later work.
- **In progress**: an active or ongoing development track.
- **Future**: an accepted direction that has not started or is intentionally
  deferred.

## Completed Foundations

### Language and Execution Model

**Status: Completed**

The current language and execution model is the foundation for later work. Its
behavior belongs in the [README](../README.md), [combinator
reference](./combinators.md), and [runtime internals](./runtime-internals.md),
not in this roadmap.

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

### Validation and Formatting Foundation

**Status: Completed**

Portable CTest cases distinguish value regressions, expected failures, exact
output contracts, and manual host interaction. User examples live separately.
The shared Go formatter provides a CLI and standard LSP document formatting
while preserving author-selected line layout.

### Debugging Foundation

**Status: Completed**

Unhandled failures report source location, a bounded data-stack snapshot, and
a bounded Toy call chain. The terminal debugger provides instruction stepping,
while the DAP adapter provides file launch, source breakpoints, step
in/over/out, stack and frame inspection, and output forwarding for compatible
editors. The REPL remains the primary interactive development environment;
further DAP or graphical-interface work is deferred until a concrete workflow
requires it.

## Work in Progress

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
