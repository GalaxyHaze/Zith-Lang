/* Character-by-character reveal, scoped to the Code Spotlight viewer and the
   install commands (see notes/decisions/0004-caret-escopado.md).

   type(element, text, options):
   - render(prefix) decides what a partial string becomes (default: textContent).
     The Code Spotlight passes a highlighter so code stays coloured as it grows.
   - Animates instantly when the user prefers reduced motion. */
(function () {
    "use strict";

    var reducedMotion = window.matchMedia &&
        window.matchMedia("(prefers-reduced-motion: reduce)").matches;

    function type(element, text, options) {
        options = options || {};
        var render = options.render || function (prefix) { element.textContent = prefix; };
        var perCharMs = options.perCharMs || 16;

        if (element.__typewriterTimer) clearTimeout(element.__typewriterTimer);

        if (reducedMotion) {
            render(text);
            element.classList.remove("is-typing");
            return;
        }

        var index = 0;
        element.classList.add("is-typing");

        function step() {
            if (index >= text.length) {
                render(text);
                element.classList.remove("is-typing");
                element.__typewriterTimer = null;
                return;
            }
            render(text.slice(0, index));
            index += 1;
            element.__typewriterTimer = setTimeout(step, perCharMs);
        }

        step();
    }

    window.ZithTypewriter = { type: type };
})();
