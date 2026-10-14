# The source tree is laid out by area

The source tree mirrors the modules of the general design:

```text
src/
  context/     shared arenas, interner, diagnostic collector
  frontend/    Lex, Scan, Import, Resolve
  sema/        semantic analysis
  proof/       ZPK: NIA, RRA, MRA, NRA
  ir/          HIR
  backends/
    vir/       HIR -> VIR
    vm/        VIR execution
    tiny/      VIR -> C object (future)
    llvm/      HIR -> LLVM IR (reused emitters)
  drivers/
    cli/
    lsp/
    wasm/
  cache/
  cinterop/    C binder and C ABI surface
```

One folder per module keeps locality and makes each seam visible in the tree.
The tree is cheap to change while there is no code, but fixing it now gives the
per-stage sessions a stable skeleton to land in.
