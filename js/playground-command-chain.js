(function (root) {
    "use strict";

    function parseCommandChain(input) {
        if (typeof input !== "string") {
            throw new TypeError("Command must be a string.");
        }

        const commands = input.split("&&").map(command => command.trim());
        if (commands.some(command => command.length === 0)) {
            throw new Error("Empty command in chain; remove the extra '&&'.");
        }
        return commands;
    }

    function dispatchCommandChain(input, dispatch, onValidated) {
        if (typeof dispatch !== "function") {
            throw new TypeError("Command dispatcher must be a function.");
        }

        const commands = parseCommandChain(input);
        if (typeof onValidated === "function") {
            onValidated(commands);
        }
        for (const command of commands) {
            if (dispatch(command) === false) {
                return false;
            }
        }
        return true;
    }

    const api = { parseCommandChain, dispatchCommandChain };
    if (typeof module === "object" && module.exports) module.exports = api;
    else root.PlaygroundCommandChain = api;
})(typeof window === "undefined" ? globalThis : window);
