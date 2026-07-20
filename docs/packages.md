# Packages and Imports

Toy packages are directories. A directory is the unit of namespacing and
loading; individual files only split its implementation.

## Source Packages

Every `.toy` file directly inside a package directory begins with the same
package declaration:

```text
project/
|-- app/
|   `-- main.toy
`-- math/
    |-- arithmetic.toy
    `-- constants.toy
```

```toy
\ math/arithmetic.toy
'math package

'double [ 2 * ] def
'internal-helper [ 1 + ] def
'internal-helper private
```

```toy
\ app/main.toy
'main package

"../math" import

'main [
    21 math.double print
] def
```

Run the executable package by passing its directory to Toy:

```console
toy project/app
```

The CLI accepts only a package named `main` as an executable and invokes its
public `main` word. `main` is otherwise an ordinary Toy word: it has no special
syntax or implicit parameters. Command-line arguments after the directory are
available through `argc` and `argv`.

## Declarations and Visibility

Package files contain declarations at top level:

- `'name package` first in every file;
- `"path" import` or `"path" 'alias import-as`;
- `'name [ ... ] def`;
- `'name private`.

Definitions are public by default. A private word remains available to every
file in its package but cannot be called through an imported qualifier.
Definitions, imports, and privacy declarations are collected across the whole
directory before any word body runs, so file names and file order do not affect
the package. Duplicate definitions, inconsistent package names, executable
top-level code, and dependency cycles are errors. Subdirectories are separate
packages and are not loaded automatically.

There is deliberately no package `init` hook. Initialization with observable
effects is an ordinary word called explicitly by the application. This keeps
imports independent of file order and makes construction visible in the call
graph.

## Import Resolution

An import string has one deterministic interpretation:

- a relative directory is resolved from the importing package;
- an absolute directory is used directly;
- `core:name` resolves `name` beneath Toy's configured core directory.

There are no filename substitutions, fallback directories, executable probes,
or environment-variable search paths. Toy identifies a loaded package by its
canonical directory, loads it once per interpreter state, and detects cycles.

Without an alias, the package's declared name becomes the qualifier:

```toy
"../math" import
21 math.double
```

`import-as` changes only the local qualifier:

```toy
"../math" 'm import-as
21 m.double
```

The dot means namespace qualification, never path traversal. Paths look like
paths; calls look like `package.word`.

The installed CLI finds `core:` packages in the SDK's `core` directory. A
source-tree build also accepts `core` beside the executable. `--core-path DIR`
overrides either location. Embedders set
`toy_state_config.core_package_path` explicitly.

## Evaluation Outside Packages

`--eval` and `--file` execute code in the host/root scope rather than
turning a file into a package. They are useful for short scripts, tests, and
debugger sessions:

```console
toy --eval "1 2 + print"
toy --file examples/factorial.toy
```

`--file` may be repeated; each file runs in the same state. Arguments after the
last file are available through `argc` and `argv`. Root code can
also use `import` and qualified package words; its relative imports use the
process working directory because root evaluation has no package directory.
There is no language-level `load` word.

## C Extensions and External Libraries

A Toy package can contain Toy source, a C extension, or both. The extension is
a shared library named by the directory's `toy.package` manifest:

```ini
name = sqlite
extension = toy_sqlite.dll
```

`extension` is an exact path to the compiled `.dll`, `.so`, or `.dylib`, either
relative to the package directory or absolute. The library exports Toy's
versioned extension descriptor from
`include/toy.h`; its descriptor name, manifest name, and any
source-package declaration must agree.

## Building a C Extension by Hand

The complete contract is intentionally small: compile a shared library that
defines `TOY_EXTENSION_IMPLEMENTATION` and includes the standalone `toy.h`,
then write the manifest yourself. No Toy runtime or package-support library
participates in that link.

```console
cc -std=c11 -Wall -Wextra -Wpedantic -shared vendor/sqlite/toy_sqlite.c -I path/to/toy/include -I path/to/sqlite/include path/to/sqlite/lib/sqlite3.lib -o vendor/sqlite/toy_sqlite.dll
```

Create `vendor/sqlite/toy.package` beside the compiled library:

```ini
name = sqlite
extension = toy_sqlite.dll
```

On Linux, add `-fPIC` and use `toy_sqlite.so`; on macOS use `-dynamiclib` and
`toy_sqlite.dylib`. Static foreign libraries are linked into the extension.
When using a shared foreign library, its runtime dependencies must still be
discoverable by the operating-system loader. See the dependency notes below.

The shipped [basic package](../examples/packages/basic/) contains a
dependency-free C extension and demo that exercise this exact route.

## `toy-c-package` Convenience Tool

The installed C-extension builder performs the same compile, link, and manifest
steps in place:

```console
toy-c-package vendor/sqlite vendor/sqlite/toy_sqlite.c --include path/to/sqlite/include --lib-dir path/to/sqlite/lib --lib sqlite3
```

The application then uses the same import syntax as for Toy source:

```toy
"../vendor/sqlite" 'db import-as
```

Dependencies are exact directories chosen by the project. A common layout
commits small bindings under `vendor/` while obtaining the C library through
the project's normal build process; no registry, lockfile, or global package
state participates in resolution.

### What the C Artifacts Do

A header supplies declarations, not implementation. At build time:

- a static `.a` or `.lib` is linked into the C extension;
- a shared `.so`, `.dylib`, or `.dll` is linked through its import library or
  loaded explicitly, and the operating-system loader must be able to find its
  runtime dependencies;
- include directories only tell the C compiler where headers live.

The `toy.package` manifest tells Toy where its wrapper is. It does not replace
the C compiler/linker settings and does not control the operating system's
shared-library lookup.

### Three Interop Routes

1. Dynamic FFI: import `core:ffi`, open a shared library by an explicit path or
   platform name, declare a supported signature, and call it. This needs no C
   wrapper but offers the smallest type and ownership boundary.
2. Generated binding: write an explicit JSON manifest, emit its C wrapper with
   `toy-bindgen`, then either compile and manifest the extension manually or
   use `toy-c-package`. The tools are shipped in the SDK but are not required
   by the C-extension ABI.
3. Handwritten binding: implement callbacks with `toy.h`, then compile
   and build the extension manually or with `toy-c-package`. This covers
   structs, callbacks, custom validation, and library-specific ownership rules
   that the generator cannot express.

`core:ffi` ships in Toy's `core` directory. The Raylib and SQLite examples are
templates for ordinary project packages built through the same boundary.

## Relation to Odin

Like Odin, Toy makes the directory the compilation and namespace unit, imports
packages rather than files, and qualifies imported declarations with a package
name. Toy's model is smaller and runtime-oriented: package files are
declaration-only, declarations are public by default, import locations are
literal directories (plus the fixed `core:` prefix), and C extensions join
the same namespace through a tiny manifest. It intentionally has no package
collection layer or package manager over path imports.
