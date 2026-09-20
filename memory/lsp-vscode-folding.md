# LSP / VS Code Folding Workflow

Zith's LSP already owns `textDocument/foldingRange`, so VS Code folding is a
thin integration point: the extension starts the bundled `server/zith-lsp`
with `vscode-languageclient`, the server advertises `foldingRangeProvider:
true`, and the editor maps brace ranges directly to collapse controls.
The feature does not need an explicit extension-side folding provider.

## Current Behavior

- `foldingRangesForContent` lives in
  `/home/diogo/zith-lsp/src/lsp/lsp-server.cpp` and returns one LSP range per
  brace block that spans at least two lines.
- `textDocument/foldingRange` returns the ranges immediately from the in-memory
  document snapshot; it does not wait for compiler analysis.
- The unit/integration coverage is `TestFoldingRange` in
  `/home/diogo/zith-lsp/tests/test_lsp.py`.
- The extension smoke host can assert availability through
  `vscode.languages.getFoldingRanges(document)`.

## Local Dev Loop

- Build `zith-lsp` with CMake from `/home/diogo/zith-lsp`; the resulting
  `build/zith-lsp` is what the tests and the extension should use.
- The VSIX artifact must be rebuilt after changing the LSP or extension host
  because `zith-language-0.4.0.vsix` is gitignored and has to be copied into
  the extension later.
- `code-oss --install-extension /tmp/zith-language-0.4.0.vsix --force` installs
  the locally rebuilt package. Verify the installed server SHA-256 matches the
  local build after forcing a reinstall.
- `node test/integration/run-host.js` from `vs-code` exercises the extension
  against the development checkout; `run-vsix-host.js` accepts an optional
  VSIX path as `process.argv[2]` after the 2026-09-20 update.

## Environment Quirks

- This sandbox mounts `/home/diogo` read-only, so `vsce package` cannot unlink
  or replace in-place VSIX files inside the extension repository. Write the
  package to `/tmp` instead and install that artifact.
- `code-oss` smoke runners can time out on startup while the extension host
  itself already exits 0. When that happens, inspect the test profile
  `smoke-done` marker and `exthost.log` before treating the run as failed.
- The standalone `zith-lsp` tests point at `build/zith-lsp`, so they can run
  without VS Code and are the fastest way to validate folding ranges.
