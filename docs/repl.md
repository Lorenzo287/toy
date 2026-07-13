# Toy REPL

Running `toy` without a file starts an interactive REPL. The REPL is the best
place to learn Toy because the language can document itself: inspect the word
catalog, ask for a word's docs, try a small expression, and immediately see the
stack.

```powershell
cmake --build build
.\build\toy.exe
```

## Interactive Discovery

Useful first commands:

```text
toy> help
toy> 'map doc print
toy> 'map see print
toy> "priority" search-words
toy> words
```

The catalog groups words by primary concept, not by implementation file. Shared
words such as `push-back` or `pairs` are listed under their main language idea
even when they support more than one representation.

## Interactive Development Loop

Type `trace` to toggle the automatic stack display. Type `hints` to toggle
syntax-aware input hints.

Definitions persist for the whole REPL session.

The data stack also persists. By default, the REPL prints the stack after each
evaluation. Use `.s` or `.S` when you want to inspect the stack explicitly 
(might be useful when trace toggle is off), `empty` to clear it.

## Scope of Captures

Captures created with `| ... |` live only while the word or quotation that
created them is running. They do not become persistent REPL variables:

```text
toy> 5 |a| $a print
5
toy> 5 |a|
toy> $a print
runtime error: undefined variable '$a'
```

Use `def` for persistent names.

## Runtime Diagnostics

Unhandled runtime and program errors report the source location and live data
stack. Errors inside nested words or quotations also include the Toy call
chain:

```text
runtime error: '+' expected number at stack depth 0, found string
  at example.toy:2:15
  stack <2> 10 "oops"
  in inner at example.toy:2:15
  in outer at example.toy:6:5
  in <program> at example.toy:9:1
```

Stack values use the same bottom-to-top display order as the REPL. Reports show
at most the top eight stack values and eight Toy frames, with omitted entries
summarized. Errors handled by `try` do not produce a report.

## tdb Instruction Debugger

Type `tdb` to toggle the terminal debugger for subsequent REPL input. The VM
pauses before each Toy instruction and opens a distinct `(tdb)` prompt:

```text
toy> 'inc [ 1 + ] def
toy> tdb
toy[tdb]> 3 inc
paused: <repl>:1:1 in <program> (pc 0/2, depth 1)
  => 3
(tdb) step
paused: <repl>:1:3 in <program> (pc 1/2, depth 1)
  => inc
(tdb) step
paused: <repl>:1:8 in inc (pc 0/2, depth 2)
  => 1
```

The three prompts make debugger state explicit:

- `toy>` is the normal REPL;
- `toy[tdb]>` means tdb is enabled and waiting for an evaluation;
- `(tdb)` means an evaluation is currently paused.

Debugger commands are:

- `Enter`, `s`, or `step`: execute the displayed instruction and pause again;
- `c` or `continue`: run the rest of the current REPL input;
- `p` or `stack`: display the data stack without advancing;
- `bt` or `backtrace`: display program and native-continuation frames;
- `off`: disable tdb and continue;
- `q` or `abort`: unwind the current input as an interruption;
- `h`, `help`, or `?`: show the command summary.

Native words are single instructions. If a native combinator schedules Toy
code, stepping enters the scheduled quotation on the next pause. `continue`
also applies to nested file execution started by the current input. The
debugger remains enabled for the next REPL input unless `off` or `tdb` turns it
off. Standalone REPL controls such as `tdb`, `trace`, `hints`, and `help` are
handled outside VM evaluation, so toggling tdb does not stop to debug the
toggle itself.

The pause line describes only the current frame. Its `depth` field tells you
how many VM frames are active. `bt` is most useful when depth is greater than
one: it shows the complete chain of user words, quotations, native
continuations, and the root program.

To debug a file or an evaluated source string directly from PowerShell:

```powershell
.\build\toy.exe --tdb program.toy
.\build\toy.exe --tdb --eval "1 2 + print"
```

The process exits when file or `--eval` execution finishes. Running
`.\build\toy.exe --tdb` without a file starts the REPL with tdb already armed.
Inside an existing REPL, `tdb` followed by stepping into a `load` call also
debugs the loaded file in the same context.

tdb is separate from the CLI `--parsed` option, which prints the parsed program
and final stack without interactive stepping.

## Multiline Input

The REPL keeps reading until the current input is structurally complete.

This applies to:

- quotations/vectors: `[ ... ]`
- lists: `( ... )`
- maps/sets: `{ ... }` / `#{ ... }`
- capture lists: `| ... |`
- strings: `" ... "`
- block comments: `/* ... */`

## Editing, History, Completion

The REPL uses `linenoise` for:

- line editing;
- command history;
- tab completion for known words;
- syntax-aware hints (`hints`);
- automatic stack display (`trace`);
- categorized help (`help`).

Stack display uses unambiguous value forms. Deques and priority queues are
shown as `deque[...]` and `pqueue[...]` display forms so they read as single
values. `repr` and `.S` still use reconstructable source forms.

## Exiting and Interrupting

To exit:

- Unix-like systems / WSL: `Ctrl-D`
- Windows console: `Ctrl-Z`
- at the prompt: press `Ctrl-C` twice in a row
- from Toy code: `exit`

Use `Ctrl-C` once to interrupt a running program such as an infinite loop. The
REPL reports this as an interrupt rather than as a generic runtime error. When
no program is running, the first `Ctrl-C` clears the current input and prints a
reminder that pressing `Ctrl-C` again exits.
