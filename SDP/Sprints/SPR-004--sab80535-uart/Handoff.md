# Handoff

## Current Objective

Complete fresh REV-SLC-008 review of corrective HEAD `336c50a0...`, then run
Master verification only if approved.

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
- REV-SLC-007 completed with `changes-required`: broken SAB SBUF callbacks and
  unsupported mode/BD writes replacing pending mode3 frames.
- SLC-008 corrective commit `336c50a03da4067c22d9567de6075724261a66e9`
  completed; fresh review is running.

## Not Done

- SLC-008 fresh review, Master verification and PR.
- Stage 2, ADC, Timer2, P1000 target or any live/physical I/O; unauthorized.

## Exact Next Step

Fresh Reviewer probes callback/mode isolation on exact corrective HEAD.

## Verification Completed

REV-SLC-007 reran all platforms/sanitizers and reproduced F001/F002. Prior
suites remain green.

## Traceability IDs In Play

MND-001, STU-001, REQ-011, ARCH-002/003/004/005/006, DES-027..DES-038,
SPR-004, ITR-008, SLC-008, REV-SLC-007-F001/F002, REV-SLC-008 and
VER-SLC-008.

## Traceability Update State

- CurrentIndex updated: yes, SLC-008 in review.
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

Reviewer changes-required checkpoint:
`https://github.com/Hans-Einar/emuSA80535-N/issues/2#issuecomment-5486557035`.

Corrective Worker checkpoint:
`https://github.com/Hans-Einar/emuSA80535-N/issues/2#issuecomment-5486601021`.
