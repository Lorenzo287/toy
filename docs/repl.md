# Toy REPL

Toy includes an interactive REPL when started without a filename.

Type `help` to display the current native words grouped by category. REPL-only
commands appear in their own section, and definitions created during the
session appear under `User words`. The catalog is formatted to the current
terminal width.

## Starting It

```powershell
cmake --build build
.\build\toy.exe
```

On Unix-like systems or WSL:

```bash
cmake --build build
./build/toy
```

## What Persists Between Entries

The REPL keeps one interpreter context alive across entries. This means:

- user-defined words persist
- the data stack persists
- the global dictionary persists

Example:

```toy
'sq [ |n| $n $n * ] def
5 sq print
```

## What Does Not Persist

Variable captures created with `| ... |` are local to the current execution,
not to the whole REPL session.

This works:

```toy
5 |a| $a print
```

This does not:

```toy
5 |a|
$a print
```

The reason is that captured variables live in execution frames and are released
when that top-level REPL command finishes.

## Multiline Input

The REPL keeps reading until the current input is structurally complete.

This applies to:

- vectors/quotations: `[ ... ]`
- lists: `( ... )`
- maps/sets: `{ ... }` / `#{ ... }`
- capture lists: `| ... |`
- strings: `" ... "`
- block comments: `/* ... */`

Example:

```toy
'sq [
|n|
$n $n *
] def
```

## Editing, History, and Completion

The REPL uses vendored `linenoise`:

- in-line editing
- command history
- tab completion for known words
- syntax-aware hints (type `hints` to toggle)
- automatic stack display after successful input (type `trace` to toggle)
- categorized word catalog (type `help`)

The catalog groups words by their primary language concept. Shared endpoint
operations are listed together even when they support several collection
representations.

## Colors and Status

The REPL uses colored output to distinguish the main categories:

- prompt: light blue
- success with stack display on: green `<n>` followed by unambiguous display
  forms; deques and priority queues are tagged as `deque[...]` and
  `pqueue[...]`
- success with stack display off: green `ok`
- parsing/runtime errors: red labels
- interrupt: yellow `interrupt: ...`
- contextual/fallback messages: dimmer status text

## Exiting

- Unix-like systems / WSL: `Ctrl-D`
- Windows console: `Ctrl-Z`
- At the prompt: press `Ctrl-C` twice in a row
- Portable explicit exit: `exit`

## Interrupting Execution

Use `Ctrl-C` to interrupt a running program, for example an infinite loop.

Interrupts are reported as a dedicated outcome, not as a generic runtime
failure. In the REPL this should produce a single interrupt message rather than
secondary errors from words such as `while`.

When no program is running, the first `Ctrl-C` clears the current REPL input and
prints a reminder that pressing `Ctrl-C` again exits the REPL.
