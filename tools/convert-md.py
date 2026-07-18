#!/usr/bin/env python3
"""
Zith Documentation Markdown-to-HTML Converter
Converts old Docusaurus .md files into HTML content fragments
for the gh-pages documentation system (loaded via loadTree.js).

Usage:
    python3 tools/convert_md.py --file input.md --source-path section/file.md
    python3 tools/convert_md.py < input.md
"""

import re
import sys
import os
import argparse
import unicodedata

# ── Old file path → new D-file name ──

OLD_TO_NEW = {
    "advanced/01-overview.md":        "D-advanced-overview.html",
    "advanced/data-structures.md":    "D-advanced-data-structures.html",
    "advanced/generics-deep.md":      "D-advanced-generics-deep.html",
    "advanced/how-to-use.md":         "D-advanced-how-to-use.html",
    "advanced/macros.md":             "D-advanced-macros.html",
    "advanced/metaprogramming.md":    "D-advanced-metaprogramming.html",
    "advanced/raw-pointers.md":       "D-advanced-raw-pointers.html",
    "advanced/traits.md":             "D-advanced-traits.html",
    "advanced/unsafe.md":             "D-advanced-unsafe.html",
    "cli/01-overview.md":             "D-cli.html",
    "cli/02-overview.md":             "D-cli-check.html",
    "cli/build.md":                   "D-cli-build.html",
    "cli/clean.md":                   "D-cli-clean.html",
    "cli/compile.md":                 "D-cli-compile.html",
    "cli/docs.md":                    "D-cli-docs.html",
    "cli/flags.md":                   "D-cli-flags.html",
    "cli/fmt.md":                     "D-cli-fmt.html",
    "cli/new.md":                     "D-cli-new.html",
    "cli/repl.md":                    "D-cli-repl.html",
    "cli/run.md":                     "D-cli-run.html",
    "community/overview.mdx":         "D-community-overview.html",
    "community/chat.mdx":             "D-community-chat.html",
    "community/contributing.mdx":     "D-community-contributing.html",
    "community/code-of-conduct.mdx":  "D-community-code-of-conduct.html",
    "faq/01-overview.md":             "D-faq-overview.html",
    "faq/02-philosophy.md":           "D-faq-philosophy.html",
    "faq/03-security.md":             "D-faq-security.html",
    "faq/04-rust-comparison.md":      "D-faq-rust-comparison.html",
    "faq/05-use-cases.md":            "D-faq-use-cases.html",
    "intro/01-overview.md":           "D-intro-overview.html",
    "intro/installation.md":          "D-intro-installation.html",
    "intro/why-zith.md":              "D-intro-why-zith.html",
    "language/01-overview.md":        "D-language-overview.html",
    "language/concurrency.md":        "D-language-concurrency.html",
    "language/contexts.md":           "D-language-contexts.html",
    "language/control-flow.md":       "D-language-control-flow.html",
    "language/ecs.md":                "D-language-ecs.html",
    "language/errors.md":             "D-language-errors.html",
    "language/functions.md":          "D-language-functions.html",
    "language/generics.md":           "D-language-generics.html",
    "language/intrinsics.md":         "D-language-intrinsics.html",
    "language/memory.md":             "D-language-memory.html",
    "language/modules.md":            "D-language-modules.html",
    "language/ownership.md":          "D-language-ownership.html",
    "language/packs.md":              "D-language-packs.html",
    "language/spec-core-topics.md":   "D-language-spec-core-topics.html",
    "language/syntax.md":             "D-language-syntax.html",
    "language/types.md":              "D-language-types.html",
    "language/variables.md":          "D-language-variables.html",
    "project/01-overview.md":         "D-project-overview.html",
    "quickstart/01-hello-world.md":   "D-quickstart.html",
    "reference/stdlib.md":            "D-reference-stdlib.html",

    # Spec files
    "Zith-spec.md":                   "D-spec-overview.html",
    "impl-status.md":                 "D-spec-impl-status.html",
    "Zith-spec-overview.md":          "D-spec-overview.html",
    "02-module-system.md":            "D-spec-module-system.html",
    "03-type-system.md":              "D-spec-type-system.html",
    "04-traits-interfaces.md":        "D-spec-traits-interfaces.html",
    "05-functions.md":                "D-spec-functions.html",
    "06-mutability-bindings.md":      "D-spec-mutability-bindings.html",
    "07-memory-model.md":             "D-spec-memory-model.html",
    "08-error-handling.md":           "D-spec-error-handling.html",
    "09-control-flow.md":             "D-spec-control-flow.html",
    "10-concurrency.md":              "D-spec-concurrency.html",
    "11-comptime.md":                 "D-spec-comptime.html",
    "12-assets.md":                   "D-spec-assets.html",
    "13-raw-unsafe.md":               "D-spec-raw-unsafe.html",
    "14-polymorphism.md":             "D-spec-polymorphism.html",
    "15-macros.md":                   "D-spec-macros.html",
    "16-words.md":                    "D-spec-words.html",
    "17-contexts.md":                 "D-spec-contexts.html",
    "18-c-interop.md":                "D-spec-c-interop.html",
    "19-project-config.md":           "D-spec-project-config.html",
    "20-standard-library.md":         "D-spec-standard-library.html",
    "21-best-practices.md":           "D-spec-best-practices.html",
}


def resolve_link(url, source_dir):
    """Resolve a link URL relative to the source file's directory.

    Returns the ./D-*.html path if found in OLD_TO_NEW,
    otherwise falls back to swapping .md/.mdx → .html.
    """
    if url.startswith("http"):
        return url
    if url.startswith("#"):
        return url

    # Split anchor from the URL
    anchor = ""
    if "#" in url:
        url, anchor = url.split("#", 1)
        anchor = f"#{anchor}"

    # Resolve the URL relative to the source directory
    resolved = os.path.normpath(os.path.join(source_dir, url))
    resolved = resolved.lstrip("./")

    if resolved in OLD_TO_NEW:
        return f"./{OLD_TO_NEW[resolved]}{anchor}"

    # Fallback: just swap the extension
    return re.sub(r'\.mdx?$', '.html', url) + anchor


def convert_link(match, source_dir):
    text = match.group(1)
    url = match.group(2).strip()
    return f'<a href="{resolve_link(url, source_dir)}">{text}</a>'


def convert_image(match):
    alt = match.group(1)
    url = match.group(2)
    return f'<img src="{url}" alt="{alt}" />'


def inline_convert(text, source_dir):
    """Process inline Markdown within a paragraph/block of text.

    Tokenizes by inline code spans first so that bold/italic
    regexes never see asterisks inside backtick code.
    """
    parts = []
    last = 0
    for m in re.finditer(r'`([^`]+)`', text):
        if m.start() > last:
            parts.append(('text', text[last:m.start()]))
        parts.append(('code', m.group(1)))
        last = m.end()
    if last < len(text):
        parts.append(('text', text[last:]))

    result_parts = []
    for kind, content in parts:
        if kind == 'code':
            result_parts.append(f'<code>{content}</code>')
        else:
            content = re.sub(r'!\[([^\]]*)\]\(([^)]+)\)', convert_image, content)
            content = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', lambda m: convert_link(m, source_dir), content)
            content = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', content)
            content = re.sub(r'(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)', r'<em>\1</em>', content)
            result_parts.append(content)

    return ''.join(result_parts)


def process_table(lines, source_dir):
    html = ["<table>"]
    for i, line in enumerate(lines):
        line = line.strip()
        if not line or line.startswith("|---") or line.startswith("|--"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        tag = "th" if i == 0 else "td"
        html.append("  <tr>")
        for cell in cells:
            html.append(f"    <{tag}>{inline_convert(cell, source_dir)}</{tag}>")
        html.append("  </tr>")
    html.append("</table>")
    return "\n".join(html)


def process_callout_block(lines, callout_type, source_dir):
    title = ""
    body_lines = []
    for line in lines:
        line = line.strip()
        if not title and line:
            title = line
        elif line:
            body_lines.append(line)

    icon_map = {
        "warning": "&#9888;",
        "caution": "&#9888;",
        "info": "&#8505;",
        "tip": "&#128161;",
        "danger": "&#10060;",
    }
    icon = icon_map.get(callout_type, "")
    label = callout_type.capitalize()

    html = [f'<div class="callout callout-{callout_type}">']
    if title:
        html.append(f'  <p><strong>{icon} {label}:</strong> {inline_convert(title, source_dir)}</p>')

    # Process callout body lines: detect lists, otherwise paragraphs
    i = 0
    while i < len(body_lines):
        line = body_lines[i]
        stripped = line.strip()

        # Check for list inside callout
        if is_list_line(stripped):
            list_lines = [line]
            i += 1
            while i < len(body_lines) and body_lines[i].strip():
                list_lines.append(body_lines[i])
                i += 1

            ordered = bool(re.match(r'^\d+\.\s', list_lines[0].strip()))
            tag = "ol" if ordered else "ul"
            html.append(f"  <{tag}>")
            for li in list_lines:
                content = re.sub(r'^(\s*)(?:[-*+]\s|\d+\.\s)', '', li)
                html.append(f"    <li>{inline_convert(content, source_dir)}</li>")
            html.append(f"  </{tag}>")
        else:
            html.append(f"  <p>{inline_convert(line, source_dir)}</p>")
            i += 1

    html.append("</div>")
    return "\n".join(html)


def strip_emoji(text):
    """Remove emoji and other purely decorative Unicode symbols from text."""
    result = []
    for c in text:
        cat = unicodedata.category(c)
        cp = ord(c)
        if cat == 'So':                     # Symbol-Other (most emoji)
            continue
        if cp in (0xFE0F, 0xFE0E):          # Variation selectors
            continue
        if cp == 0x200D:                    # Zero-width joiner
            continue
        if 0x1F1E6 <= cp <= 0x1F1FF:        # Regional indicators (flags)
            continue
        result.append(c)
    return ''.join(result)


def is_table_line(line):
    return line.strip().startswith("|")


def is_list_line(line):
    stripped = line.strip()
    return stripped.startswith("- ") or stripped.startswith("* ") or re.match(r'^\d+\.\s', stripped)


def is_hr(line):
    return re.match(r'^[-*_]{3,}\s*$', line.strip())


def convert(md_text, source_path=None):
    """Convert full Markdown document to HTML fragment.

    Args:
        md_text: Raw Markdown text.
        source_path: Path of the source .md file relative to
                     docsaurus/docs/ root (e.g. 'language/syntax.md').
                     Used for link resolution.
    """
    source_dir = os.path.dirname(source_path) if source_path else ""

    # Remove YAML frontmatter
    md_text = re.sub(r'^---[\s\S]*?---\n*', '', md_text, count=1)

    # Remove Mermaid code blocks entirely
    md_text = re.sub(r'```mermaid\n[\s\S]*?\n```\n*', '', md_text)

    lines = md_text.split("\n")
    output = []
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]
        stripped = line.strip()

        if not stripped and i == 0:
            i += 1
            continue

        # ── Heading ──
        heading_match = re.match(r'^(#{1,6})\s+(.+)$', stripped)
        if heading_match:
            level = len(heading_match.group(1))
            text = inline_convert(heading_match.group(2), source_dir)
            output.append(f"<h{level}>{text}</h{level}>")
            i += 1
            continue

        # ── Horizontal rule ──
        if is_hr(stripped):
            output.append("<hr>")
            i += 1
            continue

        # ── Callout block ──
        callout_match = re.match(r'^:::{(\w+)}\s*$', stripped)
        if not callout_match:
            callout_match = re.match(r'^:::(\w+)(\s+.*)?$', stripped)
        if callout_match:
            callout_type = callout_match.group(1).lower()
            i += 1
            callout_lines = []
            while i < n:
                if lines[i].strip() == ":::":
                    i += 1
                    break
                callout_lines.append(lines[i])
                i += 1
            output.append(process_callout_block(callout_lines, callout_type, source_dir))
            continue

        # ── Fenced code block ──
        if stripped.startswith("```"):
            lang = stripped[3:].strip()
            i += 1
            code_lines = []
            while i < n and not lines[i].strip().startswith("```"):
                code_lines.append(lines[i])
                i += 1
            i += 1  # skip closing ```
            code_text = "\n".join(code_lines)
            escaped = code_text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            if lang:
                output.append(f'<pre><code class="language-{lang}">{escaped}</code></pre>')
            else:
                output.append(f"<pre><code>{escaped}</code></pre>")
            continue

        # ── Table ──
        if is_table_line(stripped):
            table_lines = []
            while i < n and is_table_line(lines[i]):
                table_lines.append(lines[i])
                i += 1
            output.append(process_table(table_lines, source_dir))
            continue

        # ── Blockquote ──
        if stripped.startswith(">"):
            quote_lines = []
            while i < n:
                s = lines[i].strip()
                if s.startswith(">"):
                    quote_lines.append(s.lstrip(">").strip())
                    i += 1
                else:
                    break
            quote_text = inline_convert(" ".join(quote_lines), source_dir)
            output.append(f"<blockquote><p>{quote_text}</p></blockquote>")
            continue

        # ── List ──
        if is_list_line(stripped):
            list_lines = []
            while i < n and lines[i].strip():
                list_lines.append(lines[i])
                i += 1

            ordered = bool(re.match(r'^\d+\.\s', list_lines[0].strip()))
            tag = "ol" if ordered else "ul"
            html = [f"<{tag}>"]
            for li in list_lines:
                content = re.sub(r'^(\s*)(?:[-*+]\s|\d+\.\s)', '', li)
                html.append(f"  <li>{inline_convert(content, source_dir)}</li>")
            html.append(f"</{tag}>")
            output.append("\n".join(html))
            continue

        # ── Empty line ──
        if not stripped:
            i += 1
            continue

        # ── Paragraph ──
        para_lines = []
        while i < n and lines[i].strip():
            para_lines.append(lines[i].strip())
            i += 1

        para_text = " ".join(para_lines)
        if is_hr(para_text):
            output.append("<hr>")
        else:
            output.append(f"<p>{inline_convert(para_text, source_dir)}</p>")

    html = "\n".join(output)
    return strip_emoji(html)


def main():
    parser = argparse.ArgumentParser(description="Convert Zith .md docs to HTML fragments")
    parser.add_argument("--file", "-f", help="Input .md file path")
    parser.add_argument("--output", "-o", help="Output .html file path")
    parser.add_argument("--source-path", help="Old source path relative to docsaurus/docs/ (for link resolution)")
    args = parser.parse_args()

    if args.file:
        with open(args.file, "r") as f:
            md = f.read()
    else:
        md = sys.stdin.read()

    html = convert(md, source_path=args.source_path)

    if args.output:
        with open(args.output, "w") as f:
            f.write(html)
    else:
        print(html, end="")


if __name__ == "__main__":
    main()
