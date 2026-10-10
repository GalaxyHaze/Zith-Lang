/* Dev Log index: lists posts from about/posts/index.json (no remote innerHTML). */
(function () {
    "use strict";

    var list = document.getElementById("postList");
    if (!list) return;

    fetch("posts/index.json")
        .then(function (response) {
            if (!response.ok) throw new Error("dev log fetch failed");
            return response.json();
        })
        .then(function (entries) {
            if (!Array.isArray(entries) || !entries.length) {
                list.replaceChildren();
                return;
            }
            list.replaceChildren();
            entries.forEach(function (entry) {
                var item = document.createElement("li");
                item.className = "devlog-item";

                var date = document.createElement("span");
                date.className = "devlog-date";
                date.textContent = entry.date;

                var link = document.createElement("a");
                link.className = "devlog-link";
                link.href = "posts/" + entry.file;
                link.textContent = entry.title;

                item.appendChild(date);
                item.appendChild(link);
                list.appendChild(item);
            });
        })
        .catch(function () {
            list.replaceChildren();
            var item = document.createElement("li");
            item.className = "devlog-error";
            item.textContent = "Failed to load entries.";
            list.appendChild(item);
        });
})();
