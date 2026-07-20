# Binding Manifest Reference

This page describes the complete `toy-bindgen` manifest. Start with
[Using C Libraries](./c-libraries.md#generated-bindings) for the normal
workflow and return here only when a binding needs more than scalar functions.

`toy-bindgen` writes a C extension whose compiler sees the real C declarations.
The manifest stays explicit because a header cannot describe ownership or the
Toy API that should be presented.

## Functions and Types

A manifest names one package, the headers included by its generated C file, and
the exported functions:

```json
{
  "package": "clib",
  "headers": ["string.h"],
  "functions": [
    {
      "name": "strlen",
      "returns": "usize",
      "args": ["cstr"]
    },
    {
      "name": "strcmp",
      "word": "compare",
      "returns": "i32",
      "args": ["cstr", "cstr"]
    }
  ]
}
```

`name` is the C identifier. Optional `word` selects another public Toy word
name. Duplicate words, unsafe header paths, unknown fields, unsupported types,
and `void` arguments are rejected before C is emitted.

Supported scalar types are:

- `bool`;
- `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, and `u64`;
- `isize` and `usize`;
- `f32` and `f64`;
- `cstr`;
- `void` as a return type only.

Integer inputs are range-checked. A Toy string passed as `cstr` is copied to a
temporary NUL-terminated buffer. A non-null returned `cstr` is treated as
borrowed and copied into Toy. Unsigned results outside Toy's signed 64-bit
range are errors.

## Hidden Arguments

Argument objects can provide values that do not consume the Toy stack:

```json
"args": [
  {"resource": "database"},
  "cstr",
  {"const": "i32", "value": -1},
  {"outResource": "statement"},
  {"null": true}
]
```

`const` accepts a supported boolean, integer, or floating type and matching JSON
value. `null` emits `NULL`. `cConstant` emits one validated C identifier, such
as `{"cConstant": "SQLITE_TRANSIENT"}`, declared by a manifest header.

## Byte Strings

A byte argument turns one binary-safe Toy string into adjacent pointer and
length C arguments:

```json
{"bytes": {"length": "i32"}}
```

The generator passes the string's borrowed bytes directly and range-checks the
length. The C function must copy the bytes if it retains them.

A borrowed pointer result can use a companion function to obtain its length:

```json
"returns": {
  "bytes": {
    "lengthFunction": "widget_result_size",
    "lengthType": "usize"
  }
}
```

The companion receives the same C arguments as the primary function. The
result is validated and copied into a Toy string before borrowed inputs are
released.

## Opaque Resources

The optional `resources` array declares owned pointer types:

```json
{
  "package": "widget",
  "headers": ["widget.h"],
  "resources": [
    {
      "name": "handle",
      "cType": "widget_handle *",
      "destructor": "widget_destroy"
    },
    {
      "name": "view",
      "cType": "widget_view *",
      "destructor": "widget_view_destroy",
      "dependsOn": "handle"
    }
  ],
  "functions": [
    {
      "name": "widget_create",
      "word": "create",
      "returns": {"resource": "handle"},
      "args": ["i32"]
    },
    {
      "name": "widget_value",
      "word": "value",
      "returns": "i32",
      "args": [{"resource": "handle"}]
    },
    {
      "name": "widget_view",
      "word": "view",
      "returns": {"resource": "view"},
      "args": [{"resource": "handle"}]
    },
    {
      "name": "widget_open",
      "word": "open",
      "returns": {"status": "i32", "success": [0]},
      "args": ["cstr", {"outResource": "handle"}]
    }
  ]
}
```

A resource name is local to its package: `handle` above becomes
`widget.handle`. `cType` must be a C pointer type. `destructor` names the C
function that releases an owned value.

`{"resource":"handle"}` as a return declares a new, non-null owned pointer.
As an argument it unwraps an exact typed resource and passes the borrowed C
pointer. `{"outResource":"handle"}` passes a pointer-to-pointer without
consuming a Toy input. A function may have at most one output resource and must
return either `void` or a status declaration.

`dependsOn` keeps one parent resource alive until the child destructor has
finished. Every function producing that child must receive exactly one resource
of the named parent type.

## Status Results

A status declaration consumes the C result instead of pushing it. `success`
lists accepted codes; another code becomes a Toy error:

```json
"returns": {"status": "i32", "success": [0]}
```

For predicate-like APIs, `map` converts accepted numeric codes into booleans:

```json
"returns": {"status": "i32", "map": {"100": true, "101": false}}
```

An optional error accessor adds a library message:

```json
"returns": {
  "status": "i32",
  "success": [0],
  "error": {"function": "widget_error", "resource": "handle"}
}
```

The named resource must occur exactly once among the function's resource inputs
or output. The message is copied before failed output handles or consumed
inputs are released. Failed and rejected output handles are destroyed
automatically.

Resource inputs are consumed like ordinary Toy inputs; use `dup` when the same
handle is needed again. Returned strings are copied before those inputs are
released, which permits getters that borrow text from their handle.

## Commands

Generate a wrapper with the SDK frontend:

```console
toy-bindgen --package clib path/to/clib.json vendor/clib/generated.c
```

`--check` verifies an existing generated file without rewriting it. Generated
output is deterministic and begins with a do-not-edit marker.

Compile it and create the package manifest with:

```console
toy-c-package vendor/clib vendor/clib/generated.c --include path/to/headers --lib the_c_library
```

The target directory basename must match the manifest's package name.
Headers remain in the JSON because they affect generated C. Platform-specific
include and library paths remain compiler options rather than manifest data.
Run either tool with `--help` for its complete command-line interface.

## Limits

The manifest must match its C headers semantically; compatible C conversions
can conceal a mistaken scalar declaration. Generated bindings do not support
arbitrary pointers, general output buffers, structs, unions, variadic
functions, callbacks, or library-specific state transitions. Use a
[handwritten extension](./c-libraries.md#handwritten-c-extensions) when the
desired boundary needs those features.
