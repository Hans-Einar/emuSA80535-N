# Scrum iterations

## ITR-013 — Deterministic external interrupt edges

Status: active
Iteration ID: ITR-013
Active Slice: SLC-013

### Slice contract — SLC-013

**Goal:** connect SLC-010 canonical resolved pins to DES-002 canonical request
flags so deterministic INT0..INT6 virtual transitions are serviced only by the
accepted Siemens controller.

**Why now:** Steering accepted SLC-010 and the frozen debug runtime on current
master, revalidated PR #25 consumer evidence, and explicitly authorized the
external-edge remainder of REQ-012.

**Expected product files/modules:** public generic source/trace/schedule types
and APIs in `emu8051.h`; resolved-pin, reset, cycle scheduling and canonical
request integration in `core.c`; focused `tests/test_stage2_edges.c`; test
build integration and minimal generic README/API documentation. Touch
`opcodes.c` only if required to keep canonical latch writes and edge sampling
coherent. Do not modify `emu_debug.c`, `emu_debug.h`, `emu_debug_server.c` or
the frozen protocol surface.

**Required behavior:**

1. map INT0..INT6 to the canonical pins/flags in DES-064;
2. provide bounded deterministic current/future machine-cycle stimulus;
3. make detection consume exactly the SLC-010 resolved pin level;
4. never mutate a CPU port latch from external stimulus;
5. implement INT0/INT1 IT0/IT1 edge/level persistence and re-arm;
6. implement both I2FR/I3FR selections without synthetic edges on mode change;
7. implement independently audited Siemens fixed edges for INT4..INT6;
8. keep requests pending across EAL and individual-enable masking;
9. use only canonical TCON/IRCON flags and accepted controller arbitration;
10. preserve equal-level polling, priority preemption, wait-until-RETI and
    post-RETI/enable-write inhibit rules;
11. emit immutable observer-neutral edge/request records with exact cycles;
12. reset queues/history without a spurious edge or target pull assumption;
13. preserve classic variants and every accepted prior behavior;
14. satisfy generic bounded length and saw fixtures through vectors 0003/0053;
15. add no unauthorized peripheral, target, live/physical or protocol scope.

**Siemens evidence rule:** the Worker records manual chapter/table/page
references for the INT0..INT6 pin, flag and edge matrix. The Reviewer performs
the same reconciliation independently rather than trusting product enums or
tests. Any authority ambiguity is reported to Steering before encoding it.

**Invariants:** SLC-010 latch/external/resolved truth; DES-002 12-source
controller, flags, priority, entry and RETI; accepted Stage0/Timer/UART timing;
classic compatibility; deterministic virtual time; immutable observers;
frozen `emu-debug` 1.0 behavior and protocol.

**Non-goals:** ADC, Siemens Timer2 behavior, compare/capture expansion, P1000
board/NEC/D71055/protocol/signal semantics, live I/O, wall-clock timing,
physical control, direct PC/vector jumps or debug-protocol changes.

**Traceability:** MND-001, STU-001, REQ-012 target remainder,
ARCH-002/003/004/005/006, DES-011..DES-018, DES-039..DES-049,
DES-064..DES-073, SPR-007, ITR-013, SLC-013, expected REV-SLC-013 and
VER-SLC-013. External authority is Issue #7, Siemens manual and Ponsse PR #25
HEAD `6a22d8713b607308c94f02df17d35ddbe8a36d6a` as consumer evidence only.

**Required verification:** every Issue #7 class 1..23; literal independent
edge/pin/flag probes; immediate and scheduled/replay cases; masking,
arbitration, priority, preemption and RETI cases; length/saw fixtures; Windows
GCC, strict Clang C99/`-Werror`/`-pedantic`, WSL GCC and WSL ASan+UBSan; all
Stage0/IRQ/timer/UART/port/debug facade/process suites; product diff,
no-target/no-ADC/no-Timer2/no-live/no-protocol-change, NDJSON and ancestry
audits.

**Expected completion signal:** a fresh Worker commits bounded product/tests
and a compliance report with exact evidence. A separate fresh Reviewer audits
the exact product HEAD and either approves it or opens stable findings. Master
accepts only after independent exact-HEAD verification.

### Worker result

Fresh Worker commit `3cfc4a8e9a5accb4a91df36b0b03119bc4d1de9b`
implements the bounded external-line scheduler/detector, canonical request
integration, immutable trace and focused SLC-013 tests from parent
`a9167b3acfb317315534b5059fad22d58f105dd8`.

The Worker reports all 23 Steering classes and accepted Stage0/IRQ/timer/UART/
port/debug facade/process gates passing on Windows GCC and strict Clang, with
all core suites plus sanitizer coverage passing in WSL. The WSL process test is
environment-blocked by Python 3.6, while its identical Windows gate passes.
The DAP integration checkout is not the accepted pinned revision and was not
modified to bypass identity.

The exact product HEAD is not accepted until fresh REV-SLC-013 and independent
Master verification complete.
