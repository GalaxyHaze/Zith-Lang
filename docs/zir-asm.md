# ZIR Assembly Format (`.zas`)

Register-based intermediate representation for the Zith compiler. Designed for both
fast threaded-code interpretation and LLVM IR lowering.

## Lexical Conventions

| Token | Meaning |
|-------|---------|
| `; comment text` | Line comment (to end of line) |
| `# multi-line text #` | Block comment (non-nestable) |
| `label:` | Label (defines a block boundary) |
| `ident` | Identifier (instruction mnemonic, register alias, symbol name) |
| `123`, `-42` | Decimal integer literal |
| `0xFF` | Hex integer literal |
| `0b1010` | Binary integer literal |
| `"hello"` | String literal (constant pool) |
| `.directive args` | Assembler directive |
| `,` | Comma (operand separator — optional, for readability) |
| `\n` | Instruction/directive terminator |

## Register Aliases

```
zero    r0   hardwired zero (reads as 0, writes ignored)
sp      r1   stack pointer
fp      r2   frame pointer
ret     r3   return value register
a0      r4   argument 0
a1      r5   argument 1
a2      r6   argument 2
a3      r7   argument 3
a4      r8   argument 4
a5      r9   argument 5
a6      r10  argument 6
ap      r11  stack args pointer (>7 args)
t0-r19  r12-r28  general purpose / temporaries
db      r29  data segment base (set by loader)
t17     r30  general purpose
link    r31  link register (written by Call)
```

In instructions you may use either the alias or `rN` form.

## Sections

A `.zas` file has one implicit **code section** (interleaved with directives).
The linker/loader maps `db` to the **data section** (constants, globals).

## Directives

### `.extern`

Declare an external C function for FFI.

```
.extern  name,  arg_count
```

Example:
```
.extern  puts,  1
.extern  printf, 2
```

### `.const`

Add an entry to the data section constant pool. The assembler assigns sequential
offsets starting at 0 within the data segment.

```
.const  label,  value
```

`value` can be:

| Form | Encoding | Size |
|------|----------|------|
| `42` | Signed 64-bit integer | 8 bytes |
| `3.14` | IEEE 754 double | 8 bytes |
| `"hello\n"` | Null-terminated string | strlen + 1 |
| `.asciz "hello\n"` | Same as bare string | strlen + 1 |
| `.i64 42` | Explicit typed constant | 8 bytes |
| `.i32 100` | 32-bit integer (zero-extended) | 4 bytes |
| `.ptr label` | Address of a code block or data label | 8 bytes |

`.const` labels can be used as immediates in `lea` instructions, which resolve
to `db + offset_of_label` at runtime.

Example:
```
.const  msg,   "Hello, World!\n"
.const  answer, 42

main:
    lea   a0, db, msg
    callext puts, 1
    li    ret, 0
    ret
```

### `.data`

Define arbitrary data block. Useful for structs, arrays, vtables:

```
.data  label
    .i64   1, 2, 3, 4
    .i32   100, 200
    .asciz "done"
```

Labels defined with `.data` resolve to the same offset space as `.const`.

### Macros (textual replacement)

Simple C-style `#define` for textual substitution within the file.

```
#define REGION_SIZE 4096
#define EXIT(val)   li ret, val; ret

main:
    li   a0, REGION_SIZE
    alloca t0, REGION_SIZE
    EXIT(0)
```

Macros are processed during lexing as simple text replacement (no parameters in
the initial version — just single-token or sequence replacement).

## Instruction Set

### Data Movement

```
mov   rd, rs1           rd = rs1
li    rd, imm           rd = imm (43-bit signed constant)
lea   rd, base, offset  rd = base + offset   # (like x86 LEA)
ld    rd, base, offset  rd = mem[base + offset]
st    rs2, base, offset mem[base + offset] = rs2
ldr   rd, base, idx     rd = mem[base + idx]      # register offset
str   rs2, base, idx    mem[base + idx] = rs2      # register offset
```

### Integer Arithmetic

```
add   rd, rs1, rs2
sub   rd, rs1, rs2
mul   rd, rs1, rs2
div   rd, rs1, rs2
rem   rd, rs1, rs2
udiv  rd, rs1, rs2
urem  rd, rs1, rs2
```

### Float Arithmetic

```
fadd  rd, rs1, rs2
fsub  rd, rs1, rs2
fmul  rd, rs1, rs2
fdiv  rd, rs1, rs2
frem  rd, rs1, rs2
```

### Bitwise & Shift

```
and   rd, rs1, rs2
or    rd, rs1, rs2
xor   rd, rs1, rs2
shl   rd, rs1, rs2    # logical left
shr   rd, rs1, rs2    # logical right
ashr  rd, rs1, rs2    # arithmetic right
```

### Comparison (rd = 0 or 1)

```
seqz  rd, rs1          rd = rs1 == 0
snez  rd, rs1          rd = rs1 != 0

slt   rd, rs1, rs2     rd = rs1 < rs2   (signed)
sle   rd, rs1, rs2     rd = rs1 <= rs2  (signed)
sgt   rd, rs1, rs2     rd = rs1 > rs2   (signed)
sge   rd, rs1, rs2     rd = rs1 >= rs2  (signed)

sltu  rd, rs1, rs2     rd = rs1 < rs2   (unsigned)
sleu  rd, rs1, rs2     rd = rs1 <= rs2  (unsigned)
sgtu  rd, rs1, rs2     rd = rs1 > rs2   (unsigned)
sgeu  rd, rs1, rs2     rd = rs1 >= rs2  (unsigned)

fseq  rd, rs1, rs2     rd = rs1 == rs2  (float)
fslt  rd, rs1, rs2     rd = rs1 < rs2   (float)
fsle  rd, rs1, rs2     rd = rs1 <= rs2  (float)
fsgt  rd, rs1, rs2     rd = rs1 > rs2   (float)
fsge  rd, rs1, rs2     rd = rs1 >= rs2  (float)
```

### Type Conversion

```
sext    rd, rs1         sign extend
zext    rd, rs1         zero extend
trunc   rd, rs1         truncate
ftoi    rd, rs1         float → signed int
itof    rd, rs1         signed int → float
ftou    rd, rs1         float → unsigned int
utof    rd, rs1         unsigned int → float
bitcast rd, rs1         reinterpret bits
ptoi    rd, rs1         pointer → int
itop    rd, rs1         int → pointer
fptrunc rd, rs1         double → float
fpext   rd, rs1         float → double
```

### Control Flow

```
br    label               unconditional branch
bz    rs1, label          branch if rs1 == 0
bnz   rs1, label          branch if rs1 != 0
bgt   rs1, label          branch if rs1 > 0
bge   rs1, label          branch if rs1 >= 0
blt   rs1, label          branch if rs1 < 0
ble   rs1, label          branch if rs1 <= 0

call  target_label, nargs  internal call (link = pc+1, jump to label)
callext extern_name, nargs external C call
ret                        return (pc = link)
select rd, cond, t, f      rd = cond ? t : f
```

Note: `call` is **not** an immediate instruction — the assembler resolves the
label to a register load + branch. The target function's address is loaded into
`link` and control is transferred. This means recursion is handled by saving
`link` on entry.

### Memory Management

```
alloca   rd, size    rd = allocate(size bytes on stack)
memset   rd, val, n  mem[rd..rd+n-1] = val   (byte fill)
memcpy   rd, rs1, n  mem[rd..rd+n-1] = mem[rs1..rs1+n-1]
```

### Special

```
syscall   rd, num, nargs  rd = syscall(num, a0..)
halt                      terminate execution
nop                       no operation
unreachable               dead code marker (halt in interpreter)
```

## Instruction Encoding (64-bit fixed width)

Two formats selected by opcode:

**R-format** (3 registers + small immediate):
```
bits 63-56: opcode  (8)
bits 55-48: type    (8)   — metadata tag for LLVM lowering, zero in assembly
bits 47-43: rd      (5)
bits 42-38: rs1     (5)
bits 37-33: rs2     (5)
bits 32-0:  imm33   (33 signed)
```

**I-format** (1 register + large immediate):
```
bits 63-56: opcode  (8)
bits 55-48: type    (8)
bits 47-43: rd      (5)
bits 42-0:  imm43   (43 signed)
```

The `type` byte is always 0 in assembly. The LLVM lowering pass or compiler
frontend fills it for typed codegen.

## Opcode Map

| Range | Category | Mnemonics |
|-------|----------|-----------|
| 0x00–0x04 | Special | `halt`, `nop`, `unreachable` |
| 0x05–0x0A | Data | `mov`, `li`, `ld`, `st`, `ldr`, `str` |
| 0x0B–0x11 | Int arith | `add`, `sub`, `mul`, `div`, `rem`, `udiv`, `urem` |
| 0x12–0x16 | Float arith | `fadd`, `fsub`, `fmul`, `fdiv`, `frem` |
| 0x17–0x1C | Bitwise | `and`, `or`, `xor`, `shl`, `shr`, `ashr` |
| 0x1D–0x28 | Int cmp | `seqz`, `snez`, `slt`..`sgeu` (12) |
| 0x29–0x2D | Float cmp | `fseq`, `fslt`, `fsle`, `fsgt`, `fsge` |
| 0x2E–0x39 | Conv | `sext`..`fpext` (12) |
| 0x3A–0x44 | Control | `br`, `bz`, `bnz`, `bgt`, `bge`, `blt`, `ble`, `call`, `callext`, `ret`, `select` |
| 0x45–0x47 | Memory | `alloca`, `memset`, `memcpy` |
| 0x48 | FFI | `syscall` |
| 0x49–0x4A | Thread | `tlsld`, `tlsst` (reserved) |
| 0x4B–0xFF | — | Reserved |

## Example: Factorial

```zas
; factorial.zas — compute 5!
.extern  printf, 2

.const  fmt,  "fact(%d) = %d\n"
.const  input, 5

entry:
    lea   a0, db, fmt           ; a0 = format string
    lea   a1, db, input         ; a1 = &input (but we need the value)
    ld    a1, db, input         ; a1 = 5   (load the constant's value)
    mov   a2, a1                ; save n for later display
    mov   t0, a1                ; t0 = n (argument for fact)

    ; call fact(n)
    call  fact, 1               ; result in ret
    mov   a2, ret               ; a2 = fact(5)

    lea   a0, db, fmt           ; a0 = format string
    lea   a1, db, input
    ld    a1, db, input         ; a1 = 5
    callext printf, 3           ; printf(fmt, n, fact(n))
    li    ret, 0
    ret

; fact(n) — recursive
fact:
    ; save link
    alloca t1, 8
    st    link, t1, 0           ; mem[t1] = link
    mov   t2, a0                ; t2 = n
    ble   t2, 1, base           ; if n <= 1, return 1

    ; fact(n-1)
    sub   a0, t2, 1             ; a0 = n - 1
    call  fact, 1
    mul   ret, t2, ret          ; return n * fact(n-1)
    br    done

base:
    li    ret, 1                ; return 1

done:
    ld    link, t1, 0           ; restore link
    ret
```

## Tools

```
zasm file.zas              # assemble + run (interpret)
zasm file.zas -o out.zir   # assemble to binary only
zasm file.zas --asm        # assemble + print disassembly
zasm file.zas --emit-llvm  # (future) assemble + emit LLVM IR
```
