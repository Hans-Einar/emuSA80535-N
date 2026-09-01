# SPR-005 — SAB80535 ports and MOVX seam

Status: ready-for-pr
Sprint ID: SPR-005
Started: 2026-09-01
Steering authority: Issue #7 SLC-010
Base: `master` at `88ccb2b45976c137820cdc56eec550d953bcf76d`

## Goal

Introduce separate latch/external/resolved state for SAB P1/P3/P4/P5 and an
immutable bank-aware MOVX transaction context based on P1 latch.

## Authorized scope

- SLC-010 subset of REQ-012 only;
- deterministic quasi-bidirectional digital port resolution;
- ordinary pin reads versus documented RMW latch reads;
- virtual host drive/release/latch/pin API;
- callback-compatible port reads/writes;
- P1-latch MOVX context for DPTR and @Ri forms;
- focused Stage0/Stage1/classic regressions.

## Non-goals

- INT0..INT6 edge/level request generation or timestamps;
- ADC, Timer2, Stage 2 continuation beyond SLC-010;
- P1000 bank decode, SRAM/D71055/protocol logic;
- live GPIO/serial, OS device APIs or physical machine control.

## Planned Slice

- **ITR-010 / SLC-010:** port latch/pin model and MOVX seam.
- Corrective Slice(s) only if review opens findings.

## Exit criteria

- all 20 Issue #7 tests and RMW audit pass;
- callback and classic compatibility pass;
- every MOVX form reports exact P1 latch context while backing/callback behavior
  remains intact;
- all accepted suites pass Windows/WSL/strict/sanitizer;
- independent review approves exact product HEAD and Master verifies;
- Issue #7/Ponsse checkpoints and focused PR are published;
- no unauthorized edge/target/live behavior enters the diff.

## Verified result

All technical exit criteria are satisfied at product HEAD
`ba00ef17af57076b01c7f548e8996a7d36a5c591` through REV-SLC-010 and
VER-SLC-010. Sprint remains `ready-for-pr` until the real PR identity exists.
