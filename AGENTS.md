# Toy: Agent Manual

Toy is a small concatenative language/runtime in C. It has a stack-based
execution model, first-class quotations and symbols, explicit call nodes,
refcounted collection objects, dynamic captures (`| a b |` / `$a`), source
modules with `.` qualified exports, a generated builtin registry, a scoped
word dictionary, and an iterative VM frame stack for user words.

Roadmap work lives in `docs/language-roadmap.md`. Keep this file focused on
navigation and development rules.

## Project Map

- `builtins.json`: canonical builtin metadata used to generate registry, docs,
  runtime help, LSP data, Tree-sitter word lists, and VS Code grammar data.
- `src/`: interpreter implementation.
- `src/tf_builtins.inc`, `src/tf_docs.c`, `src/tf_repl_builtins.inc`:
  generated builtin/runtime-doc files; do not hand-edit.
- `include/`: internal APIs; read these before engine, lexer, object, or native edits.
- `examples/programs/`: standalone Toy programs; formatting-sensitive quines
  live under `examples/programs/quines/`.
- `examples/embedding/`: C hosts that embed and call the Toy runtime.
- `examples/interop/`: dynamic FFI, generated binding, and handwritten
  external-library examples. Library-specific adapters are test cases for the
  general boundary, not built-in integrations.
- `modules/`: optional general-purpose native modules maintained with Toy.
- `tests/toy/`, `tests/c/`: language cases and C API regressions. Toy test
  prefixes declare behavior: `test_`, `fail_`, `output_`, and `manual_`.
- `docs/`: build, REPL, tooling, and roadmap docs.
- `docs/combinators.md`: examples for nontrivial control, recursion, and
  collection combinator usage.
- `docs/data-model.md`: collection syntax, interop, complexity, equality, and
  hashing reference.
- `docs/runtime-internals.md`: VM/object/allocation implementation notes.
- `docs/embedding.md`: experimental C embedding and native-word API.
- `benchmarks/`: reproducible performance workloads, runner, and recorded
  experiment results.
- `benchmarks/results/`: benchmark result notes and comparison templates.
- `tools/generate-builtins.js`: builtin metadata generator and consistency
  checker.
- `tools/tree-sitter-toy/`: Tree-sitter grammar, generated parser inputs,
  queries, and tests.
- `tools/toy-lsp/`: Go LSP implementation and generated builtin docs.
- `tools/toy-lsp/internal/dap/`, `tools/toy-lsp/cmd/toy-dap/`: DAP adapter
  and executable entry point.
- `tools/toy-lsp/internal/formatter/`: shared formatter used by the CLI
  and LSP formatting method.
- `tools/vscode-toy/`: VS Code extension and generated grammar metadata.
- `.github/workflows/`: CI and release automation.
- `nob.c`: self-contained build entry point for the runtime, CLI, examples,
  modules, and tests; `tools/nob/build.h` contains compiler and build helpers,
  while `tools/nob/tests.h` contains the isolated test harness.
- `deps/`: vendored `linenoise`, `stb_leakcheck`, and `nob`.

## Fast Context

- Builtin source of truth: `builtins.json`; run
  `node tools/generate-builtins.js` after edits and use `--check` in validation.
- Native word registry: generated grouped tables in `src/tf_builtins.inc`,
  included by `src/tf_exec.c` and registered by `tf_ctx_new()`.
- Native declarations: `include/tf_lib.h`.
- Experimental public C and native-module API: `include/toy.h`, implemented by
  `src/toy.c`.
- Standalone shared native-module API and implementation:
  `include/toy_module.h`; platform loading: `src/tf_native_loader.c`.
- Experimental libffi module: `modules/ffi/toy_ffi.c`; signature and safety
  contract: `docs/ffi.md`.
- Explicit-manifest binding generator: `tools/generate-binding.js`; Nob
  integration: `nob bindgen`; contract: `docs/bindgen.md`.
- External shared-module examples: `examples/interop/raylib/toy_raylib.c` and
  `examples/interop/sqlite/toy_sqlite.c`.
- Execution engine: `include/tf_exec.h`, `src/tf_exec.c`.
- Module registry and scoped lookup: `include/tf_exec.h`, `src/tf_exec.c`;
  module loading and path resolution: `src/tf_lib_io.c`.
- Shared debugger run control: `include/tf_debug_control.h`,
  `src/tf_debug_control.c`.
- Read-only debugger frame, capture, and word views: `include/tf_exec.h`,
  `src/tf_exec.c`.
- Objects/ownership: `include/tf_obj.h`, `src/tf_obj.c`, `include/tf_alloc.h`.
- Lexer: `include/tf_lexer.h`, `src/tf_lexer.c`.
- REPL: `include/tf_repl.h`, `src/tf_repl.c`.
- Language plan: `docs/language-roadmap.md`.
- Data model reference: `docs/data-model.md`.
- Test conventions: `docs/testing.md`.
- Formatter behavior and configuration: `docs/formatter.md`.

## Workflow

- Start with `git status --short`; do not overwrite user changes.
- For language behavior, check `README.md`, then the relevant C files and tests.
- For roadmap work, read `docs/language-roadmap.md` first. For collection or
  data-structure work, read `docs/data-model.md` too.
- For native word changes, update `builtins.json`, declarations, and focused
  `tests/toy/` cases, then regenerate and commit all generated metadata.
- Bootstrap the build with `clang -std=c11 nob.c -o nob.exe`; use
  `.\nob.exe build` and run `.\nob.exe test` for the default suite. Use
  `--mode leak` for ownership, stack-effect, or execution-flow changes.

## Development Rules

- C style: snake_case, 4-space indentation. Use `tf_` for exported/project-wide
  symbols; file-local `static` helpers may use unprefixed module-local names.
- Memory: `tf_obj_retain` when storing references, `tf_obj_release` when done,
  use `tf_xmalloc` helpers.
- Native callable runners should schedule frames or native continuations and
  return to the VM loop. Do not add synchronous `tf_vm_exec()` re-entry for new
  native words.
- Language direction: settle semantics before adding syntax. Prefer
  quotation-first words and combinators over new syntax.
- Vocabulary: overload an existing word when the language concept is the same
  across types. Avoid aliases that expose only implementation categories. New
  words need clear stack effects, focused tests, and a real use case.
- Stack effects: ordinary words consume their declared inputs. Predicate
  quotations in control/predicate combinators restore the surrounding data stack
  after reading a boolean result; side effects inside them are not undone.
- Data behavior: represent absence with a predicate, caller-provided default,
  or runtime error rather than an unrelated sentinel. Update-style words return
  values and must not expose shared in-place mutation.
- Sequence words should be uniform across vectors, lists, and strings when the
  result type is clear. Keep one-pass operations O(n) for every finite sequence
  and document capability-specific complexity differences. Strings are byte
  sequences; a string item is a one-byte string.
- Numeric behavior: integers are signed 64-bit values and floats are doubles.
  Mixed comparisons must preserve exact integer ordering where possible.
- Tooling: builtin metadata is generated from `builtins.json`; do not hand-edit
  generated registry, runtime-doc, LSP, Tree-sitter word-list, VS Code grammar,
  or README table outputs. Regenerate the Tree-sitter parser after generated
  word-list changes when the CLI is available. Use `npm run generate` from
  `tools/tree-sitter-toy`; it also synchronizes `parser.c` into the Go
  parser package so normal Go cache invalidation remains correct.
- Docs: README and its focused references describe current user-visible
  behavior; AGENTS contains repository navigation and durable development
  rules; the roadmap contains only current status, sequencing, and future work.
  Use Git history rather than the roadmap as a changelog.
- Shell: assume Windows PowerShell; do not output bash syntax.
