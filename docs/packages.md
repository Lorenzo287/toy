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

```powershell
toy project\app
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

`--eval` and `--eval-file` execute code in the host/root scope rather than
turning a file into a package. They are useful for short scripts, tests, and
debugger sessions:

```powershell
toy --eval "1 2 + print"
toy --eval-file tools\inspect.toy
```

`--eval-file` may be repeated; each file runs in the same state. Root code can
also use `import` and qualified package words; its relative imports use the
process working directory because root evaluation has no package directory.
There is no language-level `load` word.

## C-Backed and External-Library Packages

A C-backed package is a Toy package whose implementation includes machine code
compiled into a shared library. Its directory contains a `toy.package`
manifest:

```text
name = sqlite
native = toy_sqlite.dll
```

`native` is the manifest's name for that compiled `.dll`, `.so`, or `.dylib`.
It is an exact path relative to the package directory (or an absolute path).
The library exports Toy's versioned package descriptor from
`include/toy_package.h`; its descriptor name, manifest name, and any
source-package declaration must agree. A directory can be C-only or combine C
words with Toy definitions.

The installed C-package builder creates both the shared library and
manifest in place:

```powershell
toy-c-package vendor\sqlite vendor\sqlite\toy_sqlite.c `
    --include C:\sqlite\include `
    --lib-dir C:\sqlite\lib `
    --lib sqlite3
```

The application then uses the same import syntax as for Toy source:

```toy
"../vendor/sqlite" 'db import-as
```

Toy does not use a package manager, registry, lockfile, or dependency resolver.
Projects choose where dependency directories live, commonly committing small
bindings under `vendor/` while obtaining the actual C library through their
normal system or project build process.

### What the C Artifacts Do

A header supplies declarations, not implementation. At build time:

- a static `.a` or `.lib` is linked into the C-backed Toy package;
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
   `toy-bindgen`, then compile it with `toy-c-package`. The tools are shipped in
   the SDK and write the shared library and `toy.package` in the target
   directory.
3. Handwritten binding: implement callbacks with `toy_package.h` and compile
   the adapter with `toy-c-package`. This covers structs, callbacks, custom
   validation, and library-specific ownership rules that the generator cannot
   express.

`core:ffi` is an official package maintained and built with Toy. Raylib,
SQLite, and similar bindings remain ordinary project packages even when the
repository contains examples for them.

## Relation to Odin

Like Odin, Toy makes the directory the compilation and namespace unit, imports
packages rather than files, and qualifies imported declarations with a package
name. Toy's model is smaller and runtime-oriented: package files are
declaration-only, declarations are public by default, import locations are
literal directories (plus the fixed `core:` prefix), and native libraries join
the same namespace through a tiny manifest. It intentionally has no package
collection layer or package manager over path imports.
