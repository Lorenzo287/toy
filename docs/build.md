# Developing Toy from Source

This page is for contributors building Toy itself. A release SDK is the normal
way to use the language in another project; see [Installation](./installation.md)
and the [examples](../examples/). Nob is deliberately a repository tool,
not part of the user-facing SDK workflow.

## Bootstrap Nob

Toy vendors [Nob](https://github.com/tsoding/nob.h). Build its single source
file with a C compiler:

```powershell
clang -std=c11 nob.c -o nob.exe
```

On Linux or macOS, use `cc -std=c11 nob.c -o nob`. Nob rebuilds itself when its
source or included build headers change. The normal source build also needs
libffi headers and a linkable `ffi` library because it builds the official
experimental `core:ffi` package.

## Development Commands

```powershell
.\nob.exe build
.\nob.exe test
.\nob.exe test --filter package
.\nob.exe dist
.\nob.exe clean
```

`build` produces the Toy CLI, the `toy_runtime` static archive, and `core:ffi`
under `build/<compiler>/<mode>/`. Run the checkout's interpreter directly:

```powershell
.\build\gcc\release\toy.exe --file examples\factorial.toy
.\build\gcc\release\toy.exe --file examples\quines\quine.toy
```

`test` runs isolated Toy, package, debugger, embedding, native-loader, and
binding-generator regressions. It does not provide a separate way to build
user examples: examples document ordinary SDK use and are compiled directly
with a C compiler where appropriate.

`dist` stages the consumer SDK at `dist/toy`. It
builds the runtime and precompiled Go tools, then copies public headers, core
packages, generator and Tree-sitter assets, examples, docs, and installers.
Release archives contain that directory unchanged. The generated Tree-sitter C
parser must exist first; follow [Building the SDK from Source](./installation.md#building-the-sdk-from-source).

The source checkout intentionally has no root installer. Run
`dist\toy\install.ps1` on Windows or `sh dist/toy/install.sh` on Unix after
staging. For C-backed packages and generated bindings, use the installed SDK guides in
[Packages](./packages.md#c-backed-and-external-library-packages) and
[Generated C Bindings](./bindgen.md).

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

```powershell
.\nob.exe --mode alloc build
.\build\gcc\alloc\toy.exe --file benchmarks\dispatch.toy

.\nob.exe --cc clang --mode profile build
samply record .\build\clang\profile\toy.exe --file benchmarks\runtime-internals.toy
```

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
[Experimental Dynamic FFI](./ffi.md) for its runtime dependency and safety
constraints.
