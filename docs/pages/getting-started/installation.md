---
id: getting-started-installation
title: Installation
section: Getting Started
output: getting-started/D-installation.html
aliases: intro/D-installation.html
kind: editorial
---
# Installation

The `zithc` compiler for Zith-- provides package-manager installs, fetch
installers, and a source build. The beta supports Linux x86-64 with the GNU
runtime; other operating systems and architectures are candidates until their
installation route and check path are verified end to end. Read the
[Beta Support And Limitations](doc:getting-started-beta-support) table before
choosing an install route.

## Homebrew

Homebrew installs target macOS, which is outside the beta support scope. The
formula may exist, but macOS is not a verified beta target.

```bash
brew tap galaxyhaze/zithc
brew install zithc
```

## Scoop

Scoop installs target Windows, which is a candidate rather than a verified beta
target.

```powershell
scoop bucket add zithc https://github.com/GalaxyHaze/Zith.git
scoop install zithc
```

## Fetch installer

On Linux:

```bash
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.sh | sh
```

Linux musl is outside the beta support scope. The installer still exposes
`--musl`, but the resulting binary is not verified for the beta and the
language server has no musl release asset.

On Windows PowerShell:

```powershell
Invoke-RestMethod https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.ps1 | Invoke-Expression
```

Windows is a candidate, not a supported beta target, until its check path is
verified end to end.

## Build from source

Required toolchain versions are documented in the [compiler README](https://github.com/GalaxyHaze/Zith#readme).

```bash
git clone https://github.com/GalaxyHaze/Zith.git
cd Zith
cmake -S . -B build
cmake --build build
```

```bash
./build/zithc --help
```

Run `./build/zithc --help` after the build finishes, or add the build directory to `PATH`.
LLVM 18+ is optional but required for LLVM native code generation; without it, the compiler falls back to a VM backend. Use [First Program](doc:getting-started-quick-start) once the compiler is available.
