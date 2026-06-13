#!/usr/bin/env python3
"""Scan about/posts/ for HTML files and generate about/posts/index.json."""

import json
import re
import os
from pathlib import Path

POSTS_DIR = Path(__file__).resolve().parent.parent / "about" / "posts"
OUTPUT = POSTS_DIR / "index.json"

entries = []
for f in sorted(POSTS_DIR.iterdir(), reverse=True):
    if not re.match(r"^\d{4}-\d{2}-\d{2}-", f.name) or not f.name.endswith(".html"):
        continue
    date = f.name[:10]
    content = f.read_text(encoding="utf-8")
    title_match = re.search(r"<title>(.*?)</title>", content, re.IGNORECASE)
    title = title_match.group(1) if title_match else f.name
    entries.append({"file": f.name, "title": title, "date": date})

OUTPUT.write_text(json.dumps(entries, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Generated {OUTPUT} with {len(entries)} entries")
