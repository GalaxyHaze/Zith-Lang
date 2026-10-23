#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
compiler=${ZITHC:-zithc}

if ! command -v "$compiler" >/dev/null 2>&1; then
    printf 'test runner: compiler not found: %s\n' "$compiler" >&2
    exit 127
fi

temp_root=$(mktemp -d "${TMPDIR:-/tmp}/self-zith-tests.XXXXXX")
trap 'rm -rf "$temp_root"' EXIT HUP INT TERM

cp "$repo_root/ZithProject.toml" "$temp_root/"
cp -R "$repo_root/src" "$temp_root/src"
cp -R "$repo_root/tests" "$temp_root/tests"

"$compiler" test "$temp_root"
"$compiler" run --virtual-machine --no-cache "$temp_root/tests/run.zith"
