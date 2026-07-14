const RELEASE_API = "https://api.github.com/repos/GalaxyHaze/Zith-Lang/releases/latest";
const LOCAL_WASM_URL = "./zith-playground.wasm";
const DEFAULT_SOURCE = `fn main() {
}`;

const editor = document.getElementById("source-code");
const lineNumbers = document.getElementById("line-numbers");
const cursorPosition = document.getElementById("cursor-position");
const output = document.getElementById("output");




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

function updateEditorMeta() {
    const lines = editor.value.split("\n");
    lineNumbers.textContent = lines.map((_, index) => index + 1).join("\n");
    const beforeCursor = editor.value.slice(0, editor.selectionStart);
    const line = beforeCursor.split("\n").length;
    const column = beforeCursor.length - beforeCursor.lastIndexOf("\n");
    cursorPosition.textContent = `Ln ${line}, Col ${column}`;
}

function isWasm(bytes) {
    const view = new Uint8Array(bytes);
    return view.length >= 4 && view[0] === 0 && view[1] === 97 && view[2] === 115 && view[3] === 109;
}

function findEndOfCentralDirectory(view) {
    const minOffset = Math.max(0, view.byteLength - 65557);
    for (let offset = view.byteLength - 22; offset >= minOffset; offset -= 1) {
        if (view.getUint32(offset, true) === 0x06054b50) return offset;
    }
    throw new Error("The download is not a readable ZIP archive.");
}

async function unzip(bytes) {
    const view = new DataView(bytes);
    const decoder = new TextDecoder();
    const eocd = findEndOfCentralDirectory(view);
    const entries = view.getUint16(eocd + 10, true);
    let offset = view.getUint32(eocd + 16, true);
    const files = [];

    for (let index = 0; index < entries; index += 1) {
        if (view.getUint32(offset, true) !== 0x02014b50) throw new Error("Invalid ZIP directory.");
        const compression = view.getUint16(offset + 10, true);
        const compressedSize = view.getUint32(offset + 20, true);
        const nameLength = view.getUint16(offset + 28, true);
        const extraLength = view.getUint16(offset + 30, true);
        const commentLength = view.getUint16(offset + 32, true);
        const localOffset = view.getUint32(offset + 42, true);
        const name = decoder.decode(new Uint8Array(bytes, offset + 46, nameLength));
        const localNameLength = view.getUint16(localOffset + 26, true);
        const localExtraLength = view.getUint16(localOffset + 28, true);
        const dataStart = localOffset + 30 + localNameLength + localExtraLength;
        const compressed = new Uint8Array(bytes.slice(dataStart, dataStart + compressedSize));
        let data;

        if (compression === 0) {
            data = compressed;
        } else if (compression === 8 && "DecompressionStream" in window) {
            const stream = new Blob([compressed]).stream().pipeThrough(new DecompressionStream("deflate-raw"));
            data = new Uint8Array(await new Response(stream).arrayBuffer());
        } else {
            throw new Error("This browser cannot decompress the WASM ZIP.");
        }

        files.push({ name, data });
        offset += 46 + nameLength + extraLength + commentLength;
    }
    return files;
}

function unpackArArchive(bytes) {
    const decoder = new TextDecoder();
    if (decoder.decode(new Uint8Array(bytes.slice(0, 8))) !== "!<arch>\n") return [];
    const files = [];
    let stringTable = "";
    let offset = 8;
    while (offset + 60 <= bytes.byteLength) {
        const entry = new Uint8Array(bytes.slice(offset, offset + 60));
        const rawName = decoder.decode(entry.slice(0, 16)).trim();
        const size = Number.parseInt(decoder.decode(entry.slice(48, 58)).trim(), 10);
        const start = offset + 60;
        if (!Number.isFinite(size) || start + size > bytes.byteLength) break;
        const data = new Uint8Array(bytes.slice(start, start + size));

        if (rawName === "//") {
            stringTable = decoder.decode(data);
        } else if (rawName !== "/") {
            const nameOffset = rawName.match(/^\/(\d+)$/);
            const name = nameOffset
                ? stringTable.slice(Number.parseInt(nameOffset[1], 10)).split("\n")[0].replace(/\/$/, "")
                : rawName.replace(/\/$/, "");
            files.push({ name, data });
        }
        offset = start + size + (size % 2);
    }
    return files;
}

async function inspectWasmModules(files) {
    const modules = [];
    for (const file of files) {
        if (!isWasm(file.data)) continue;
        const module = await WebAssembly.compile(file.data);
        modules.push({ name: file.name, exports: WebAssembly.Module.exports(module).map(entry => entry.name) });
    }
    return modules;
}

function getWasmAsset(release) {
    const assets = Array.isArray(release.assets) ? release.assets : [];
    return assets.find(asset => /playground.*\.wasm$|zithc.*\.wasm$/i.test(asset.name))
        || assets.find(asset => /wasm.*\.zip$/i.test(asset.name))
        || assets.find(asset => /\.wasm$/i.test(asset.name));
}

async function loadRuntime() {
    try {
        
        
        
        
        
        writeOutput("Fetching local WebAssembly build...", "terminal-dim");

        const response = await fetch(LOCAL_WASM_URL);
        if (!response.ok) throw new Error(`Local module returned ${response.status}.`);
        const module = await WebAssembly.compile(await response.arrayBuffer());
        const exports = WebAssembly.Module.exports(module).map(entry => entry.name);
        const requiredExports = ["memory", "zith_alloc", "zith_free", "zith_compile_source"];
        const missingExports = requiredExports.filter(name => !exports.includes(name));
        const nativeImports = WebAssembly.Module.imports(module).filter(entry => entry.module !== "zith");
        

        if (missingExports.length) {
            runtimeStatusCompact.textContent = "Compiler: ABI Incomplete";
            
            writeOutput(`The local build is missing: ${missingExports.join(", ")}.`, "terminal-error");
            return;
        }

        if (nativeImports.length) {
            runtimeStatusCompact.textContent = "Compiler: Native missing";
            
            writeOutput(`The local build has ${nativeImports.length} unresolved native imports (first: ${nativeImports[0].module}.${nativeImports[0].name}).`, "terminal-warn");
            writeOutput("Link libc++, libc++abi, libc and compiler dependencies into the WASM binary before browser execution.", "terminal-dim");
            return;
        }

        let instance;
        const hostWrite = (stream, pointer, length) => {
            if (!instance || length < 0) return;
            const bytes = new Uint8Array(instance.exports.memory.buffer, pointer, length);
            writeOutput(decoder.decode(bytes), stream === 2 ? "terminal-error" : "terminal-ok");
        };
        instance = await WebAssembly.instantiate(module, { zith: { host_write: hostWrite } });
        runtime = instance.exports;
        runtimeStatusCompact.textContent = "Compiler: Ready (local)";
        
        
        writeOutput("Local browser runtime loaded.", "terminal-ok");
    } catch (error) {
        const message = error instanceof Error ? error.message : "Unknown loading error.";
        
        
        runtimeStatusCompact.textContent = "Compiler: Load Failed";
        writeOutput(`Could not load the local WASM build: ${message}`, "terminal-error");
    }
}

editor.addEventListener("input", updateEditorMeta);
editor.addEventListener("click", updateEditorMeta);
editor.addEventListener("keyup", updateEditorMeta);
editor.addEventListener("scroll", () => { lineNumbers.scrollTop = editor.scrollTop; });






const terminalInput = document.getElementById("terminal-input");
const commandHistory = [];
let historyIndex = -1;

terminalInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
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
  zithc run [--opt-level <0|1|2|3>] [--emit <tokens|ast|hir|ir|asm|all>]
  zithc check [--opt-level <0|1|2|3>] [--emit <tokens|ast|hir|ir|asm|all>]
  clear
  help`, "terminal-ok");
        return;
    }

    if (cmd.startsWith("zithc run") || cmd.startsWith("zithc check")) {
        if (!runtime) {
            writeOutput("Compiler not loaded.", "terminal-error");
            return;
        }
        if (!runtime.zith_compile_source) {
            writeOutput("Compiler ABI incompatible (missing zith_compile_source).", "terminal-error");
            return;
        }
        
        const mode = cmd.startsWith("zithc run") ? 1 : 0;
        let optLevel = 0;
        let emitMask = 0;
        
        const args = cmd.split(/\s+/);
        for (let i = 2; i < args.length; i++) {
            if (args[i] === "--opt-level" && i + 1 < args.length) {
                optLevel = parseInt(args[i + 1], 10);
                i++;
            } else if (args[i] === "--emit" && i + 1 < args.length) {
                const emits = args[i + 1].split(",");
                for (const e of emits) {
                    if (e === "tokens") emitMask |= 1;
                    else if (e === "ast") emitMask |= 2;
                    else if (e === "hir") emitMask |= 4;
                    else if (e === "ir") emitMask |= 8;
                    else if (e === "asm") emitMask |= 16;
                    else if (e === "all") emitMask |= 31;
                }
                i++;
            }
        }
        
        runCompiler(mode, optLevel, emitMask);
        return;
    }
    
    writeOutput(`zithc: command not found: ${cmd.split(" ")[0]}`, "terminal-error");
}

function runCompiler(mode, optLevel, emitMask) {
    let pointer = 0;
    try {
        const sourceText = editor.value;
        const source = encoder.encode(sourceText);
        pointer = runtime.zith_alloc(source.length);
        if (!pointer) throw new Error("The WASM allocator returned a null pointer.");
        new Uint8Array(runtime.memory.buffer, pointer, source.length).set(source);
        
        const result = runtime.zith_compile_source(pointer, source.length, mode, optLevel, emitMask);
        
        if (result === 0) {
            writeOutput(`Program finished successfully.`, "terminal-ok");
        } else {
            writeOutput(`Program finished with error. (Status: ${result})`, "terminal-error");
        }
    } catch (error) {
        const message = error instanceof Error ? error.message : "Unknown runtime error.";
        writeOutput(`Execution failed: ${message}`, "terminal-error");
    } finally {
        if (pointer) runtime.zith_free(pointer, encoder.encode(editor.value).length);
    }
}

const savedCode = new URLSearchParams(window.location.hash.slice(1)).get("code");
if (savedCode) { try { editor.value = decodeURIComponent(escape(atob(savedCode))); } catch (_) { /* Ignore malformed share links. */ } }
updateEditorMeta();
loadRuntime();

document.getElementById("terminal-container").addEventListener("click", () => {
    document.getElementById("terminal-input").focus();
});
