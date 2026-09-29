# WASM Playground ABI

This document describes the stable ABI exported by `zith-playground.wasm` for the browser
playground. The playground performs lexing, parsing, type checking, HIR lowering, and execution through
the portable VM v2. It does not include LLVM codegen.

The module is a standalone Emscripten build with no entry function. JavaScript provides the
`zith.host_write` import; the `wasi_snapshot_preview1.fd_write` stub forwards writes to that same
host callback so compiler and program streams can be rendered without a filesystem.

The playground can compile a source buffer once into a flat HIR blob and then execute that blob
with the portable VM. The flat HIR format is versioned and self-contained; it is defined by
`src/wasm/abi-hir.*` and is independent of the `.zirl` cache format.

## Exports

| Export | Signature | Purpose |
|---|---|---|
| `zith_alloc` | `(size: i32) -> i32` | Allocate a byte buffer readable by the module. |
| `zith_free` | `(ptr: i32, size: i32) -> ()` | Free a buffer returned by `zith_alloc`. |
| `zith_register_stdlib_pack` | `(ptr: i32, len: i32) -> i32` | Validate and register the host-delivered canonical stdlib pack. Must be called before compilation. |
| `zith_compile_source` | `(ptr, len, mode, opt_level, emit_mask: i32) -> i32` | Check and emit compiler stages up to HIR. |
| `zith_run_source` | `(ptr: i32, len: i32) -> i32` | Compile the source to HIR, execute it through VM v2, forward output, and expose its exit code. |
| `zith_emit_hir` | `(ptr: i32, len: i32) -> i32` | Compile source to a flat HIR blob and expose it through `zith_last_buffer_ptr/len`. |
| `zith_execute_hir` | `(ptr: i32, len: i32) -> i32` | Decode a flat HIR blob, lower it into VM v2 IR, and run it. |
| `zith_last_buffer_ptr` | `() -> i32` | Pointer to the flat HIR blob produced by the last `zith_emit_hir` call. |
| `zith_last_buffer_len` | `() -> i32` | Byte length of the flat HIR blob. |
| `zith_exit_code` | `() -> i64` | Guest `main` exit code from the last VM v2 run. |
| `zith_last_error_ptr` | `() -> i32` | Pointer to the accumulated error text from the last call. |
| `zith_last_error_len` | `() -> i32` | Byte length of the last error buffer. |
| `zith_last_output_ptr` | `() -> i32` | Pointer to compiler emission output from the last call. |
| `zith_last_output_len` | `() -> i32` | Byte length of the last output buffer. |
| `zith_error_count` | `() -> i32` | Number of rendered diagnostics from the last call. |
| `zith_error_at` | `(index: i32) -> i32` | Pointer to one rendered `severity: message` line, or `0`. |
| `zith_compiler_version_ptr` | `() -> i32` | Pointer to the compiler version string from `ZITH_VERSION`. |
| `zith_compiler_version_len` | `() -> i32` | Byte length of the compiler version string. |

## Imports

| Module | Import | Purpose |
|---|---|---|
| `zith` | `host_write(stream: i32, ptr: i32, len: i32)` | Render UTF-8 bytes to the playground terminal. Stream `1` is stdout, stream `2` is stderr. |
| `wasi_snapshot_preview1` | standard WASI imports | Stubs; `fd_write` forwards write buffers to `zith.host_write`. |

## Return Codes

| Code | Meaning |
|---|---|
| `0` | Success: source was checked and staged output was produced. |
| `1` | Compilation failure: diagnostics were rendered, and `last_error` is non-empty. |
| `2` | Invalid parameter: the call did not enter the compiler pipeline, and `last_error` is non-empty. |
| `3` | VM v2 runtime trap or missing guest `main`. |
| `4` | VM v2 ran out of linear memory or guest memory capacity. |
| `5` | The flat HIR uses a construct outside the current VM v2 lowering slice. |

`zith_register_stdlib_pack` returns `2` for a malformed pack and clears the previously registered
pack before attempting registration. `zith_emit_hir` returns `1` when the source fails HIR
lowering, `2` for an invalid buffer, and `0` when a blob is available. `zith_execute_hir` returns
`1` for malformed flat HIR, `5` when lowering rejects the program, and `3`/`4` for runtime
failures. `zith_run_source` combines the emit and execute operations and uses the same status
codes.

Invalid parameters are reported before any session is created, so callers must check
`zith_last_error_ptr/len` instead of treating non-zero status as a compiler diagnostic.

## `mode`

The playground accepts only `0` for check and `1` for run. Broad compiler modes such as Debug,
Release, Fast, and Small are not part of the browser ABI. They are native CLI options.

## `opt_level`

`opt_level` must be in the range `0..3`. The playground does not perform native code generation but
keeps the range consistent with the CLI and C API.

## `emit_mask`

| Bit | Stage |
|---|---|
| `1` | Tokens |
| `2` | AST |
| `4` | HIR |
| `8` | IR |
| `16` | ASM |
| `32` | CST |
| `64` | VIR, the VM v2 execution IR |

The bits are cumulative. CST is the concrete syntax tree after parsing, AST is the frontend AST,
HIR is the semantic high-level IR, and VIR is the result of `HIR -> vm::lowerModule` for the
portable VM v2. VIR is not LLVM IR. IR and ASM require an LLVM backend, which is not available
in this WASM build. Passing either bit produces a diagnostic and return code `1`; it does not
silently ignore the request.
Bits outside `1|2|4|8|16|32|64` are invalid parameters and return code `2`.

All textual emissions use the existing `zith_last_output_ptr/len` buffer. CST and VIR are not
serialized into the flat HIR blob returned by `zith_emit_hir`; that operation remains exclusively
for producing the blob, while `zith_execute_hir` remains exclusively for executing it. The output
pointer and length remain valid until the next call that replaces the output buffer.

## Diagnostics

`zith_error_at` returns one stable line per diagnostic, rendered as:

```text
severity: message
```

The line remains valid until the next call that replaces the output or diagnostic buffers. An `index`
greater than or equal to `zith_error_count()` returns `0`. This stable line format is intended for
the playground. Structured JSON diagnostics will be added by a future LSP-facing API.

## Standard Library Pack

The browser host downloads the pack separately from the WASM module and registers it through
`zith_register_stdlib_pack`. The pack is generated from the repository's canonical `stdlib/`
directory by `scripts/package-stdlib.py`; no source files are embedded into the WASM module.

The current format is deliberately small and deterministic:

```text
bytes[8]  magic = "ZSTDLIB2"
u32       little-endian ABI version (`2`)
u32       little-endian entry count
repeat entry count times:
  u32     little-endian UTF-8 path length
  u32     little-endian source byte length
  bytes   UTF-8 relative path
  bytes   Zith source
```

Paths are sorted by their POSIX relative path. The runtime validates the magic, ABI version,
entry count, safe relative paths, duplicate paths, length bounds, and exact end-of-buffer before
exposing entries as virtual sources under `stdlib/`. The WASM-only `stdio.h`, `stdlib.h`, and
`string.h` bindings are added as small virtual headers because the browser has no native include
filesystem; the regular Zith stdlib remains the canonical implementation.

## JavaScript Buffer Helpers

```js
function readString(instance, ptr, len) {
  return ptr && len ? new TextDecoder().decode(new Uint8Array(instance.exports.memory.buffer, ptr, len)) : "";
}

function compilerErrorAt(instance, index) {
  const ptr = instance.exports.zith_error_at(index);
  if (!ptr) return null;
  const length = instance.exports.memory.buffer.byteLength;
  const bytes = new Uint8Array(instance.exports.memory.buffer, ptr, length - ptr);
  const line = [];
  for (const byte of bytes) {
    if (byte === 0 || byte === 10) break;
    line.push(byte);
  }
  return new TextDecoder().decode(new Uint8Array(line));
}

function readCompilerOutput(instance) {
  return readString(
    instance,
    instance.exports.zith_last_output_ptr(),
    instance.exports.zith_last_output_len()
  );
}

function readCompilerError(instance) {
  const ptr = instance.exports.zith_last_error_ptr();
  return readString(instance, ptr, instance.exports.zith_last_error_len());
}

function compilerVersion(instance) {
  return readString(
    instance,
    instance.exports.zith_compiler_version_ptr(),
    instance.exports.zith_compiler_version_len()
  );
}
```
