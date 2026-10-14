# Diagnostics are structured and accumulated

Diagnostics are structured records (severity, code, byte-offset span, message),
accumulated in the shared context and returned in the result. They are not
formatted strings and not exceptions.

Structured diagnostics let the CLI driver and the language-server driver consume
the same result without re-parsing text, and match the existing WASM ABI, which
already exposes severity, message, code, and a byte-offset span. Best-effort
analysis over incomplete source needs diagnostics to be data, not control flow.
