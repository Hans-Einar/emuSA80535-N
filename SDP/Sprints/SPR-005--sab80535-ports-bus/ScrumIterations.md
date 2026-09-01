# Scrum iterations

## ITR-010 — Port latch/pin and MOVX context

Status: complete
Iteration ID: ITR-010
Active Slice: SLC-010

### Slice contract — SLC-010

**Goal:** implement the Issue #7 port-state substrate and immutable P1-latch
MOVX context without external-edge behavior.

**Expected product files:** port state/API and MOVX context types in
`emu8051.h`, central SFR/reset integration in `core.c`, audited opcode RMW
paths in `opcodes.c`, focused Stage2 tests/build integration and README.

**Required behavior:**

1. P1/P3/P4/P5 reset-high latch and resolved state;
2. separate external mask/levels and resolution formula from DES-040;
3. virtual drive/release/get-latch/get-pins API with invalid-input safety;
4. ordinary byte/bit reads use resolved pins;
5. full documented RMW set in DES-042 uses latch and writes latch;
6. P4/P5 bit-addressable paths use the same substrate;
7. external drive never mutates latch or produces an IRQ;
8. SFR callbacks preserve read override/write observation/reentrant coherence;
9. immutable MOVX context snapshots cycle/PC/address/direction/value/P1 latch;
10. snapshot precedes mutable legacy callback effects;
11. DPTR and @Ri read/write forms share the seam;
12. external P1 pin forcing never changes bank snapshot;
13. legacy callbacks and no-callback XDATA backing remain compatible;
14. no target decode or unauthorized edge/peripheral/live behavior.

**RMW evidence:** Siemens table 7-2 is authoritative: ANL/ORL/XRL, JBC, CPL,
INC, DEC, DJNZ, MOV Px.y,C, CLR Px.y and SETB Px.y. Add literal opcode probes
for byte and bit cases; audit each relevant handler rather than one example.

**Invariants:** accepted Stage0/IRQ/timer/UART timing and callbacks remain
unchanged; classic 8051/8052 behavior does not gain the SAB built-in model;
immutable observers remain behavior-neutral.

**Non-goals:** external interrupt edge/level generation, ADC, Timer2, P1000/
D71055/bank policy, live GPIO/serial or physical I/O.

**Traceability:** MND-001, STU-001, REQ-012 target subset,
ARCH-002/003/004/005/006, DES-039..DES-049, SPR-005, ITR-010, SLC-010,
expected REV-SLC-010 and VER-SLC-010. Authority includes Issue #7 and Ponsse
PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`.

**Required verification:** all 20 Issue classes; independent RMW handler audit;
all prior suites; Windows GCC/strict Clang; WSL GCC/ASan+UBSan; product diff,
no-target/no-edge/no-live audits.

**Completion signal:** fresh Worker commits bounded product/tests; fresh
Reviewer approves or opens findings; Master accepts only after verification.

### Worker result

Worker commit `ba00ef17af57076b01c7f548e8996a7d36a5c591`
implements bounded ports/RMW/MOVX context and a focused Stage2 suite. Worker
reports all five suites passing cross-platform/sanitizer and scope audits clean.
REV-SLC-010 is reviewing exact HEAD; no acceptance is recorded yet.

### Final result

Accepted. REV-SLC-010 approved exact product HEAD
`ba00ef17af57076b01c7f548e8996a7d36a5c591` with no findings. VER-SLC-010
passed the independent Master matrix. SLC-010 is complete; external-edge scope
remains unauthorized.
