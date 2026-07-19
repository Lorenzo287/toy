# Experimental Dynamic FFI

The official `core:ffi` package uses libffi to call functions from shared C
libraries without compiling a wrapper for each function. It is maintained and
built alongside Toy, but remains experimental and is not a sandbox.

Release SDKs include the version-matched compiled package beneath their `core`
directory. Import it directly:

```toy
"core:ffi" import
```

The package is ready to use after installation. Building Toy itself from
source needs libffi headers and libraries; if they are outside the compiler's
default paths:

```powershell
.\nob.exe --cc gcc build `
    --include C:\libffi\include `
    --lib-dir C:\libffi\lib
```

The default library name is `ffi`. Supplying `--lib libffi` or an exact path
replaces that default when an installation uses another name.

The compiler must match the libffi distribution. An MSYS2/MinGW installation
normally requires `--cc gcc`. If libffi is dynamically linked, its runtime
library must also be discoverable by the operating-system loader. Run
`nob test --filter optional_ffi` to build a small foreign library and exercise
every supported signature category.

The package provides three public words:

| Word | Stack effect | Purpose |
|---|---|---|
| `ffi.open` | `path -- library` | Open an exact shared-library path or platform loader name. |
| `ffi.bind` | `library symbol signature -- function` | Resolve a symbol and prepare a fixed C signature. |
| `ffi.call` | `arg... function -- result?` | Call the function, consuming its arguments and handle. |

The function resource retains its library, so the library remains open after
`bind` consumes the original library value. Dropping the final reference closes
the foreign library automatically.

## Signatures

A signature contains a return type followed by comma-separated argument types:

```text
i32(i32,i32)
usize(cstr)
cstr()
void(i32)
```

Supported types are:

| Signature type | Toy value |
|---|---|
| `bool` | Boolean |
| `i8`, `i16`, `i32`, `i64` | Range-checked integer |
| `u8`, `u16`, `u32`, `u64` | Non-negative, range-checked integer |
| `isize`, `usize` | Pointer-sized integer |
| `f32`, `f64` | Float, or an integer converted to a float |
| `cstr` | String copied to or from a NUL-terminated C string |
| `void` | Return type only; no Toy result is pushed |

Toy integers are signed 64-bit values. A `u64` or `usize` result greater than
`INT64_MAX` is therefore an error after the foreign call. Input strings are
copied and embedded NUL bytes are rejected. A returned `cstr` is treated as a
borrowed, non-null pointer and copied immediately; FFI cannot free an owned
string returned by a library.

Arguments are pushed in C order, followed by the bound function:

```toy
"core:ffi" 'c import-as

20 22
"path/to/library" c.open
"add" "i32(i32,i32)" c.bind
c.call                         \ 42
```

The runnable `strlen` example accepts the platform C runtime library as its
first argument:

```powershell
$ToySdk = 'C:\Tools\Toy'
toy --file "$ToySdk\examples\ffi\main.toy" msvcrt.dll
```

Common Unix names include `libc.so.6` on Linux and
`/usr/lib/libSystem.B.dylib` on macOS.

## Boundary and Safety

The package supports only the platform's default C calling convention and
fixed scalar/string signatures. It does not support raw pointers, output
parameters, arrays, structs, unions, variadic functions, callbacks, alternate
calling conventions, owned return values, or automatic header parsing.

A signature is a promise made by the Toy program. libffi cannot verify that a
symbol has the declared C type. A wrong signature, invalid returned pointer,
library bug, or hostile library can corrupt memory or terminate the process.
Only open trusted libraries and verify signatures against their C headers.

For known APIs, the [binding generator](./bindgen.md) emits a C-backed package
whose C compiler sees the declarations. A handwritten package remains the
choice for custom ownership and richer C types.
