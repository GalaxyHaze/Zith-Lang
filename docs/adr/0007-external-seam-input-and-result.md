# The external seam takes a revision and returns diagnostics and artifacts

`input` is a small immutable unit: the revision text, the module identity, the
resolution root, and immutable options. `result` carries structured
diagnostics, artifacts (HIR/VIR/object), and residual facts. The driver parses
the command line and project file and passes options inside the input; the core
never reads the CLI or configuration files.

The core is a function of one revision, so its input is the revision plus the
minimum needed to resolve imports, not a file path or the whole project. Keeping
options in the driver is what keeps the core pure: dragging options, paths, and
linking into it is exactly what made the previous `CompilationSession` heavy.
