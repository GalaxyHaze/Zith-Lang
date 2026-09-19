# Sequenta Orientation: v2-slice

Next ready task: `v2-slice::vmV2Shape`

## signature

`vmV2Shape() -> void`

## dependencies

None. Existing host symbols:

- `v2-host::typed-ir` -> `src/vm/typed-ir.hpp`
- `v2-host::vm-v2` -> `src/vm/vm-v2.*`
- `v2-host::linear-memory` -> `src/vm/vm-memory.*`
- `v2-host::runtime-ffi` -> future `src/vm/ffi.*`

## contract promise

`VMV2-01`: IR v2 is linear and typed. The first slice must keep the VM v2
harness separate from execution IR v1 and preserve `src/ir`/`src/interp`.

## implementation guidance

- Confirm the opcodes/registers already present in `src/vm/typed-ir.hpp`.
- Extend the typed register model and opcodes needed for memory, calls, and
  control without introducing SSA or complex basic-block lowering.
- Keep instructions as fixed-size rows and add explicit traps for invalid
  registers/operations.
- Do not promote `src/vm/*` into `zithcLib` yet; keep the harness target
  `tests/test-vm-v2.cpp` compiling the slice directly.

## acceptance tests

Run `ctest --test-dir build -R 'test-vm-v2' --output-on-failure`. The current
harness must stay green while new typed-register/opcode tests are added to
`tests/test-vm-v2.cpp`.

## fact check

Existing VM v2 API lives in:

- `src/vm/typed-ir.hpp`
- `src/vm/vm-v2.hpp`
- `src/vm/vm-v2.cpp`
- `src/vm/vm-memory.hpp`
- `src/vm/vm-memory.cpp`

Do not use `src/ir` or `src/interp` in this task; they are execution IR v1.
