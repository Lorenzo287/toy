# Embedding Toy in C

The Toy SDK ships a static `toy_runtime` library and `include/toy.h` alongside
the command-line tools. The API lets a C host
create an interpreter state, evaluate source, call Toy words, inspect primitive
or opaque resource stack values, retain arbitrary Toy values, traverse basic
collections, and register synchronous native words or packages.

The boundary is intentionally small. Toy owns the interpreter and its values;
the host works through opaque handles and explicit stack operations.

## Buildable Example

[`examples/embedding/embed.c`](../examples/embedding/embed.c) demonstrates both
directions of the boundary: Toy calls a registered C word, then C calls a
Toy-defined word.
[`examples/embedding/callbacks.c`](../examples/embedding/callbacks.c) captures
binary-safe Toy output, redirects diagnostics separately, and inspects a
detailed parser error.
[`examples/embedding/values.c`](../examples/embedding/values.c) constructs
structured input, traverses a returned map, and calls a retained Toy quotation.
Copy those sources into an application directory and compile them directly
against the SDK's `include/` and `lib/` directories. The
[embedding example guide](../examples/embedding/README.md) gives exact Windows
and Unix commands for all three hosts. Use the same compiler ABI as the shipped
archive.

## Output and Diagnostics

`toy_state_new()` accepts a nullable configuration pointer. Passing a
configuration lets a host redirect normal Toy output and diagnostics
independently:

```c
static void append_bytes(void *userdata, const char *data, size_t length) {
    struct host_log *log = userdata;
    host_log_append(log, data, length);
}

toy_state_config config = {
    .output = append_bytes,
    .output_userdata = &console_log,
    .diagnostic = append_bytes,
    .diagnostic_userdata = &error_log,
};
toy_state *state = toy_state_new(&config);
```

The complete buildable version is
[`examples/embedding/callbacks.c`](../examples/embedding/callbacks.c).

Callbacks run synchronously and receive borrowed byte spans, which may contain
NUL bytes and are not NUL-terminated. They must copy data that needs to outlive
the call and must not re-enter the state. Null callbacks retain the default
stdout and stderr destinations. Passing `NULL` to `toy_state_new()` selects
both defaults without constructing a configuration.

The output callback receives language output from words such as `print`,
`printf`, `.`, `.s`, `.S`, and `clear`. The diagnostic callback receives parser
and runtime diagnostics, including source locations and runtime stack context.
Callbacks and userdata are state-local.

## Execution Boundary

`toy_eval()` parses and runs an in-memory source string. `source_name` appears
in source locations and may be `NULL` to use `<eval>`. Definitions and stack
values remain in the state across calls.

`toy_call()` invokes one named word to completion. A host can therefore define
callbacks in Toy and drive them from C:

```c
toy_eval(state, "game.toy", "'update [ 1 + ] def");
toy_push_int(state, 41);
toy_call(state, "update");
```

`toy_import_package()` loads a directory package into the host root, with an
optional qualifier alias. `toy_run_package()` loads an executable package and
invokes its public `main` word. Both are idle-state operations. Set
`toy_state_config.core_package_path` when imported code may use `core:`; unlike
the CLI, an embedder has no implied installation directory.

Both entry points require an idle state. A native word must not recursively call
`toy_eval()` or `toy_call()` on the state that is currently executing. Native
continuations remain an internal VM facility.

The main statuses are:

- `TOY_OK`: execution completed;
- `TOY_ERROR`: parsing, execution, or native code failed;
- `TOY_INTERRUPTED`: `toy_interrupt()` requested an unwind;
- `TOY_EXIT_REQUESTED`: Toy executed `exit`; the host decides whether its
  process should terminate.

## Packages Registered by a Host

A descriptor groups local C word names under one Toy package:

```c
static const toy_native_word host_words[] = {
    {"log", host_log},
    {"double", host_double},
};

static const toy_native_package host_package = {
    "host",
    host_words,
    sizeof(host_words) / sizeof(host_words[0]),
};

toy_register_package(state, &host_package);
```

Package descriptor names are single identifiers; word entries are local names
and cannot contain dots.

Registration copies the names, validates the entire descriptor before making
changes, makes every word public, and imports the package into the host root.
Evaluated Toy code can call it immediately:

```toy
21 host.double
"ready" host.log
```

Package names and namespace ownership cannot collide with imported packages or
previously registered qualified words. The callback functions themselves must
remain valid for the lifetime of the state. `toy_register_word()` remains
available for standalone root words that do not need package identity.

## C Extensions

`include/toy.h` defines C-extension ABI version 1 as well as the embedding API.
An extension exports the fixed `toy_extension_init` entry point, accepts the ABI
version and size-tagged host function table, and returns a static extension
descriptor:

```c
#define TOY_EXTENSION_IMPLEMENTATION
#include "toy.h"

static toy_status twice(toy_state *state) {
    int64_t value = 0;
    if (!toy_get_int(state, 0, &value)) {
        return toy_fail(state, "sample.twice expected an integer");
    }
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "sample.twice failed to pop its input");
    }
    return toy_push_int(state, value * 2);
}

static const toy_native_word words[] = {
    {"twice", twice},
};

static const toy_extension extension = {
    sizeof(toy_extension),
    "sample",
    words,
    sizeof(words) / sizeof(words[0]),
};

TOY_EXTENSION_EXPORT const toy_extension *toy_extension_init(
    uint32_t abi_version, const toy_extension_api *api) {
    if (!toy_extension_bind(abi_version, api)) return NULL;
    return &extension;
}
```

`TOY_EXTENSION_IMPLEMENTATION` emits the small forwarding layer in that C file.
For a multi-file package, define it in exactly one translation unit. The header
is otherwise self-contained: copy `toy.h` beside the source and compile
the shared library without linking Toy or a Toy support library:

```console
clang -shared sample.c -o toy_sample.dll
```

On Linux, add `-fPIC` and name the result `toy_sample.so`. The package directory
also needs an exact `toy.package` manifest:

```ini
name = sample
extension = toy_sample.dll
```

Use the platform suffix in the real manifest. The installed SDK tool creates
both artifacts and supplies the correct public-header path:

```console
toy-c-package vendor/sample vendor/sample/sample.c
```

Use repeatable `--include`, `--lib-dir`, and `--lib` options for additional
C dependencies. The extension must still use a compiler and foreign library
whose architecture and C ABI match the Toy executable.

Import the directory exactly as any source package:

```toy
"../vendor/sample" 's import-as
21 s.twice
```

The loader passes ABI version 1 to the entry point, and both structure sizes
must match exactly. These checks reject stale or mismatched extension binaries
before their function tables are used. Packages must still match the runtime's
target architecture and C ABI. The host table has process lifetime; the library
itself remains loaded until state destruction. Stacks, frames, definitions, and
resource destructors are released before unloading, so destructors implemented
inside the extension remain callable. C extensions execute unrestricted
machine code and must be trusted like any other library loaded into the
process.

## Persistent Values and Collections

`toy_value_retain()` creates an opaque, persistent reference to the value at a
stack depth. The reference keeps that value alive after `toy_pop()` and across
later evaluation. `toy_push_value()` places the value back on its original
state's stack without consuming the reference:

```c
toy_eval(state, "callback.toy", "[ 3 * ]");
toy_value *callback = toy_value_retain(state, 0);
toy_pop(state, 1);

toy_push_int(state, 14);
toy_call_value(state, callback);  /* leaves 42 */
toy_value_release(callback);
```

Values are state-bound. Passing one to `toy_push_value()` or
`toy_call_value()` with another state is an error. Every retained value,
including items returned by the collection accessors, must be released exactly
once and before its state is destroyed. This ordering keeps quotation package
context and native resource destructors valid. A retained value is a reference,
not a serialized copy.

`toy_value_type()` and the `toy_value_get_*()` functions inspect primitive or
resource values without temporarily restoring them to the stack. Returned
string bytes, resource type names, and resource pointers are borrowed until the
value is released or its state is destroyed.

`toy_sequence_size()` and `toy_sequence_get()` traverse vectors, lists, and
strings. A string item is a copied one-byte string, matching Toy's language
semantics. `toy_map_size()` and `toy_map_entry()` traverse maps in insertion
order; each successful entry access returns two new retained values.

Construction remains stack-oriented so hosts and C extensions can reuse the
primitive push API:

```c
toy_push_int(state, 3);
toy_push_int(state, 4);
toy_push_int(state, 5);
toy_make_vector(state, 3);  /* 3 4 5 -- [3 4 5] */

toy_push_string(state, "name", 4);
toy_push_string(state, "Ada", 3);
toy_make_map(state, 1);     /* "name" "Ada" -- {"name" "Ada"} */
```

Both constructors preserve deepest-to-top order and validate all inputs before
consuming them. Map inputs alternate key and value; keys follow Toy's normal
hashability rules. `toy_call_value()` accepts quotations, symbols, and call
values, requires an idle state, and leaves the retained reference reusable.
Execution errors retain their normal non-transactional stack behavior.

## Opaque Foreign Resources

Native code can wrap an owned external handle without exposing pointers or
runtime objects to Toy:

```c
static void close_widget(void *resource, void *userdata) {
    (void)userdata;
    widget_close(resource);
}

widget *value = widget_open();
toy_status status = toy_push_resource(state, "host.widget", value,
                                      close_widget, NULL);
if (status != TOY_OK) widget_close(value);
```

Resource type names use dotted identifier segments and are copied during a
successful push. The pointer must be non-null. Ownership transfers to Toy only
when `toy_push_resource()` succeeds; the destructor may be null for a borrowed
or externally managed handle.

`toy_get_resource()` checks an exact type name and returns a borrowed pointer.
`toy_get_resource_type()` returns the copied, borrowed type name. Neither
pointer should be retained after the resource value is released. Native words
should finish using the borrowed pointer before consuming their inputs with
`toy_pop()`.

Resources use ordinary Toy reference counting. `dup`, vectors, lists, captures,
and other containers retain the same wrapper, and the destructor runs
synchronously exactly once when its final reference is released, including
during `toy_pop()` or state destruction. Errors do not roll back prior stack
effects, so a resource pushed before an error remains owned by the stack until
the host or Toy code removes it. Resource destructors should only release the
external handle and must not re-enter the Toy state.

At the language level, resources compare by wrapper identity, are not valid map
keys or set items, and report `resource` through `type-of` and `resource?`.
Their display form is `<resource:type.name>` and is intentionally opaque rather
than reconstructable source.

The [Raylib](../examples/packages/raylib/) and
[SQLite](../examples/packages/sqlite/) examples apply this resource boundary to
real libraries. Their READMEs contain the dependency-specific build commands
and explain the small policies added by each handwritten adapter. The
[generated binding examples](../examples/packages/bindgen/) show the same
boundary driven by explicit manifests.

## Stack and Ownership

Depth zero addresses the top of the data stack. Getters borrow a value without
popping it, while `toy_pop()` releases removed values. Primitive push functions
create state-owned values. `toy_push_string()` copies its input bytes.

The pointer returned through `toy_get_string()` is a borrowed view. Do not keep
it across stack mutation, further execution, or state destruction.

The public API intentionally does not expose `tf_obj`, reference
counts, collection storage, dictionary entries, or VM frames. Persistent values
and collection accessors keep those layouts opaque.

## Errors and State Reuse

`toy_get_error()` returns the most recent runtime or parser message. Parser
messages include the source name, line, and column. The pointer is owned by the
state and remains valid until another execution, `toy_clear_error()`, or state
destruction.

Native callbacks can use `toy_fail()` to store a diagnostic and return
`TOY_ERROR` in one operation.

Errors unwind the current VM invocation but do not roll back definitions or
data-stack effects that already occurred. The state may be used again after the
host inspects or repairs its stack.

## Runtime Boundaries

- states are not safe for concurrent access;
- C callbacks are synchronous and cannot re-enter the same state;
- `read-key` and `read-line` still use process standard input;
- allocation failure terminates the process;
- the normal filesystem, environment, process, and shell builtins are enabled;
- the runtime uses bounded process-global allocation caches;
- retained values must be released manually before their state is destroyed;
- public construction and traversal cover vectors, maps, and
  vector/list/string sequence access rather than every collection family;
- the C-extension ABI is versioned together with the Toy SDK;
- the optional [dynamic FFI](./ffi.md) handles explicit scalar
  and copied-string signatures.

The focused C regression is `tests/c/test_embed_api.c`; the structured host is
`examples/embedding/values.c`.
