# Tests

`zithc test` recursively type-checks every `.zith` file under the configured
`test_dir`; it does not execute test functions or verify runtime behavior.
Behavioral tests are called by `tests/run.zith` and run through the VM.

Run both checks with:

```sh
ZITHC=/path/to/zithc sh tests/run.sh
```

The runner copies `ZithProject.toml`, `src/`, and `tests/` into a new temporary
project, runs `zithc test` there, and then runs `tests/run.zith` with
`--virtual-machine --no-cache`. Generated object files and caches stay inside
the temporary project and are removed when the runner exits. `ZITHC` must name
one compiler executable that supports the VM v2 command-line option.

Add behavior tests as functions called by `tests/run.zith`. Each test returns
zero on success and a nonzero status on failure; `main` returns the first
failure status. Keep the `.zith` tests valid as standalone type-check inputs,
because `zithc test` compiles every test file independently.
