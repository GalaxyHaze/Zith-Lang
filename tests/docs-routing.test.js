const assert = require("node:assert/strict");
const { execFileSync } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const test = require("node:test");

const { resolveDocsRoute } = require("../js/docs-routing.js");
const root = path.resolve(__dirname, "..");
const specDir = [
    path.join(root, "Zith", "docs"),
    path.resolve(root, "..", "Zith", "docs")
].find(candidate => fs.existsSync(candidate));

test("the docs root is the neutral comparison page", () => {
    assert.deepEqual(resolveDocsRoute("/docs/"), {
        scope: "neutral",
        page: "./D-overview.html"
    });
    assert.deepEqual(resolveDocsRoute("/docs/index.html"), {
        scope: "neutral",
        page: "./D-overview.html"
    });
    assert.deepEqual(resolveDocsRoute("/preview/docs/zith/"), {
        scope: "zith",
        page: "./zith/D-overview.html"
    });
});

test("the language routes open their own overview, with or without a trailing slash", () => {
    assert.deepEqual(resolveDocsRoute("/docs/zith"), {
        scope: "zith",
        page: "./zith/D-overview.html"
    });
    assert.deepEqual(resolveDocsRoute("/docs/zith/"), {
        scope: "zith",
        page: "./zith/D-overview.html"
    });
    assert.deepEqual(resolveDocsRoute("/docs/zith/index.html"), {
        scope: "zith",
        page: "./zith/D-overview.html"
    });
    assert.deepEqual(resolveDocsRoute("/docs/zith--"), {
        scope: "zith--",
        page: "./zith--/D-overview.html"
    });
    assert.deepEqual(resolveDocsRoute("/docs/zith--/"), {
        scope: "zith--",
        page: "./zith--/D-overview.html"
    });
    assert.deepEqual(resolveDocsRoute("/docs/zith--/index.html"), {
        scope: "zith--",
        page: "./zith--/D-overview.html"
    });
});

test("the static build emits an index shell for each docs route", () => {
    assert.ok(specDir, "the canonical Zith specification should be checked out");
    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), "zith-doc-routes-"));
    const output = path.join(tempDir, "site");

    try {
        execFileSync("python3", [
            "tools/build.py", "--out", output, "--baseurl", "/preview"
        ], {
            cwd: root,
            stdio: "inherit"
        });
        execFileSync("python3", [
            "tools/build_docs.py", "--site-dir", output, "--spec-dir", specDir
        ], {
            cwd: root,
            stdio: "inherit"
        });
        execFileSync("python3", [
            "tools/build_docs.py", "--check", "--site-dir", output, "--spec-dir", specDir
        ], {
            cwd: root,
            stdio: "inherit"
        });

        for (const [route, scope] of [
            ["docs/D-home.html", "neutral"],
            ["docs/index.html", "neutral"],
            ["docs/zith/index.html", "zith"],
            ["docs/zith--/index.html", "zith--"]
        ]) {
            const shell = fs.readFileSync(path.join(output, route), "utf8");
            assert.match(shell, new RegExp(`data-docs-scope="${scope}"`));
            assert.doesNotMatch(shell, /\{\{baseurl\}\}|include:header/);
            assert.match(shell, /data-docs-base="\/preview\/docs\/"/);
            assert.match(shell, /data-tree-url="\/preview\/json\/tree\.json"/);
        }

        for (const target of [
            "docs/D-overview.html",
            "docs/zith/D-overview.html",
            "docs/zith--/D-overview.html",
            "json/tree.json",
            "js/docs-routing.js",
            "js/loadTree.js"
        ]) {
            assert.ok(fs.existsSync(path.join(output, target)), `${target} should be published`);
        }

        const landing = fs.readFileSync(path.join(output, "docs/D-overview.html"), "utf8");
        assert.match(landing, /data-docs-route="zith"/);
        assert.match(landing, /data-docs-route="zith--"/);
        const tree = JSON.parse(fs.readFileSync(path.join(output, "json/tree.json"), "utf8"));
        assert.equal(tree.version, 4);
    } finally {
        fs.rmSync(tempDir, { recursive: true, force: true });
    }
});

test("the Pages workflow generates documentation fragments before deployment", () => {
    const workflow = fs.readFileSync(
        path.join(root, ".github/workflows/build.yml"),
        "utf8"
    );

    assert.match(workflow, /repository:\s*GalaxyHaze\/Zith/);
    assert.match(
        workflow,
        /python3 tools\/build_docs\.py --site-dir _site --spec-dir Zith\/docs/
    );
    assert.match(
        workflow,
        /python3 tools\/build_docs\.py --check --site-dir _site --spec-dir Zith\/docs/
    );
});
