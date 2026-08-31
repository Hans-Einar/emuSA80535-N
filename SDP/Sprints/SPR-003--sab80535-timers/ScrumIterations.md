# Scrum iterations

## ITR-006 — Timer0 mode1 and Timer1 mode2

Status: active
Iteration ID: ITR-006
Active Slice: SLC-006

### Slice contract — SLC-006

**Goal:** deliver deterministic, machine-cycle-accurate Timer0 mode1 and Timer1
mode2 behavior plus an overflow-event seam that is independent of TF flags.

**Why now:** the interrupt controller is merged, while the future 9-bit UART
requires a verified Timer1 overflow stream. Steering authorizes timers only.

**Expected product files:** narrow shared timer code in `core.c`, public/internal
timer event types/state in `emu8051.h`, focused timer tests and Makefile/README
integration. Avoid unrelated opcode or peripheral changes.

**Required behavior:**

1. Timer0 mode1 increments exactly once per eligible machine cycle;
2. DCEF remains below overflow for 8976 ticks and wraps/sets TF0 on tick 8977;
3. FFFF wraps to 0000 with TH0/TL0 coherent;
4. live TL0/TH0 writes while running affect the live counter;
5. ET0+EAL vectors through accepted controller at 000B and clears TF0;
6. Timer0 advances through entry/ISR cycles; separate TL0 then TH0 software
   reload shows real latency;
7. a generic synthetic ROM ISR demonstrates four deterministic Timer0
   interrupts and every-fourth software cadence without target policy;
8. Timer1 mode2 reloads TL1 from the current TH1 on every wrap;
9. FD steady state emits overflow events every exactly three cycles;
10. repeated Timer1 events occur while sticky TF1 remains set;
11. ET1/EAL vector 001B auto-clear remains controller-correct;
12. TH1 write while running leaves current TL1 unchanged and changes next reload;
13. long integer-cycle run has exact event count and no drift;
14. Stage0/controller and classic timer regressions remain green;
15. generic record-only event observation is deterministic and neutral;
16. no UART/target/live-I/O behavior is added.

**Invariants:** one `tick()` remains one machine cycle; accepted run/breakpoint/
interrupt-entry semantics remain unchanged; TF flags stay sticky until their
documented clear path; event counters are monotonic 64-bit state reset only by
CPU reset.

**Non-goals:** REQ-011, SBUF/TB8/RB8/RI/TI timing, serial shift/divider state,
counter/gate pin completion, ADC, Timer2, GPIO edges or P1000 behavior.

**Traceability:** MND-001, STU-001, REQ-010, ARCH-003/004/005/006,
DES-019..DES-026, SPR-003, ITR-006, SLC-006, expected REV-SLC-006 and
VER-SLC-006. Authority includes Issue #2 checkpoint and Ponsse PR #25 commit
`7891aceb2b713659ea9936b9743a5aee73579ae9`.

**Required verification:** all 16 Steering classes; original Stage0/controller
suites; Windows GCC and strict Clang; openSUSE WSL GCC and ASan/UBSan; exact
integer-cycle/trace-repeat checks; product diff and no-target/no-UART audit.

**Completion signal:** fresh Worker commits a bounded timer/test implementation;
fresh Reviewer approves or opens stable findings; Master accepts only after
independent verification.

