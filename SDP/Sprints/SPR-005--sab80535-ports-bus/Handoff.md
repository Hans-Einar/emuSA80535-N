# Handoff

## Current Objective

Implement and independently review SLC-010 port latch/pin and MOVX context only.

## Authoritative Source Documents

- repository `AGENTS.md` and lifecycle SDP;
- `SDP/05--Design/EMU-SAB80535-DES-005.md`;
- accepted Stage0/Stage1 review/verification;
- this Sprint's `sprint.md` and `ScrumIterations.md`;
- Issue #7;
- Ponsse PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`;
- Siemens port driver/read-modify-write documentation;
- Ponsse Issue #26 / PR #27 CPU-board boundary.

## Done

- UART PR #5 merged to current master `88ccb2b...`.
- SLC-010 authority and RMW set reconciled.
- SPR-005 / ITR-010 / SLC-010 opened from current master.

## Not Done

- SLC-010 product implementation, review, verification and PR.
- external-edge Slice, ADC, Timer2, target logic or live/physical I/O.

## Exact Next Step

Fresh Worker implements the active port/MOVX contract and focused tests. Fresh
Reviewer then audits exact product HEAD and full RMW classification.

## Verification Completed

Authority/readiness only. All accepted suites are the regression baseline.

## Traceability IDs In Play

MND-001, STU-001, REQ-012 target subset, ARCH-002/003/004/005/006,
DES-039..DES-049, SPR-005, ITR-010, SLC-010, REV-SLC-010 and VER-SLC-010.

## Traceability Update State

- CurrentIndex updated: yes, SLC-010 active.
- Relations updated: yes.
- Ledger updated: PR #5 merge and Sprint/Iteration/Slice start recorded.

## Open Risks Or Ambiguities

- Existing SFR callbacks are mutable; port canonical state must remain coherent
  after reentrant callback writes.
- P1 snapshot must be captured before legacy MOVX callback mutation.
- External drive must not generate any interrupt request in this Slice.

## Worktree Notes

Branch `codex/sab80535-ports-bus` starts exactly from current master
`88ccb2b45976c137820cdc56eec550d953bcf76d`.

