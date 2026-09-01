# VER-SLC-009 — Mode-3 9-bit UART verification

Status: passed
Verified Slice: SLC-009 / cumulative SLC-007
Verified product HEAD: `d2d3f63b1a10a0b042d052dd6d872c708db967e0`
Cumulative UART commit: `d2fe5d31ac887d81f0dfd17cbbfa222abba1acf3`
Verifier: Master
Verified: 2026-09-01

## Result

Passed. REQ-011 and the Issue #2 SLC-007 gate are satisfied. This record stops
before Stage2, ADC, Timer2 or target/live I/O.

## Master evidence

1. Windows GCC `make core-test` — Stage0, IRQ, timer and UART suites passed.
2. Windows strict Clang C99/`-Werror`/`-pedantic` — all four suites passed.
3. openSUSE Leap 15.5 WSL GCC — all four suites passed.
4. WSL GCC ASan/UBSan with warnings-as-errors — all four passed, no report.
5. Product blobs after final reviewed HEAD — unchanged by SDP-only commits.
6. Product diff checks for SLC-007/008/009 — passed.
7. Product no-target and no-OS/live-API added-line audit — no hits.
8. Ledger parsing and current-master ancestry — passed.
9. Test cleanup returned a clean worktree.

## Coverage

All 18 Steering classes are covered: exact 16/32 divider/phase and long-run
19200 timing; separate SBUF roles; TX bit order/TB8/TI/STOP/back-to-back/repeat;
REN/RI/SM2/RB8 RX timing/loss; full duplex; RI/TI vector/software/RETI;
instruction ISR fixture; immutable observer/reset; classic variants. Three
independent review passes additionally cover callback contracts, mode/BD
isolation and reentrant RX coherence.

## First remaining blocker

Publish/review the focused UART PR and report exact CPU product HEAD to Ponsse
Issue #26. Then stop pending new Steering authorization.

