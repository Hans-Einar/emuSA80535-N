# Scrum iterations

## ITR-004 — Siemens interrupt controller

Status: in-review
Iteration ID: ITR-004
Active Slice: SLC-004

### Slice contract — SLC-004

**Goal:** implement the full generic SAB80535 12-source/four-level interrupt
controller authorized by Issue #2, without entering Timer/UART behavioral
slices.

**Why now:** Stage 0 deliberately gates classic interrupt handling for the SAB
variant because B8 is IEN1, not classic IP. Firmware scheduling cannot proceed
until the variant owns correct source, priority, nesting and RETI state.

**Expected product files:** `emu8051.h`, `core.c`, the RET/RETI path in
`opcodes.c`, focused Stage 1 tests/build integration, and narrowly required
README API documentation. Keep controller logic separate enough that later
peripheral producers assert requests through a generic source API.

**Required behavior:**

1. stable source enum and vector table for all 12 DES-011 sources;
2. separate pending mask, enable calculation and in-service stack;
3. EAL/individual masking never destroys pending requests;
4. IEN0/IEN1/IP0/IP1 writes are SAB-specific and B8 never aliases classic IP;
5. priority pair mapping and fixed source polling order from DES-014;
6. strictly higher priority may nest; equal/lower waits;
7. `RET` preserves in-service state; `RETI` releases exactly the top entry;
8. entry flag clearing follows DES-016, including software-clear UART, ADC and
   Timer2 classes;
9. instruction-boundary arbitration and post-IEN/IP/RETI one-instruction
   inhibit preserve Stage 0 cycle/run semantics;
10. immutable IRQ trace records contain pending, enabled, selected source/
    priority and in-service depth/state;
11. generic synthetic request API enables deterministic tests and future
    peripherals without physical pin or target policy.

**Invariants:** classic 8051/8052 paths retain their established register map,
two-level behavior, cycle boundaries and stack failure handling; record-only
trace stays immutable; no host wall clock or live I/O.

**Non-goals:** Timer0/Timer1 implementation changes, 19200-baud generation,
mode-3 UART behavior, edge-qualified port inputs, ADC completion, Timer2
behavior or any P1000/board logic.

**Evidence details that must not be guessed:**

- enable bits and pair mapping in DES-013/DES-014;
- vectors and fixed order in DES-011;
- IE0/IE1 edge-only, TF0/TF1 and IEX2..IEX6 auto-clear;
- RI/TI, IADC, TF2 and EXF2 software-clear;
- Siemens instruction-after-RETI/IEN/IP-write arbitration block.

**Traceability:** MND-001, STU-001, REQ-009, ARCH-002/003/005/006,
DES-011..DES-018, SPR-002, ITR-004, SLC-004, expected REV-SLC-004 and
VER-SLC-004. External authority is Issue #2 and Ponsse PR #25 commit
`7891aceb2b713659ea9936b9743a5aee73579ae9`.

**Required verification:** all 15 Issue #2 test classes; original Stage 0 suite;
classic 8051/8052 interrupt regression; Windows GCC and strict Clang; openSUSE
WSL GCC and ASan/UBSan; targeted no-alias/no-target audit; product diff check;
trace determinism comparison; exact reviewed HEAD.

**Completion signal:** Worker commits one bounded SLC-004 product/test change
and reports evidence. A fresh Reviewer either approves exact HEAD or opens
stable findings. Master verifies and accepts only after review is approved.

### Worker result

Worker commit `529602d800e36e87f495ef088f17e67ba3659a1a`
implements the bounded controller and focused Stage 0/Stage 1 tests. Worker
reported Windows GCC/strict Clang, openSUSE WSL GCC and WSL ASan/UBSan passing.
No SDP files or later Timer/UART behavior were included. REV-SLC-004 is now
reviewing the exact product HEAD; this result is not yet accepted.
