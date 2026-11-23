const assert = require("node:assert/strict");
const test = require("node:test");

const { dispatchCommandChain, parseCommandChain } =
    require("../js/playground-command-chain.js");

test("command chains trim segments and preserve their order", () => {
    assert.deepEqual(parseCommandChain("  clear  &&  run  "), ["clear", "run"]);
});

test("clear && run clears old output then runs the current editor buffer", () => {
    let output = "previous output";
    const editorBuffer = "fn main(): i32 { 0 }";
    let ranSource = "";

    const succeeded = dispatchCommandChain("clear && run", command => {
        if (command === "clear") {
            output = "";
            return true;
        }
        if (command === "run") {
            ranSource = editorBuffer;
            output = `ran: ${ranSource}`;
            return true;
        }
        return false;
    });

    assert.equal(succeeded, true);
    assert.equal(ranSource, editorBuffer);
    assert.equal(output, `ran: ${editorBuffer}`);
});

test("empty leading, middle, and trailing segments reject before dispatch", async t => {
    for (const command of ["&& run", "clear && && run", "clear &&"]) {
        await t.test(JSON.stringify(command), () => {
            let dispatchCount = 0;
            let validationCount = 0;
            assert.throws(
                () => dispatchCommandChain(command, () => {
                    dispatchCount++;
                }, () => {
                    validationCount++;
                }),
                /Empty command/
            );
            assert.equal(dispatchCount, 0);
            assert.equal(validationCount, 0);
        });
    }
});

test("a failed command stops the chain", () => {
    const dispatched = [];
    const succeeded = dispatchCommandChain("run && build && check", command => {
        dispatched.push(command);
        return command !== "build";
    });

    assert.equal(succeeded, false);
    assert.deepEqual(dispatched, ["run", "build"]);
});

test("async commands preserve order and stop the chain on failure", async () => {
    const dispatched = [];
    const succeeded = await dispatchCommandChain("run && build && check", async command => {
        dispatched.push(command);
        return command !== "build";
    });

    assert.equal(succeeded, false);
    assert.deepEqual(dispatched, ["run", "build"]);
});

test("a validated command is echoed once before dispatch", () => {
    const events = [];
    dispatchCommandChain("clear && run", command => {
        events.push(`dispatch:${command}`);
        return true;
    }, commands => {
        events.push(`echo:${commands.join(" && ")}`);
    });

    assert.deepEqual(events, ["echo:clear && run", "dispatch:clear", "dispatch:run"]);
});
