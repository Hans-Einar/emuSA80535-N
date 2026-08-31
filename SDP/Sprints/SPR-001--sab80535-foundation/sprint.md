# SPR-001 — SAB80535 deterministic foundation

Status: complete  
Sprint ID: SPR-001  
Started: 2026-08-31

## Goal

Deliver the first reviewable PR on top of the exact upstream fork: establish
SDP, prove a core-only upstream baseline, introduce an explicit SAB80535
variant foundation, deterministic time/run/loader/trace APIs, 256-byte indirect
IRAM and focused tests without implementing P1000 board behavior.

## Scope

- REQ-001..REQ-008;
- deterministic Stage 0 only;
- upstream-compatible build/test harness;
- variant architecture and SAB SFR/reset foundation;
- raw loader, counters, bounded run control and diagnostics;
- tests and independent review/verification.

## Non-goals

- Stage 1 interrupts, timers and UART completion;
- Stage 2 GPIO/edge/MOVX board integration behavior beyond a generic trace seam;
- ADC or Siemens Timer 2 behavior;
- P1000 firmware/ROM assets or board code;
- live I/O of any kind.

## Planned slices

1. **SLC-001:** upstream core-only harness plus complete deterministic Stage 0
   API foundation and tests.
2. Corrective Slice only if independent review identifies changes required.
3. Stage 1 begins in a later Sprint/iteration after Stage 0 acceptance.

## Exit criteria

- exact upstream baseline and fork ancestry recorded;
- core-only build/test is reproducible on the available toolchain;
- classic regression tests and new Stage 0 tests pass;
- SAB variant structurally resolves A8/A9/B8/B9 meanings;
- 256-byte indirect IRAM, exact raw loader, counters, run control and trace
  contracts are covered by tests;
- no-target audit finds no P1000 implementation leakage;
- independent review is approved and verification record passes;
- PR is opened from `codex/sab80535-foundation`.

## Verified result

All technical exit criteria are satisfied at
`dbcc6b74adbc34896a71401366991229c0d58922` through accepted SLC-003,
REV-SLC-003 and VER-SLC-003. The verified branch was pushed and opened as
`Hans-Einar/emuSA80535-N` PR #1:
`https://github.com/Hans-Einar/emuSA80535-N/pull/1`.

SPR-001 is complete. Stage 1 requires a new authorized Sprint/Slice.
