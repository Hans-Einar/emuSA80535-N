# VER-SLC-010 — Port latch/pin and MOVX verification

Status: passed
Verified Slice: SLC-010
Verified product HEAD: `ba00ef17af57076b01c7f548e8996a7d36a5c591`
Verifier: Master
Verified: 2026-09-01

## Result

Passed. The Issue #7 SLC-010 portion of REQ-012 is accepted. REQ-012 remains
target/active because the external-edge portion is not authorized or delivered.

## Master evidence

1. Windows GCC `make core-test` — Stage0, IRQ, timer, UART and Stage2 passed.
2. Windows strict Clang C99/`-Werror`/`-pedantic` — all five passed.
3. openSUSE Leap 15.5 WSL GCC — all five passed.
4. WSL GCC ASan/UBSan warnings-as-errors — all five passed, no report.
5. Product blobs after reviewed HEAD — unchanged by SDP handoff commit.
6. Product diff check `459e731..ba00ef1` — passed.
7. Product no-target and no-edge/live added-line audit — no hits.
8. Ledger parsing and current-master ancestry — passed.
9. Test cleanup returned a clean worktree.

## Coverage

All 20 Issue #7 classes are covered: reset/resolution/API, byte/bit ordinary and
full RMW set, P4/P5, external-latch independence, mutable callbacks, all MOVX
forms/context/snapshot ordering, backing/legacy callbacks, no-edge state and
prior/classic regressions. REV-SLC-010 adds independent handler-table and
literal opcode audit.

## First remaining blocker

Publish/review the focused SLC-010 PR and report exact CPU product HEAD to
Ponsse Issue #26. Then stop pending external-edge authorization.

