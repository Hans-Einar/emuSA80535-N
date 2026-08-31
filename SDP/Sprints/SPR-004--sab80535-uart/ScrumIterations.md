# Scrum iterations

## ITR-007 — Mode-3 9-bit UART

Status: active
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

