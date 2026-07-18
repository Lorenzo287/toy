# Build Instructions for Toy

Toy uses a self-contained [Nob](https://github.com/tsoding/nob.h) build. The
repository vendors `deps/nob/nob.h`; bootstrap Nob with a C compiler:

```powershell
clang -std=c11 nob.c -o nob.exe
```

On Linux or macOS, the equivalent command is:

```powershell
cc -std=c11 nob.c -o nob
```

Nob rebuilds itself whenever its source or included build headers change. A
source build also needs libffi headers and a linkable `ffi` library because the
official `core:ffi` package is built with the CLI.

## Everyday Commands

```powershell
.\nob.exe build
.\nob.exe test
.\nob.exe test --filter package
.\nob.exe examples
.\nob.exe dist
.\nob.exe clean
```

Run the resulting interpreter directly:

```powershell
.\build\gcc\release\toy.exe examples\programs\factorial
.\build\gcc\release\toy.exe --eval-file examples\eval\quines\quine.toy
```

`build` produces the Toy CLI, the `toy_runtime` static archive used by C hosts,
and `core/ffi` beneath `build/<compiler>/<mode>/`. The CLI automatically uses
the `core` directory beside its executable. The build also writes
`compile_commands.json` for editor tooling.

The complete `test` command runs isolated positive, negative, and golden-output
Toy cases, package integration cases, debugger transport tests, C embedding
and native-loader tests, and the binding-generator tests.

`dist` stages the complete consumer SDK under
`build/<compiler>/<mode>/dist/toy`. It builds the runtime first, then the
precompiled Go tooling, and copies public headers, core packages, generator and
Tree-sitter assets, examples, docs, and installation scripts. Release archives
contain that directory unchanged. The generated Tree-sitter C parser must
exist first; follow the source-generation commands in the
[installation guide](./installation.md#building-the-sdk-from-source).

Nob is only the repository build orchestrator. It deliberately does not run
user programs, compile user C packages, or expose binding generation.
Those workflows belong to `toy`, `toy-c-package`, and `toy-bindgen` from the
staged or installed SDK.

## Compilers and Modes

Select a compiler and mode with:

```powershell
.\nob.exe --cc gcc --mode debug build
```

Supported compilers are `clang`, `gcc`, `msvc`, and `clang-cl`. On Windows,
`msvc` requires a Visual Studio Developer PowerShell. Supported modes are:

- `release`: optimized build;
- `debug`: unoptimized build with debug information;
- `alloc`: optimized build with allocation counters;
- `leak`: leak instrumentation (`stb_leakcheck` on Windows, LeakSanitizer on
  supported Unix compilers);
- `profile`: optimized build with symbols and frame pointers.

Independent C compilations run in parallel. Use `-j` or `--jobs` to change the
default worker count. Profiling with MinGW GCC on Windows is not supported.

Examples:

```powershell
.\nob.exe --mode alloc build
.\build\gcc\alloc\toy.exe --eval-file benchmarks\dispatch.toy

.\nob.exe --cc clang --mode profile build
samply record .\build\clang\profile\toy.exe --eval-file benchmarks\runtime-internals.toy
```

## C Embedding Examples

The `examples` command builds the C hosts under `examples/embedding/`:

```powershell
.\nob.exe examples
.\build\gcc\release\toy_embed_example.exe
.\build\gcc\release\toy_embed_callbacks_example.exe
.\build\gcc\release\toy_embed_values_example.exe
```

Use the directory matching the selected compiler and mode. The examples link
the same `toy_runtime` archive produced by `build`.

## C-Backed Packages and External Libraries

A C-backed package includes `include/toy_package.h`. Define
`TOY_PACKAGE_IMPLEMENTATION` before including it in exactly one C file; the
header supplies the host-table forwarding layer, so no Toy support library is
linked into the package.

Stage or install the SDK, create a package directory, then build its C source
with the consumer tool:

```powershell
.\nob.exe dist
.\build\gcc\release\dist\toy\bin\toy-c-package.exe `
    vendor\sample vendor\sample\sample.c
```

The directory basename is the package name. `toy-c-package` creates the
platform-specific shared library and a `toy.package` manifest in that
directory. The C export
descriptor must report the same name. Outside repository development, invoke
`toy-c-package` from `PATH` without the build-directory prefix.

For external dependencies, repeat these options as needed:

```text
--include <directory>
--lib-dir <directory>
--lib <name-or-path>
```

For example:

```powershell
toy-c-package vendor\image vendor\image\image.c `
    --include C:\deps\image\include `
    --lib-dir C:\deps\image\lib `
    --lib image
```

Names are translated to the compiler's normal library syntax; a library file
may also be passed directly. Toy never downloads external dependencies, and
the selected compiler must be ABI-compatible with them. Static libraries are
linked into the Toy package. Shared-library dependencies must still be
discoverable by the operating-system loader at runtime.

The package can then be imported by its exact directory:

```toy
"../vendor/image" import
```

See [Packages and Imports](./packages.md#c-backed-and-external-library-packages)
for the runtime model and [`examples/interop/`](../examples/interop/) for
handwritten Raylib and SQLite adapters.

## Generated Bindings

The installed generator emits C, and `toy-c-package` compiles that C into the
specified package directory:

```powershell
toy-bindgen --package clib `
    examples\interop\bindgen\clib\clib.json `
    examples\interop\bindgen\clib\generated.c
toy-c-package examples\interop\bindgen\clib `
    examples\interop\bindgen\clib\generated.c
toy examples\interop\bindgen\demos\clib
```

Pass external include and library options when the manifest refers to a
third-party C library. See [Generated C Bindings](./bindgen.md) for the
manifest contract.

## libffi

The normal build compiles `core/ffi/toy_ffi.c` and links it with libffi. If the
headers or library are outside the compiler's default paths, pass their
locations to `build`, `test`, or `dist`:

```powershell
.\nob.exe --cc gcc build `
    --include C:\libffi\include `
    --lib-dir C:\libffi\lib `
    --lib ffi
```

With no `--lib` option, the build links `ffi`. Supplying one or more `--lib`
options replaces that default list, which permits `--lib libffi` or an exact
library path on unusual installations. The selected compiler must match the
libffi distribution; MSYS2/MinGW libffi normally uses `--cc gcc`. See
[Experimental Dynamic FFI](./ffi.md) for signatures and safety constraints.
