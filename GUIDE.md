# Zith Website Guide

How the site is built, and how to add documentation pages.

## Layout

```
partials/                 header.html, footer.html  <- single source for the chrome
tools/build.py            expands the includes, writes _site/
tools/build_docs.py       regenerates the docs fragments from docs/pages/*.md
docs/pages/*.md           Markdown source for the documentation
html/documentation/       generated fragments (D-*.html) + the shell D-home.html
json/tree.json            generated navigation tree (sidebar structure)
css/                      tokens + per-area styles
js/                       per-page scripts
```

Every top-level page (`index.html`, `changelog.html`, `about/`, `playground/`,
`helios/`, `html/documentation/D-home.html`) is a **source** that declares the
shared chrome with an include:

```html
<!-- include:header active="docs" -->
...
<!-- include:footer -->
```

`tools/build.py` expands those into `_site/`, resolves `{{baseurl}}`, and also
injects the chrome into the Dev Log posts under `about/posts/`.

## Preview

The site is static but the sources contain includes, so build first:

```bash
python3 tools/build.py
python3 -m http.server 5501 --directory _site
```

On GitHub Pages the workflow (`.github/workflows/build.yml`) runs the build and
publishes `_site/`.

## Documentation

Each `D-*.html` file under `html/documentation/` is a **content fragment** — the
inner HTML of a page, with no `<html>`, `<head>`, or `<body>`. The shell
(`D-home.html`) loads them with `fetch()` when you click a sidebar link.

### Write a page

1. Write the Markdown under `docs/pages/` (frontmatter with `id` and `title`).
2. Regenerate the fragments and the navigation tree:

   ```bash
   make docs          # python3 tools/build_docs.py
   make docs-check    # fails if the fragments are out of date
   ```

Supported Markdown: headings, bold/italic, inline code, fenced code blocks,
links (`.md` links are remapped to `./D-*.html`), blockquotes, lists, tables,
`---`, and callouts (`:::info`, `:::warning`, `:::tip`, `:::caution`,
`:::danger`).

Callout and content styles live in `css/docs.css`.
