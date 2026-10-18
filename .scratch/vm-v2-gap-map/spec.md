# VM v2 Parity For Native-Supported Zith--

Status: draft

This specification defines the next host VM v2 parity slice for
Zith-- programs accepted by the native compiler path. The native path is the
comparison reference, while `docs/Zith--.md` remains the language contract.
This draft is based on the reviewed inventory and scope decisions in `map.md`.
It defines a proposed contract only and does not authorize implementation in
this documentation session.

## Problem Statement

The host-native compiler accepts Zith-- programs that the VM v2 path cannot
currently lower. The existing parity test allow-lists seven native-success /
VM-status-5 cases, but does not report the observed failure phase for those
cases. It also does not establish a successful VM/native runtime comparison
for those examples. As a result, maintainers cannot reliably distinguish
unsupported language constructs from runtime behavior differences or
FFI-dependent cases.

## Solution

Extend the VM v2 support contract and its existing host-native-versus-VM
parity seam. The seam must identify whether a VM case failed before HIR
lowering, during VM lowering, or during execution, and must compare observable
results for cases both backends execute. The proposed slice covers
specification-consistent Zith-- behavior in the reviewed scope. State-machine
transfers are included, although their VM outcome still needs paired evidence.
The slice does not claim parity for the entire native compiler or for programs
whose behavior depends on external FFI.

## User Stories

1. As a Zith-- programmer, I want a program accepted by the language contract
   and native compiler to lower through VM v2 when it uses a supported
   in-scope construct, so that I can run that program on the portable runtime.
2. As a Zith-- programmer, I want optional values to preserve their present
   and absent states through construction, extraction, and checks, so that VM
   execution does not confuse a missing value with an uninitialized result.
3. As a Zith-- programmer, I want `must` extraction of an absent optional to
   follow the language's specified failure behavior, so that VM execution
   agrees with the optional contract instead of silently continuing.
4. As a Zith-- programmer, I want enum values and their discriminants to
   behave as specified, so that casts and comparisons produce the same
   observable result on VM v2 and the native path.
5. As a Zith-- programmer, I want tagged- and raw-union operations to preserve
   their specified value and type-check behavior, so that union-based code can
   run through VM v2.
6. As a Zith-- programmer, I want supported `dyn` values and method dispatch
   to call the implementation selected by the dynamic value, so that
   specification-supported dynamic dispatch is usable on VM v2.
7. As a Zith-- programmer, I want bare `opaque` values to support the
   specified erase, check, and extraction operations, so that type-erased
   programs do not depend on a backend-specific execution path. `raw opaque`
   interop remains outside this scope.
8. As a Zith-- programmer, I want pipeline expressions and their current-value
   references to preserve evaluation and argument semantics, so that
   runtime `|>` and `do` programs produce the same result on VM v2 and the
   native path.
9. As a Zith-- programmer, I want state-machine entry and transitions to
   preserve their specified control flow, so that `state`, `dock`, and `jump`
   programs behave the same on VM v2 and the native path.
10. As a VM v2 user, I want observable output and the program exit code to match
   the native path for parity cases that both backends execute, so that
   selecting VM v2 does not change the program's externally visible result.
11. As a VM v2 user, I want traps and other non-success outcomes to remain
    distinguishable from unsupported programs, so that an execution failure
    cannot be mistaken for a lowering limitation.
12. As a compiler maintainer, I want parity failures to identify the stage
    reached and the observed diagnostic or runtime detail, so that an
    allow-list explanation cannot mask the actual failure.
13. As a compiler maintainer, I want a native-success / VM-unsupported result
    to fail the parity test for an in-scope case, so that known VM gaps cannot
    remain permanently green through an allow-list.
14. As a compiler maintainer, I want every intentionally excluded parity case
    to state its exclusion category, so that FFI-dependent cases are not
    confused with missing core language support.
15. As a compiler maintainer, I want native-versus-VM disagreements that
    conflict with `docs/Zith--.md` to be recorded as possible native-path or
    specification discrepancies, so that the native backend is not treated as
    the language specification by default.
16. As a compiler contributor, I want focused VM-only tests to cover the
    semantics of new IR v2 operations, while the parity harness checks
    end-to-end behavior, so that unit-level correctness and backend parity
    remain independently diagnosable.
17. As a standard-library maintainer, I want standard-library parity cases
    that depend on validated VM runtime externs to remain explicitly
    conditional on the FFI scope, so that this work does not silently expand
    the VM FFI contract.
18. As a release maintainer, I want the parity suite to keep native acceptance
    as a prerequisite for a paired case, so that a VM result is not compared
    against a native compile or run failure.

## Implementation Decisions

- The native LLVM execution path is the comparison reference for paired
  programs. The `Zith--` language specification determines whether a behavior
  is correct when the native and VM paths disagree.
- The scope is the active VM v2 path and specification-supported Zith--
  programs accepted by the native path, excluding behavior that requires
  external FFI.
- The proposed gap areas are optional construction and extraction, numeric
  enum values, tagged- and raw-union operations, supported `dyn` dispatch,
  bare `opaque` operations, runtime pipeline HIR, and state-machine transfers.
- Numeric enums preserve current behavior when no numeric type is declared
  and may specify a numeric type. `enum: union` is outside Zith--.
- Optional parity includes the specified absent-value behavior of `must`.
  General runtime-panic terminators are not a separate supported gap area.
- State-machine transfers are in scope, but their VM outcome is unverified in
  the baseline evidence. Do not claim a specific runtime divergence until a
  paired case establishes it.
- `raw opaque`, allocator and `InPlace` contracts, global constants, cleanup,
  comptime pipelines, and the full variadic-slice surface are not acceptance
  requirements for this slice.
- Unsupported HIR or IR v2 operations must not be silently omitted in a way
  that changes stores, returns, control flow, or traps. An unsupported result
  must retain enough stage and detail information to diagnose the case.
- The existing host-native-versus-VM parity harness is the single end-to-end
  test seam. VM-only tests remain appropriate for internal IR v2 semantics,
  but they do not substitute for parity evidence.
- The parity contract does not promise support for the full native compiler
  surface. New cases must be in the documented `Zith--` scope and must not
  rely on external FFI.
- Existing validated VM runtime externs are not expanded or redefined here.
  Standard-library cases that depend on those externs remain conditional
  until their inclusion in a future parity baseline is decided.
- FFI-dependent cases may remain in the inventory for diagnosis, but they
  cannot establish acceptance or expand the VM FFI contract.
- This draft does not choose implementation order, estimate effort, or define
  a new FFI mode-selection interface.

## Testing Decisions

- Test externally observable program behavior through the existing parity
  harness, not private lowerer data structures.
- For each paired case, require native compilation and execution to succeed
  before interpreting the VM result.
- Record the VM stage reached: HIR lowering, VM lowering, or VM execution.
  Preserve the observed VM detail for failure reporting instead of printing
  only a static allow-list reason.
- For successful paired executions, compare stdout bytes and program exit
  code. For non-success outcomes, compare compatible outcome categories and
  keep trap, out-of-memory, and unsupported statuses distinct. Do not require
  identical diagnostic wording across backends.
- Add focused, deterministic parity cases for each in-scope gap area using
  minimal programs whose expected behavior is covered by `docs/Zith--.md` or
  existing native tests.
- Treat a native-success / VM-unsupported result as a test failure for an
  in-scope case. An excluded case may remain documented only with an explicit
  scope category and a verified observed result.
- Keep tests for internal typed IR v2 operations in the existing VM unit-test
  surface. Use the parity harness to establish behavior across the native and
  VM execution paths.
- Prior art is the existing `test-stdlib-parity` harness for paired execution
  and `test-vm-v2` for VM-specific execution semantics.

## Out of Scope

- Implementing code, tests, or behavior changes in this session.
- External FFI expansion, FFI auditing, or selecting between external-FFI and
  sandbox modes.
- Treating external-FFI-dependent programs as core VM parity requirements.
- WASM/browser packaging or browser runtime parity.
- Performance benchmarking or optimization.
- Claiming runtime mismatches for cases that have not successfully executed
  through both paths.
- Treating general runtime-panic terminators, global constants, cleanup, or
  other excluded candidates as requirements.

## Further Notes

- The inventory baseline is commit `0c1ae39f` (`chore: checkpoint compiler
  and stdlib updates`).
- At that baseline, the parity harness reports seven native-success /
  VM-status-5 cases. Their exact failure phases are not established by the
  passing test output, and no runtime output or exit-code mismatch is
  confirmed.
- The current parity documentation describes three console-literal cases as
  passing even though the harness allow-lists status 5 for them. Correcting
  that documentation is a separate doc change, not evidence that VM behavior
  already passes.
- State-machine transfers are in the proposed scope, but their VM execution
  outcome was not established by the baseline research.
- The absent-value behavior of `must` is in scope as optional semantics. This
  does not bring general runtime-panic terminators into scope.
- The map and supporting evidence remain local under `.scratch/vm-v2-gap-map/`.
  This draft is not published as a GitHub issue, in keeping with the decision
  to keep notes local for this phase.
