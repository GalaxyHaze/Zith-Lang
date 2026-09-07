# Zith 0.7.0 Zith-- Planning

> Updated for the current product split: `Zith--` is the compiler subset built
> and maintained by `main`; comptime and the full-Zith capability base are not
> `Zith--` targets.

## Scope

This directory is the planning home for the current `Zith--` iteration:

- `drop` as deterministic scope cleanup, extending the already-shipped
  `defer` path. See `docs/plans/defer-drop.md`.
- Monolith splits as infrastructure work. See `docs/plans/monolith-splits.md`.
- Platform-specific imports (`foo.<arch>.<os>.zith`) as module system work.
  See `docs/plans/platform-imports.md`.
- Feature hardening and debt closure from `docs/implementation-debt.md`.

Traits, generics, conformance and generic constraints are already implemented
in `Zith--` and documented under `docs/impl-status.md`.

## Related Plan Locations

- `docs/plans/defer-drop.md`: `drop` roadmap, the recommended next feature.
- `docs/plans/monolith-splits.md`: source reorganization priority and contract.
- `docs/plans/archive/0.7.0-zith/`: the older 0.7.0 full-Zith planning for
  comptime, introspection, type construction, capabilities and NRA steps.
- `docs/plans/standalone-c-toolchain.md` and `docs/plans/tiny-c-backend.md`:
  native toolchain roadmap.

The older full-Zith plan is intentionally archived. Comptime evaluation,
reflection, type mutation and activated capabilities remain Zith (full-spec)
features, not `Zith--` candidates.

## Retired Steps

- `docs/plans/archive/parse-input-cast.old.md`: `ParseInput` and
  `InputLine.cast<T>` are shipped; no active implementation steps remain.

Active plans must not repeat implemented wave work. Status lives in
`docs/impl-status.md`; roadmap statuses and debt annotations point back to it.
