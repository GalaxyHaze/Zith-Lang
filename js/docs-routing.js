(function (root, factory) {
    const routing = factory();
    if (typeof module === "object" && module.exports) {
        module.exports = routing;
    } else {
        root.DocsRouting = routing;
    }
}(typeof globalThis === "object" ? globalThis : this, function () {
    const routes = [
        { suffix: "/docs/zith--", scope: "zith--", page: "./zith--/D-overview.html" },
        { suffix: "/docs/zith", scope: "zith", page: "./zith/D-overview.html" },
        { suffix: "/docs", scope: "neutral", page: "./D-overview.html" }
    ];

    function resolveDocsRoute(pathname) {
        const normalized = (pathname.replace(/\/index\.html$/, "").replace(/\/+$/, "")) || "/";
        const route = routes.find(candidate => normalized.endsWith(candidate.suffix));
        return route
            ? { scope: route.scope, page: route.page }
            : { scope: "neutral", page: "./D-overview.html" };
    }

    return { resolveDocsRoute };
}));
