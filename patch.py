import re

with open('playground/index.html', 'r') as f:
    html = f.read()

# Remove release-card
html = re.sub(r'<div class="release-card".*?</div>', '', html, flags=re.DOTALL)

# Remove workspace-top wrapper (just keep editor-panel)
html = html.replace('<div class="workspace-top">', '')
# Remove the closing div for workspace-top which is just before <article class="panel output-panel">
html = html.replace('</aside>\n            </div>\n\n            <article class="panel output-panel">', '</aside>\n\n            <article class="panel output-panel">')

# Remove side-stack
html = re.sub(r'<aside class="side-stack">.*?</aside>', '', html, flags=re.DOTALL)

# Remove buttons in editor-panel
html = re.sub(r'<div class="buttons">\s*<button.*?</button>\s*<button.*?</button>\s*<button.*?</button>\s*</div>', '', html, flags=re.DOTALL)

# Update editor-footer
html = html.replace('<span>UTF-8 &nbsp; zith</span>', '<span><span id="runtime-status-compact">Compiler: Connecting...</span> &nbsp;|&nbsp; UTF-8 &nbsp; zith</span>')

# Update terminal
old_terminal = '''<button id="clear-output" class="icon-button" type="button" aria-label="Clear output">clear</button>
                </div>
                <pre id="output" class="terminal" aria-live="polite"><span class="terminal-dim">$</span> zith playground
<span class="terminal-dim">Waiting for the WebAssembly compiler...</span></pre>'''

new_terminal = '''</div>
                <div class="terminal-container" id="terminal-container" aria-live="polite">
                    <pre id="output" class="terminal-history"></pre>
                    <div class="terminal-input-line">
                        <span class="terminal-prompt">&gt;</span>
                        <input type="text" id="terminal-input" autocomplete="off" spellcheck="false" aria-label="Terminal input" autofocus>
                    </div>
                </div>'''

html = html.replace(old_terminal, new_terminal)

with open('playground/index.html', 'w') as f:
    f.write(html)
