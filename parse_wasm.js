const fs = require('fs');
const wasm = fs.readFileSync('playground/zith-playground.wasm');
WebAssembly.compile(wasm).then(module => {
    console.log(WebAssembly.Module.exports(module).map(e => e.name));
});
