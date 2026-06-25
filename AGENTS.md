# Toy: Agent Manual

Toy is a small concatenative language/runtime in C. It has a stack-based
execution model, first-class quotations and symbols, refcounted collection
objects, dynamic captures (`| a b |` / `$a`), a generated builtin registry, a
global word dictionary, and an iterative VM frame stack for user words.

Roadmap work lives in `docs/language-roadmap.md`. Keep this file focused on
navigation and development rules.

## Project Map

- `builtins.json`: canonical builtin metadata used to generate registry, docs,
  runtime help, LSP data, Tree-sitter word lists, and VS Code grammar data.
- `src/`: interpreter implementation.
- `src/tf_builtins.inc`, `src/tf_docs.c`, `src/tf_repl_builtins.inc`:
  generated builtin/runtime-doc files; do not hand-edit.
- `include/`: internal APIs; read these before engine, lexer, object, or native edits.
- `toy/`: Toy scripts and regression tests.
- `docs/`: build, REPL, tooling, and roadmap docs.
- `docs/combinators.md`: examples for nontrivial control, recursion, and
  collection combinator usage.
- `docs/data-model.md`: collection syntax, interop, complexity, equality, and
  hashing reference.
- `docs/runtime-internals.md`: VM/object/allocation implementation notes.
- `benchmarks/`: reproducible performance workloads, runner, and recorded
  experiment results.
- `benchmarks/results/`: benchmark result notes and comparison templates.
- `tools/generate-builtins.js`: builtin metadata generator and consistency
  checker.
- `tools/tree-sitter-toyforth/`: Tree-sitter grammar, generated parser inputs,
  queries, and tests.
- `tools/toyforth-lsp/`: Go LSP implementation and generated builtin docs.
- `tools/vscode-toyforth/`: VS Code extension and generated grammar metadata.
- `.github/workflows/`: release automation.
- `deps/`: vendored `linenoise` and `stb_leakcheck`.

## Fast Context

- Builtin source of truth: `builtins.json`; run
  `node tools/generate-builtins.js` after edits and use `--check` in validation.
- Native word registry: generated grouped tables in `src/tf_builtins.inc`,
  included by `src/tf_exec.c` and registered by `tf_ctx_new()`.
- Native declarations: `include/tf_lib.h`.
- Execution engine: `include/tf_exec.h`, `src/tf_exec.c`.
- Objects/ownership: `include/tf_obj.h`, `src/tf_obj.c`, `include/tf_alloc.h`.
- Lexer: `include/tf_lexer.h`, `src/tf_lexer.c`.
- REPL: `include/tf_repl.h`, `src/tf_repl.c`.
- Language plan: `docs/language-roadmap.md`.
- Data model reference: `docs/data-model.md`.

## Workflow

- Start with `git status --short`; do not overwrite user changes.
- For language behavior, check `README.md`, then the relevant C files and tests.
- For roadmap work, read `docs/language-roadmap.md` first. For collection or
  data-structure work, read `docs/data-model.md` too.
- For native word changes, update `builtins.json`, declarations, and focused
  `toy/` tests, then regenerate and commit all generated metadata.
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
- Tooling: builtin metadata is generated from `builtins.json`; do not hand-edit
  generated registry, runtime-doc, LSP, Tree-sitter word-list, VS Code grammar,
  or README table outputs. Regenerate the Tree-sitter parser after generated
  word-list changes when the CLI is available.
- Docs: README is public-facing; AGENTS is for agent rules; roadmap is the
  implementation plan.
- Shell: assume Windows PowerShell; do not output bash syntax.
