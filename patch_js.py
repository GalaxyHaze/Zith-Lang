import re

with open('js/playground.js', 'r') as f:
    js = f.read()

# Remove UI elements not in DOM anymore
js = re.sub(r'const runButton = .*?;', '', js)
js = re.sub(r'const releaseVersion = .*?;', '', js)
js = re.sub(r'const releaseDetail = .*?;', '', js)
js = re.sub(r'const releaseLink = .*?;', '', js)
js = re.sub(r'const runtimeStatus = .*?;', 'const runtimeStatusCompact = document.getElementById("runtime-status-compact");', js)
js = re.sub(r'const runtimeAsset = .*?;', '', js)
js = re.sub(r'const runtimeModule = .*?;', '', js)

# In loadRuntime
js = js.replace('releaseVersion.textContent = "Local build";', '')
js = js.replace('releaseDetail.textContent = "Loading ./zith-playground.wasm...";', '')
js = js.replace('releaseLink.href = LOCAL_WASM_URL;', '')
js = js.replace('releaseLink.textContent = "Open local module";', '')
js = js.replace('runtimeAsset.textContent = "zith-playground.wasm";', '')

js = js.replace('const requiredExports = ["memory", "zith_alloc", "zith_free", "zith_last_error_ptr", "zith_last_error_len", "zith_run_source"];', 'const requiredExports = ["memory", "zith_alloc", "zith_free", "zith_compile_source"];')
js = js.replace('runtimeModule.textContent = exports.join(", ");', '')

js = js.replace('runtimeStatus.textContent = "ABI incomplete";', 'runtimeStatusCompact.textContent = "Compiler: ABI Incomplete";')
js = js.replace('releaseDetail.textContent = `Missing exports: ${missingExports.join(", ")}.`;', '')

js = js.replace('runtimeStatus.textContent = "Native runtime missing";', 'runtimeStatusCompact.textContent = "Compiler: Native missing";')
js = js.replace('releaseDetail.textContent = `${nativeImports.length} C/C++ imports still need to be linked into the module.`;', '')

js = js.replace('runtimeStatus.textContent = "Ready";', 'runtimeStatusCompact.textContent = "Compiler: Ready (local)";')
js = js.replace('releaseDetail.textContent = "Local browser runtime is ready.";', '')
js = js.replace('runButton.disabled = false;', '')

js = js.replace('releaseVersion.textContent = "Unavailable";', '')
js = js.replace('releaseDetail.textContent = "The local WASM build could not be loaded.";', '')
js = js.replace('runtimeStatus.textContent = "Load failed";', 'runtimeStatusCompact.textContent = "Compiler: Load Failed";')


# Remove event listeners
js = re.sub(r'document\.getElementById\("clear-output"\)\.addEventListener\("click", \(\) => {.*?}\);', '', js, flags=re.DOTALL)
js = re.sub(r'document\.getElementById\("reset-code"\)\.addEventListener\("click", \(\) => {.*?}\);', '', js, flags=re.DOTALL)
js = re.sub(r'document\.getElementById\("copy-link"\)\.addEventListener\("click", async \(\) => {.*?}\);', '', js, flags=re.DOTALL)
js = re.sub(r'runButton\.addEventListener\("click", \(\) => {.*?}\);', '', js, flags=re.DOTALL)

terminal_js = '''
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
        
        const args = cmd.split(/\\s+/);
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
'''
# Append the new logic before the last few lines (which are savedCode and loadRuntime)
parts = js.split('const savedCode = ')
js = parts[0] + terminal_js + '\nconst savedCode = ' + parts[1]

# Small fix for writeOutput
js = js.replace('output.insertAdjacentHTML("beforeend", `${prefix}<span class="${className}">${escapeHtml(message)}</span>`);', 
                'output.insertAdjacentHTML("beforeend", `${prefix}<span class="${className}">${escapeHtml(message)}</span>`);\n    document.getElementById("terminal-container").scrollTop = document.getElementById("terminal-container").scrollHeight;')


with open('js/playground.js', 'w') as f:
    f.write(js)
