# Scrum iterations

## ITR-014 — Deterministic MYMOS single-conversion ADC

Status: active
Iteration ID: ITR-014
Active Slice: SLC-014

### Slice contract — SLC-014

**Goal:** implement the Issue #13 ADC vertical Slice from generic normalized
AN0..AN7 input through exact DAPR conversion timing, ADDAT/BSY/IADC and normal
vector `0043` service.

**Why now:** current master `a4d2478...` contains accepted Stage0..Stage2 and
frozen debug behavior; Steering explicitly authorized REQ-013 and no later
peripheral.

**Expected product files/modules:** public generic ADC input/trace types and
APIs plus bounded CPU-owned state in `emu8051.h`; SFR/reset/tick/controller
integration in `core.c`; focused `tests/test_stage3_adc.c`; test build wiring
and minimal generic README/API documentation. Touch `opcodes.c` only if a
literal SFR access path cannot use the existing gateway. Do not modify
`emu_debug.c`, `emu_debug.h`, `emu_debug_server.c` or the frozen protocol.

**Required behavior:**

1. implement DES-074 ADCON ownership while preserving BD/CLK/reserved state;
2. provide safe deterministic normalized AN0..AN7 host injection;
3. make every DAPR write arm one conversion regardless of byte value;
4. coalesce mutable/reentrant DAPR callbacks into one final start/restart;
5. latch final channel/DAPR/input at the first post-write machine cycle;
6. assert BSY at cycle 1 and complete exactly at cycle 15;
7. atomically update valid ADDAT, clear BSY and set IADC at completion;
8. restart/supersede busy work without an old ghost completion;
9. preserve pre-existing IADC across start/restart until software clears it;
10. implement valid nibble ranges, zero endpoints and four-step minimum span;
11. use the exact DES-080 integer floor conversion and clipping rules;
12. time unsupported reference combinations normally while preserving ADDAT
    and diagnosing invalid completion;
13. keep active conversion context immutable across later ADCON/input changes;
14. perform one explicit conversion for ADM=1 but no continuous auto-restart;
15. use only canonical IRCON.IADC and the accepted controller/vector path;
16. reset deterministically and isolate classic variants;
17. emit immutable observer-neutral START/RESTART/COMPLETE records;
18. satisfy the bounded generic ROM-style consumer fixture;
19. preserve all prior behavior and every explicit stop boundary.

**Siemens evidence rule:** Worker and Reviewer separately record Edition 08.95
manual sections/figures/tables/pages for ADCON, channel selection, DAPR valid
ranges/zero endpoints, reference formula, 15-cycle MYMOS timing, BSY, ADDAT and
IADC. They must explicitly audit DES-078's atomic cycle-15 observation against
the manual's unnumbered generic anticipation diagram and report a blocker if
the chosen architectural boundary is not defensible.

**Invariants:** accepted 12-source controller and software-clear IADC;
machine-cycle scheduling; UART BD/CLK behavior; callback semantics; classic
variant isolation; deterministic in-memory execution; frozen debug protocol.

**Non-goals:** Timer2 counting/overflow/reload/P5.4, capture/compare, full ADM=1
continuous conversion, target sensor/voltage/calibration/board/NEC behavior,
live/physical I/O, wall-clock scheduling or debug-protocol changes.

**Traceability:** MND-001, STU-001, REQ-013, ARCH-002..ARCH-006,
DES-011..DES-018, DES-074..DES-085, SPR-008, ITR-014, SLC-014, expected
REV-SLC-014 and VER-SLC-014. External authority is Issue #13, Siemens manual
Edition 08.95 and Ponsse PR #25 HEAD `19ef6ff...` as consumer evidence only.

**Required verification:** every Issue #13 class 1..28; literal SFR/opcode and
independent arithmetic/reference vectors; multi-cycle/interrupt/callback/
restart/long-replay probes; Windows GCC, strict Clang C99/`-Werror`/`-pedantic`,
WSL GCC and WSL ASan+UBSan; all accepted core/port/edge/debug facade/process
suites; frozen DAP exact-head real integration; product diff,
no-target/no-Timer2/no-live/no-protocol-change, NDJSON and ancestry audits.

**Expected completion signal:** a fresh Worker commits bounded product/tests
and exact evidence. A separate fresh Reviewer audits the exact product HEAD and
either approves it or opens stable findings. Master accepts only after
independent exact-HEAD verification.

### Worker result

Fresh Worker commit `0bd39132b2eaffbfc5190e223b54743f17fc68fa`
implements the bounded ADC product/test Slice from parent
`8906274ff1a5096d1403b3021569bbae37920e97`.

The Worker reports all 28 Issue classes passing: normalized eight-channel
input, exhaustive DAPR programming, exact cycle-1/15 state, restart and
reentrant callback coalescing, integer conversion oracle, ADDAT/BSY/IADC,
controller/vector behavior, replay/observer/reset/classic isolation and the
generic ISR fixture. Windows GCC, clean strict Clang, WSL GCC, WSL ASan+UBSan,
frozen debug and exact-DAP gates all pass.

The exact product HEAD is not accepted until fresh REV-SLC-014 independently
adjudicates Siemens timing/reference semantics and Master verification passes.

### Review result

REV-SLC-014 approved exact product/test HEAD
`0bd39132b2eaffbfc5190e223b54743f17fc68fa` with no findings. The Reviewer
independently approved DES-078 timing, exhaustively checked arithmetic and DAPR
encodings, passed adversarial restart/callback/BSY/controller probes, repeated
the complete cross-platform/debug/DAP matrix and confirmed the scope/identity
boundary. Master VER-SLC-014 remains before acceptance.
