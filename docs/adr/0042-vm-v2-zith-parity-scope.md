# VM v2 Zith-- Parity Scope

Status: accepted (scope direction; implementation pending)

Extend the host VM v2 parity target to native-accepted Zith-- behavior that
does not depend on external FFI: optionals, numeric enums with existing
behavior and an optional explicit numeric type, tagged and raw unions,
supported `dyn` dispatch, bare `opaque`, runtime pipelines, and state-machine
transfers. Optional parity includes the specified absent-value behavior of
`must`, but general runtime-panic terminators remain out of scope. State
machine VM behavior still requires paired evidence. `enum: union`, `raw
opaque`, allocator/`InPlace` contracts, global constants, cleanup, comptime
pipelines, and FFI mode selection or expansion remain outside this scope.

This decision supersedes only the future-scope statements for state machines,
`dyn`, and `opaque` in [ADR-0021](0021-vm-v2-portable-execution.md). Its other
promises remain unchanged. The parity specification is still a draft, and this
ADR does not authorize implementation in the documentation phase.
