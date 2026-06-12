#!/usr/bin/env python3
"""
Batch converter: extract all old .md files from git commit 5eb89e7,
convert them to HTML fragments, and write to html/documentation/.
"""

import subprocess
import sys
import os

# Add parent dir to path for importing convert-md
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.convert_md import convert

COMMIT = "5eb89e7"
DOCS_PREFIX = "docsaurus/docs/"
OUTPUT_DIR = "html/documentation/"

FILES = [
    # advanced/
    ("advanced/01-overview.md",        "D-advanced-overview.html"),
    ("advanced/data-structures.md",    "D-advanced-data-structures.html"),
    ("advanced/generics-deep.md",      "D-advanced-generics-deep.html"),
    ("advanced/how-to-use.md",         "D-advanced-how-to-use.html"),
    ("advanced/macros.md",             "D-advanced-macros.html"),
    ("advanced/metaprogramming.md",    "D-advanced-metaprogramming.html"),
    ("advanced/raw-pointers.md",       "D-advanced-raw-pointers.html"),
    ("advanced/traits.md",             "D-advanced-traits.html"),
    ("advanced/unsafe.md",             "D-advanced-unsafe.html"),

    # cli/
    ("cli/01-overview.md",             "D-cli.html"),
    ("cli/02-overview.md",             "D-cli-check.html"),
    ("cli/build.md",                   "D-cli-build.html"),
    ("cli/clean.md",                   "D-cli-clean.html"),
    ("cli/compile.md",                 "D-cli-compile.html"),
    ("cli/docs.md",                    "D-cli-docs.html"),
    ("cli/flags.md",                   "D-cli-flags.html"),
    ("cli/fmt.md",                     "D-cli-fmt.html"),
    ("cli/new.md",                     "D-cli-new.html"),
    ("cli/repl.md",                    "D-cli-repl.html"),
    ("cli/run.md",                     "D-cli-run.html"),

    # community/
    ("community/overview.mdx",         "D-community-overview.html"),
    ("community/chat.mdx",             "D-community-chat.html"),
    ("community/contributing.mdx",     "D-community-contributing.html"),
    ("community/code-of-conduct.mdx",  "D-community-code-of-conduct.html"),

    # faq/
    ("faq/01-overview.md",             "D-faq-overview.html"),
    ("faq/02-philosophy.md",           "D-faq-philosophy.html"),
    ("faq/03-security.md",             "D-faq-security.html"),
    ("faq/04-rust-comparison.md",      "D-faq-rust-comparison.html"),
    ("faq/05-use-cases.md",            "D-faq-use-cases.html"),

    # intro/
    ("intro/01-overview.md",           "D-intro-overview.html"),
    ("intro/installation.md",          "D-intro-installation.html"),
    ("intro/why-zith.md",              "D-intro-why-zith.html"),

    # language/
    ("language/01-overview.md",        "D-language-overview.html"),
    ("language/concurrency.md",        "D-language-concurrency.html"),
    ("language/contexts.md",           "D-language-contexts.html"),
    ("language/control-flow.md",       "D-language-control-flow.html"),
    ("language/ecs.md",                "D-language-ecs.html"),
    ("language/errors.md",             "D-language-errors.html"),
    ("language/functions.md",          "D-language-functions.html"),
    ("language/generics.md",           "D-language-generics.html"),
    ("language/intrinsics.md",         "D-language-intrinsics.html"),
    ("language/memory.md",             "D-language-memory.html"),
    ("language/modules.md",            "D-language-modules.html"),
    ("language/ownership.md",          "D-language-ownership.html"),
    ("language/packs.md",              "D-language-packs.html"),
    ("language/spec-core-topics.md",   "D-language-spec-core-topics.html"),
    ("language/syntax.md",             "D-language-syntax.html"),
    ("language/types.md",              "D-language-types.html"),
    ("language/variables.md",          "D-language-variables.html"),

    # project/
    ("project/01-overview.md",         "D-project-overview.html"),

    # quickstart/
    ("quickstart/01-hello-world.md",   "D-quickstart-hello-world.html"),

    # reference/
    ("reference/stdlib.md",            "D-reference-stdlib.html"),
]


def extract_from_git(path):
    """Extract file content from git commit."""
    result = subprocess.run(
        ["git", "show", f"{COMMIT}:{DOCS_PREFIX}{path}"],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"  ERROR: Could not extract {path}: {result.stderr.strip()}")
        return None
    return result.stdout


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    total = len(FILES)
    ok = 0

    for src, dst in FILES:
        dst_path = os.path.join(OUTPUT_DIR, dst)
        print(f"[{ok+1}/{total}] {src} → {dst}")

        md_content = extract_from_git(src)
        if md_content is None:
            continue

        html = convert(md_content, source_path=src)
        with open(dst_path, "w") as f:
            f.write(html)
            f.write("\n")
        ok += 1

    print(f"\nDone! {ok}/{total} files converted successfully.")


if __name__ == "__main__":
    main()
