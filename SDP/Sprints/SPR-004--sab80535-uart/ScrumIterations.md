# Scrum iterations

## ITR-007 — Mode-3 9-bit UART

Status: changes-required
Iteration ID: ITR-007
Active Slice: SLC-007

### Slice contract — SLC-007

**Goal:** implement REQ-011 exactly as authorized: Timer1-derived, full-duplex,
mode-3 9-bit UART with deterministic in-memory I/O and shared interrupts.

**Expected product files:** variant-aware UART state/API in `emu8051.h`, narrow
SFR/timer integration in `core.c`, only necessary opcode/SFR gateway changes,
focused UART tests/build integration and README API/timing documentation.

**Required behavior:**

1. consume internal Timer1 overflow events directly;
2. divider phase runs continuously: SMOD1/16, SMOD0/32;
3. startup FD/SMOD1 produces 48-cycle bits and exact 19200 integer timing;
4. SBUF TX writes and RX reads use separate storage;
5. SBUF write captures byte+TB8 and starts at next divider wrap;
6. TX emits START,D0..D7,TB8,STOP at exact boundaries;
7. TI rises at STOP start and stays software-clear;
8. write during STOP queues contiguous next frame without truncation;
9. repeated multi-frame TX preserves data/ninth/timing;
10. REN-gated injected RX uses independent timed progression;
11. accepted RX loads receive SBUF/RB8/RI at final shift;
12. RI-already-set and SM2/ninth rejection preserve prior data;
13. TX/RX remain structurally independent under full duplex;
14. RI/TI combinations use only existing vector 0023 and remain software-clear;
15. software SETB/CLR TI and RETI semantics remain canonical;
16. immutable UART observers are deterministic and neutral;
17. synthetic instruction fixture covers TI/TB8/SBUF/SBUF-read/RB8 pattern;
18. no unauthorized product or live-I/O scope.

**Invariants:** one tick remains one machine cycle; Timer1 overflow counters and
public observers stay unchanged; timer overflow is independent of TF1; classic
8051/8052 behavior and accepted interrupt semantics do not regress.

**Non-goals:** dedicated internal baud generator, physical oversampling/RxD/
TxD pins, framing-error extensions, Stage 2, ADC, Timer2, target protocol or
host transport.

**Traceability:** MND-001, STU-001, REQ-011, ARCH-002/003/004/005/006,
DES-027..DES-038, SPR-004, ITR-007, SLC-007, expected REV-SLC-007 and
VER-SLC-007. Authority includes the latest Issue #2 checkpoint and Ponsse PR
#25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`.

**Required verification:** all 18 Steering classes; long-run phase/drift and
back-to-back ISR tests; Stage0/IRQ/timer suites; Windows GCC/strict Clang;
openSUSE WSL GCC and ASan/UBSan; no-target/no-live-I/O audit; exact product diff.

**Completion signal:** fresh Worker commits bounded UART/tests; fresh Reviewer
approves or opens stable findings; Master accepts only after independent
verification and stops at this Slice boundary.

### Worker result

Worker commit `d2fe5d31ac887d81f0dfd17cbbfa222abba1acf3`
implements bounded SAB mode-3 TX/RX/divider/interrupt/trace state and focused
UART tests. Worker reports all four suites passing on Windows GCC/strict Clang
and WSL GCC/ASan+UBSan. REV-SLC-007 is reviewing exact HEAD; no acceptance is
recorded yet.

### Review result

REV-SLC-007 reviewed exact product HEAD `d2fe5d31ac887d81f0dfd17cbbfa222abba1acf3`
and set `changes-required`. Open findings are REV-SLC-007-F001/F002. SLC-007 is
not accepted; corrective work moves to ITR-008 / SLC-008.

## ITR-008 — SBUF callback and mode-isolation correction

Status: changes-required
Iteration ID: ITR-008
Active Slice: SLC-008

### Slice contract — SLC-008

**Goal:** close REV-SLC-007-F001/F002 without changing accepted UART timing or
adding any new mode/peripheral behavior.

**Expected product files:** minimal SBUF gateway/UART pending logic in `core.c`
and focused `tests/test_stage1_uart.c`; header/README only if contract wording
needs clarification.

**Required corrections:**

1. SAB SBUF read callback return overrides the architectural read value;
2. SAB SBUF write callback observes the actual TX write byte while separate RX
   storage remains unchanged after callback completion;
3. generic SFR trace continues to record the TX write value;
4. unsupported non-mode3 or ADCON.BD SBUF writes do not create or replace any
   mode3 pending/in-flight byte or TB8;
5. restoring supported mode/source transmits the original pending frame;
6. callback fixes do not re-couple TX/RX storage or regress classic variants;
7. preserve all REV-SLC-007 positive timing/interrupt/full-duplex behavior.

**Non-goals:** adding behavior for modes 0/1/2 or dedicated generator, Stage2,
ADC, Timer2, target logic or live/physical I/O.

**Traceability:** SLC-008 corrects SLC-007 and addresses REV-SLC-007-F001/F002.
Expected review REV-SLC-008 and verification VER-SLC-008.

**Required verification:** full four-suite Windows/WSL/strict/sanitizer matrix;
read override and TX-write callback probes; RX preservation; pending mode1/BD
isolation/restoration; trace value; all 18 SLC-007 classes and classic SBUF.

**Completion signal:** fresh Worker commits only the correction; fresh Reviewer
approves exact corrective HEAD; Master reruns and accepts.

### Worker result

Corrective commit `336c50a03da4067c22d9567de6075724261a66e9`
changes only `core.c` and focused UART tests. Worker reports callback override/
observation and mode1/BD isolation regressions plus all four cross-platform
suites passing. REV-SLC-008 is reviewing exact HEAD; no acceptance yet.

### Review result

REV-SLC-008 confirmed REV-SLC-007-F001/F002 resolved but opened
REV-SLC-008-F001 for stale post-callback RX mirror restoration. SLC-008 is not
accepted; corrective work moves to ITR-009 / SLC-009.

## ITR-009 — Post-callback RX mirror coherence

Status: active
Iteration ID: ITR-009
Active Slice: SLC-009

### Slice contract — SLC-009

**Goal:** close REV-SLC-008-F001 without changing any UART timing, callback
override, mode-isolation or storage behavior.

**Expected product files:** minimal SBUF write callback restoration in `core.c`
and focused UART regression only.

**Required correction:** after the transient TX mirror/write callback completes,
set `mSFR[SBUF]` from the current canonical `mSABUartRxData`, not a saved stale
mirror. Cover callback mutation of canonical+mirror and guarded reentrant receive
progress. Preserve read override, write observation, TX trace and RX separation.

**Non-goals:** any new UART feature/mode or other peripheral/target behavior.

**Traceability:** SLC-009 corrects SLC-008 and addresses REV-SLC-008-F001.
Expected review REV-SLC-009 and verification VER-SLC-009.

**Required verification:** full four-suite matrix plus callback mutation/
reentrancy coherence and all SLC-007/008 regressions.

**Completion signal:** fresh Worker commits the minimal fix; fresh Reviewer
approves exact HEAD; Master reruns verification.
