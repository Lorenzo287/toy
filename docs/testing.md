# Testing Toy

Toy keeps automated regressions under `toy/tests/` and user-facing programs
under `toy/examples/`. Examples may be smoke-tested separately, but their
source should teach or demonstrate the language rather than act as the
correctness suite.

## Run the Suite

Configure and build with testing enabled, then run CTest:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`BUILD_TESTING` defaults to `ON`. Each Toy case runs in a fresh process with a
timeout and an isolated working directory under the build tree. This prevents
definitions, stack values, environment changes, and temporary files from
leaking between tests.

Use CTest labels to select a class of case:

```powershell
ctest --test-dir build -C Release -L positive --output-on-failure
ctest --test-dir build -C Release -L negative --output-on-failure
ctest --test-dir build -C Release -L output --output-on-failure
```

## File Conventions

The flat `toy/tests/` directory uses filename prefixes to declare how each case
is evaluated:

- `test_*.toy` must exit successfully. These files should use the test-only
  assertions from `testlib.toy` and remain silent on success where practical.
- `fail_*.toy` must exit with status 1. An adjacent `.stderr` file contains a
  stable fragment that must appear in the diagnostic.
- `output_*.toy` must exit successfully and match an adjacent `.stdout` file
  exactly after line-ending normalization.
- `manual_*.toy` covers interactive or visual behavior and is not registered
  with CTest.
- `testlib.toy` defines shared assertions and is copied beside each case in its
  isolated working directory.

For example:

```text
toy/tests/fail_runtime_divide_by_zero.toy
toy/tests/fail_runtime_divide_by_zero.stderr
toy/tests/output_repr.toy
toy/tests/output_repr.stdout
```

Use value assertions for language semantics and stack effects. Use expected
failure cases for lexer, CLI, diagnostic, and unhandled-error behavior. Golden
output should be limited to words whose output is the public contract.

## Manual and Integration Behavior

Terminal input, cursor control, ANSI behavior, and intentionally slow examples
belong in `manual_*.toy`. Filesystem, environment, and process regressions may
remain automated when they are deterministic and portable; the isolated test
working directory is available for their temporary files.

Do not combine cases by loading every test into one VM. Tests may define the
same words, and a shared stack or dictionary would make results order-dependent.
