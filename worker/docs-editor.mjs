const API_VERSION = "2026-03-10";
const SESSION_TTL_SECONDS = 60 * 60;
const OAUTH_TTL_SECONDS = 10 * 60;
const MAX_REQUEST_BYTES = 1_500_000;
const MAX_FILES = 64;

const repositories = {
    "GalaxyHaze/Zith-Lang": {
        branch: "gh-pages",
        allows: path => path.startsWith("docs/pages/") && path.endsWith(".md")
    },
    "GalaxyHaze/Zith": {
        branch: "main",
        allows: path => [
            "docs/Zith-spec.md",
            "docs/impl-status.md",
            "docs/Zith--.md",
            ...Array.from({ length: 20 }, (_, index) =>
                `docs/${String(index + 2).padStart(2, "0")}-${[
                    "module-system", "type-system", "traits-interfaces", "functions",
                    "mutability-bindings", "memory-model", "error-handling",
                    "control-flow", "concurrency", "comptime", "assets", "raw-unsafe",
                    "polymorphism", "macros", "words", "contexts", "c-interop",
                    "project-config", "standard-library", "best-practices"
                ][index]}.md`)
        ].includes(path)
    }
};

function json(data, status = 200, headers = {}) {
    return new Response(JSON.stringify(data), {
        status,
        headers: { "Content-Type": "application/json; charset=utf-8", ...headers }
    });
}

function corsHeaders(request, env) {
    const origin = request.headers.get("Origin");
    if (!origin || !allowedOrigins(env).includes(origin)) return {};
    return {
        "Access-Control-Allow-Origin": origin,
        "Access-Control-Allow-Methods": "POST, OPTIONS",
        "Access-Control-Allow-Headers": "Authorization, Content-Type",
        "Access-Control-Max-Age": "600",
        "Vary": "Origin"
    };
}

function apiJson(request, env, data, status = 200) {
    return json(data, status, {
        "Cache-Control": "no-store",
        ...corsHeaders(request, env)
    });
}

function allowedOrigins(env) {
    return (env.SITE_ORIGINS || "https://zith-lang.org")
        .split(",").map(origin => origin.trim()).filter(Boolean);
}

function apiError(request, env, message, status = 400) {
    return apiJson(request, env, { error: message }, status);
}

function decodeBase64Url(value) {
    const padded = value.replace(/-/g, "+").replace(/_/g, "/") +
        "=".repeat((4 - value.length % 4) % 4);
    return Uint8Array.from(atob(padded), character => character.charCodeAt(0));
}

function base64Url(bytes) {
    let binary = "";
    bytes.forEach(byte => { binary += String.fromCharCode(byte); });
    return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, "");
}

function randomToken(byteLength = 32) {
    const bytes = crypto.getRandomValues(new Uint8Array(byteLength));
    return base64Url(bytes);
}

async function sha256Base64Url(value) {
    const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));
    return base64Url(new Uint8Array(digest));
}

function safeReturnUrl(raw, env) {
    const origins = allowedOrigins(env);
    const fallback = `${origins[0] || "https://zith-lang.org"}/docs/`;
    try {
        const result = new URL(raw || fallback);
        if (!origins.includes(result.origin) || !result.pathname.startsWith("/docs/")) return fallback;
        result.hash = "";
        return result.href;
    } catch (_) {
        return fallback;
    }
}

function sessionCookie(state, maxAge) {
    return `docs_oauth_state=${state}; Path=/auth/callback; Max-Age=${maxAge}; Secure; HttpOnly; SameSite=Lax`;
}

function decodePemPrivateKey(pem) {
    const base64 = pem
        .replace(/-----BEGIN PRIVATE KEY-----/g, "")
        .replace(/-----END PRIVATE KEY-----/g, "")
        .replace(/\s/g, "");
    return decodeBase64Url(base64.replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/g, ""));
}

async function createAppJwt(env) {
    const key = await crypto.subtle.importKey(
        "pkcs8",
        decodePemPrivateKey(env.GITHUB_PRIVATE_KEY),
        { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" },
        false,
        ["sign"]
    );
    const now = Math.floor(Date.now() / 1000);
    const header = base64Url(new TextEncoder().encode(JSON.stringify({
        alg: "RS256",
        typ: "JWT"
    })));
    const payload = base64Url(new TextEncoder().encode(JSON.stringify({
        iat: now - 60,
        exp: now + 540,
        iss: env.GITHUB_APP_ID
    })));
    const message = `${header}.${payload}`;
    const signature = await crypto.subtle.sign(
        "RSASSA-PKCS1-v1_5",
        key,
        new TextEncoder().encode(message)
    );
    return `${message}.${base64Url(new Uint8Array(signature))}`;
}

async function githubRequest(path, token, init = {}) {
    const headers = new Headers(init.headers || {});
    if (!headers.has("Accept")) headers.set("Accept", "application/vnd.github+json");
    headers.set("Authorization", `Bearer ${token}`);
    headers.set("X-GitHub-Api-Version", API_VERSION);
    headers.set("User-Agent", "Zith-Docs-Editor");
    if (init.body && !headers.has("Content-Type")) {
        headers.set("Content-Type", "application/json");
    }
    const response = await fetch(`https://api.github.com${path}`, { ...init, headers });
    if (!response.ok) {
        const detail = await response.json().catch(() => ({}));
        const error = new Error(detail.message || `GitHub API request failed (${response.status}).`);
        error.status = response.status;
        throw error;
    }
    return response;
}

async function currentUser(token) {
    const response = await fetch("https://api.github.com/user", {
        headers: {
            "Accept": "application/vnd.github+json",
            "Authorization": `Bearer ${token}`,
            "X-GitHub-Api-Version": API_VERSION,
            "User-Agent": "Zith-Docs-Editor"
        }
    });
    if (!response.ok) throw new Error("GitHub could not verify the signed-in account.");
    const user = await response.json();
    return { id: user.id, login: user.login };
}

async function getInstallationToken(repository, env) {
    const jwt = await createAppJwt(env);
    const repositoryName = repository.split("/")[1];
    const response = await fetch(
        `https://api.github.com/app/installations/${encodeURIComponent(env.GITHUB_INSTALLATION_ID)}/access_tokens`,
        {
            method: "POST",
            headers: {
                "Accept": "application/vnd.github+json",
                "Authorization": `Bearer ${jwt}`,
                "Content-Type": "application/json",
                "X-GitHub-Api-Version": API_VERSION,
                "User-Agent": "Zith-Docs-Editor"
            },
            body: JSON.stringify({
                repositories: [repositoryName],
                permissions: { contents: "write", pull_requests: "write" }
            })
        }
    );
    if (!response.ok) throw new Error("The documentation GitHub App is not installed on this repository.");
    return (await response.json()).token;
}

async function getSession(request, env) {
    const authorization = request.headers.get("Authorization") || "";
    const match = authorization.match(/^Bearer ([A-Za-z0-9_-]{40,})$/);
    if (!match) return null;
    const record = await env.DOCS_SESSIONS.get(`session:${match[1]}`, "json");
    if (!record || record.expiresAt < Date.now()) return null;
    return record;
}

function validSource(repository, branch, path) {
    const config = repositories[repository];
    return Boolean(config && config.branch === branch &&
        typeof path === "string" && !path.includes("..") &&
        !path.startsWith("/") && config.allows(path));
}

async function startAuth(request, env) {
    const url = new URL(request.url);
    const state = randomToken(24);
    const verifier = randomToken(32);
    const returnUrl = safeReturnUrl(url.searchParams.get("return"), env);
    await env.DOCS_SESSIONS.put(`oauth:${state}`, JSON.stringify({
        verifier,
        returnUrl
    }), { expirationTtl: OAUTH_TTL_SECONDS });
    const authorize = new URL("https://github.com/login/oauth/authorize");
    authorize.searchParams.set("client_id", env.GITHUB_CLIENT_ID);
    authorize.searchParams.set("redirect_uri", `${new URL(request.url).origin}/auth/callback`);
    authorize.searchParams.set("state", state);
    authorize.searchParams.set("code_challenge", await sha256Base64Url(verifier));
    authorize.searchParams.set("code_challenge_method", "S256");
    return new Response(null, {
        status: 302,
        headers: {
            "Location": authorize.href,
            "Set-Cookie": sessionCookie(state, OAUTH_TTL_SECONDS),
            "Cache-Control": "no-store"
        }
    });
}

async function authCallback(request, env) {
    const url = new URL(request.url);
    const state = url.searchParams.get("state") || "";
    const cookie = request.headers.get("Cookie") || "";
    const cookieState = cookie.match(/(?:^|;\s*)docs_oauth_state=([A-Za-z0-9_-]+)/)?.[1] || "";
    const stateRecord = state && state === cookieState
        ? await env.DOCS_SESSIONS.get(`oauth:${state}`, "json")
        : null;
    const clearCookie = sessionCookie("", 0);
    if (!stateRecord || !url.searchParams.get("code")) {
        return new Response("GitHub sign-in could not be verified. Close this page and try again.", {
            status: 400,
            headers: { "Content-Type": "text/plain; charset=utf-8", "Set-Cookie": clearCookie }
        });
    }
    await env.DOCS_SESSIONS.delete(`oauth:${state}`);
    const tokenResponse = await fetch("https://github.com/login/oauth/access_token", {
        method: "POST",
        headers: { "Accept": "application/json", "Content-Type": "application/json" },
        body: JSON.stringify({
            client_id: env.GITHUB_CLIENT_ID,
            client_secret: env.GITHUB_CLIENT_SECRET,
            code: url.searchParams.get("code"),
            redirect_uri: `${url.origin}/auth/callback`,
            code_verifier: stateRecord.verifier
        })
    });
    const tokenResult = await tokenResponse.json();
    if (!tokenResponse.ok || !tokenResult.access_token) {
        return new Response("GitHub sign-in failed. Close this page and try again.", {
            status: 502,
            headers: { "Content-Type": "text/plain; charset=utf-8", "Set-Cookie": clearCookie }
        });
    }
    const user = await currentUser(tokenResult.access_token);
    const session = randomToken(32);
    await env.DOCS_SESSIONS.put(`session:${session}`, JSON.stringify({
        user,
        oauthToken: tokenResult.access_token,
        expiresAt: Date.now() + SESSION_TTL_SECONDS * 1000
    }), { expirationTtl: SESSION_TTL_SECONDS });
    const destination = new URL(stateRecord.returnUrl);
    destination.hash = `docs-session=${session}`;
    return new Response(null, {
        status: 302,
        headers: {
            "Location": destination.href,
            "Set-Cookie": clearCookie,
            "Cache-Control": "no-store",
            "Referrer-Policy": "no-referrer"
        }
    });
}

async function readRepositoryFile(repository, path, ref, token) {
    const encodedPath = path.split("/").map(encodeURIComponent).join("/");
    const response = await githubRequest(
        `/repos/${repository}/contents/${encodedPath}?ref=${encodeURIComponent(ref)}`,
        token,
        { headers: { "Accept": "application/vnd.github.raw+json" } }
    );
    return response.text();
}

async function publishEndpoint(request, env) {
    const contentLength = Number(request.headers.get("Content-Length") || 0);
    if (contentLength > MAX_REQUEST_BYTES) {
        return apiError(request, env, "The draft is too large to publish.", 413);
    }
    const session = await getSession(request, env);
    if (!session) return apiError(request, env, "Sign in again to publish this draft.", 401);

    let input;
    try {
        const body = await request.text();
        if (new TextEncoder().encode(body).length > MAX_REQUEST_BYTES) {
            return apiError(request, env, "The draft is too large to publish.", 413);
        }
        input = JSON.parse(body);
    } catch (_) {
        return apiError(request, env, "The publish request is not valid JSON.", 400);
    }
    const { repository, branch, files } = input || {};
    const config = repositories[repository];
    if (!config || config.branch !== branch) {
        return apiError(request, env, "This repository or target branch is not allowed.", 400);
    }
    if (!Array.isArray(files) || files.length < 1 || files.length > MAX_FILES) {
        return apiError(request, env, `Publish between 1 and ${MAX_FILES} Markdown pages at a time.`, 400);
    }
    const seen = new Set();
    let totalBytes = 0;
    for (const file of files) {
        if (!file || !validSource(repository, branch, file.path) ||
            typeof file.content !== "string" || typeof file.baseContent !== "string" ||
            seen.has(file.path)) {
            return apiError(request, env, "The draft contains an invalid or duplicate Markdown path.", 400);
        }
        seen.add(file.path);
        totalBytes += new TextEncoder().encode(file.content).length +
            new TextEncoder().encode(file.baseContent).length;
    }
    if (totalBytes > MAX_REQUEST_BYTES) {
        return apiError(request, env, "The draft is too large to publish.", 413);
    }

    const token = await getInstallationToken(repository, env);
    const ref = await githubRequest(
        `/repos/${repository}/git/ref/heads/${encodeURIComponent(branch)}`,
        token
    ).then(response => response.json());
    const baseSha = ref.object.sha;
    const baseCommit = await githubRequest(
        `/repos/${repository}/git/commits/${baseSha}`,
        token
    ).then(response => response.json());

    for (const file of files) {
        const current = await readRepositoryFile(repository, file.path, baseSha, token);
        if (current !== file.baseContent) {
            return apiError(
                request,
                env,
                `“${file.path}” changed in the repository since you started editing. Reload it and reapply your changes.`,
                409
            );
        }
    }
    const changedFiles = files.filter(file => file.content !== file.baseContent);
    if (!changedFiles.length) return apiError(request, env, "There are no changes to publish.", 400);

    const rateKey = `publish-rate:${session.user.id}`;
    const recentPublishes = Number(await env.DOCS_SESSIONS.get(rateKey) || "0");
    if (recentPublishes >= 5) {
        return apiError(request, env, "Please wait before submitting another documentation PR.", 429);
    }
    await env.DOCS_SESSIONS.put(rateKey, String(recentPublishes + 1), {
        expirationTtl: 60 * 60
    });

    const tree = await githubRequest(`/repos/${repository}/git/trees`, token, {
        method: "POST",
        body: JSON.stringify({
            base_tree: baseCommit.tree.sha,
            tree: changedFiles.map(file => ({
                path: file.path,
                mode: "100644",
                type: "blob",
                content: file.content
            }))
        })
    }).then(response => response.json());
    const commit = await githubRequest(`/repos/${repository}/git/commits`, token, {
        method: "POST",
        body: JSON.stringify({
            message: `docs: update ${changedFiles.length} page${changedFiles.length === 1 ? "" : "s"} via web editor`,
            tree: tree.sha,
            parents: [baseSha],
            author: {
                name: session.user.login,
                email: `${session.user.id}+${session.user.login}@users.noreply.github.com`
            }
        })
    }).then(response => response.json());
    const branchName = `docs-editor-${Date.now()}-${randomToken(5)}`;
    await githubRequest(`/repos/${repository}/git/refs`, token, {
        method: "POST",
        body: JSON.stringify({ ref: `refs/heads/${branchName}`, sha: commit.sha })
    });

    try {
        const pull = await githubRequest(`/repos/${repository}/pulls`, token, {
            method: "POST",
            body: JSON.stringify({
                title: `Docs: update ${changedFiles.length} page${changedFiles.length === 1 ? "" : "s"}`,
                head: branchName,
                base: branch,
                body: [
                    "Proposed from the Zith documentation editor.",
                    "",
                    `Editor: @${session.user.login}`,
                    "",
                    "Pages:",
                    ...changedFiles.map(file => `- \`${file.path}\``)
                ].join("\n")
            })
        }).then(response => response.json());
        return apiJson(request, env, {
            number: pull.number,
            url: pull.html_url
        }, 201);
    } catch (error) {
        await githubRequest(
            `/repos/${repository}/git/refs/heads/${encodeURIComponent(branchName)}`,
            token,
            { method: "DELETE" }
        ).catch(() => {});
        throw error;
    }
}

async function handleRequest(request, env) {
    const url = new URL(request.url);
    if (request.method === "OPTIONS" && url.pathname.startsWith("/api/")) {
        return new Response(null, { status: 204, headers: corsHeaders(request, env) });
    }
    if (url.pathname === "/auth/start" && request.method === "GET") {
        return startAuth(request, env);
    }
    if (url.pathname === "/auth/callback" && request.method === "GET") {
        return authCallback(request, env);
    }
    if (url.pathname === "/api/publish" && request.method === "POST") {
        return publishEndpoint(request, env);
    }
    return json({ error: "Not found." }, 404);
}

export default {
    async fetch(request, env) {
        const origin = request.headers.get("Origin");
        if (origin && request.url.includes("/api/") &&
            !allowedOrigins(env).includes(origin)) {
            return json({ error: "Origin not allowed." }, 403);
        }
        try {
            return await handleRequest(request, env);
        } catch (error) {
            const url = new URL(request.url);
            if (url.pathname.startsWith("/api/")) {
                return apiError(request, env, error.message || "The request failed.", error.status || 502);
            }
            return new Response("The documentation editor service could not complete this request.", {
                status: 502,
                headers: { "Content-Type": "text/plain; charset=utf-8" }
            });
        }
    }
};

export {
    createAppJwt,
    githubRequest,
    handleRequest,
    repositories,
    safeReturnUrl,
    validSource
};
