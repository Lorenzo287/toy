# Embedding Toy in C

Toy builds a static `toy_runtime` library alongside the `toy` command-line
executable. The experimental version-zero API in `include/toy.h` lets a C host
create an interpreter state, evaluate source, call Toy words, inspect primitive
or opaque resource stack values, and register synchronous native words or
modules.

The API is intentionally small and does not yet promise source or binary
compatibility between releases.

## Buildable Example

[`examples/c/embed.c`](../examples/c/embed.c) demonstrates both directions of
the boundary: Toy calls a registered C word, then C calls a Toy-defined word.
[`examples/c/embed_callbacks.c`](../examples/c/embed_callbacks.c) captures
binary-safe Toy output, redirects diagnostics separately, and inspects a
detailed parser error. Build and run either example from the repository root:

```powershell
cmake --build build --target toy_embed_example
.\build\toy_embed_example.exe

cmake --build build --target toy_embed_callbacks_example
.\build\toy_embed_callbacks_example.exe
```

When Toy is included as a CMake subproject, link the host against
`toy::runtime`:

```cmake
add_subdirectory(path/to/toy)
target_link_libraries(my_host PRIVATE toy::runtime)
```

Pass `-DBUILD_TESTING=OFF` when the host does not need Toy's test targets.
There are no installation or package-discovery rules yet.

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
[`examples/c/embed_callbacks.c`](../examples/c/embed_callbacks.c).

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

Both entry points require an idle state. A native word must not recursively call
`toy_eval()` or `toy_call()` on the state that is currently executing. Native
continuations remain an internal VM facility in API version zero.

The main statuses are:

- `TOY_OK`: execution completed;
- `TOY_ERROR`: parsing, execution, or native code failed;
- `TOY_INTERRUPTED`: `toy_interrupt()` requested an unwind;
- `TOY_EXIT_REQUESTED`: Toy executed `exit`; the host decides whether its
  process should terminate.

## Native Modules

A descriptor groups local C word names under one Toy module:

```c
static const toy_native_word host_words[] = {
    {"log", host_log},
    {"double", host_double},
};

static const toy_native_module host_module = {
    "host",
    host_words,
    sizeof(host_words) / sizeof(host_words[0]),
};

toy_register_module(state, &host_module);
```

Module descriptor names may contain dotted namespace segments; word entries are
local names and therefore cannot contain dots.

Registration copies the names, validates the entire descriptor before making
changes, exports every word, and marks the module loaded. Toy can therefore use
the ordinary module vocabulary without a `host.toy` file:

```toy
"host" require
21 host.double

"host" 'h require-as
"ready" h.log
```

Module names and namespace ownership cannot collide with source modules or
previously registered qualified words. The callback functions themselves must
remain valid for the lifetime of the state. `toy_register_word()` remains
available for standalone words that do not need module identity.

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

Resource type names follow module-name syntax and are copied during a
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

## Raylib Proof

The optional `bindings/raylib/` adapter is the first real native module built on
this API. Its host only creates a state, registers `raylib`, and loads
`examples/toy/raylib.toy`; the window loop, close predicate, frame boundaries,
and drawing calls are ordinary Toy code.

Configure with `-DTOY_BUILD_RAYLIB=ON` after installing Raylib, then build and
run `toy_raylib_example`. The module currently provides:

- window lifecycle: `init-window`, `close-window`,
  `window-should-close?`, and `set-target-fps`;
- frames and drawing: `begin-drawing`, `end-drawing`, `clear-background`,
  `draw-circle`, `draw-rectangle`, `draw-text`, and `draw-texture`;
- textures: `load-texture`, returning an owned `raylib.texture` resource;
- values and queries: `rgba`, `mouse-x`, `mouse-y`, and `frame-time`.

`rgba` converts four byte integers to a packed Toy integer accepted by the
drawing words. `draw-texture` consumes its texture input like an ordinary word;
use `dup` to retain it across frames, and release every remaining reference with
`drop` before `close-window` so the destructor can unload the GPU texture while
the window context is still active:

```toy
"sprite.png" rl.load-texture
dup 40 60 255 255 255 255 rl.rgba rl.draw-texture
drop
rl.close-window
```

## Stack and Ownership

Depth zero addresses the top of the data stack. Getters borrow a value without
popping it, while `toy_pop()` releases removed values. Primitive push functions
create state-owned values. `toy_push_string()` copies its input bytes.

The pointer returned through `toy_get_string()` is a borrowed view. Do not keep
it across stack mutation, further execution, or state destruction.

The version-zero public API intentionally does not expose `tf_obj`, reference
counts, collection storage, dictionary entries, or VM frames. Collections
remain opaque even though they can retain public resource values internally.

## Errors and State Reuse

`toy_get_error()` returns the most recent runtime or parser message. Parser
messages include the source name, line, and column. The pointer is owned by the
state and remains valid until another execution, `toy_clear_error()`, or state
destruction.

Native callbacks can use `toy_set_error()` to store a diagnostic and return
`TOY_ERROR` in one operation.

Errors unwind the current VM invocation but do not roll back definitions or
data-stack effects that already occurred. The state may be used again after the
host inspects or repairs its stack.

## Current Limitations

- states are not safe for concurrent access;
- native callbacks are synchronous and cannot re-enter the same state;
- `read-key` and `read-line` still use process standard input;
- allocation failure terminates the process;
- the normal filesystem, environment, process, and shell builtins are enabled;
- the runtime uses bounded process-global allocation caches;
- collection values still have no public construction or traversal API;
- there is no shared-library ABI, loader, or dynamic FFI yet.

The focused C regression is `tests/c/test_embed_api.c`.
