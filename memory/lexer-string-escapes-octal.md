# String Escapes: Octal `\NNN` Added, `\0` No Longer Truncates

This note records the fix for ANSI color codes such as `\033` silently
corrupting a string literal in Zith--, and the earlier root cause. It is useful
when touching `decodeEscapes` in `src/sema/hir-lower-utils.cpp`, the
string-literal path in `src/sema/hir-lower-expr-value.cpp`, or the sema literal
checks in `src/sema/sema-expr.cpp`.

## Status: Fixed

`decodeEscapes` now accepts C-style octal escapes `\NNN` (one to three octal
digits), so `\033` is the ESC byte and `\0` followed by digits no longer leaves
the digits as literal characters. `PerModuleSema::checkLiteralEscapes` runs in
`inferLiteral` and reports `E0003` for unknown escapes and for char literals
that do not decode to exactly one character, so `check` now agrees with
`build`/`run`. The sections below keep the original diagnosis for context.

## Symptom

A program that prints an ANSI escape sequence produces no visible output:

```zith
import "stdio.h"

fn main(argc: i32, argv: **char){
    _ = printf("\033[1;31mThis text is bold red.\033[0m\n");
}
```

The same program with a plain string (`printf("normal text\n")`) prints fine.
`check` reports no error for the colored version; the failure is silent.

## Root Cause

The language has no octal escapes. The documented escape set is
`\n`, `\r`, `\t`, `\0`, `\\`, `\'`, `\"`, `\xHH`, plus the `\#` extension
(`docs/Zith--.md`, "Strings e caracteres decodificam o conjunto de escapes
estilo C"). `decodeEscapes` in `src/sema/hir-lower-utils.cpp` implements
exactly that set.

When it sees `\0`, it emits a single NUL byte and does not consume following
digits. So `\033` decodes to NUL followed by the literal characters `3`, `3`:

```
"\033[1;31m..."  ->  \0 '3' '3' '[' '1' ';' ... "m" \n
```

`printf` stops at the embedded NUL, so the entire string prints nothing. This
is why the escape appears to "swallow" the string. The `run` path emits no
diagnostic because `\0` is a valid escape and `3` is a valid character.

Proof via LLVM IR (`zithc --emit-ir build`):

```
@".strA\0033B\0A" = private constant [7 x i8] c"A\0033B\0A\00"
```

The byte list is `A`, `\00`, `3`, `3`, `B`, `\n`.

## Correct Spelling

Both the hexadecimal and the octal forms now work. Hexadecimal for the ESC
byte (`0x1b`):

```zith
_ = printf("\x1b[1;31mThis text is bold red.\x1b[0m\n");
```

and the C-style octal form:

```zith
_ = printf("\033[1;31mThis text is bold red.\033[0m\n");
```

Both print the expected ANSI sequences. Before the fix only `\xHH` worked and
`\NNN` was misread as a NUL plus literal digits.

## Frontend Versus Sema

The scanner in `src/frontend/ast-lowerer.cpp` accepts any `\` + char while
tokenizing a string, so an invalid escape is not caught lexically. Before the
fix, validation happened only in `decodeEscapes` during HIR lowering, so
`check` (which stops at `TypeChecked`) accepted an unknown escape such as `\q`
while `build`/`run` reported `E0003`. Now `PerModuleSema::checkLiteralEscapes`
validates during sema, so `check` reports `E0003` too.

## Known Follow-Ups

- The frontend scanner still accepts any `\` + char; validation lives in sema
  (`checkLiteralEscapes`) and lowering. A future cleanup could validate the
  escape set in the scanner so the error span matches the scanner's token.
- Octal runs are capped at three digits like C, so `\400` overflows modulo 256
  (`\400` is byte 0x00). This matches C behaviour but is worth documenting if
  a stricter rule is ever wanted.

## Reproducer Files

Manual repros in `/tmp` avoid stale cache. Minimal cases:

```zith
_ = printf("A\tB\n");      // tab
_ = printf("A\033B\n");    // ESC (fixed); was NUL + "33" before
_ = printf("A\x1bB\n");    // ESC byte
_ = printf("A\qB\n");      // check and build both report E0003 now
```
