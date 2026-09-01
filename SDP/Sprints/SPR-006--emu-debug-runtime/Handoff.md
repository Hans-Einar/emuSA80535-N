# Handoff

## Current Objective

Implement and verify SLC-011, the complete frozen `emu-debug` protocol 1.0
runtime, without changing DAP product sources or entering unrelated emulator
scope.

## Authoritative Source Documents

- repository `AGENTS.md` and SDP lifecycle;
- Issue #6;
- DAP PR #4 exact HEAD `36639b48ddb2ffbafa14c00da794fe1734f7483b`;
- `SDP/02--Study/EMU-DEBUG-STU-002.md`;
- `SDP/03--Requirements/EMU-SAB80535-REQ-001.md` REQ-016;
- `SDP/04--Architecture/EMU-DEBUG-ARCH-002.md`;
- `SDP/05--Design/EMU-DEBUG-DES-006.md`;
- this Sprint's `sprint.md` and `ScrumIterations.md`.

## Done

- PR #8 merged and SLC-010 ancestry confirmed.
- Exact post-merge master and exact DAP authority frozen.
- Contract revalidated with no architectural conflict found.
- SPR-006 / ITR-011 / SLC-011 defined and traceability activated.

## Not Done

- Worker implementation, review, verification, DAP real integration and PR.

## Exact Next Step

Dispatch one fresh Worker against SLC-011. Immediately after its committed
result, dispatch a separate fresh Reviewer before acceptance.

## Verification Completed

Only baseline ancestry and frozen-authority checks. No SLC-011 product evidence
yet.

## Traceability IDs In Play

MND-001, STU-002, REQ-016, ARCH-007..ARCH-010, DES-050..DES-063, SPR-006,
ITR-011, SLC-011, REV-SLC-011 and VER-SLC-011.

## Traceability Update State

- CurrentIndex updated: yes; SPR-006 / ITR-011 / SLC-011 active.
- Relations updated: yes.
- Ledger updated: baseline, Sprint, Iteration and Slice events appended.

## Open Risks Or Ambiguities

- Wire details must match the frozen DAP fake/client exactly, not merely the
  prose summary.
- Decode predecessor knowledge and breakpoint replacement must remain
  deterministic across load/reset/run invalidation.
- Windows stdio/text-mode behavior must not corrupt NDJSON or executable
  lifecycle.

## Worktree Notes

Branch `codex/emu-debug-runtime` starts exactly at
`b8a8fe67f37af250cbe20e3ce3450edfe4ea5fdf`.

## Issue Checkpoint

Baseline/activation checkpoint:
https://github.com/Hans-Einar/emuSA80535-N/issues/6#issuecomment-5492796877
