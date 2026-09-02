# Handoff

## Current Objective

Execute SPR-009 / ITR-015 / SLC-015 only: the reached Siemens Timer2
timer-function/software-reload path authorized by Issue #17.

## Authoritative Source Documents

- repository `AGENTS.md` and lifecycle SDP;
- Issue #17;
- `SDP/05--Design/EMU-SAB80535-DES-009.md`;
- accepted Stage0..Stage3 review/verification;
- Siemens SAB80515/SAB80C515 User's Manual Edition 08.95 Timer2 sections;
- Ponsse PR #25 HEAD `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9`
  as consumer evidence only.

## Frozen State

- Master baseline: `bc86d2633b6057529e6fd1e666896c24d72822aa`.
- Branch: `codex/sab80535-timer2`.
- Active work: SPR-009 / ITR-015 / SLC-015.
- Product implementation has not yet been committed at this opening handoff.

## Exact Next Step

Fresh Worker implements the bounded product/test Slice, runs focused and
accepted regressions, commits exact product/test HEAD and reports it for fresh
independent review.

## Stop Boundary

Do not add external/gated Timer2 inputs, hardware reload modes, EXF2 producer,
compare/capture, automatic P5.4 coupling, target/board semantics, physical/live
I/O or `emu-debug` protocol changes.

## Traceability IDs In Play

MND-001, STU-001, REQ-014, ARCH-002..ARCH-006, DES-086..DES-095, SPR-009,
ITR-015, SLC-015; expected REV-SLC-015 and VER-SLC-015.
