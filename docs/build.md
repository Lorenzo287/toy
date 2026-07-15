# Build Instructions for Toy

Toy uses a self-contained [Nob](https://github.com/tsoding/nob.h) build. The
repository vendors `deps/nob/nob.h`; only a C compiler is needed to bootstrap
the build program.

On Windows:

```powershell
clang -std=c11 nob.c -o nob.exe
```

On Linux or macOS:

```powershell
cc -std=c11 nob.c -o nob
```

After bootstrapping, Nob rebuilds itself whenever its source or included build
headers change.

## Everyday Commands

```powershell
.\nob.exe build
.\nob.exe test
.\nob.exe test --filter modules
.\nob.exe examples
.\nob.exe run examples\toy\factorial.toy
.\nob.exe clean
```

For `run`, put compiler and mode options before the command. Every argument
after `run` is forwarded directly to Toy, so no separator is needed:

```powershell
.\nob.exe --mode debug run --tdb program.toy
```

`build` produces the Toy CLI, the `toy_runtime` static archive used by C hosts,
and the `toy_module_support` archive used by loadable native modules. Products
are kept under `build/<compiler>/<mode>/`; loadable modules go in its
`modules` directory. The build also writes `compile_commands.json` for editor
tooling.

The complete `test` command runs isolated positive, negative, and golden-output
Toy cases, the debugger transport test, C embedding/debugger tests, native
loader tests, the Raylib adapter test, and both generator tests.

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
default worker count.

Examples:

```powershell
.\nob.exe --mode alloc build
.\build\clang\alloc\toy.exe benchmarks\dispatch.toy

.\nob.exe --cc clang --mode profile build
samply record .\build\clang\profile\toy.exe benchmarks\runtime-internals.toy
```

Profiling with MinGW GCC on Windows is not supported.

## C Embedding Examples

Build all three C hosts with:

```powershell
.\nob.exe examples
.\build\clang\release\toy_embed_example.exe
.\build\clang\release\toy_embed_callbacks_example.exe
.\build\clang\release\toy_embed_values_example.exe
```

Use the directory matching the selected compiler and mode. The examples link
the same `toy_runtime` archive produced by `build`.

## Native Modules and External Libraries

A dependency-free shared module needs only its logical module name and C file:

```powershell
.\nob.exe module sample sample.c
```

For external dependencies, repeat these options as needed:

```text
--include <directory>
--lib-dir <directory>
--lib <name-or-path>
```

They work with handwritten and generated modules. For example:

```powershell
.\nob.exe module image image.c `
    --include C:\deps\image\include `
    --lib-dir C:\deps\image\lib `
    --lib image
```

Names are translated to the compiler's normal library syntax; an absolute or
relative library file may be passed directly. Toy never downloads external
dependencies, and the selected compiler must be ABI-compatible with them.

## Generated Bindings

The `bindgen` command runs the Node.js generator and compiles its result as a
loadable module:

```powershell
.\nob.exe bindgen clib examples\bindings\clib.json
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\toy\generated_clib.toy
```

Pass the external include and library options when the manifest refers to a
third-party C library. See [Generated C Bindings](./bindgen.md) for the manifest
contract.

## Raylib

After installing Raylib, provide its installation directories if they are not
already on the compiler's default search path:

```powershell
.\nob.exe raylib `
    --include C:\raylib\include `
    --lib-dir C:\raylib\lib
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\toy\raylib_shapes.toy
```

`raylib` is the default library name. Use `--lib <name-or-path>` when the
installed library uses another name. On Windows the required system libraries
are added automatically. The command builds both
the loadable `toy_raylib` module and the statically registered
`toy_raylib_example` C host.

The texture demo accepts an image path:

```powershell
.\nob.exe run examples\toy\raylib_texture.toy path\to\image.png
```

## libffi

The optional dynamic FFI module is built similarly:

```powershell
.\nob.exe ffi `
    --include C:\libffi\include `
    --lib-dir C:\libffi\lib
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
```

The default library name is `ffi`; use `--lib libffi` or a direct library path
when necessary. See [Experimental Dynamic FFI](./ffi.md) for the runtime
contract and safety limitations.

Run the optional integration test with the same dependency options:

```powershell
.\nob.exe ffi-test --include C:\libffi\include --lib-dir C:\libffi\lib
```
