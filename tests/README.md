# Tests

`zithc test` recursively type-checks every `.zith` file under the configured
`test_dir`; it does not execute test functions or verify runtime behavior.
Behavioral smoke tests in `tests/run.zith` run through the VM. The HIR data
contract test in `tests/hir-contract.zith` and canonical type-table test in
`tests/canonical-types.zith`, plus the ZIRL envelope test in
`tests/zirl-artifact.zith` and typed HIR-artifact composition test in
`tests/zirl-hir-artifact.zith`, run through native codegen because current VM
v2 lowering does not support the size of the HIR/cache aggregates those
fixtures use.

Run both checks with:

```sh
ZITHC=/path/to/zithc sh tests/run.sh
```

The runner copies `ZithProject.toml`, `src/`, and `tests/` into a new temporary
project, runs `zithc test` there, then runs the VM smoke test and native
HIR/cache contract runners. Both `src/` and `tests/` are supplied as include
directories so the domain test modules can import their shared fixtures.
Generated object files and caches stay inside the temporary project and are
removed when the runner exits. `ZITHC` must name one compiler executable that
supports the VM v2 command-line option and native codegen.

Add VM behavior tests as functions called by `tests/run.zith`. Add native HIR
contract tests to the matching `tests/hir-contract-*.zith` module and call them
from `tests/hir-contract.zith`; add typed artifact tests to the matching
`tests/zirl-hir-artifact-*-tests.zith` module and call runner-level tests from
`tests/zirl-hir-artifact.zith`. Each test returns zero on success and a
nonzero status on failure; the runner returns the first failure status. Keep
the `.zith` tests valid as standalone type-check inputs, because `zithc test`
compiles every test file independently.
