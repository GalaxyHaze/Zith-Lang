/* Ambient audio toggle — flat, opt-in, injected on every page.
   Off by default; the browser only plays after a user gesture. */
(function () {
    "use strict";

    var STORAGE_KEY = "zws:ambient";
    var script = document.currentScript || document.querySelector('script[src*="ambient.js"]');
    var src = new URL("../media/audio/ambient.mp3", script.src).href;
    var audio = null;
    var enabled = false;

    function readEnabled() {
        try {
            return localStorage.getItem(STORAGE_KEY) === "on";
        } catch (_) {
            return false;
        }
    }

    function writeEnabled(on) {
        try {
            localStorage.setItem(STORAGE_KEY, on ? "on" : "off");
        } catch (_) {}
    }

    function ensureAudio() {
        if (audio) return audio;
        audio = new Audio(src);
        audio.loop = true;
        audio.preload = "none";
        audio.addEventListener("play", function () { setToggle(true); });
        audio.addEventListener("pause", function () { setToggle(false); });
        return audio;
    }

    function setToggle(playing) {
        var toggle = document.getElementById("audioToggle");
        if (!toggle) return;
        toggle.classList.toggle("is-playing", playing);
        toggle.setAttribute("aria-pressed", playing ? "true" : "false");
        toggle.setAttribute("aria-label", playing ? "Pause ambient audio" : "Play ambient audio");
    }

    function play() {
        enabled = true;
        writeEnabled(true);
        var promise = ensureAudio().play();
        if (promise && promise.catch) promise.catch(function () { setToggle(false); });
    }

    function pause() {
        enabled = false;
        writeEnabled(false);
        if (audio) audio.pause();
        setToggle(false);
    }

    function toggle() {
        if (audio && !audio.paused) pause();
        else play();
    }

    function injectToggle() {
        if (document.getElementById("audioToggle")) return;
        var button = document.createElement("button");
        button.id = "audioToggle";
        button.type = "button";
        button.className = "audio-toggle";
        button.setAttribute("aria-pressed", "false");
        button.setAttribute("aria-label", "Play ambient audio");
        button.innerHTML =
            '<svg viewBox="0 0 24 24" aria-hidden="true" width="18" height="18">' +
            '<path d="M9 18V6l10-2v12" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/>' +
            '<circle cx="6.5" cy="18" r="2.5" fill="none" stroke="currentColor" stroke-width="1.7"/>' +
            '<circle cx="16.5" cy="16" r="2.5" fill="none" stroke="currentColor" stroke-width="1.7"/></svg>';
        button.addEventListener("click", toggle);
        document.body.appendChild(button);
        if (enabled) play();
    }

    enabled = readEnabled();

    if (document.body) injectToggle();
    document.addEventListener("DOMContentLoaded", injectToggle);
})();
