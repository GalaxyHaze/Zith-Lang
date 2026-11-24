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
        let pending = null;
        for (const command of commands) {
            if (pending) {
                pending = pending.then(succeeded => {
                    if (!succeeded) return false;
                    return Promise.resolve(dispatch(command)).then(result => result !== false);
                });
                continue;
            }
            const result = dispatch(command);
            if (result && typeof result.then === "function") {
                pending = Promise.resolve(result).then(value => value !== false);
            } else if (result === false) {
                return false;
            }
        }
        return pending || true;
    }

    const api = { parseCommandChain, dispatchCommandChain };
    if (typeof module === "object" && module.exports) module.exports = api;
    else root.PlaygroundCommandChain = api;
})(typeof window === "undefined" ? globalThis : window);
