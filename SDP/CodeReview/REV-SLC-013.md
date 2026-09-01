# REV-SLC-013 — Deterministic external-edge review

Status: approved
Reviewed Slice: SLC-013
Reviewed product/test HEAD: `3cfc4a8e9a5accb4a91df36b0b03119bc4d1de9b`
Reviewed tree: `f691504135f3caf2f61dd3e85069f77b2a6fd7ec`
Reviewed parent: `a9167b3acfb317315534b5059fad22d58f105dd8`
Reviewer: fresh independent reviewer
Reviewed: 2026-09-02

## Disposition

Approved with no findings. No `REV-SLC-013-Fxxx` IDs were opened. The exact
product/test commit satisfies DES-064..DES-073 and all 23 Issue #7 classes
without bypassing the accepted interrupt controller.

## Independent Siemens reconciliation

Primary authority was the Siemens *SAB 80515/SAB 80C515 Family User's Manual*,
Edition 08.95:

- chapter 7.1.3, table 7-1, page 42: P3.2/INT0, P3.3/INT1, P1.4/INT2 and
  P1.0..P1.3/INT3..INT6;
- figure 7-5, page 43: alternate inputs require released/high port latches;
- chapter 8.1, figures 8-4..8-6, pages 117..119: TCON/T2CON/IRCON flag and
  selection paths;
- table 8-2, page 124: canonical flags and vectors;
- chapter 8.4, page 125: INT0/INT1 level or falling edge, I2FR/I3FR selectable
  falling/rising edges, INT4..INT6 fixed rising edges and machine-cycle
  sampling.

The independently accepted matrix is:

| Source | Pin | Flag | Trigger | Vector |
|---|---|---|---|---:|
| INT0 | P3.2 | TCON.IE0 | IT0=0 low; IT0=1 falling | `0003` |
| INT1 | P3.3 | TCON.IE1 | IT1=0 low; IT1=1 falling | `0013` |
| INT2 | P1.4 | IRCON.IEX2 | I2FR=0 falling; 1 rising | `004B` |
| INT3 | P1.0 | IRCON.IEX3 | I3FR=0 falling; 1 rising | `0053` |
| INT4 | P1.1 | IRCON.IEX4 | rising | `005B` |
| INT5 | P1.2 | IRCON.IEX5 | rising | `0063` |
| INT6 | P1.3 | IRCON.IEX6 | rising | `006B` |

Compare/Timer2 producer paths in the same manual remain excluded.

## Behavioral audit

- The bounded 64-event CPU queue rejects past/out-of-order/overflow work,
  applies current events synchronously, future events at exact completed
  cycles and equal timestamps FIFO. Ring wrap/refill and multi-cycle cases are
  deterministic and memory-safe.
- Detection consumes canonical resolved pins. Latch-low dominates external
  high, stimulus never rewrites latches, and latch writes/reentrant callbacks
  leave detector and port reads coherent.
- INT0/INT1 edge latch/auto-clear/re-arm and level persistence/release are
  correct across masking, service, RETI and inhibit.
- I2FR/I3FR are consulted when a transition is observed; selection changes do
  not synthesize, reinterpret or clear an edge. INT4..INT6 are independent
  fixed-rising sources.
- Only TCON.IE0/IE1 and IRCON.IEX2..IEX6 are produced. Existing priority,
  polling, preemption, in-service, entry, RETI, auto-clear and inhibit logic is
  the only service path.
- Reset releases drives, clears queue/history, seeds high prior samples and
  creates no edge or trace. Trace records are normalized value-only data and
  observer-neutral.
- Generic length and saw fixtures reach real vectors `0003`/`0053`; P3.5 and
  P1.1 remain ordinary resolved-pin samples.
- The revised SLC-010 regression removes only its obsolete edge-neutral
  expectation and retains latch preservation plus no-direct-service proof.

Siemens page 120 says level-mode IE0/IE1 are line-controlled and cannot be set
by writing one. The accepted DES-012 synthetic request seam deliberately
permits direct flag assertion and predates SLC-013. The new hardware-line path
tracks its own level assertion and is correctly line-controlled. This frozen
test seam is not an SLC-013 finding.

## Independent evidence

- Windows GCC 12.2: all six core suites, debug facade and process suite pass.
- Windows Clang 18.1.8 strict C99/`-Werror`/`-pedantic`: all six core suites,
  debug facade and process suite pass.
- openSUSE WSL GCC 7.5: all six core suites, debug facade and process suite
  pass with Python 3.13.9.
- WSL ASan+UBSan strict: all six core suites, debug facade and process suite
  pass without report.
- Independent Windows and WSL-sanitized queue wrap/refill/FIFO, multi-cycle,
  callback and reentrancy probes pass.
- Diff, ancestry, NDJSON, immutable trace and forbidden-scope audits pass.

The sibling DAP checkout is not the frozen accepted revision, so it was not
modified or used to bypass identity. This is not an SLC-013 blocker because
the frozen debug modules and protocol tests are unchanged and green.

## Scope and identity

The reviewed product diff is exactly `README.md`, `core.c`, `emu8051.h`,
`tests/Makefile`, `tests/test_stage2_edges.c` and `tests/test_stage2_ports.c`.
`opcodes.c` and all frozen `emu-debug` product/tests have unchanged blobs.

No ADC, Siemens Timer2 behavior, P1000/NEC/D71055/board/signal policy,
live/physical I/O, direct PC/vector hook, second controller, wall-clock/thread
scheduler or protocol change was added. Later commits through
`f6b479ceb4b22158925b5913caa61aa5c39f4329` modify SDP only; product/test blobs
remain identical to the reviewed commit.

## First remaining blocker

Master must complete VER-SLC-013 against exact product HEAD and integrate
acceptance before the focused implementation PR and cross-repository handoffs.
