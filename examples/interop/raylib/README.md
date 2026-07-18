# Raylib Interop Example

This directory exposes Raylib as an ordinary native Toy package. Raylib is not
a Toy runtime dependency or core integration.

The C adapter is handwritten because it translates by-value structs and packed
colors and coordinates window/GPU lifetimes. It wraps `Texture2D` as an owned
`raylib.texture` resource.

After installing a Raylib build compatible with your compiler, build the
package in place. A typical Windows command is:

```powershell
.\nob.exe package examples\interop\raylib `
    examples\interop\raylib\toy_raylib.c `
    --include C:\raylib\include `
    --lib C:\raylib\lib\raylib.lib `
    --lib opengl32 --lib gdi32 --lib winmm --lib shell32
```

Static Raylib builds or other platforms may need the system libraries
documented by their Raylib distribution. The command writes the native library
and `toy.package` into this directory. Run either package:

```powershell
.\nob.exe run examples\interop\raylib\demos\shapes
.\nob.exe run examples\interop\raylib\demos\texture path\to\image.png
```

Both demos import `../..`, so no global installation or search path is needed.
There is no custom Raylib host executable.
