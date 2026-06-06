# Error Codes Reference

All Zith errors follow the pattern `error[E0123]: message` at compile time and `error[R0123]: message` at runtime.
In release mode, runtime errors abort immediately — look up the code here to understand what happened.

---

## Compile-time Errors (E)

### Lexical (E0001–E0999)

| Code | Title | Tip |
|------|-------|-----|
| E0001 | Unknown token | Check for stray or invalid characters in the source |
| E0002 | Unclosed string literal | Add a closing double quote to terminate the string |
| E0003 | Invalid escape sequence | Use valid escapes: `\n`, `\t`, `\"`, `\\`, `\0` |
| E0004 | Invalid integer literal | Ensure digits are valid for the given base (0x, 0o, 0b) |

### Parse (E1001–E1999)

| Code | Title | Tip |
|------|-------|-----|
| E1001 | Expected expression | Write a value, variable, or function call here |
| E1002 | Expected semicolon | Terminate statements with `;` |
| E1003 | Unclosed parenthesis | Add a closing `)` to match the opening `(` |
| E1004 | Expected identifier | A name (variable, function, type) is required here |

### Semantic (E2001–E2999)

| Code | Title | Tip |
|------|-------|-----|
| E2001 | Undefined identifier | Bind the name with `let` or pass it as a parameter |
| E2002 | Duplicate declaration | Rename one of the declarations or remove the duplicate |
| E2003 | Wrong number of arguments | Check the function signature and provide the correct number of arguments |
| E2004 | Unused declaration | Remove the unused binding or prefix with `_` |

### Types (E3001–E3999)

| Code | Title | Tip |
|------|-------|-----|
| E3001 | Type mismatch | Convert the value to the expected type or change the expression |
| E3002 | Cannot infer type | Add an explicit type annotation to disambiguate |
| E3003 | Invalid cast | The source and target types are not compatible for casting |

### Ownership / NRA (E4001–E4999)

| Code | Title | Tip |
|------|-------|-----|
| E4001 | Use after move | Re-bind the value before using it, or pass a reference instead |
| E4002 | Borrow conflict | A mutable reference conflicts with existing borrows |
| E4003 | Double borrow | Only one mutable borrow is allowed at a time |

### MIR / Lowering (E5001–E5999)

| Code | Title | Tip |
|------|-------|-----|
| E5001 | Invalid IR | This is an internal compiler error — report it |
| E5002 | Unreachable code | Remove the dead code or add a condition guard |

---

## Runtime Errors (R)

### Panic (R10001+)

| Code | Title | Tip |
|------|-------|-----|
| R10001 | Index out of bounds | Ensure the index is less than the array or slice length |
| R10002 | Division by zero | Check the divisor is non-zero before dividing |
| R10003 | Null dereference | Check if the optional value is `Some` before unwrapping |
| R10004 | Panic | An unrecoverable error occurred — check the error context |
