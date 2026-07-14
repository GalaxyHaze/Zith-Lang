import re

with open('css/playground.css', 'r') as f:
    css = f.read()

# Update .workspace-top to remove grid rules since it's removed, but we actually removed the class itself.
# We should remove .workspace-top and .side-stack from CSS.
css = re.sub(r'\.workspace-top\s*{[^}]+}', '', css)
css = re.sub(r'\.side-stack\s*{[^}]+}', '', css)
css = re.sub(r'\.runtime-list.*?(?=\.actions)', '', css, flags=re.DOTALL)
css = re.sub(r'\.actions\s*{[^}]+}', '', css)

# Terminal layout updates
css = css.replace('.terminal { flex: 1; padding: 17px; overflow: auto; color: #e0e0e0; white-space: pre-wrap; background: #111; }',
'''
.terminal-container {
    flex: 1;
    display: flex;
    flex-direction: column;
    padding: 17px;
    overflow: auto;
    color: #e0e0e0;
    background: #111;
    font: 0.92rem/28px ui-monospace, monospace;
}
.terminal-history {
    margin: 0;
    white-space: pre-wrap;
    word-break: break-all;
    font: inherit;
}
.terminal-input-line {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-top: 4px;
}
.terminal-prompt {
    color: var(--cyan);
    font-weight: bold;
}
#terminal-input {
    flex: 1;
    background: transparent;
    border: none;
    color: #FAF3E1;
    font: inherit;
    outline: none;
    caret-color: var(--orange);
}
''')

# Media query fixes
css = css.replace('.workspace-top { grid-template-columns: 1fr; }', '')
css = css.replace('.side-stack { grid-template-rows: auto auto; }', '')

with open('css/playground.css', 'w') as f:
    f.write(css)
