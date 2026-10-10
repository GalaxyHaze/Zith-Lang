/* Portal page: install tabs, copy button, latest dev-log entries. */
(function () {
    "use strict";

    var installCmds = {
        brew: "brew tap galaxyhaze/zithc && brew install zithc",
        scoop: "scoop bucket add zithc https://github.com/GalaxyHaze/Zith.git && scoop install zithc",
        curl: "curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.sh | sh",
        ps1: "Invoke-RestMethod https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.ps1 | Invoke-Expression"
    };
    var currentPm = "brew";

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

    function initInstall() {
        var tabs = document.querySelectorAll("#installTabs .install-tab");
        var cmd = document.getElementById("installCmd");
        var copyBtn = document.getElementById("copyInstallBtn");
        if (!tabs.length || !cmd) return;

        tabs.forEach(function (tab) {
            tab.addEventListener("click", function () {
                var pm = tab.dataset.pm;
                if (!installCmds[pm]) return;
                currentPm = pm;
                cmd.textContent = installCmds[pm];
                tabs.forEach(function (other) {
                    other.classList.toggle("is-active", other === tab);
                });
            });
        });

        if (copyBtn) {
            copyBtn.addEventListener("click", function () {
                copyText(installCmds[currentPm], copyBtn);
            });
        }
    }

    function initNews() {
        var grid = document.getElementById("newsGrid");
        if (!grid) return;
        fetch("./about/posts/index.json")
            .then(function (response) {
                if (!response.ok) throw new Error("news fetch failed");
                return response.json();
            })
            .then(function (entries) {
                if (!Array.isArray(entries) || !entries.length) {
                    grid.replaceChildren();
                    return;
                }
                grid.replaceChildren();
                entries.slice(0, 3).forEach(function (entry) {
                    var card = document.createElement("article");
                    card.className = "news-card";
                    var date = document.createElement("span");
                    date.className = "news-date";
                    date.textContent = entry.date;
                    var heading = document.createElement("h3");
                    var link = document.createElement("a");
                    link.href = "./about/posts/" + entry.file;
                    link.textContent = entry.title;
                    heading.appendChild(link);
                    card.appendChild(date);
                    card.appendChild(heading);
                    grid.appendChild(card);
                });
            })
            .catch(function () {
                grid.replaceChildren();
            });
    }

    function initSpotlight() {
        var spotlight = document.querySelector("[data-code-spotlight]");
        if (spotlight && window.ZithCodeSpotlight) window.ZithCodeSpotlight.init(spotlight);
    }

    initInstall();
    initNews();
    initSpotlight();
})();
