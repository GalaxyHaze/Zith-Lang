const WASM_URL = new URL("../playground/zith-playground.wasm", location.href);
const STDLIB_PACK_URL = new URL("../playground/zith-stdlib.pack", location.href);
const RUNTIME_MANIFEST_URL = new URL("../playground/runtime.json", location.href);
const DEFAULT_SOURCE = `from std/io/console

fn main() {
    _ = println("Hello, World!");
}`;

const editor = document.getElementById("source-code");
const lineNumbers = document.getElementById("line-numbers");
const syntaxHighlight = document.getElementById("syntax-highlight");
const cursorPosition = document.getElementById("cursor-position");
const output = document.getElementById("output");
const layoutMode = document.getElementById("layout-mode");
const LAYOUT_STORAGE_KEY = "zith-playground-layout";




const runtimeStatusCompact = document.getElementById("runtime-status-compact");


let runtime = null;
const encoder = new TextEncoder();
const decoder = new TextDecoder();

function escapeHtml(value) {
    return value.replace(/[&<>"]/g, character => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[character]);
}

function writeOutput(message, className = "") {
    const prefix = output.textContent.trim() ? "\n" : "";
    output.insertAdjacentHTML("beforeend", `${prefix}<span class="${className}">${escapeHtml(message)}</span>`);
    document.getElementById("terminal-container").scrollTop = document.getElementById("terminal-container").scrollHeight;
    output.scrollTop = output.scrollHeight;
}

function writeOutputBlock(title) {
    writeOutput(`--- ${title} ---`, "terminal-dim");
}

function updateEditorMeta() {
    const lines = editor.value.split("\n");
    lineNumbers.textContent = lines.map((_, index) => index + 1).join("\n");
    const beforeCursor = editor.value.slice(0, editor.selectionStart);
    const line = beforeCursor.split("\n").length;
    const column = beforeCursor.length - beforeCursor.lastIndexOf("\n");
    cursorPosition.textContent = `Ln ${line}, Col ${column}`;
}

function updateSyntaxHighlight() {
    if (!syntaxHighlight) return;
    syntaxHighlight.innerHTML = window.ZithHighlight
        ? window.ZithHighlight.highlight(editor.value)
        : escapeHtml(editor.value);
    syntaxHighlight.scrollTop = editor.scrollTop;
    syntaxHighlight.scrollLeft = editor.scrollLeft;
}

function indentSelection(indentation) {
    const start = editor.selectionStart;
    const end = editor.selectionEnd;
    const lineStart = editor.value.lastIndexOf("\n", start - 1) + 1;
    const lineEndIndex = editor.value.indexOf("\n", end);
    const lineEnd = lineEndIndex === -1 ? editor.value.length : lineEndIndex;
    const selectedLines = editor.value.slice(lineStart, lineEnd);
    const lines = selectedLines.split("\n");
    const changed = lines.map(line => {
        if (indentation === "\t") return indentation + line;
        return line.startsWith("\t") ? line.slice(1) : line.replace(/^ {1,4}/, "");
    }).join("\n");

    editor.value = editor.value.slice(0, lineStart) + changed + editor.value.slice(lineEnd);
    const nextStart = start + (indentation === "\t" ? 1 : -Math.min(4, selectedLines.length - selectedLines.trimStart().length));
    const nextEnd = end + changed.length - selectedLines.length;
    editor.setSelectionRange(
        Math.max(lineStart, nextStart),
        Math.max(lineStart, nextEnd)
    );
    updateEditorMeta();
    updateSyntaxHighlight();
}

function focusWorkspace(direction) {
    const terminalInput = document.getElementById("terminal-input");
    if (direction === "next") {
        terminalInput.focus();
        return;
    }
    editor.focus();
}

function formatSource(source) {
    const lines = source.replace(/\r\n/g, "\n").split("\n");
    let indent = 0;

    const formatted = lines.map(line => {
        const trimmed = line.trim();
        if (!trimmed) return "";
        if (/^}/.test(trimmed)) indent = Math.max(0, indent - 1);

        const result = `${"    ".repeat(indent)}${trimmed}`;
        let inString = false;
        let escaped = false;
        let lineComment = false;
        let delta = 0;

        for (let index = 0; index < trimmed.length; index += 1) {
            const character = trimmed[index];
            const next = trimmed[index + 1];
            if (lineComment) break;
            if (character === "/" && next === "/" && !inString) {
                lineComment = true;
                continue;
            }
            if (character === '"' && !escaped) {
                inString = !inString;
                continue;
            }
            if (inString) {
                escaped = character === "\\" && !escaped;
                if (character !== "\\") escaped = false;
                continue;
            }
            if (character === "{") delta += 1;
            if (character === "}") delta -= 1;
        }

        indent = Math.max(0, indent + delta);
        return result;
    });

    return formatted.join("\n").replace(/\n{3,}/g, "\n\n").trimEnd() + "\n";
}

function setLayoutMode(mode) {
    const nextMode = mode === "stacked" ? "stacked" : "side-by-side";
    document.body.classList.toggle("layout-stacked", nextMode === "stacked");
    if (layoutMode) layoutMode.textContent = nextMode === "stacked" ? "Top / Bottom" : "Left / Right";
    try {
        localStorage.setItem(LAYOUT_STORAGE_KEY, nextMode);
    } catch (_) {
        // Layout preference is optional when storage is unavailable.
    }
}

function toggleLayoutMode() {
    setLayoutMode(document.body.classList.contains("layout-stacked") ? "side-by-side" : "stacked");
}

function restoreLayoutMode() {
    let savedMode = "side-by-side";
    try {
        const storedMode = localStorage.getItem(LAYOUT_STORAGE_KEY);
        savedMode = storedMode === "stacked" ? "stacked" : savedMode;
    } catch (_) {
        // Use the default layout when storage is unavailable.
    }
    setLayoutMode(savedMode);
}

async function readRuntimeManifest() {
    const response = await fetch(RUNTIME_MANIFEST_URL);
    if (!response.ok) throw new Error(`Runtime manifest returned ${response.status}.`);
    const manifest = await response.json();
    if (manifest.abi !== 2 || manifest.stdlib !== "zith-stdlib.pack") {
        throw new Error("Runtime manifest has an incompatible ABI or stdlib asset.");
    }
    return manifest;
}

async function sha256Hex(bytes) {
    const digest = await crypto.subtle.digest("SHA-256", bytes);
    return [...new Uint8Array(digest)].map(byte => byte.toString(16).padStart(2, "0")).join("");
}

function buildImportObject(module, instanceRef, writeOutput) {
    const imports = WebAssembly.Module.imports(module);
    const usedModules = new Set(imports.map(entry => entry.module));
    const importObject = {};

    const hostWrite = (stream, pointer, length) => {
        if (!instanceRef.instance || length < 0) return;
        const bytes = new Uint8Array(instanceRef.instance.exports.memory.buffer, pointer, length);
        writeOutput(decoder.decode(bytes), stream === 2 ? "terminal-error" : "terminal-ok");
    };

    const syscallNames = new Set(
        imports
            .filter(entry => entry.module === "env" && entry.kind === "function")
            .map(entry => entry.name)
    );
    const syscallStubs = Object.fromEntries(
        [...syscallNames].map(name => [name, () => -1])
    );

    const wasiStubNames = new Set(
        imports
            .filter(entry => entry.module === "wasi_snapshot_preview1" && entry.kind === "function")
            .map(entry => entry.name)
    );
    const wasiStubs = {
        clock_time_get: (clockId, precision, timePointer) => {
            if (!instanceRef.instance) return 8;
            const memory = instanceRef.instance.exports.memory;
            const now = BigInt(Date.now()) * 1000000n;
            new BigUint64Array(memory.buffer, timePointer, 1)[0] = now;
            return 0;
        },
        fd_write: (fd, iovecsPointer, iovecsLength, writtenPointer) => {
            if (!instanceRef.instance) return 8;
            const memory = instanceRef.instance.exports.memory;
            const iov = new DataView(memory.buffer, iovecsPointer, iovecsLength * 8);
            let total = 0;
            for (let index = 0; index < iovecsLength; index += 1) {
                const pointer = iov.getUint32(index * 8, true);
                const length = iov.getUint32(index * 8 + 4, true);
                if (length > 0) {
                    const bytes = new Uint8Array(memory.buffer, pointer, length);
                    writeOutput(decoder.decode(bytes), fd === 2 ? "terminal-error" : "terminal-ok");
                    total += length;
                }
            }
            if (writtenPointer && total <= 0xffffffff) {
                new Uint32Array(memory.buffer, writtenPointer, 1)[0] = total;
            }
            return 0;
        },
        fd_read: () => 8,
        fd_fdstat_get: () => 8,
        fd_prestat_get: () => 8,
        fd_prestat_dir_name: () => 8,
        fd_readdir: () => 8,
        fd_close: () => 0,
        fd_seek: () => 0,
        args_get: () => 8,
        args_sizes_get: () => 8,
        environ_get: () => 8,
        environ_sizes_get: () => 8,
        path_create_directory: () => 8,
        path_filestat_get: () => 8,
        path_open: () => 8,
        path_readlink: () => 8,
        random_get: () => 8,
        proc_exit: () => { throw new Error("WebAssembly compiler exited."); }
    };
    for (const name of wasiStubNames) {
        if (name !== "clock_time_get" && name !== "fd_write") wasiStubs[name] = () => 8;
    }

    if (usedModules.has("zith")) importObject.zith = { host_write: hostWrite };
    if (usedModules.has("wasi_snapshot_preview1")) importObject.wasi_snapshot_preview1 = wasiStubs;
    if (usedModules.has("env")) importObject.env = syscallStubs;
    return importObject;
}

async function loadRuntime() {
    try {
        runtimeStatusCompact.textContent = "Compiler: Loading…";
        writeOutput("Fetching local WebAssembly build…", "terminal-dim");

        const manifest = await readRuntimeManifest();
        const wasmResponse = await fetch(WASM_URL);
        if (!wasmResponse.ok) throw new Error(`Local module returned ${wasmResponse.status}.`);
        const stdlibResponse = await fetch(STDLIB_PACK_URL);
        if (!stdlibResponse.ok) throw new Error(`Local stdlib pack returned ${stdlibResponse.status}.`);
        const wasmBytes = await wasmResponse.arrayBuffer();
        const stdlibBytes = await stdlibResponse.arrayBuffer();
        if (manifest.wasm_sha256 && await sha256Hex(wasmBytes) !== manifest.wasm_sha256) {
            throw new Error("WASM SHA-256 does not match the runtime manifest.");
        }
        if (manifest.stdlib_sha256 && await sha256Hex(stdlibBytes) !== manifest.stdlib_sha256) {
            throw new Error("Stdlib pack SHA-256 does not match the runtime manifest.");
        }
        const module = await WebAssembly.compile(wasmBytes);
        const exports = WebAssembly.Module.exports(module).map(entry => entry.name);
        const requiredExports = [
            "memory",
            "zith_alloc",
            "zith_free",
            "zith_compile_source",
            "zith_run_source",
            "zith_register_stdlib_pack",
        ];
        const missingExports = requiredExports.filter(name => !exports.includes(name));

        if (missingExports.length) {
            runtimeStatusCompact.textContent = "Compiler: ABI Incomplete";
            writeOutput(`The local build is missing: ${missingExports.join(", ")}.`, "terminal-error");
            return;
        }

        const instanceRef = {};
        const importObject = buildImportObject(module, instanceRef, writeOutput);
        const instance = await WebAssembly.instantiate(module, importObject);
        instanceRef.instance = instance;
        const stdlibPack = new Uint8Array(stdlibBytes);
        const packPointer = instance.exports.zith_alloc(stdlibPack.length);
        if (!packPointer) throw new Error("The WASM allocator returned a null stdlib pack pointer.");
        new Uint8Array(instance.exports.memory.buffer, packPointer, stdlibPack.length).set(stdlibPack);
        const packStatus = instance.exports.zith_register_stdlib_pack(packPointer, stdlibPack.length);
        instance.exports.zith_free(packPointer, stdlibPack.length);
        if (packStatus !== 0) throw new Error(readLastErrorFrom(instance.exports));

        const displayVersion = manifest.version ? `release ${manifest.version}` : "local";
        runtime = instance.exports;
        runtimeStatusCompact.textContent = `Compiler: Ready (${displayVersion})`;
        writeOutput(`WebAssembly compiler ready (${displayVersion}).`, "terminal-ok");
    } catch (error) {
        const message = error instanceof Error ? error.message : "Unknown loading error.";
        runtimeStatusCompact.textContent = "Compiler: Load Failed";
        writeOutput(`Could not load the WebAssembly compiler: ${message}`, "terminal-error");
    }
}

editor.addEventListener("input", () => {
    updateEditorMeta();
    updateSyntaxHighlight();
});
editor.addEventListener("click", updateEditorMeta);
editor.addEventListener("keyup", updateEditorMeta);
editor.addEventListener("scroll", () => {
    lineNumbers.scrollTop = editor.scrollTop;
    updateSyntaxHighlight();
});
editor.addEventListener("keydown", (event) => {
    if (event.key === "Tab" && !event.ctrlKey && !event.metaKey && !event.altKey) {
        event.preventDefault();
        indentSelection(event.shiftKey ? "unindent" : "\t");
        return;
    }

    if (event.altKey && event.ctrlKey && event.key === "ArrowRight") {
        event.preventDefault();
        focusWorkspace("next");
    } else if (event.altKey && event.ctrlKey && event.key === "ArrowLeft") {
        event.preventDefault();
        focusWorkspace("previous");
    }
});

document.addEventListener("keydown", (event) => {
    if (event.ctrlKey && event.altKey && event.key.toLowerCase() === "l") {
        event.preventDefault();
        toggleLayoutMode();
    }
});

const terminalInput = document.getElementById("terminal-input");
const commandHistory = [];
let historyIndex = -1;

terminalInput.addEventListener("keydown", (e) => {
    if (e.ctrlKey && e.altKey && e.key === "ArrowLeft") {
        e.preventDefault();
        focusWorkspace("previous");
    } else if (e.key === "Enter") {
        const cmd = terminalInput.value.trim();
        terminalInput.value = "";
        if (cmd) {
            commandHistory.push(cmd);
            historyIndex = commandHistory.length;
            executeCommand(cmd);
        }
    } else if (e.key === "ArrowUp") {
        if (historyIndex > 0) {
            historyIndex--;
            terminalInput.value = commandHistory[historyIndex];
        }
        e.preventDefault();
    } else if (e.key === "ArrowDown") {
        if (historyIndex < commandHistory.length - 1) {
            historyIndex++;
            terminalInput.value = commandHistory[historyIndex];
        } else {
            historyIndex = commandHistory.length;
            terminalInput.value = "";
        }
        e.preventDefault();
    }
});

function executeCommand(cmd) {
    writeOutput(`> ${cmd}`, "terminal-dim");

    if (cmd === "clear") {
        output.textContent = "";
        return;
    }

    if (cmd === "help") {
        writeOutput(`Available commands:
  zithc run [--emit-tokens|--emit-ast|--emit-hir|--emit-cst|--emit-vir]
  zithc check [--emit-tokens|--emit-ast|--emit-hir|--emit-cst|--emit-vir]
  zithc build [--emit-tokens|--emit-ast|--emit-hir|--emit-cst|--emit-vir]
  zithc fmt
  zithc format
  --emit-tokens | --emit-ast | --emit-hir | --emit-cst | --emit-vir
  clear
  help`, "terminal-ok");
        return;
    }

    const args = cmd.split(/\s+/).filter(Boolean);
    const first = args[0];
    const rawSubcommand = first === "zithc" ? args[1] : first;
    const rawCommandArgs = first === "zithc" ? args.slice(2) : args.slice(1);
    const standaloneEmit = rawSubcommand && (rawSubcommand.startsWith("--emit-") || rawSubcommand === "--emit");
    const subcommand = standaloneEmit ? "check" : rawSubcommand;
    const commandArgs = standaloneEmit
        ? args.slice(first === "zithc" ? 1 : 0)
        : rawCommandArgs;

    if (subcommand === "fmt" || subcommand === "format") {
        if (commandArgs.length) {
            writeOutput("zithc fmt: unexpected argument; use 'zithc fmt' without arguments in the browser.", "terminal-error");
            return;
        }
        writeOutputBlock("fmt");
        editor.value = formatSource(editor.value);
        updateEditorMeta();
        updateSyntaxHighlight();
        writeOutput("source formatted", "terminal-ok");
        return;
    }

    if (subcommand === "run" || subcommand === "check" || subcommand === "build") {
        if (!runtime) {
            writeOutput("Compiler not loaded.", "terminal-error");
            return;
        }
        if (!runtime.zith_compile_source || !runtime.zith_run_source) {
            writeOutput("Compiler ABI incompatible (missing compile/run exports).", "terminal-error");
            return;
        }

        let parsed = null;
        try {
            parsed = parseCompilerArgs(subcommand, commandArgs);
        } catch (error) {
            writeOutput(error.message, "terminal-error");
            return;
        }
        const mode = subcommand === "run" ? 1 : 0;
        runCompiler(mode, parsed.emitMask, subcommand);
        return;
    }

    writeOutput(`zithc: command not found: ${first || subcommand}`, "terminal-error");
}

function parseCompilerArgs(subcommand, args) {
    let emitMask = 0;

    for (let i = 0; i < args.length; i++) {
        if (args[i] === "--emit" && i + 1 < args.length) {
            const emits = args[i + 1].split(",");
            for (const emit of emits) {
                emitMask |= emitMaskForName(emit);
            }
            i++;
        } else if (
            args[i] === "--emit-tokens" ||
            args[i] === "--emit-ast" ||
            args[i] === "--emit-hir" ||
            args[i] === "--emit-cst" ||
            args[i] === "--emit-vir"
        ) {
            emitMask |= emitMaskForName(args[i].slice("--emit-".length));
        } else {
            throw new Error(`zithc: unexpected argument for ${subcommand}: ${args[i]}`);
        }
    }

    return { emitMask };
}

function emitMaskForName(name) {
    const masks = { tokens: 1, ast: 2, hir: 4, cst: 32, vir: 64 };
    if (!Object.prototype.hasOwnProperty.call(masks, name)) {
        throw new Error(`zithc: emitter '${name}' is unavailable in this browser WASM build; use tokens, ast, hir, cst, or vir.`);
    }
    return masks[name];
}

function emitLabel(emitMask) {
    return [
        ["tokens", 1],
        ["ast", 2],
        ["hir", 4],
        ["cst", 32],
        ["vir", 64],
    ]
        .filter(([, mask]) => emitMask & mask)
        .map(([name]) => name.toUpperCase())
        .join(" + ");
}

function runCompiler(mode, emitMask, subcommand) {
    let pointer = 0;
    let sourceLength = 0;
    try {
        const sourceText = editor.value;
        const source = encoder.encode(sourceText);
        sourceLength = source.length;
        pointer = runtime.zith_alloc(sourceLength);
        if (!pointer) throw new Error("The WASM allocator returned a null pointer.");
        new Uint8Array(runtime.memory.buffer, pointer, sourceLength).set(source);

        if (emitMask) writeOutputBlock(emitLabel(emitMask));
        const result = subcommand === "run" && !emitMask
            ? runtime.zith_run_source(pointer, sourceLength)
            : runtime.zith_compile_source(pointer, sourceLength, mode, 0, emitMask);
        const lastError = readLastError();

        if (result === 0) {
            if (subcommand === "check") {
                writeOutput("check passed", "terminal-ok");
            } else if (subcommand === "build") {
                writeOutput("build prepared (HIR artifact ready for cache integration)", "terminal-ok");
            } else if (subcommand === "run" && emitMask) {
                writeOutputBlock("RUN");
                const runResult = runtime.zith_run_source(pointer, sourceLength);
                const runError = readLastError();
                if (runResult !== 0) {
                    if (runError) writeOutput(runError, "terminal-error");
                    writeOutput(`run failed (Status: ${runResult})`, "terminal-error");
                } else {
                    const exitCode = runtime.zith_exit_code ? runtime.zith_exit_code() : 0;
                    writeOutput(`run completed (exit code ${exitCode})`, "terminal-ok");
                }
            } else {
                const exitCode = runtime.zith_exit_code ? runtime.zith_exit_code() : 0;
                writeOutput(`run completed (exit code ${exitCode})`, "terminal-ok");
            }
        } else {
            if (lastError) writeOutput(lastError, "terminal-error");
            if (subcommand === "check") {
                writeOutput(`check failed (Status: ${result})`, "terminal-error");
            } else if (subcommand === "build") {
                writeOutput(`build failed (Status: ${result})`, "terminal-error");
            } else {
                writeOutput(`compile error (Status: ${result})`, "terminal-error");
            }
        }
    } catch (error) {
        const message = error instanceof Error ? error.message : "Unknown runtime error.";
        writeOutput(`Execution failed: ${message}`, "terminal-error");
    } finally {
        if (pointer) runtime.zith_free(pointer, sourceLength);
    }
}

function readLastError() {
    return readLastErrorFrom(runtime);
}

function readLastErrorFrom(exports) {
    if (!exports || !exports.zith_last_error_ptr || !exports.zith_last_error_len) return "";
    const pointer = exports.zith_last_error_ptr();
    const length = exports.zith_last_error_len();
    if (!pointer || !length) return "";
    return decoder.decode(new Uint8Array(exports.memory.buffer, pointer, length));
}

const savedCode = new URLSearchParams(window.location.hash.slice(1)).get("code");
if (savedCode) { try { editor.value = decodeURIComponent(escape(atob(savedCode))); } catch (_) { /* Ignore malformed share links. */ } }
updateEditorMeta();
updateSyntaxHighlight();
restoreLayoutMode();
loadRuntime();

document.getElementById("terminal-container").addEventListener("click", () => {
    document.getElementById("terminal-input").focus();
});
