# Scrum iterations

## ITR-015 — Reached Siemens Timer2 timer-function path

Status: active
Iteration ID: ITR-015
Active Slice: SLC-015

### Slice contract — SLC-015

**Goal:** implement only the Issue #17 Timer2 vertical Slice from Siemens
T2CON timer-function selection through deterministic live TL2/TH2 counting,
reload-disabled overflow, canonical TF2 and normal vector `002B` service.

**Why now:** current master `bc86d263...` contains accepted Stage0..Stage3,
including ADC and frozen debug behavior; Steering explicitly authorized
REQ-014 and no later completeness/capture work.

**Expected product files/modules:** generic Timer2 identity/T2CON masks in
`emu8051.h`; Timer2 tick/controller synchronization in `core.c`; focused
`tests/test_stage4_timer2.c`; test build wiring and minimal generic README/API
documentation. Reuse the accepted generic timer-overflow observer/count surface
rather than adding a competing mutable callback. Do not modify `opcodes.c`
unless the existing SFR gateway cannot express a required live write. Do not
modify frozen `emu_debug.c`, `emu_debug.h`, `emu_debug_server.c` or protocol
behavior.

**Required behavior:**

1. implement canonical T2CON masks T2PS/I3FR/I2FR/T2R1/T2R0/T2CM/T2I1/T2I0;
2. preserve accepted I2FR/I3FR external-edge behavior;
3. count only timer-function input `T2I1:T2I0=01` in this Slice;
4. stop without state loss for input selection `00`;
5. increment once per machine cycle for T2PS=0;
6. increment on every second global machine-cycle boundary for T2PS=1;
7. use the DES-087 free-running oscillator-divider phase across mode changes;
8. use live TH2:TL2 as a 16-bit up-counter;
9. let independent TL2/TH2 writes change live state immediately;
10. with T2R1=0, wrap FFFF->0000 without CRCL/CRCH reload;
11. set canonical sticky IRCON.TF2 on every wrap;
12. count/trace repeated wraps even while TF2 is already set;
13. leave TF2 software-clear across vector entry and RETI;
14. let ET2/EAL gate service only, never counting or TF2 production;
15. use only the accepted Timer2 source/vector 002B controller path;
16. keep unsupported `10`/`11` input and T2R1=1 reload modes producer-inert;
17. reset the Timer2 overflow count deterministically while the global machine
    cycle counter supplies the free-running /24 phase;
18. emit immutable observer-neutral overflow records through the accepted timer
    observer and expose the existing 64-bit per-timer count;
19. satisfy the exact 43691-cycle `5555` fixture;
20. satisfy a generic ISR-style stop/port-write/live-reload/clear/restart fixture;
21. preserve all prior behavior and every explicit stop boundary.

**Siemens evidence rule:** Worker and Reviewer separately record Edition 08.95
Timer2 section/table/figure evidence for T2CON bit positions, input selection,
T2PS fosc/12 vs fosc/24, reload-disabled semantics and TF2 behavior. Ponsse PR
#25 may prove reached values/consumer ordering but cannot define generic CPU
semantics.

**Deterministic transition convention:** Siemens figure 7-33 places the
oscillator-derived divide-by-two stage before the Timer2 input selector.
SLC-015 therefore treats /24 phase as free-running with global virtual machine
time: completed cycles 2,4,6,... are /24 boundaries after reset. T2CON changes
gate/select that clock but do not invent a prescaler reset. Reviewer must
challenge this convention against the primary manual and open a finding if
stronger contradictory evidence exists.

**Invariants:** machine-cycle scheduling, sticky/software-clear TF2, accepted
12-source interrupt controller, accepted I2FR/I3FR, P5 latch/pin semantics,
classic variant isolation, deterministic in-memory execution and frozen debug
protocol.

**Non-goals:** external/gated Timer2 input, CRC/T2EX reload, EXF2 production,
compare/capture, automatic P5.4 toggle, target/board policy, live/physical I/O,
wall-clock scheduling or debug-protocol changes.

**Traceability:** MND-001, STU-001, REQ-014, ARCH-002..ARCH-006,
DES-011..DES-018, DES-086..DES-095, SPR-009, ITR-015, SLC-015, expected
REV-SLC-015 and VER-SLC-015. External authority is Issue #17, Siemens manual
Edition 08.95 and Ponsse PR #25 HEAD `19ef6ff...` as consumer evidence only.

**Required verification:** every Issue #17 class; literal SFR writes and
independent cycle oracle; sticky repeated-overflow; multi-cycle/interrupt/IDLE;
software reload order; Windows GCC, strict Clang, WSL GCC and WSL ASan+UBSan;
all accepted core/debug suites; frozen DAP integration where available;
product diff, no-target/no-reload/no-capture/no-live/no-protocol-change,
traceability and ancestry audits.

**Expected completion signal:** a bounded Worker product/test commit with exact
evidence. A separate fresh Reviewer audits that exact product HEAD and either
approves it or opens stable findings. Master accepts only after verification.
