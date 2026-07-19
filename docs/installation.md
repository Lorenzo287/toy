# Installing Toy

Toy releases are complete, relocatable SDK directories. A release can run Toy
programs, build C-backed packages, generate bindings, provide editor tooling, and
support C embedding without a repository checkout or the Nob build executable.

## Release Installation

Download and extract the archive for your platform. The archive contains a
single `toy` directory. On Windows, install it for the current user with:

```powershell
.\toy\install.ps1 -AddToPath
```

Without `-AddToPath`, the script copies the SDK but only prints the directory
that should be added to `PATH`. Choose another destination with:

```powershell
.\toy\install.ps1 -InstallDir C:\Tools\Toy -AddToPath
```

On Linux or macOS:

```powershell
sh toy/install.sh
```

This installs the SDK under `$XDG_DATA_HOME/toy`, or
`$HOME/.local/share/toy`, and creates command links in `$HOME/.local/bin`.
Use `--prefix`, `--bin-dir`, or `--no-links` to change those choices:

```powershell
sh toy/install.sh --prefix /opt/toy --bin-dir "$HOME/bin"
```

The archive is also portable: adding its `bin` directory to `PATH` is enough.
Verify either installation with:

```powershell
toy --help
toy --eval "20 22 + print"
```

## SDK Layout

```text
toy/
|-- bin/       Toy, formatter, LSP, DAP, bindgen, and C-package builder
|-- core/      version-matched core: packages
|-- include/   public embedding and C-package headers
|-- lib/       the static embedding runtime for the release compiler
|-- share/     generator payload and Tree-sitter grammar assets
|-- examples/  language, embedding, FFI, and native-package examples
`-- docs/      the complete reference documentation
```

The tools discover this root relative to their executable. `TOY_ROOT` overrides
SDK discovery for `toy-bindgen` and `toy-c-package`; it does not add an import
search path. The interpreter continues to resolve ordinary imports exactly and
accepts `--core-path` when an embedder or unusual installation stores `core`
elsewhere.

`toy-bindgen` currently requires Node.js, but it uses the generator shipped in
the same SDK rather than a repository copy. `toy-c-package` requires a C
compiler and accepts `--cc` plus explicit include and library options. Normal
Toy source packages require neither tool. A C-backed package contains compiled
machine code in a `.dll`, `.so`, or `.dylib`; its manifest calls that library
`native`. The optional `core:ffi` package may also require the platform's
libffi shared library at runtime; [the FFI guide](./ffi.md) describes that
dependency.

## Building the SDK from Source

Before the first distribution build, generate the Tree-sitter parser used by
the precompiled editor tools:

```powershell
Set-Location tools\tree-sitter-toy
npm ci --ignore-scripts
npm rebuild tree-sitter-cli
npm run generate
Set-Location ..\..
```

Then stage the same directory produced by release automation:

```powershell
clang -std=c11 nob.c -o nob.exe
.\nob.exe dist
```

The result is `dist/toy`, the same layout that release archives contain. Run
its `install.ps1` or `install.sh` exactly as you would from an extracted
release. `nob dist` requires Go plus
the generated parser; release users receive the resulting binaries and parser
assets and need neither dependency. The normal runtime build also needs libffi
for `core:ffi`. See [Build Instructions](./build.md) for compiler and mode
configuration.

## Installed and Project Packages

The SDK's `core` directory is versioned with Toy and should be treated as
read-only. Project dependencies do not belong in the SDK root: keep them at
exact project-relative paths such as `vendor/`, then import those directories.
Toy currently has no registry, dependency resolver, or mutable global package
installation area. The SDK's `examples` directory is also a read-only source of
templates: run pure Toy examples there if useful, but copy embedding and
interop examples into a project before compiling or generating files.
