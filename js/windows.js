const windows = document.querySelectorAll(".window");
const windowStates = {};
let highestZ = 1;

let activeDrag = null;

windows.forEach(windowEl => {
    const titlebar = windowEl.querySelector(".titlebar");

    const state = {
        el: windowEl,
        isDragging: false,
        offsetX: 0,
        offsetY: 0,
        startLeft: 0,
        startTop: 0,
        translateX: 0,
        translateY: 0,
    };
    windowStates[windowEl.id] = state;

    titlebar.addEventListener("pointerdown", (e) => {
        if (e.target.tagName === "BUTTON") return;

        highestZ++;
        windowEl.style.zIndex = highestZ;

        const rect = windowEl.getBoundingClientRect();
        state.offsetX = e.clientX - rect.left;
        state.offsetY = e.clientY - rect.top;
        state.startLeft = parseInt(windowEl.style.left) || 0;
        state.startTop = parseInt(windowEl.style.top) || 0;
        state.translateX = 0;
        state.translateY = 0;
        state.isDragging = true;
        activeDrag = state;

        document.body.style.userSelect = "none";
        windowEl.style.willChange = "transform";
    });
});

document.addEventListener("pointermove", (e) => {
    if (!activeDrag) return;

    const state = activeDrag;
    const el = state.el;

    state.translateX = e.clientX - state.offsetX - state.startLeft;
    state.translateY = e.clientY - state.offsetY - state.startTop;

    const rect = el.getBoundingClientRect();
    const maxX = window.innerWidth - rect.width;
    const maxY = window.innerHeight - rect.height;

    const clampedLeft = Math.max(0, Math.min(state.startLeft + state.translateX, maxX));
    const clampedTop = Math.max(0, Math.min(state.startTop + state.translateY, maxY));
    state.translateX = clampedLeft - state.startLeft;
    state.translateY = clampedTop - state.startTop;

    el.style.transform = `translate(${state.translateX}px, ${state.translateY}px)`;
});

document.addEventListener("pointerup", () => {
    if (!activeDrag) return;

    const state = activeDrag;
    const el = state.el;

    const newLeft = state.startLeft + state.translateX;
    const newTop = state.startTop + state.translateY;

    el.style.left = `${newLeft}px`;
    el.style.top = `${newTop}px`;
    el.style.transform = "none";
    el.style.willChange = "auto";

    state.isDragging = false;
    document.body.style.userSelect = "auto";
    activeDrag = null;
});

function openWindow(id) {
    document.getElementById(id).classList.remove("hidden");
}

function closeWindow(id) {
    document.getElementById(id).classList.add("hidden");
}

function toggleMaximize(id) {
    const el = document.getElementById(id);

    if (!windowStates[id + "_max"]) {
        windowStates[id + "_max"] = {
            maximized: false,
            top: el.style.top,
            left: el.style.left,
            height: el.style.height,
            width: el.style.width,
        };
    }

    const state = windowStates[id + "_max"];

    if (!state.maximized) {
        state.top = el.style.top;
        state.left = el.style.left;
        state.height = el.style.height;
        state.width = el.style.width;

        el.style.transform = "none";
        el.style.top = "0";
        el.style.left = "0";
        el.style.height = "100vh";
        el.style.width = "100vw";

        highestZ++;
        el.style.zIndex = highestZ;

        state.maximized = true;
    } else {
        el.style.top = state.top || "";
        el.style.left = state.left || "";
        el.style.height = state.height || "";
        el.style.width = state.width || "";

        state.maximized = false;
    }
}
