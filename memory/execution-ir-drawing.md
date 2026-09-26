# Execution IR Lifecycle Status

The older execution IR contract from
`docs/adr/0018-execution-ir-interpreter-contract.md` was superseded by the
typed VM v2 contract in `docs/adr/0020-vm-v2-typed-execution-ir.md` and
`docs/adr/0021-vm-v2-portable-execution.md`.

The v1 implementation (`src/ir/exec-ir.hpp`, `src/ir/hir-to-ir.*`, and
`src/interp/ir-vm.*`) is archived under `archive/execution-ir-v1/`. It is no
longer compiled into `zithcLib`, is not part of
`tests/test-abi-execution.cpp`, and must not be brought back into active
sources without a new drawing.

The live VM path is `src/vm/typed-ir.hpp`, `src/vm/hir-to-vm.cpp`, and
`src/vm/vm-v2.cpp`. The WASM runtime exports lower flat HIR into this typed
VM when `ZITH_IS_WASM` is enabled; `--interpreted` remains the HIR
interpreter path.
