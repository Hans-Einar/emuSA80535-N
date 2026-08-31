# VER-SLC-006 — Deterministic Timer0/Timer1 verification

Status: passed
Verified Slice: SLC-006
Verified product HEAD: `30cf42efa845d29a47a950eca7bbaf657490fbe6`
Verifier: Master
Verified: 2026-08-31

## Result

Passed. REQ-010 and the Issue #2 SLC-006 acceptance gate are satisfied. This
record does not authorize REQ-011.

## Master evidence

1. Windows GCC `make core-test` — Stage0, IRQ and timer suites passed.
2. Windows strict Clang C99/`-Werror`/`-pedantic` — all three suites passed.
3. openSUSE Leap 15.5 WSL GCC `make core-test` — all three suites passed.
4. WSL GCC ASan/UBSan with warnings-as-errors — all three suites passed with no
   sanitizer report.
5. Product blobs after reviewed HEAD — unchanged by SDP-only commits.
6. Product diff check `b2d3491..30cf42e` — passed.
7. Product no-target and added-line no-UART audit — no hits.
8. Ledger NDJSON parsing and current-master ancestry — passed.
9. Test cleanup returned a clean worktree.

## Coverage

The focused timer suite covers all 16 Steering verification classes, including
exact 8977/3-cycle boundaries, live SFR writes, controller vectors, entry/ISR
counting, four-interrupt software cadence, repeated events independent of TF1,
long integer no-drift, classic regression, observer neutrality and UART-state
noninterference. REV-SLC-006 adds independent long-run, IDLE, eligibility and
legacy-serial probes.

## First remaining blocker

Publish/review the focused timer PR and report the exact CPU product commit to
Ponsse Issue #26. The 9-bit UART Slice remains behind Steering authorization.

