# Raylib Interop Example

This directory demonstrates how an application can expose an external C
library as a loadable Toy module. Raylib is not a Toy runtime dependency or a
built-in integration: the example is built with the same generic `module`
command available to any handwritten binding.

The C adapter is necessary because this example uses by-value Raylib structs,
packed color conversion, and coordinated window/GPU lifetime rules that the
manifest generator does not express. It converts ordinary Toy values, wraps
`Texture2D` as an owned `raylib.texture`, and exports a normal shared native
module discovered by `require`.

After installing a Raylib build compatible with your compiler, the adapter can
be compiled directly. A typical Windows Clang command run from the repository
root is:

```powershell
clang -std=c11 -shared -Iinclude -IC:\raylib\include `
    examples\interop\raylib\toy_raylib.c C:\raylib\lib\raylib.lib `
    opengl32.lib gdi32.lib winmm.lib shell32.lib -o toy_raylib.dll
```

Static Raylib builds or other platforms may require different system libraries
documented by their Raylib package. The equivalent generic Nob command is:

```powershell
.\nob.exe module raylib examples\interop\raylib\toy_raylib.c `
    --include C:\raylib\include --lib C:\raylib\lib\raylib.lib `
    --lib opengl32 --lib gdi32 --lib winmm --lib shell32
```

Point Toy at the generated module and run either program:

```powershell
$env:TOY_MODULE_PATH = (Resolve-Path .).Path
.\nob.exe run examples\interop\raylib\shapes.toy
.\nob.exe run examples\interop\raylib\texture.toy path\to\image.png
```

When using `nob module`, point `TOY_MODULE_PATH` at the matching
`build\<compiler>\<mode>\modules` directory instead.

The example deliberately uses the ordinary Toy executable. There is no custom
Raylib host executable and no Raylib-specific build-system path.
