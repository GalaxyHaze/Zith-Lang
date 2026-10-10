/* Reusable Code Spotlight component.
   Markup: <div class="spotlight-grid" data-code-spotlight="json/code-spotlight.json"></div>
   Tabs and viewer are built from the JSON with DOM APIs (no remote innerHTML). */
(function () {
    "use strict";

    function el(tag, className, text) {
        var node = document.createElement(tag);
        if (className) node.className = className;
        if (text != null) node.textContent = text;
        return node;
    }

    function highlight(text) {
        return window.ZithHighlight
            ? window.ZithHighlight.highlight(text)
            : String(text == null ? "" : text);
    }

    function copyText(text, button) {
        function done() {
            var original = button.textContent;
            button.textContent = "Copied!";
            setTimeout(function () { button.textContent = original; }, 1500);
        }
        function fallback() {
            var area = document.createElement("textarea");
            area.value = text;
            area.setAttribute("readonly", "");
            area.style.position = "fixed";
            area.style.opacity = "0";
            document.body.appendChild(area);
            area.select();
            try { document.execCommand("copy"); } catch (_) {}
            document.body.removeChild(area);
            done();
        }
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text).then(done).catch(fallback);
        } else {
            fallback();
        }
    }

    function renderEntry(container, entry) {
        container.__spotlight.current = entry;
        container.querySelector(".window-filename").textContent = entry.file;
        var code = container.querySelector("code");
        if (window.ZithTypewriter) {
            window.ZithTypewriter.type(code, entry.code, {
                render: function (prefix) { code.innerHTML = highlight(prefix); }
            });
        } else {
            code.innerHTML = highlight(entry.code);
        }
        container.querySelector(".code-explanation").innerHTML = entry.explanation;
        container.querySelectorAll(".tab-btn").forEach(function (button) {
            button.classList.toggle("active", button.dataset.key === entry.key);
        });
    }

    function build(container, entries) {
        var byKey = {};
        entries.forEach(function (entry) { byKey[entry.key] = entry; });
        container.__spotlight = { byKey: byKey };

        var tabs = el("div", "spotlight-tabs");
        entries.forEach(function (entry) {
            var button = el("button", "tab-btn", entry.label);
            button.type = "button";
            button.dataset.key = entry.key;
            button.addEventListener("click", function () { renderEntry(container, entry); });
            tabs.appendChild(button);
        });
        container.appendChild(tabs);

        var viewer = el("div", "code-viewer");
        var bar = el("div", "code-window-bar");
        bar.appendChild(el("span", "window-filename"));
        var copy = el("button", "button button-secondary copy-btn", "Copy Code");
        copy.type = "button";
        copy.addEventListener("click", function () {
            var current = container.__spotlight.current;
            if (current) copyText(current.code, copy);
        });
        bar.appendChild(copy);
        viewer.appendChild(bar);

        var block = el("div", "code-block");
        var pre = el("pre");
        pre.appendChild(el("code"));
        block.appendChild(pre);
        viewer.appendChild(block);
        viewer.appendChild(el("p", "code-explanation"));
        container.appendChild(viewer);

        renderEntry(container, entries[0]);
    }

    function init(container) {
        var url = container.getAttribute("data-code-spotlight");
        if (!url) return;
        fetch(url)
            .then(function (response) {
                if (!response.ok) throw new Error("spotlight fetch failed");
                return response.json();
            })
            .then(function (payload) {
                var entries = Array.isArray(payload) ? payload : payload.snippets;
                if (!Array.isArray(entries) || !entries.length) return;
                build(container, entries);
            })
            .catch(function () {
                container.replaceChildren(el("p", "news-status", "Failed to load code samples."));
            });
    }

    window.ZithCodeSpotlight = { init: init };
})();
