const sidebar = document.getElementById("sidebar");
const content = document.getElementById("content");
const filebarPath = document.getElementById("filebarPath");

(function loadMenu() {
    const cached = sessionStorage.getItem("tree_json");
    if (cached) {
        try {
            renderMenu(JSON.parse(cached), sidebar);
            return;
        } catch (_) {}
    }

    fetch("../../json/tree.json")
        .then(response => {
            if (!response.ok) throw new Error("Failed to load navigation");
            return response.json();
        })
        .then(menu => {
            try {
                sessionStorage.setItem("tree_json", JSON.stringify(menu));
            } catch (_) {}
            renderMenu(menu, sidebar);
        })
        .catch(() => {
            sidebar.innerHTML = "<p style='color:#ff6d1f;padding:8px'>Failed to load navigation</p>";
        });
})();

function renderMenu(items, parent) {
    const ul = document.createElement("ul");

    items.forEach(item => {
        const li = document.createElement("li");
        const a = document.createElement("a");
        a.textContent = item.title;
        a.href = item.link || "#";

        if (item.title === "Home") {
            a.dataset.native = true;
        }

        li.appendChild(a);

        if (item.children) {
            renderMenu(item.children, li);
        }

        ul.appendChild(li);
    });

    parent.appendChild(ul);
}

function updateFilebar(path) {
    const filename = path.split("/").pop() || "_";
    filebarPath.textContent = filename;
}

let initialLoad = true;

function loadPage(path, anchor = null, opts = {}) {
    fetch(path)
        .then(res => {
            if (!res.ok) throw new Error("Page not found");
            return res.text();
        })
        .then(html => {
            content.innerHTML = html;

            updateFilebar(path);

            const filename = path.split("/").pop() || "_";
            const titleMatch = html.match(/<h1>([^<]+)<\/h1>/);
            const pageTitle = titleMatch ? titleMatch[1] : filename;
            document.title = pageTitle + " — Zith Documentation";

            const skipPush = opts.fromPop || initialLoad;
            if (initialLoad) {
                initialLoad = false;
            }
            if (!skipPush) {
                history.pushState(
                    { path: path, anchor: anchor },
                    "",
                    "?page=" + path.replace("./", "")
                );
            }

            requestAnimationFrame(() => {
                if (anchor) {
                    requestAnimationFrame(() => {
                        const target = content.querySelector(`#${anchor}`);
                        if (!target) return;

                        const container = document.querySelector(".bottom");
                        const offsetTop = target.offsetTop;

                        container.scrollTo({
                            top: offsetTop,
                            behavior: "smooth"
                        });
                    });
                }
            });
        })
        .catch(() => {
            content.innerHTML = "<p style='color:#ff6d1f'>Failed to load page content.</p>";
        });
}

function getPageFromURL() {
    const params = new URLSearchParams(window.location.search);
    const page = params.get("page");
    if (page) {
        return "./" + page;
    }
    return null;
}

document.addEventListener("click", e => {
    const link = e.target.closest("a");
    if (!link) return;

    const href = link.getAttribute("href");
    if (!href || href === "#") return;
    if (href.startsWith("http")) return;
    if (link.dataset.native === "true") return;

    e.preventDefault();

    const [file, anchor] = href.split("#");
    loadPage(file, anchor || null);
});

window.addEventListener("popstate", e => {
    if (e.state && e.state.path) {
        loadPage(e.state.path, e.state.anchor || null, { fromPop: true });
    } else {
        const page = getPageFromURL();
        if (page) {
            loadPage(page, null, { fromPop: true });
        }
    }
});

{
    const page = getPageFromURL();
    if (page) {
        loadPage(page);
    }
}
