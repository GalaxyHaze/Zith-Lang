
const windows = document.querySelectorAll(".window");

const windowStates = {};

let highestZ = 1;

windows.forEach(windowE1 => {

const titlebar = windowE1.querySelector(".titlebar");

let isDragging = false;
let offsetX = 0;
let offsetY = 0;

titlebar.addEventListener("pointerdown", (e) => {

    if(e.target.tagName === "BUTTON") return;
    isDragging = true;

    highestZ++;

    windowE1.style.zIndex = highestZ;

    document.body.style.userSelect = "none";

    offsetX = e.clientX - windowE1.offsetLeft;
    offsetY = e.clientY - windowE1.offsetTop;
});

document.addEventListener("pointermove", (e) => {
    if(!isDragging) return;

    const rect = windowE1.getBoundingClientRect();

    let newX = e.clientX - offsetX;
    let newY = e.clientY - offsetY;

    const maxX = window.innerWidth - rect.width;
    const maxY = window.innerHeight - rect.height;

    newX = Math.max(0, Math.min(newX,maxX));
    newY = Math.max(0, Math.min(newY,maxY));

    windowE1.style.left = newX + "px";
    windowE1.style.top = newY + "px";
});

document.addEventListener("pointerup", () => {
    document.body.style.userSelect = "auto";
    isDragging = false;
});

})

function openWindow(id){
    document.getElementById(id).classList.remove("hidden");
}
function closeWindow(id){
    document.getElementById(id).classList.add("hidden");
}

function toggleMaximize(id){
    const el = document.getElementById(id);

    if(!windowStates[id]){
        windowStates[id] = {
            maximized: false,
            top: el.style.top,
            left: el.style.left,
            height: el.style.height,
            width: el.style.width,  
        };
    }

    const state = windowStates[id];

    if(!state.maximized){
        state.top = el.style.top;
        state.left = el.style.left;
        state.height = el.style.height;
        state.width = el.style.width;

        el.style.top = "0";
        el.style.left = "0";
        el.style.height = "100vh";
        el.style.width = "100vw";
        
        highestZ++;
        el.style.zIndex = highestZ;

        state.maximized = true;
    } else {
        el.style.top = state.top;
        el.style.left = state.left;
        el.style.height = state.height;
        el.style.width = state.width;

        state.maximized = false;
    }
}