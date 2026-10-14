# Three drivers over one seam

There are three drivers over the same external seam: the CLI driver (one
revision), the language-server driver (persistent `Workspace` plus snapshots),
and the WASM playground driver (compile once, execute from cache). Tests are
consumers of the seam, not a driver.

The playground driver keeps the core target-agnostic, as the existing WASM
playground already does by compiling HIR once and replaying it. One driver per
target was rejected: those drivers would drift apart.
