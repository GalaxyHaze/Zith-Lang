const http = require('http');
const { exec } = require('child_process');
const path = require('path');

const PORT = 37891;
const DOCS_DIR = path.resolve(__dirname, 'docs');

const EDITOR = process.env.EDITOR_CMD || 'code-oss';

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://localhost:${PORT}`);
  const filePath = url.searchParams.get('file');

  res.setHeader('Access-Control-Allow-Origin', '*');

  if (!filePath) {
    res.writeHead(400);
    res.end('Missing ?file= parameter');
    return;
  }

  const fullPath = path.join(DOCS_DIR, filePath);

  exec(`"${EDITOR}" "${fullPath}"`, (err) => {
    if (err) {
      console.error(`Failed to open ${fullPath} with ${EDITOR}:`, err.message);
      res.writeHead(500);
      res.end(`Failed to open editor: ${err.message}`);
      return;
    }
    console.log(`Opened ${fullPath} in ${EDITOR}`);
    res.end('OK');
  });
});

server.listen(PORT, () => {
  console.log(`[open-editor] Listening on http://localhost:${PORT}`);
  console.log(`[open-editor] Editor: ${EDITOR}`);
  console.log(`[open-editor] Docs dir: ${DOCS_DIR}`);
});
