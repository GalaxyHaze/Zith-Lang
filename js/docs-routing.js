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

    function resolveLanguageSwitch(page, targetScope, topicEquivalents) {
        const normalizedPage = page.replace(/^\.\//, "");
        const equivalent = topicEquivalents[normalizedPage]?.[targetScope];
        const overview = targetScope === "zith"
            ? "zith/D-overview.html"
            : targetScope === "zith--"
                ? "zith--/D-overview.html"
                : null;

        if (!overview) {
            throw new Error(`Unknown documentation scope: ${targetScope}`);
        }

        return {
            page: `./${equivalent || overview}`,
            equivalent: Boolean(equivalent)
        };
    }

    function filterDocsNavigation(navigation, scope) {
        const home = navigation.find(group => group.title === "Home");
        if (!home) return navigation;

        const children = home.children || [];
        const languageTitle = scope === "zith" ? "Zith" :
            scope === "zith--" ? "Zith--" : null;
        const visible = languageTitle
            ? children.filter(item => item.title === languageTitle)
            : children.filter(item => item.title !== "Zith" && item.title !== "Zith--");
        return [{ ...home, children: visible }];
    }

    return { resolveDocsRoute, resolveLanguageSwitch, filterDocsNavigation };
}));
