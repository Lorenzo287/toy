# Experimental Dynamic FFI

Toy's optional `ffi` native module uses libffi to call functions from shared C
libraries without compiling a handwritten wrapper for each function. It is an
experiment on top of the shared-module ABI, not a stable or sandboxed interface.
Its implementation lives in [`modules/ffi/`](../modules/ffi/); it is not linked
into the core runtime.

Build it after installing libffi. Provide include and library directories when
they are not already on the compiler's search path:

```powershell
.\nob.exe ffi --include C:\libffi\include --lib-dir C:\libffi\lib
$env:TOY_MODULE_PATH = (Resolve-Path .\build\clang\release\modules).Path
```

The default library name is `ffi`; pass `--lib libffi` or a direct library
path when the installation uses another name.

Use `nob ffi-test` with the same options to build a small foreign library and
exercise the module against every supported signature category.

The libffi binary itself must also be discoverable by the operating system when
it is dynamically linked.

The module provides three words:

| Word | Stack effect | Purpose |
|---|---|---|
| `ffi.open` | `path -- library` | Open a shared library using the platform loader. |
| `ffi.bind` | `library symbol signature -- function` | Resolve a symbol and prepare a fixed C signature. |
| `ffi.call` | `arg... function -- result?` | Call the function, consuming its arguments and handle. |

The function resource retains its library, so the library remains open after
`ffi.bind` consumes the original library value. Normal Toy references can keep
libraries or bound functions for reuse. Dropping the final reference closes the
foreign library automatically.

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
`INT64_MAX` therefore produces an error after the foreign call. Input strings
are copied and embedded NUL bytes are rejected. A returned `cstr` is treated as
a borrowed, non-null pointer and copied immediately; the module cannot free an
owned string returned by a library.

Arguments are pushed in C order, followed by the bound function:

```toy
"ffi" 'c require-as

20 22
"path/to/library" c.open
"add" "i32(i32,i32)" c.bind
c.call                         \ 42
```

For a concrete example, pass the platform C runtime library to
`examples/interop/ffi/strlen.toy`:

```powershell
.\nob.exe run examples\interop\ffi\strlen.toy msvcrt.dll
```

Common Unix C runtime names include `libc.so.6` on Linux and
`/usr/lib/libSystem.B.dylib` on macOS.

## Boundary and Safety

The prototype supports only the platform's default C calling convention and
fixed scalar/string signatures. It does not yet support raw pointers, output
parameters, arrays, structs, unions, variadic functions, callbacks, alternate
calling conventions, owned return values, or automatic header parsing.

Signatures are promises made by the Toy program. libffi cannot verify that a
symbol actually has the declared C type. A wrong signature, invalid returned
pointer, library bug, or hostile library can corrupt memory or terminate the
process. Only load trusted libraries and verify signatures against their C
headers.

For known APIs that can be compiled ahead of time, the
[binding generator](./bindgen.md) turns the same basic type vocabulary into a
normal loadable module. That removes runtime symbol/signature setup and lets the
C compiler see the library declarations.
