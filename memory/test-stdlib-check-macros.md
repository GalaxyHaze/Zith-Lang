# Std Test Check Macros

`pub macro` bodies expand in the caller's scope. A macro must not rely on
private imports from its defining module. Put behavior that needs those imports
behind a public helper in that module.

The VM v2 backend supports formatting through helpers that receive
`dyn Formatable`, but currently cannot lower a resolved generic call such as
`T.format(...)` inside a generic helper. For formatted macro diagnostics, pass
values to public helpers with `dyn Formatable` parameters.

`checkEachOneOf(actuals, expectedValues)` checks each actual element for
membership in the allowed values and accumulates one check per element. Its
failure output includes the element index, actual value, and each allowed
value. Values used by this helper must implement `Formatable` so failures can
include those details.
