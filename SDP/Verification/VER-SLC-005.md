# VER-SLC-005 — Siemens interrupt controller verification

Status: passed
Verified Slice: SLC-005 / cumulative SLC-004
Verified product HEAD: `85fa6e1f8318c691598b9a2ae191d1bd94b436c8`
Cumulative controller commit: `529602d800e36e87f495ef088f17e67ba3659a1a`
Verifier: Master
Verified: 2026-08-31

## Result

Passed. The Siemens SAB80535 interrupt-controller portion of REQ-009 satisfies
the Issue #2 SLC-004 acceptance gate. No later Stage 1 slice is authorized by
this result.

## Master commands and results

1. Windows GCC: `make core-test` — Stage 0 and Stage 1 passed.
2. Windows strict Clang through `tests/Makefile` with C99, `-Werror`,
   `-Wno-unused-parameter`, `-Wshadow`, `-pedantic` and UCRT annotation define —
   Stage 0 and Stage 1 passed.
3. openSUSE Leap 15.5 WSL: `make core-test` — Stage 0 and Stage 1 passed.
4. WSL ASan/UBSan with `-fsanitize=address,undefined` — both suites passed with
   no report.
5. Product blobs after the reviewed corrective HEAD — unchanged; subsequent
   commits contain SDP records only.
6. Product diff checks for `f09b1d3..529602d` and `ec5155f..85fa6e1` — passed.
7. Product no-target audit — no target-specific or physical-control identities
   in implementation/test sources.
8. `Ledger.ndjson` line parsing — passed.
9. Accepted Stage 0 commit `dbcc6b74...` is an ancestor — passed.
10. Test cleanup returned the worktree to a clean state.

## Integrated coverage

Twelve Stage 1 test groups cover all source/vector/order, masking, pair
priority/four levels, nesting/RETI, RET non-release, request clearing,
Timer2 split gates, Timer1 persistence, inhibit timing, immutable trace,
failed entry and classic regressions. REV-SLC-005 additionally used literal
raw-register/byte probes independent of implementation enums.

## First remaining blocker

Publish the stacked implementation PR and report the exact reviewed CPU commit
to Ponsse Issue #26. Timer0/Timer1 timing and 9-bit UART require new
SteeringGroup authorization and are not active work.

