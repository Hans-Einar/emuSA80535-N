# Handoff

## Current Objective

Independently review SLC-007 product HEAD `d2fe5d31...`, then run Master
verification only if approved.

## Authoritative Source Documents

- repository `AGENTS.md` and lifecycle SDP;
- `SDP/05--Design/EMU-SAB80535-DES-004.md`;
- accepted DES-002/DES-003, REV/VER-SLC-005 and REV/VER-SLC-006;
- this Sprint's `sprint.md` and `ScrumIterations.md`;
- latest SteeringGroup checkpoint on Issue #2;
- Ponsse PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`;
- Siemens serial-interface mode 3 and Timer1 baud chapters.

## Done

- Timer PR #4 merged to current master `c0cd6f2...`.
- SLC-007 authority reconciled with Siemens TX/RX timing and SBUF separation.
- SPR-004 / ITR-007 / SLC-007 opened from current master.
- Worker product/test commit `d2fe5d31ac887d81f0dfd17cbbfa222abba1acf3`
  completed; fresh REV-SLC-007 review is running.

## Not Done

- SLC-007 independent review, Master verification and PR.
- Stage 2, ADC, Timer2, P1000 target or any live/physical I/O; unauthorized.

## Exact Next Step

Fresh Reviewer independently challenges exact UART timing/storage/interrupt
behavior on product HEAD.

## Verification Completed

Worker-reported cross-platform evidence exists but is not accepted yet.
Stage0/IRQ/timer suites remain the regression baseline.

## Traceability IDs In Play

MND-001, STU-001, REQ-011, ARCH-002/003/004/005/006, DES-027..DES-038,
SPR-004, ITR-007, SLC-007, REV-SLC-007 and VER-SLC-007.

## Traceability Update State

- CurrentIndex updated: yes, SLC-007 in review.
- Relations updated: yes.
- Ledger updated: PR #4 merge and Sprint/Iteration/Slice start recorded.

## Open Risks Or Ambiguities

- TX starts on the global divider wrap; RX injection resets only RX subphase.
- RX final-shift update is at STOP start after START+8+ninth intervals; tests
  must freeze this documented timing independently.
- Old classic serial fields remain for classic variants and must not be reused
  as sole SAB TX/RX storage.

## Worktree Notes

Branch `codex/sab80535-uart` starts exactly from current master
`c0cd6f26bd8984c9fed10eb81716619cb1bb96e6`.

## Issue Checkpoint

Master opening checkpoint:
`https://github.com/Hans-Einar/emuSA80535-N/issues/2#issuecomment-5486344066`.

Worker completion checkpoint:
`https://github.com/Hans-Einar/emuSA80535-N/issues/2#issuecomment-5486494375`.
