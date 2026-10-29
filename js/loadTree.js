/* Documentation shell: loads the navigation tree and injects content
   fragments into #content. See GUIDE.md. */
const sidebar = document.getElementById("sidebar");
const content = document.getElementById("content");
const filebarPath = document.getElementById("filebarPath");
const docsBase = new URL(document.body.dataset.docsBase, document.baseURI);
const route = window.DocsRouting.resolveDocsRoute(window.location.pathname);
const docsScope = document.body.dataset.docsScope || route.scope;
const defaultPage = route.page;
const treeVersion = 4;

let treeModel = [];
let flatPages = [];
let pageMeta = new Map();
let aliasMap = new Map();
let currentPage = defaultPage;

function sectionDescription(trail) {
    const section = trail[0] || "";
    const descriptions = {
        "Getting Started": "Install Zith, write a first program, and learn how the language is positioned.",
        "Language Guide": "Practical documentation of Zith syntax, types, memory model, generics, concurrency, and systems programming.",
        "Language Reference": "Canonical Zith language reference, specification chapters, and implementation status.",
        "CLI Reference": "Reference for zithc commands: build, run, check, format, create, test, and project tooling.",
        "Project": "Project overview and current direction for the Zith programming language.",
        "FAQ": "Frequently asked questions about Zith stability, scope, and relationship to other languages.",
        "Community": "Community resources for reporting issues and contributing to Zith documentation.",
    };
    return descriptions[section] || "Official documentation for the Zith programming language.";
}

(function loadMenu() {
    let cached = null;
    try {
        cached = sessionStorage.getItem("tree_json");
    } catch (_) {}

    if (cached) {
        try {
            const menu = JSON.parse(cached);
            if (menu.version === treeVersion) {
                initialiseNavigation(menu);
                return;
            }
        } catch (_) {}
    }

    fetch(document.body.dataset.treeUrl)
        .then(response => {
            if (!response.ok) throw new Error("Failed to load navigation");
            return response.json();
        })
        .then(menu => {
            try {
                sessionStorage.setItem("tree_json", JSON.stringify(menu));
            } catch (_) {}
            initialiseNavigation(menu);
        })
        .catch(() => {
            sidebar.replaceChildren();
            const message = document.createElement("p");
            message.className = "docs-error";
            message.textContent = "Failed to load navigation.";
            sidebar.appendChild(message);
        });
})();

function initialiseNavigation(menuData) {
    const generatedTree = !Array.isArray(menuData) && Array.isArray(menuData.navigation);
    const navigation = generatedTree ? menuData.navigation : menuData;
    const section = docsScope === "zith" ? "Zith" :
        docsScope === "zith--" ? "Zith--" : null;
    treeModel = section
        ? navigation.map(group => ({
            ...group,
            children: (group.children || []).filter(item => item.title === section)
        }))
        : navigation;
    flatPages = [];
    pageMeta = new Map();
    aliasMap = new Map(Object.entries(generatedTree ? menuData.aliases || {} : {}));

    buildPageIndex(treeModel);
    renderMenu(treeModel, sidebar);

    const page = getPageFromURL() || defaultPage;
    loadPage(page, getAnchorFromURL(), { replaceHistory: !getPageFromURL() });
}

function renderMenu(items, parent) {
    const ul = document.createElement("ul");

    items.forEach(item => {
        const li = document.createElement("li");
        // Group headers (no link) render as plain labels, not as links.
        const a = document.createElement(item.link ? "a" : "span");
        a.className = "ui-nav-link";
        const title = document.createElement("span");
        title.className = "ui-nav-title";
        title.textContent = item.title;
        a.appendChild(title);

        if (item.badge) {
            const badge = document.createElement("span");
            badge.className = "ui-nav-badge";
            badge.textContent = item.badge;
            a.appendChild(badge);
        }

        if (item.link) {
            a.href = item.link;
            a.dataset.link = item.link;
        }

        li.appendChild(a);

        if (item.description) {
            const desc = document.createElement("span");
            desc.className = "ui-nav-description";
            desc.textContent = item.description;
            li.appendChild(desc);
        }

        if (item.children) {
            renderMenu(item.children, li);
        }

        ul.appendChild(li);
    });

    parent.appendChild(ul);
}

function buildPageIndex(items, trail = []) {
    items.forEach(item => {
        const nextTrail = item.title === "Home" ? trail : [...trail, item.title];

        if (item.link && !pageMeta.has(item.link)) {
            flatPages.push(item.link);
            pageMeta.set(item.link, {
                title: item.title,
                trail: nextTrail,
                description: sectionDescription(nextTrail)
            });
        }

        if (item.children) {
            buildPageIndex(item.children, nextTrail);
        }
    });
}

function updateFilebar(path) {
    const meta = pageMeta.get(path);
    const filename = path.split("/").pop() || "_";
    filebarPath.textContent = meta ? meta.trail.join(" / ") : filename;
}

function resolveAlias(path) {
    return aliasMap.get(path) || path;
}

function setActiveLink(path) {
    const links = sidebar.querySelectorAll("a[data-link]");
    links.forEach(link => {
        link.classList.toggle("is-active", link.dataset.link === path);
    });
}

function getPagerLinks(path) {
    const currentIndex = flatPages.indexOf(path);
    if (currentIndex === -1) {
        return { previous: null, next: null };
    }

    return {
        previous: currentIndex > 0 ? flatPages[currentIndex - 1] : null,
        next: currentIndex < flatPages.length - 1 ? flatPages[currentIndex + 1] : null
    };
}

function createPagerLink(label, path, direction) {
    if (!path) return null;

    const link = document.createElement("a");
    const meta = pageMeta.get(path);
    const title = meta ? meta.title : path.split("/").pop();

    link.href = path;
    link.dataset.link = path;
    link.className = "doc-nav-link";

    const span = document.createElement("span");
    span.className = "doc-nav-label";
    span.textContent = label;

    const strong = document.createElement("strong");
    strong.textContent = title;

    link.appendChild(span);
    link.appendChild(strong);

    if (direction === "next") {
        link.classList.add("doc-nav-link-next");
    }

    return link;
}

function injectPageNavigation(path) {
    const meta = pageMeta.get(path);
    if (!meta) return;

    const { previous, next } = getPagerLinks(path);
    if (!previous && !next) return;

    const nav = document.createElement("nav");
    nav.className = "doc-page-nav";
    nav.setAttribute("aria-label", "Documentation pagination");

    const previousLink = createPagerLink("Previous", previous, "previous");
    const nextLink = createPagerLink("Next", next, "next");

    if (previousLink) nav.appendChild(previousLink);
    if (nextLink) nav.appendChild(nextLink);

    content.appendChild(nav);
}

let initialLoad = true;

/* Zith code blocks carry class="language-zith"; highlight them with the same
   engine the landing page uses (js/zith-highlight.js). Other languages stay
   plain text so no wrong grammar is applied. */
function highlightZithBlocks(root) {
    if (!window.ZithHighlight) return;
    root.querySelectorAll("pre > code.language-zith").forEach(code => {
        code.innerHTML = window.ZithHighlight.highlight(code.textContent);
    });
}

function loadPage(path, anchor = null, opts = {}) {
    const canonicalPath = resolveAlias(path);
    fetch(new URL(canonicalPath, docsBase))
        .then(res => {
            if (!res.ok) throw new Error("Page not found");
            return res.text();
        })
        .then(html => {
            const wasInitialLoad = initialLoad;
            currentPage = canonicalPath;
            content.innerHTML = html;

            updateFilebar(canonicalPath);
            setActiveLink(canonicalPath);
            injectPageNavigation(canonicalPath);

            highlightZithBlocks(content);

            const filename = canonicalPath.split("/").pop() || "_";
            const titleMatch = html.match(/<h[12][^>]*>(.*?)<\/h[12]>/i);
            const pageTitle = titleMatch
                ? titleMatch[1].replace(/<[^>]+>/g, "").trim()
                : filename;
            const fullTitle = pageTitle + " — Zith Documentation";
            const meta = pageMeta.get(canonicalPath);
            const description = meta && meta.description ? meta.description : "Documentation for the Zith programming language.";
            const pageURL = new URL(window.location.href);
            pageURL.search = "?page=" + encodeURIComponent(path.replace("./", ""));
            pageURL.hash = "";
            const canonicalURL = pageURL.toString();

            document.title = fullTitle;

            const descriptionMeta = document.querySelector('meta[name="description"]');
            const ogTitle = document.querySelector('meta[property="og:title"]');
            const ogDescription = document.querySelector('meta[property="og:description"]');
            const ogURL = document.querySelector('meta[property="og:url"]');
            const twitterTitle = document.querySelector('meta[name="twitter:title"]');
            const twitterDescription = document.querySelector('meta[name="twitter:description"]');
            const twitterURL = document.querySelector('meta[name="twitter:url"]');
            const canonical = document.querySelector('link[rel="canonical"]');

            if (descriptionMeta) descriptionMeta.setAttribute("content", description);
            if (ogTitle) ogTitle.setAttribute("content", fullTitle);
            if (ogDescription) ogDescription.setAttribute("content", description);
            if (ogURL) ogURL.setAttribute("content", canonicalURL);
            if (twitterTitle) twitterTitle.setAttribute("content", fullTitle);
            if (twitterDescription) twitterDescription.setAttribute("content", description);
            if (twitterURL) twitterURL.setAttribute("content", canonicalURL);
            if (canonical) canonical.setAttribute("href", canonicalURL);

            if (initialLoad) {
                initialLoad = false;
            }
            const historyURL = "?page=" + path.replace("./", "") +
                (anchor ? "#" + encodeURIComponent(anchor) : "");
            if (opts.replaceHistory) {
                history.replaceState(
                    { path: path, anchor: anchor },
                    "", historyURL
                );
            } else if (!opts.fromPop && !wasInitialLoad) {
                history.pushState(
                    { path: path, anchor: anchor },
                    "", historyURL
                );
            }

            requestAnimationFrame(() => {
                if (anchor) {
                    requestAnimationFrame(() => {
                        const target = content.querySelector(`#${anchor}`);
                        if (!target) return;
                        target.scrollIntoView({ behavior: "smooth", block: "start" });
                    });
                } else {
                    window.scrollTo({ top: 0 });
                }
            });
        })
        .catch(() => {
            content.replaceChildren();
            const message = document.createElement("p");
            message.className = "docs-error";
            message.textContent = "Failed to load page content.";
            content.appendChild(message);
        });
}

function getPageFromURL() {
    const params = new URLSearchParams(window.location.search);
    const page = params.get("page");
    if (page) {
        return "./" + page.replace(/^\.\//, "");
    }
    return null;
}

function getAnchorFromURL() {
    return window.location.hash ? decodeURIComponent(window.location.hash.slice(1)) : null;
}

// Only links inside the docs content and sidebar are SPA links; the shared
// header/footer must keep navigating normally.
document.addEventListener("click", e => {
    const link = e.target.closest("a");
    if (!link) return;
    if (!link.closest(".docs-main, .docs-sidebar")) return;

    if (link.dataset.docsRoute) return;
    if (link.dataset.link) {
        e.preventDefault();
        loadPage(link.dataset.link);
        return;
    }

    const href = link.getAttribute("href");
    if (!href || href === "#") return;
    if (href.startsWith("http")) return;
    if (link.dataset.native === "true") return;

    e.preventDefault();

    const sourceURL = new URL(currentPage, docsBase);
    const targetURL = new URL(href, sourceURL);
    if (targetURL.origin !== window.location.origin ||
        !targetURL.pathname.startsWith(docsBase.pathname)) {
        window.location.assign(targetURL.href);
        return;
    }

    const routePath = targetURL.pathname.slice(docsBase.pathname.length);
    if (routePath === "zith/" || routePath === "zith--/") {
        window.location.assign(targetURL.href);
        return;
    }

    const pagePath = "./" + routePath;
    const anchor = targetURL.hash ? decodeURIComponent(targetURL.hash.slice(1)) : null;
    loadPage(pagePath, anchor);
});

window.addEventListener("popstate", e => {
    if (e.state && e.state.path) {
        loadPage(e.state.path, e.state.anchor || null, { fromPop: true });
    } else {
        const page = getPageFromURL();
        if (page) {
            loadPage(page, getAnchorFromURL(), { fromPop: true });
        }
    }
});
