# Scrum iterations

## ITR-004 — Siemens interrupt controller

Status: changes-required
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

### Review result

REV-SLC-004 independently reviewed exact product HEAD
`529602d800e36e87f495ef088f17e67ba3659a1a` and set disposition
`changes-required`. Open findings are REV-SLC-004-F001 and F002. SLC-004 is not
accepted; corrective work moves to ITR-005 / SLC-005.

## ITR-005 — Raw IRCON and Timer1 pending correction

Status: complete
Iteration ID: ITR-005
Active Slice: SLC-005

### Slice contract — SLC-005

**Goal:** close the two REV-SLC-004 findings without changing the accepted
controller structure or entering later Timer/UART behavior.

**Expected product files:** `emu8051.h`, the minimal controller/legacy serial
integration in `core.c`, focused `tests/test_stage1_irq.c`, and README only if
raw bit documentation requires correction.

**Required corrections:**

1. define raw `IRCON.C6/0x40=TF2` and `C7/0x80=EXF2` exactly;
2. ensure TF2 is gated by IEN0.ET2 and EXF2 by IEN1.EXEN2;
3. synthetic Timer2 assertion sets TF2 at C6; source clearing clears both only
   when explicitly requested by the synthetic clear API;
4. add raw numeric tests independent of enum macros, including ROM-style clear
   of C6;
5. prevent inherited serial handling from clearing SAB TF1 merely because
   SM1 is set; under startup-like TR1/mode2/SM1 and EAL/ET1 masking, TF1 and
   pending state survive until documented vector-entry auto-clear or explicit
   software clear;
6. do not implement Timer1 baud generation, UART frame timing or any later
   Issue #2 slice.

**Invariants:** preserve all REV-SLC-004 positive findings, Stage 0 cycle/run
semantics, classic 8051/8052 serial/timer behavior and the no-target boundary.

**Traceability:** SLC-005 corrects SLC-004 and addresses REV-SLC-004-F001/F002.
Expected review REV-SLC-005 and verification VER-SLC-005.

**Required verification:** full Stage 0/1 Windows/WSL/strict/sanitizer matrix;
raw C6/C7 split-gate probes; startup-like Timer1 persistence across multiple
instructions while masked; Timer1 auto-clear only on accepted vector; classic
serial regression; diff/no-target audit.

**Completion signal:** fresh Worker commits only the two bounded corrections;
fresh Reviewer approves exact corrective HEAD; Master reruns verification.

### Worker result

Corrective commit `85fa6e1f8318c691598b9a2ae191d1bd94b436c8`
changes only `emu8051.h`, `core.c` and focused Stage 1 tests. Worker reports the
full Windows/WSL/strict/sanitizer matrix passing with raw C6/C7 and masked TF1
regressions. REV-SLC-005 is reviewing exact HEAD; no acceptance is recorded yet.

### Final result

Accepted. REV-SLC-005 approved corrective HEAD
`85fa6e1f8318c691598b9a2ae191d1bd94b436c8` without findings and confirmed
REV-SLC-004-F001/F002 resolved. VER-SLC-005 passed the independent Master
matrix. SLC-005 closes the SLC-004 corrective chain; no later Timer/UART scope
is included or authorized.
