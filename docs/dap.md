# Toy Debug Adapter

`toy-dap` is a standalone [Debug Adapter Protocol](https://microsoft.github.io/debug-adapter-protocol/)
adapter for editor debugger clients such as
[nvim-dap](https://github.com/mfussenegger/nvim-dap). It launches the Toy
interpreter as a child process and translates its private machine debugger
stream into standard DAP messages.

## Current Features

- launch a Toy source file with arguments and a working directory;
- stop on entry or continue directly to a source breakpoint;
- replace all breakpoints for the launched source;
- continue, step into, step over, and step out;
- inspect Toy VM frames and source locations;
- inspect the data stack and each frame's captures as DAP scopes;
- forward Toy stdout and diagnostics to the debug console;
- report process exit and debugger termination.

This first slice supports breakpoints only in the launched source. Stack and
capture values are displayed in source form but collections are not expandable
yet. Conditional breakpoints, log points, watches, evaluation, attach, restart,
and asynchronous pause are not currently advertised.

## Build

Build the Toy runtime first:

```powershell
.\nob.exe build
```

Then build the adapter from `tools/toy-lsp`:

```powershell
go build -o toy-dap.exe ./cmd/toy-dap
```

The adapter uses standard DAP framing on stdin/stdout. It normally runs under
an editor rather than directly in a terminal.

## Neovim

Install `nvim-dap`, then add a configuration like this to `init.lua`:

```lua
local dap = require("dap")

dap.adapters.toy = {
    type = "executable",
    command = "C:/path/to/toy/tools/toy-lsp/toy-dap.exe",
    options = { detached = false }, -- useful on Windows
}

dap.configurations.toy = {
    {
        type = "toy",
        request = "launch",
        name = "Debug current Toy file",
        program = "${file}",
        cwd = "${workspaceFolder}",
        runtimeExecutable = "C:/path/to/toy/build/clang/release/toy.exe",
        stopOnEntry = true,
        args = {},
    },
}
```

With a Toy buffer open, normal `nvim-dap` actions set breakpoints, launch the
session, continue, step, and open its REPL or widgets. Setting `stopOnEntry` to
`false` starts execution immediately and stops at the first configured
breakpoint.

The repository's Neovim installation scripts build `toy-dap` and print a
configuration using the local repository and installation paths.

## Runtime Boundary

`toy --debug-protocol program.toy` is an internal transport used by the
adapter. Its output is not a stable public protocol and should not be parsed by
editor plugins directly; DAP is the supported integration boundary.
