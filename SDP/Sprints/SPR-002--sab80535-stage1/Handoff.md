# Handoff

## Current Objective

Implement and independently review SLC-004 only: the generic Siemens
SAB80535 interrupt controller.

## Authoritative Source Documents

- repository `AGENTS.md`;
- lifecycle documents under `SDP/01--Mandate` through `SDP/05--Design`;
- `SDP/05--Design/EMU-SAB80535-DES-002.md`;
- this Sprint's `sprint.md` and `ScrumIterations.md`;
- Issue `https://github.com/Hans-Einar/emuSA80535-N/issues/2`;
- Ponsse PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`;
- Siemens SAB80515/80535 user manual interrupt chapter;
- Ponsse Issue #26 and PR #27 reconciliation.

## Done

- Stage 0 accepted on PR #1.
- Issue #2 authority read and reconciled with primary Siemens semantics.
- SPR-002 / ITR-004 / SLC-004 contract and traceability opened.

## Not Done

- SLC-004 product implementation, review, verification and stacked PR.
- Timer0/Timer1 and UART later slices; these are not authorized.

## Exact Next Step

Fresh Worker implements only SLC-004 against the active contract and commits
focused product/tests. Fresh Reviewer then inspects the exact implementation
HEAD.

## Verification Completed

Authority/readiness review only. Stage 0 evidence remains the regression
baseline.

## Traceability IDs In Play

MND-001, STU-001, REQ-009, ARCH-002/003/005/006, DES-011..DES-018,
SPR-002, ITR-004, SLC-004, REV-SLC-004 and VER-SLC-004.

## Traceability Update State

- CurrentIndex updated: yes, SLC-004 active.
- Relations updated: yes.
- Ledger updated: yes, Sprint/Iteration/Slice start recorded.

## Open Risks Or Ambiguities

- Exact S5P2 electrical sampling is represented at instruction-boundary
  fidelity, as authorized; no cycle-subphase claim is made.
- Timer/UART request production is later work. SLC-004 consumes asserted
  canonical flags/synthetic requests without claiming new timing fidelity.
- PR #1 is not merged, so the Stage 1 PR must initially be stacked on the
  Stage 0 branch rather than duplicating/merging it silently.

## Worktree Notes

Working branch: `codex/sab80535-interrupt-controller` based on
`codex/sab80535-foundation` HEAD `62f40127...`.

