# Documentation editor service

The website is static on GitHub Pages. This Cloudflare Worker handles
authenticated PR creation; the GitHub App private key and user authorization
tokens stay out of the browser. Markdown editing and preview use source files
included in the static build and do not require this service.

## GitHub App

Create a GitHub App owned by `GalaxyHaze` with these settings:

- Homepage URL: `https://zith-lang.org`
- User authorization callback URL: `https://docs-api.zith-lang.org/auth/callback`
- Leave automatic user authorization during installation disabled; the editor starts the OAuth flow itself.
- Repository permissions: Contents read/write and Pull requests read/write.
- Install it only on `Zith-Lang` and `Zith`.

The app creates a branch and a pull request in the selected repository. Its
installation token is further scoped to that one repository for each publish.
The user token is only used to identify the signed-in GitHub account.

## Cloudflare

The custom domain `docs-api.zith-lang.org` must be available in the Cloudflare
account that deploys the Worker. Create the KV namespace, then replace the
placeholder `id` in `wrangler.toml` with the namespace ID:

From the repository root, convert the PEM key downloaded from the app's
settings to PKCS#8 because the Worker imports it with Web Crypto. Then create
the KV namespace, put its ID in `wrangler.toml`, and set the app values as
Worker secrets:

```sh
openssl pkcs8 -topk8 -nocrypt -in /path/to/app-private-key.pem -out /tmp/app-private-key-pkcs8.pem
cd worker
npx wrangler kv namespace create DOCS_SESSIONS
# Replace the placeholder namespace ID in wrangler.toml before deploying.
npx wrangler secret put GITHUB_APP_ID
npx wrangler secret put GITHUB_CLIENT_ID
npx wrangler secret put GITHUB_CLIENT_SECRET
npx wrangler secret put GITHUB_INSTALLATION_ID
npx wrangler secret put GITHUB_PRIVATE_KEY
npx wrangler deploy
```

When prompted for `GITHUB_PRIVATE_KEY`, paste the contents of
`/tmp/app-private-key-pkcs8.pem`.

`SITE_ORIGINS` in `wrangler.toml` allows the production site and local
`make serve` preview. Remove localhost origins for a production-only service
if browser-based local testing is not needed.

## Checks

Run the isolated Worker tests from the repository root:

```sh
node tests/docs-editor.test.js
```

The browser editor stores drafts in that browser's local storage. Publishing
requires the deployed Worker and GitHub App installation; without them, the
editor and preview still work, but the `Publish` action cannot create a PR.
