# Raylib Interop Example

This directory exposes Raylib as an ordinary C-backed Toy package. Raylib is not
a Toy runtime dependency or core integration.

The C adapter is handwritten because it translates by-value structs and packed
colors and coordinates window/GPU lifetimes. It wraps `Texture2D` as an owned
`raylib.texture` resource.

Copy this directory into an editable project tree before building it, then work
from that copy; do not write generated artifacts into the SDK installation:

```powershell
$ToySdk = 'C:\Tools\Toy'
Copy-Item "$ToySdk\examples\packages\raylib" .\raylib -Recurse
cd .\raylib
```

After installing a Raylib build compatible with your compiler, build the
package in place. A typical Windows command is:

```powershell
toy-c-package . .\toy_raylib.c `
    --include C:\raylib\include `
    --lib C:\raylib\lib\raylib.lib `
    --lib opengl32 --lib gdi32 --lib winmm --lib shell32
```

Static Raylib builds or other platforms may need the system libraries
documented by their Raylib distribution. The command writes the shared library
and `toy.package` into this directory. Run either standalone demo, passing the
package directory first:

```powershell
toy --file .\demos\shapes.toy .
toy --file .\demos\texture.toy . path\to\image.png
```

There is no global installation, search path, or custom Raylib host executable.
