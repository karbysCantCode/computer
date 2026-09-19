# Spasm Language Documentation

Spasm is the assembly language compiled by the `src3` toolchain (`Spasm::Parser`, `Spasm::Preprocessor`, `Spasm::Lexer`, `Spasm::Linker`, `Spasm::OutputGenerator`). It is **architecture-agnostic**: instruction mnemonics, register names, opcodes, and operand encodings are not hardcoded into the language — they're supplied by a separate `.arch` file (see `Arch::Architecture`) which the assembler loads alongside the source. This document covers only the Spasm language itself: syntax, directives, expressions, data declarations, and relaxors.

This documentation is based directly on the current `src3` source (lexer/parser/preprocessor/linker), not the older top-level `spasmSyntax.md`, and calls out places where the two disagree.

---

## 1. Lexical Basics

- Whitespace (space, tab) separates tokens and is otherwise ignored. Newlines **are** tokenized (`NEWLINE`) and are significant to the parser.
- Line comments start with `;` and run to the end of the line.
- Block comments are wrapped in `;*` and `*;`:
  ```
  ;*
  this whole block
  is ignored
  *;
  ```
- Special/punctuation characters are always their own token: `( ) { } [ ] , . : @ $`.
- Everything else (identifiers, keywords, instruction names, numbers) is read until whitespace or one of the special characters above.

---

## 2. Instructions

An instruction is an identifier that the loaded architecture recognizes as an `INSTRUCTION` keyword, followed by its operands:

```
InstructionName Arg1, Arg2, ..., ArgN
```

- The first operand is separated from the mnemonic by whitespace; subsequent operands are comma-separated.
- Operand kinds (register, immediate, etc.) and their count are entirely defined per-instruction by the architecture file — the parser looks up the mnemonic in `arch.m_instructionSet` and reads exactly the operands that definition specifies.
- A newline (or block boundary) ends an instruction; the parser does not require it, it simply stops once it has consumed the operand count the architecture defines.

Example (using an example architecture's mnemonics):
```
MOVI r2, 56
MOVI r3, 84
ADD  r1, r2, r3
ADI  r2, r2, [20389 * 2 + 65 - 25]
```

---

## 3. Labels

Labels are declared with an identifier followed by a colon:

```
LabelName:
```

### Nested (dot-scoped) labels
A label can be nested under another by prefixing it with the parent label's name and a dot:

```
main:
main.loop:
main.loop.body:
```

There's no depth limit. **Label references must always use the full path** — inside `main.loop` you must still write `main.loop.body`, not `.body` or `body`.

Labels resolve to their memory address and can be used anywhere a numeric constant/expression is expected (see §5).

> Note: scoping is nominal/organizational only — the assembler does not enforce visibility rules based on nesting. Manage naming collisions yourself.

---

## 4. Constants & Numbers

Numeric literals may be:
- Decimal: `56`
- Hex: `0x1A`
- Binary: `0b1011`
- Negated with a leading `-`: `-8`, `-0x1A`

---

## 5. Expressions

Anywhere a constant is expected in an operand or data initializer, you can instead write a full expression enclosed in square brackets:

```
ADI r0, r1, [8 * 2]
```

Expressions may freely combine numeric literals, preprocessor replacement macros (which are token-substituted before parsing), and identifiers (labels or declared data objects, which evaluate to their memory address):

```
WORD halfInt, 0
...
halfIntMove:
ADI r0, r1, [halfInt - 2]
...
SBI r0, r1, [halfIntMove + 2]
```

Labels/data referenced in expressions must be given by their **full, global path** (see §3).

### Operators, by precedence (lowest to highest binding)

| Level | Operators | Notes |
|---|---|---|
| 1 | `\|\|` | boolean OR |
| 2 | `&&` | boolean AND |
| 3 | `\|` | bitwise OR |
| 4 | `^` | bitwise XOR |
| 5 | `&` | bitwise AND |
| 6 | `==` `!=` | equality |
| 7 | `<` `<=` `>` `>=` | comparison |
| 8 | `<<` `>>` | shift |
| 9 | `+` `-` | additive |
| 10 | `*` `/` `%` | multiplicative |
| 11 (unary, right-assoc) | `-` `!` `$` `$$` | see below |
| — | `( ... )` | grouping |

Standard mathematical order of operations applies. Parentheses `( )` are allowed inside expressions (in addition to the outer `[ ]` that introduces the expression itself).

### Unary operators
- `-x` — arithmetic negation
- `!x` — bitwise NOT
- `$x` — **relative** operator: evaluates `x`, then subtracts the current address (i.e. produces a PC-relative offset from the point where the expression appears).
- `$$x` — **absolute** operator: evaluates `x` and takes its absolute value.

These are most useful with labels, e.g. `$myLabel` yields the signed byte offset from the current instruction to `myLabel`, which is exactly what a relative branch encoding needs (see the relaxor example in §8).

---

## 6. Data Declarations

The name given in a data declaration refers to the address of its **lowest byte**.

### Fixed-size scalars
```
BYTE  isTrue, 0            ; 8 bits  / 1 byte
WORD  number, 0xffff       ; 16 bits / 2 bytes
DWORD fullInt, 0xffffffff  ; 32 bits / 4 bytes
```
Syntax: `KEYWORD name, initialValue` (initial value may be a `[ ]` expression instead of a bare number).

### TEXT (byte arrays / strings)
```
TEXT name, elementCount, initialValue
```
- `initialValue` may be a plain number (fills every byte with that value) or a quoted string (parsed as ASCII, one byte per character).
- `elementCount` may be a literal, a `[ ]` expression, or the keyword `AUTO`.
- `AUTO` is only valid when the initializer is a **string** — `AUTO` with a numeric fill value is a compile error.

```
TEXT byteArray, 64, 0                    ; 64 bytes, all zero
TEXT greeting,  AUTO, "Hello World!!!!!" ; length inferred: 16 bytes
```

### ARRAY (typed element arrays)
```
ARRAY name, elementCount, bytesPerElement, {Element0, Element1, ..., ElementN}
```
- `elementCount` may be a literal, a `[ ]` expression, or `AUTO` (inferred from the number of elements in `{ }`).
- `bytesPerElement` may be a literal or a `[ ]` expression.
- Each element inside `{ }` can itself be a full expression.
- `AUTO` cannot be combined with a plain-number initializer (same restriction as TEXT) — it's meant for the brace-list form.

```
ARRAY numbers, AUTO, 2, {52, 385, 209, 295}   ; 8 bytes (4 elements x 2 bytes)
```

> **Divergence from the older `readmes!/spasmSyntax.md`:** that document describes a `, SIGNED` suffix for range-checking signed data. This is **not implemented** in the current `src3` parser — there is no handling of a `SIGNED` keyword in data declarations. Don't rely on it.

---

## 7. Preprocessor Directives

All directives are introduced with `@` and are resolved by `Spasm::Preprocessor` before parsing begins.

### `@include`
```
@include "path/to/file.spasm"
```
Pulls in another source file (resolved via the target's include search paths). Each file is only processed into a translation unit once; its macros become available to the including file.

### `@entry`
```
@entry MainLabel
```
Marks `MainLabel` as the program's entry point for the current build target. Only meaningful at most once per target (redefining it emits a warning).

### `@define` — Replacement macros
```
@define BYTEMAX 0xff
...
ADI r1, BYTEMAX     ; expands to: ADI r1, 0xff
```
Any token whose text matches a defined name is replaced with the macro's token sequence (up to the end of the line it was defined on).

### `@define` — Function macros
```
@define macroName(Arg0, Arg1, ..., ArgN) {
  ; body, using Arg0..ArgN as placeholders
}
```
Invoked like an instruction:
```
macroName x, y
```
Each argument is substituted (token-for-token) into every occurrence of the matching parameter name inside the macro body, then the expanded body is spliced into the token stream in place of the invocation. Function macros may take zero arguments (used purely to splice in a reusable code block).

Example:
```
@define MUL(rd, rA, rB) {
  PUSH r15
  MOV  r0, r15
  MOV  r0, rd
  ADD  rd, rA, rd
  ADI  r15, 1
  SUB  r0, rB, r15
  BRIS NE, 3
  POP  r15
}

MUL r2, r1, r5

; expands to:
PUSH r15
MOV  r0, r15
MOV  r0, r2
ADD  r2, r1, r2
ADI  r15, 1
SUB  r0, r5, r15
BRIS NE, 3
POP  r15
```

Macro definitions **cannot be nested** inside another macro's body — this is a compile error.

### Unique local names inside macros: `@@name`
Inside a function macro body, an identifier prefixed with `@@` is rewritten, on each expansion, into a name that's unique to that particular invocation (mangled with an internal per-macro invocation counter). This lets a macro declare its own local labels without colliding with itself when used more than once in the same file:

```
@define LOOP_N(count) {
  @@top:
    ; ...
    ADI r1, -1
    BRIS NE, @@top
}
```
Each call to `LOOP_N` gets its own distinct `@@top` label under the hood, so multiple invocations don't clash.

---

## 8. Relaxors — `@if` / `@elif` / `@else`

Relaxors are Spasm's mechanism for **size-dependent code selection** — picking between alternative instruction sequences based on a compile-time-evaluated condition, where that condition is typically itself dependent on addresses/offsets that only stabilize once layout is known. This is most commonly used to choose the cheapest valid branch/jump encoding based on how far away its target actually ends up being.

Syntax:
```
@if (condition) {
  ; instructions
} @elif (condition) {
  ; instructions
} @else {
  ; instructions
}
```

- `@elif` may repeat any number of times; `@else` is optional.
- Conditions are ordinary expressions (see §5) evaluated to a boolean-ish integer (nonzero = true).
- Each branch's instruction sequence can have a different byte size. The linker computes the worst-case size up front, then runs an iterative relaxation pass: it evaluates each relaxor's conditions in order, picks the first one that's true, and — if that changes the relaxor's resolved size — shifts everything after it and re-queues any other relaxor whose condition referenced a label whose address just moved. This repeats until every relaxor's chosen branch is stable.
- Because the branch actually taken can depend on distances that aren't known until other relaxors have resolved, write relaxor conditions so they remain correct regardless of iteration order (e.g. checking a signed-offset range using `$label`, as below).

Real example, using the relative (`$`) operator from §5 to pick between a compact "in-range" jump-immediate encoding and a full absolute-address fallback:

```
@define jmp(label) {
  @if (($label / 2) <= 1023 && ($label / 2) >= 0) {
    jmpia [$label / 2]
  } @elif (($label / 2) >= -1023 && ($label / 2) <= 0) {
    jmpis [$label / 2]
  } @else {
    ; label too far away for jmpia/jmpis -- fall back to a full absolute jump
    movi r14, [label & 0xffff]           ; low half
    movi r15, [(label << 16) & 0xffff]   ; high half
    mov  gpl, r14
    mov  gph, r15
    jmpr gp
  }
}
```

Here `$label` is the byte distance from the current instruction to `label`; dividing by 2 converts it to a word-offset, and the three branches cover "short forward jump", "short backward jump", and "anything else."

---

## 9. `.org`

The lexer/parser recognize a `.org` directive-like statement (`.org <address>`), but as of the current `src3` implementation it is a **stub that consumes its tokens and does nothing** (`parseOrg` performs no address relocation). Treat it as unimplemented for now rather than relying on it to reposition code/data.

---

## 10. Quick Reference

| Syntax | Meaning |
|---|---|
| `; comment` | line comment |
| `;* ... *;` | block comment |
| `Name:` | label |
| `Parent.Child:` | nested label (always referenced by full path) |
| `INSTR a, b, c` | instruction (operands defined by the architecture) |
| `[ expr ]` | expression, wherever a constant is expected |
| `$x` | relative-to-current-address value of `x` |
| `$$x` | absolute value of `x` |
| `BYTE/WORD/DWORD name, val` | fixed-size scalar data |
| `TEXT name, count\|AUTO, val\|"str"` | byte array / string |
| `ARRAY name, count\|AUTO, size, {...}` | typed element array |
| `@include "file"` | include another source file |
| `@entry Label` | set program entry point |
| `@define NAME value` | replacement macro |
| `@define NAME(args) { ... }` | function macro |
| `@@name` | per-invocation-unique identifier (inside a function macro) |
| `@if (c) { } @elif (c) { } @else { }` | relaxor: size-dependent branch selection |