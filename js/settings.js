var EFFECTS_KEY = 'zith_effects';

var defaults = {
    scanlines: 1,
    glow: 1,
    noise: 1
};

var scanlinesConfig = {
    0: { scanlineAlpha: 0 },
    1: { scanlineAlpha: 0.2 },
    2: { scanlineAlpha: 0.5 }
};

var glowConfig = {
    0: { iconShadow: 0, textGlowAlpha: 0 },
    1: { iconShadow: 0.3, textGlowAlpha: 0.5 },
    2: { iconShadow: 0.6, textGlowAlpha: 1 }
};

var noiseConfig = {
    0: { noiseOpacity: 0, vignetteOpacity: 0, animation: 'none' },
    1: { noiseOpacity: 0.05, vignetteOpacity: 0.6, animation: 'flicker-low 0.15s infinite' },
    2: { noiseOpacity: 0.1, vignetteOpacity: 0.8, animation: 'flicker-high 0.15s infinite' }
};

var levelNames = { 0: 'Off', 1: 'Low', 2: 'High' };

function applyEffects(settings) {
    var root = document.documentElement;

    var scan = scanlinesConfig[settings.scanlines];
    root.style.setProperty('--crt-scanline-alpha', scan.scanlineAlpha);

    var glow = glowConfig[settings.glow];
    root.style.setProperty('--icon-shadow-opacity', glow.iconShadow);
    root.style.setProperty('--text-glow-alpha', glow.textGlowAlpha);

    var noise = noiseConfig[settings.noise];
    root.style.setProperty('--crt-noise-opacity', noise.noiseOpacity);
    root.style.setProperty('--crt-vignette-opacity', noise.vignetteOpacity);
    root.style.setProperty('--crt-flicker-animation', noise.animation);

    document.querySelectorAll('[data-setting]').forEach(function(btn) {
        var setting = btn.dataset.setting;
        var level = parseInt(btn.dataset.level);
        btn.classList.toggle('active', level === settings[setting]);
    });

    document.querySelectorAll('.settings-group').forEach(function(group) {
        var setting = group.dataset.group;
        var status = group.querySelector('.settings-status');
        if (status) {
            status.textContent = 'Current: ' + levelNames[settings[setting]];
        }
    });
}

function saveEffects(settings) {
    applyEffects(settings);
    try {
        localStorage.setItem(EFFECTS_KEY, JSON.stringify(settings));
    } catch (e) {
        // Settings applied in-memory; persistence unavailable
    }
}

function loadEffects() {
    var saved = localStorage.getItem(EFFECTS_KEY);
    if (saved) {
        try {
            return JSON.parse(saved);
        } catch (e) {
            localStorage.removeItem(EFFECTS_KEY);
        }
    }
    return Object.assign({}, defaults);
}

(function() {
    var settings = loadEffects();
    applyEffects(settings);

    document.querySelectorAll('[data-setting]').forEach(function(btn) {
        btn.addEventListener('click', function() {
            var setting = btn.dataset.setting;
            var level = parseInt(btn.dataset.level);
            settings[setting] = level;
            saveEffects(settings);
        });
    });
})();
