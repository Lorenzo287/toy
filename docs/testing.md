# Testing Toy

Toy keeps automated regressions under `tests/toy/` and user-facing programs
under `examples/`. C API regressions live under `tests/c/`, while
buildable hosts live under `examples/embedding/`. Examples may be smoke-tested
separately, but their source should teach or demonstrate the language rather
than act as the correctness suite.

## Run the Suite

Bootstrap Nob once and run its isolated test harness directly:

```powershell
clang -std=c11 nob.c -o nob.exe
.\nob.exe test
.\nob.exe test --filter native_loader
```

The default suite covers Toy cases, directory packages, debug-protocol
transport, embedding/debugger C tests, real loadable packages, `core:ffi`, and
generated bindings. It
builds incrementally using the selected compiler and mode. Each Toy case runs
in a fresh process with a
timeout and an isolated working directory under the build tree. This prevents
definitions, stack values, environment changes, and temporary files from
leaking between tests.

`--filter` selects tests whose names contain the given text:

```powershell
.\nob.exe test --filter native_loader
.\nob.exe test --filter bindgen
.\nob.exe test --filter package
```

## File Conventions

The flat `tests/toy/` directory uses filename prefixes to declare how each case
is evaluated:

- `test_*.toy` must exit successfully. These files should use the test-only
  assertions from `testlib.toy` and remain silent on success where practical.
- `fail_*.toy` must exit with status 1. An adjacent `.stderr` file contains a
  stable fragment that must appear in the diagnostic.
- `output_*.toy` must exit successfully and match an adjacent `.stdout` file
  exactly after line-ending normalization.
- `manual_*.toy` covers interactive or visual behavior and is not registered
  with the automated suite.
- `testlib.toy` defines shared assertions and is copied beside each case in its
  isolated working directory.

Directory-package integration fixtures live under `tests/packages/` and cover
multi-file definitions, aliases, core imports, visibility, cycles, declaration
rules, and executable-package validation.

For example:

```text
tests/toy/fail_runtime_divide_by_zero.toy
tests/toy/fail_runtime_divide_by_zero.stderr
tests/toy/output_repr.toy
tests/toy/output_repr.stdout
```

Use value assertions for language semantics and stack effects. Use expected
failure cases for parser, CLI, diagnostic, and unhandled-error behavior. Golden
output should be limited to words whose output is the public contract.

## Manual and Integration Behavior

Terminal input, cursor control, ANSI behavior, and intentionally slow examples
belong in `manual_*.toy`. Filesystem, environment, and process regressions may
remain automated when they are deterministic and portable; the isolated test
working directory is available for their temporary files.

Do not combine cases by evaluating every test in one VM. Tests may define the
same words, and a shared stack or dictionary would make results order-dependent.
