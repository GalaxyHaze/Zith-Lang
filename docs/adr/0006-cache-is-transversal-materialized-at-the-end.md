# The cache is transversal and materialized at the end

Stages accumulate data as they run; the cache is written once, at the end. It
stores per-declaration headers, ZPK residual facts, and the backend artifact,
under a canonical key. At a call site the caller reads only cached premises,
never the callee body.

Modeling the cache as the last pipeline stage (as the stage list suggests) would
bind it to the interior of every stage and break the seam. Writing everything
in a single final pass keeps the stages free of persistence concerns and lets
the driver own when and where the bytes are stored.
