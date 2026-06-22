# Toy: Agent Manual

Toy is a minimalist stack-based interpreter in C. It has refcounted dynamic
objects, first-class quotations/lists, dynamic captures (`| a b |` / `$a`), a
global word dictionary, and an iterative frame stack for user words.

Roadmap work lives in `docs/language-roadmap.md`. Keep this file focused on
navigation and development rules.

## Project Map

- `src/`: interpreter implementation.
- `include/`: internal APIs; read these before engine, lexer, object, or native edits.
- `toy/`: Toy scripts and regression tests.
- `docs/`: build, REPL, tooling, and roadmap docs.
- `docs/combinators.md`: examples for nontrivial control, recursion, and
  collection combinator usage.
- `docs/data-model.md`: collection, conversion, equality, and hashing design
  plan.
- `benchmarks/`: reproducible performance workloads, runner, and recorded
  experiment results.
- `tools/`: Tree-sitter grammar, Go LSP, VS Code extension.
- `deps/`: vendored `linenoise` and `stb_leakcheck`.

## Fast Context

- Native word registry: grouped native tables in `src/tf_exec.c`, registered by
  `tf_ctx_new()`.
- Native declarations: `include/tf_lib.h`.
- Execution engine: `include/tf_exec.h`, `src/tf_exec.c`.
- Objects/ownership: `include/tf_obj.h`, `src/tf_obj.c`, `include/tf_alloc.h`.
- Lexer: `include/tf_lexer.h`, `src/tf_lexer.c`.
- REPL: `include/tf_repl.h`, `src/tf_repl.c`.
- Language plan: `docs/language-roadmap.md`.
- Data model plan: `docs/data-model.md`.

## Workflow

- Start with `git status --short`; do not overwrite user changes.
- For language behavior, check `README.md`, then the relevant C files and tests.
- For roadmap work, read `docs/language-roadmap.md` first. For collection or
  data-structure work, read `docs/data-model.md` too.
- For native word changes, update `src/tf_exec.c`, declarations, focused `toy/`
  tests, and lightweight tooling metadata.
- Build with `cmake --build build`; run relevant scripts. Use `build-leak` for
  ownership, stack-effect, or execution-flow changes.

## Development Rules

- C style: snake_case, 4-space indentation. Use `tf_` for exported/project-wide
  symbols; file-local `static` helpers may use unprefixed module-local names.
- Memory: `tf_obj_retain` when storing references, `tf_obj_release` when done,
  use `tf_xmalloc` helpers.
- Native callable runners should schedule frames or native continuations and
  return to the VM loop. Do not add synchronous `tf_vm_exec()` re-entry for new
  native words.
- Language direction: prefer quotation-first words and combinators over new
  syntax. Follow `docs/language-roadmap.md` for definition policy.
- Stack effects: ordinary words consume their declared inputs. Predicate
  quotations in control/predicate combinators restore the surrounding data stack
  after reading a boolean result; side effects inside them are not undone.
- Sequence words should be uniform across lists and strings when the result type
  is clear. Strings are byte sequences; a string item is a one-byte string.
- Tooling: keep metadata obvious when words/syntax change. The four source files
  to keep in sync are `src/tf_repl.c` (hints),
  `tools/tree-sitter-toyforth/grammar.js`,
  `tools/vscode-toyforth/syntaxes/toyforth.tmLanguage.json`, and
  `tools/toyforth-lsp/internal/analysis/builtins.go`. Regenerate the
  Tree-sitter parser after grammar changes when the CLI is available.
- Docs: README is public-facing; AGENTS is for agent rules; roadmap is the
  implementation plan.
- Shell: assume Windows PowerShell; do not output bash syntax.
