# Backends split at HIR

HIR is the single backend entry point. The VM and the future tiny backend
consume VIR (VM v2 IR), lowered from HIR. The reused LLVM emitters consume HIR
directly. VIR is not LLVM IR.

Reusing the existing LLVM emitters as they are requires that they keep reading
HIR; routing them through VIR would mean rewriting them and would no longer be
reuse. A single shared IR for every backend was rejected: it would force one
backend's semantics onto the other.
