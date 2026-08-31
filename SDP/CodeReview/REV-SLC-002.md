# REV-SLC-002 — Independent review of Stage 0 corrections

Status: changes-required
Reviewed Slice: SLC-002
Reviewed HEAD: `bbd63878c8ea4a2479e531509b3644e661eb875d`
Reviewed parent: `30e5531fc127a8d5162b54bbe8400651ae5825b4`
Reviewer: second fresh independent reviewer
Reviewed: 2026-08-31

## Disposition

Not approved. Six original findings are resolved, the original push case is
resolved, and interrupt/cycle behavior now passes broad probes. Two residual
high-severity contract gaps remain.

## Findings

### REV-SLC-002-F001 — High — trace callback remains behaviorally mutable

`const struct em8051 *` is shallow const. A callback can write through exposed
`mCodeMem`, `mExtData` or `mUpperData` pointers without a cast. Because the
instruction trace fires before operand reads, a compiling callback that changed
`mCodeMem[1]` altered `MOV A,#11` into `MOV A,#42`; traced ACC became `42` while
untraced ACC remained `11`. The callback surface must be record-only (or an
equivalently immutable value view), not a shallow-const CPU object.

### REV-SLC-002-F002 — High — classic upper-stack reads remain silent

Absent upper IRAM reads return sentinel `77` without `EXCEPTION_STACK`.
Independent probes showed classic `POP direct` at `SP=80` write `77` and return
normally, while `RET` at `SP=80/81` built PCs from sentinel data without an
exception. Stack read failure must be symmetrical with write failure and must
not silently commit destination/control-flow state.

## Prior-finding status

- REV-SLC-001-F001, F002, F003, F005, F006 and F008: resolved.
- REV-SLC-001-F004: push path resolved; residual read path is
  REV-SLC-002-F002.
- REV-SLC-001-F007: mutable-signature assignment rejected, but shallow pointer
  reachability remains as REV-SLC-002-F001.

## Evidence rerun by Reviewer

- Windows and openSUSE WSL `make core-test` passed.
- Windows GCC/Clang and WSL warnings-as-errors builds passed.
- WSL ASan/UBSan Stage 0 and loader/SFR error probes passed.
- All 256 opcodes matched raw tick/drain against step cycle behavior.
- Interrupt target/breakpoint stops and two-cycle entry passed.
- Exact trace chronology, reset wording/state, invalid SFR and null-loader
  probes passed.
- corrective diff check and product no-target audit passed.
- worktree remained clean.

## Required follow-up

SLC-003 must replace the callback CPU pointer with a record-only immutable
surface and make classic missing-upper stack reads fail explicitly without
committing invalid POP/RET state. It then requires a third fresh review.

