# Embedding Examples

These are ordinary C applications that link the static runtime shipped in the
Toy SDK.

- `embed.c` registers a C word, evaluates Toy code, and calls a Toy word.
- `callbacks.c` captures normal output and diagnostics separately.
- `values.c` exchanges collections and a retained quotation across the API.

## Windows

Use the compiler ABI that matches the SDK archive. A GCC-style SDK can build
each host directly:

```powershell
gcc .\embed.c -I "$ToySdk\include" `
    "$ToySdk\lib\libtoy_runtime.a" -luser32 -o .\embed.exe
gcc .\callbacks.c -I "$ToySdk\include" `
    "$ToySdk\lib\libtoy_runtime.a" -luser32 -o .\callbacks.exe
gcc -std=c11 .\values.c -I "$ToySdk\include" `
    "$ToySdk\lib\libtoy_runtime.a" -luser32 -o .\values.exe

.\embed.exe
.\callbacks.exe
.\values.exe
```

For an MSVC SDK, use its matching `.lib` archive and normal MSVC compiler and
linker flags instead.

## Linux and macOS

Use the archive from the same SDK with its compatible compiler. Linux also
links `dl`; macOS does not:

```text
cc -std=c11 embed.c -I "$TOY_SDK/include" "$TOY_SDK/lib/libtoy_runtime.a" -lm -ldl -o embed
cc -std=c11 callbacks.c -I "$TOY_SDK/include" "$TOY_SDK/lib/libtoy_runtime.a" -lm -ldl -o callbacks
cc -std=c11 values.c -I "$TOY_SDK/include" "$TOY_SDK/lib/libtoy_runtime.a" -lm -ldl -o values
```

On macOS, omit `-ldl`. The commands deliberately use only the public
`toy.h` header and static archive; they are a useful starting point for a
Makefile, CMake project, or another build system.
