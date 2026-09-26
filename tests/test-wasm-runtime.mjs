// WASM VM v2 runtime ABI smoke test.
//
// This harness is intentionally free of Emscripten glue. It loads the
// compiler/runtime .wasm, supplies only the zith.host_write import, and
// exercises the public execution ABI:
//
//   zith_emit_hir(ptr, len)     -> HIR flat blob, then
//   zith_execute_hir(ptr, len)  -> lowers HIR flat to VM v2 IR and runs it.
//   zith_register_stdlib_pack(ptr, len) -> registers host-delivered stdlib sources.
//
// The valid fixtures mirror the host VM v2 acceptance slice: stdlib println
// and manual extern fn puts.

import { readFile } from "node:fs/promises";
import { Buffer } from "node:buffer";

const wasmPath = process.argv[2];
const stdlibPath = process.argv[3];
if (!wasmPath || !stdlibPath) {
  throw new Error("usage: node tests/test-wasm-runtime.mjs <zith-playground.wasm> <zith-stdlib.pack>");
}

const stdoutChunks = [];
const stderrChunks = [];

function writeOut(text) {
  process.stdout.write(text);
}

function writeErr(text) {
  process.stderr.write(text);
}

async function loadInstance() {
  const bytes = await readFile(wasmPath);
  const module = await WebAssembly.compile(bytes);
  const imports = WebAssembly.Module.imports(module);
  const wasiNames = new Set(
    imports
      .filter((entry) => entry.module === "wasi_snapshot_preview1" && entry.kind === "function")
      .map((entry) => entry.name),
  );
  const wasi = Object.fromEntries([...wasiNames].map((name) => [name, () => 0]));
  const importObject = {
    zith: {
      host_write(stream, ptr, len) {
        const view = new Uint8Array(instance.exports.memory.buffer, ptr, len);
        const text = Buffer.from(view).toString("utf8");
        if (stream === 1) {
          stdoutChunks.push(text);
          writeOut(text);
        } else {
          stderrChunks.push(text);
          writeErr(text);
        }
      },
    },
    wasi_snapshot_preview1: wasi,
    env: Object.fromEntries(
      imports
        .filter((entry) => entry.module === "env" && entry.kind === "function")
        .map((entry) => [entry.name, () => 0]),
    ),
  };
  return WebAssembly.instantiate(module, importObject);
}

// Keep instance scoped to the import callback via a mutable binding.
let instance;

function readCString(ptr) {
  const view = new Uint8Array(instance.exports.memory.buffer);
  let end = ptr;
  while (end < view.length && view[end] !== 0) end += 1;
  return Buffer.from(view.subarray(ptr, end)).toString("utf8");
}

function writeString(text) {
  const bytes = Buffer.from(text, "utf8");
  const ptr = instance.exports.zith_alloc(bytes.length);
  new Uint8Array(instance.exports.memory.buffer, ptr, bytes.length).set(bytes);
  return { ptr, len: bytes.length };
}

function writeBytes(bytes) {
  const ptr = instance.exports.zith_alloc(bytes.length);
  new Uint8Array(instance.exports.memory.buffer, ptr, bytes.length).set(bytes);
  return { ptr, len: bytes.length };
}

function readBuffer(ptr, len) {
  const copy = new Uint8Array(instance.exports.memory.buffer, ptr, len);
  return Buffer.from(copy);
}

function assertEqual(actual, expected, message) {
  if (actual !== expected) {
    throw new Error(`${message}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  }
}

function compileSource(source) {
  const { ptr, len } = writeString(source);
  const status = instance.exports.zith_emit_hir(ptr, len);
  if (status !== 0) {
    const errorLen = instance.exports.zith_last_error_len();
    const errorPtr = instance.exports.zith_last_error_ptr();
    throw new Error(`zith_emit_hir failed: ${readCString(errorPtr)}`);
  }
  const hirPtr = instance.exports.zith_last_buffer_ptr();
  const hirLen = instance.exports.zith_last_buffer_len();
  return readBuffer(hirPtr, hirLen);
}

function runHir(hir) {
  const ptr = instance.exports.zith_alloc(hir.length);
  new Uint8Array(instance.exports.memory.buffer, ptr, hir.length).set(hir);
  return instance.exports.zith_execute_hir(ptr, hir.length);
}

const stdHello = `from std/io/console

fn main() {
    _ = println("Hello, WASM!");
}
`;

const externHello = `extern fn puts(msg: *char)

fn main(): i32 {
    _ = puts("hello-puts");
    7
}
`;

const unsupportedSource = `fn main() {
    _ = 1 << 1;
}
`;

async function main() {
  instance = await loadInstance();
  const stdlib = await readFile(stdlibPath);
  const stdlibBuffer = writeBytes(stdlib);
  assertEqual(instance.exports.zith_register_stdlib_pack(stdlibBuffer.ptr, stdlibBuffer.len), 0,
              "stdlib pack registration");

  const externHir = compileSource(externHello);
  const externStatus = runHir(externHir);
  assertEqual(externStatus, 0, "extern fn puts program runs");
  assertEqual(stdoutChunks.join(""), "hello-puts\n", "puts output");
  assertEqual(instance.exports.zith_exit_code(), 7n, "extern fn main exit code");

  stdoutChunks.length = 0;
  stderrChunks.length = 0;
  const hir = compileSource(stdHello);
  const status = runHir(hir);
  assertEqual(status, 0, "stdlib println program runs");
  assertEqual(stdoutChunks.join(""), "Hello, WASM!\n", "println output");

  stdoutChunks.length = 0;
  stderrChunks.length = 0;
  const printSource = writeString(`from std/io/console

fn add4(a: i32, b: i32, c: i32, d: i32): i32 {
    a + b + c + d
}

fn main(): i32 {
    print("one-");
    print("two");
    add4(1, 2, 3, 4)
}
`);
  assertEqual(instance.exports.zith_run_source(printSource.ptr, printSource.len), 0,
              "print and 4-arg function execute through VM v2");
  assertEqual(stdoutChunks.join(""), "one-two", "print outputs continuous text without newline");
  assertEqual(instance.exports.zith_exit_code(), 10n, "add4 computes 10 as main exit code");

  stdoutChunks.length = 0;
  stderrChunks.length = 0;
  const unsupportedHir = compileSource(unsupportedSource);
  const unsupportedStatus = runHir(unsupportedHir);
  assertEqual(unsupportedStatus, 5, "unsupported construct returns 5");
  assertEqual(stderrChunks.join(""), "unsupported HIR binary operator in v2 lowering\n",
              "unsupported message is reported");

  stdoutChunks.length = 0;
  stderrChunks.length = 0;
  const runSource = writeString(`extern fn puts(msg: *char)

fn main(): i32 {
    _ = puts("run-source");
    9
}
`);
  assertEqual(instance.exports.zith_run_source(runSource.ptr, runSource.len), 0,
              "run_source executes through VM v2");
  assertEqual(stdoutChunks.join(""), "run-source\n", "run_source output");
  assertEqual(instance.exports.zith_exit_code(), 9n, "run_source exit code");
}

await main();
writeOut("wasm-runtime: ok\n");
