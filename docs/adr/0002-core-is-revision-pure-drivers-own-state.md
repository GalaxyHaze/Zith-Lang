# The core is revision-pure; drivers own persistent state

The core compiles one revision of input and holds no long-lived state. The CLI
driver is thin. The language-server driver owns a persistent workspace context
and re-runs the core for each document snapshot.

This is the deliberate split between what the language server uses and what the
plain compiler uses. The persistent state that a language server needs
(interners, module cache, semantic context) belongs to the driver, so the core
stays a function over a revision and remains testable in isolation. Pointers
into core data must not escape the driver.
