# Embedding Toy in C

Toy builds a static `toy_runtime` library alongside the `toy` command-line
executable. The experimental version-zero API in `include/toy.h` lets a C host
create an interpreter state, evaluate source, call Toy words, inspect primitive
stack values, and register synchronous native words.

The API is intentionally small and does not yet promise source or binary
compatibility between releases.

## Minimal Host

```c
#include <inttypes.h>
#include <stdio.h>
#include "toy.h"

static toy_status host_double(toy_state *state) {
    int64_t value;
    if (!toy_get_int(state, 0, &value)) {
        return toy_error(state, "host/double expected an integer");
    }
    if (!toy_pop(state, 1)) {
        return toy_error(state, "host/double failed to pop its input");
    }
    return toy_push_int(state, value * 2);
}

int main(void) {
    toy_state *state = toy_state_new();
    if (!state) return 1;

    if (toy_register_native(state, "host/double", host_double) != TOY_OK ||
        toy_eval(state, "<example>", "21 host/double") != TOY_OK) {
        fprintf(stderr, "%s\n", toy_last_error(state));
        toy_state_free(state);
        return 1;
    }

    int64_t result;
    if (toy_get_int(state, 0, &result)) {
        printf("result: %" PRId64 "\n", result);
    }

    toy_state_free(state);
    return 0;
}
```

When Toy is included as a CMake subproject, link the host against
`toy_runtime`:

```cmake
add_subdirectory(path/to/toy)
target_link_libraries(my_host PRIVATE toy::runtime)
```

Pass `-DBUILD_TESTING=OFF` when the host does not need Toy's test targets.
There are no installation or package-discovery rules yet.

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

## Stack and Ownership

Depth zero addresses the top of the data stack. Getters borrow a value without
popping it, while `toy_pop()` releases removed values. Primitive push functions
create state-owned values. `toy_push_string()` copies its input bytes.

The pointer returned through `toy_get_string()` is a borrowed view. Do not keep
it across stack mutation, further execution, or state destruction.

The version-zero public API intentionally does not expose `tf_obj`, reference
counts, collection storage, dictionary entries, or VM frames.

## Errors and State Reuse

`toy_last_error()` returns the most recent plain runtime message. The pointer is
owned by the state and remains valid until another execution, `toy_clear_error()`,
or state destruction. Parser failures currently store the generic message
`source parsing failed` while the detailed lexer diagnostic is written to
standard error.

Errors unwind the current VM invocation but do not roll back definitions or
data-stack effects that already occurred. The state may be used again after the
host inspects or repairs its stack.

## Current Limitations

- states are not safe for concurrent access;
- native callbacks are synchronous and cannot re-enter the same state;
- output and diagnostics still use the process standard streams;
- allocation failure terminates the process;
- the normal filesystem, environment, process, and shell builtins are enabled;
- the runtime uses bounded process-global allocation caches;
- only primitive public stack access is available;
- there is no shared-library ABI, native module loader, foreign resource type,
  or dynamic FFI yet.

A complete bidirectional host example lives in `examples/c/embed.c`. Build and
run it from the repository root with:

```powershell
cmake --build build --target toy_embed_example
.\build\toy_embed_example.exe
```

The focused C regression is `tests/c/test_embed_api.c`.
