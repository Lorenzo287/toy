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

After installing Raylib, provide the appropriate compiler search paths and
libraries. A typical Windows build is:

```powershell
.\nob.exe module raylib examples\interop\raylib\toy_raylib.c `
    --include C:\raylib\include `
    --lib-dir C:\raylib\lib `
    --lib raylib `
    --lib opengl32 `
    --lib gdi32 `
    --lib winmm `
    --lib shell32
```

Use direct library paths when that is more convenient. Static Raylib builds or
other platforms may require additional system libraries documented by their
Raylib package.

Point Toy at the generated module and run either program:

```powershell
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
.\nob.exe run examples\interop\raylib\shapes.toy
.\nob.exe run examples\interop\raylib\texture.toy path\to\image.png
```

The example deliberately uses the ordinary Toy executable. There is no custom
Raylib host executable and no Raylib-specific build-system path.
