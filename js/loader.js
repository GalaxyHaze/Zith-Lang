const lines = [
    { type: "info", text: "Booting system..." },
    { type: "ok", text: "Booting system." },

    { type: "info", text: "Loading modules..." },
    { type: "ok", text: "Loading modules." },

    { type: "info", text: "Checking assets..." },
    { type: "ok", text: "Checking assets." },

    { type: "info", text: "Connecting to server..." },
    { type: "ok", text: "Connecting to server." },

    { type: "done", text: "Done"},
];

const terminal = document.getElementById("terminal");

let displayedLines = [];
let index = 0;

function typeLine() {

    if (index >= lines.length) {
        setTimeout(() => {
            document.getElementById("loader").style.display = "none";
        }, 500);
        return;
    }

    const line = lines[index];

    if (line.type === "ok") {
        displayedLines[displayedLines.length - 1] =
            `[ <span class="ok">OK</span> ] ${line.text}`;
    } else if(line.type === "info"){
        displayedLines.push(`       ${line.text}`);
    } else if (line.type === "done"){
        displayedLines.push(`[ <span class="ok">${line.text}</span> ]`);
    } else {
        displayedLines.push(line.text);
    }

    terminal.innerHTML = displayedLines.join("<br>");

    index++;
    setTimeout(typeLine, 300);
}

window.addEventListener("DOMContentLoaded", () => {
    const loader = document.getElementById("loader");

    const hasBooted = localStorage.getItem("boot_done");
    if (hasBooted) {
        loader.style.display = "none";
        return;
    }
    localStorage.setItem("boot_done", "true");
    typeLine();
});