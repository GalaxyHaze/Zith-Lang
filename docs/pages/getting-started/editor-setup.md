---
id: getting-started-editor-setup
title: Editor Setup
section: Getting Started
output: getting-started/D-editor-setup.html
aliases: intro/D-editor-setup.html
kind: editorial
---
# Editor Setup

The VS Code extension adds syntax highlighting, snippets, and language-server
features for `.zithm` files. The beta has no verified marketplace listing, so
install the extension from a local `.vsix` built from the extension source. Read
the [Beta Support And Limitations](doc:getting-started-beta-support) table first
because the extension is verified only on Linux x86-64 with the GNU runtime.

## 1. Install the compiler and language server

Install `zithc` with one of the routes in [Installation](doc:getting-started-installation).
The extension starts `zith-lsp`, which links the compiler as a library, so
install both before opening a project.

Download the `zith-lsp` binary for your platform from the extension repository:

```bash
git clone https://github.com/GalaxyHaze/zith-extension.git
cd zith-extension
node shared/scripts/download-lsp.js
```

The downloader writes the server to `vs-code/server/zith-lsp` (or `.exe` on
Windows).

## 2. Install the VS Code extension

Install a packaged extension with the VS Code command-line tool:

```bash
code --install-extension zith-language-0.4.0.vsix
```

Build the `.vsix` from source when you do not have the packaged file:

```bash
cd zith-extension/vs-code
npx @vscode/vsce package
```

`npx @vscode/vsce package` writes `zith-language-0.4.0.vsix` in `vs-code/`. The
extension repository also ships `vs-code/test/integration/run-vsix-host.js`,
which installs the packaged artifact into an isolated profile and exercises the
extension host.

## 3. Open a Zith-- project

Create and open a project, then confirm the editor reports the same result as
the command line:

```bash
zithc create hello-zith
code hello-zith
```

Open `src/main.zithm`. The status bar shows the language server state. Edit the
file to introduce an error, then run `Zith: Check code` and confirm the editor
shows the diagnostic at the offending span. Correct the source and confirm the
diagnostic clears.

## Verify the install

The editor journey is verified on Linux x86-64 with the GNU runtime. On any
other platform, treat the extension as unverified and fall back to the
`zithc check` and `zithc run` command-line path in [First
Program](doc:getting-started-quick-start).
