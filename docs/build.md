# Build Instructions for Toy

## Build Profiles

Builds are configured using default cmake profiles `-DCMAKE_BUILD_TYPE=<Profile>`
and with custom build modes `-DBUILD_MODE=<Mode>`.

You can use the "Unix Makefiles" Generator (defaults to MinGW on Windows) if you don't have "Ninja" installed.

It's possible to override the default compiler using `-DCMAKE_C_COMPILER=<Compiler>`.

### 1. Release (Optimized)

Optimized build for production usage (works on Windows/Linux).

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

The build produces the `toy` CLI and a static `toy_runtime` library. CMake
hosts can link the `toy::runtime` alias and use the experimental API in
`include/toy.h`; see [Embedding Toy in C](./embedding.md).

The optional Raylib binding is disabled by default. With either a Raylib CMake
package or an installation prefix containing `include/raylib.h` and its library,
build the binding and Toy-driven example with:

```powershell
cmake -S . -B build-raylib -DTOY_BUILD_RAYLIB=ON
cmake --build build-raylib --target toy_raylib_example
.\build-raylib\toy_raylib_example.exe
```

Set `CMAKE_PREFIX_PATH` to that prefix, or use a package-manager toolchain, when
CMake cannot find Raylib. The compiler must be compatible with the installed
Raylib binary. This option adds `toy::raylib`; it never downloads the
dependency.

### 2. LeakCheck

Development build for tracking leaks.

- **Windows (MSVC/MinGW)**: Defines `STB_LEAKCHECK` (ensure `stb_leakcheck_dumpmem()` is called in main).
- **Linux/WSL**: Uses AddressSanitizer.

```powershell
cmake -S . -B build-leak -G "Unix Makefiles" -DBUILD_MODE=LeakCheck
cmake --build build-leak
```

### 3. Allocation statistics

This mode counts checked allocation calls and cumulative requested bytes. It is
intended for comparing identical workloads before and after an allocation
change; requested bytes are not a live-memory or peak-memory measurement.

```powershell
cmake -S . -B build-alloc -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DBUILD_MODE=AllocationStats
cmake --build build-alloc
.\build-alloc\toy.exe benchmarks\dispatch.toy
```

### 4. Profiling

Development build for profiling symbols (uses `-O2`).

_Note: On Windows use MSVC or Clang, MinGW is not supported for this mode._

```powershell
cmake -S . -B build-prof -G "Ninja" -DBUILD_MODE=Profile -DCMAKE_C_COMPILER=clang
cmake --build build-prof
cd build-prof
samply record toy.exe ../benchmarks/runtime-internals.toy
```
