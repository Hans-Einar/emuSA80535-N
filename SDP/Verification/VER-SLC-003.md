# VER-SLC-003 — Stage 0 deterministic foundation verification

Status: passed
Verified Slice: SLC-003 / cumulative Stage 0
Verified HEAD: `dbcc6b74adbc34896a71401366991229c0d58922`
Verifier: Master
Verified: 2026-08-31

## Result

Passed. The cumulative Stage 0 product HEAD satisfies REQ-001..REQ-008 within
the documented boundary and is ready for the first pull request.

## Master commands and results

1. Windows GCC: `make core-test` — passed (`Stage-0 tests passed`).
2. Windows Clang strict:
   `clang -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -Wshadow
   -pedantic -D_CRT_SECURE_NO_WARNINGS ...` — compiled and suite passed.
3. openSUSE Leap 15.5 WSL: `make core-test` — passed.
4. WSL ASan/UBSan:
   `make -C tests clean test CFLAGS='-O1 -g -std=c99 -Wall -Wextra
   -Wno-unused-parameter -Wshadow -fsanitize=address,undefined
   -fno-omit-frame-pointer'` — passed with no sanitizer report.
5. Product commit diff checks for SLC-001, SLC-002 and SLC-003 — passed.
6. Product no-target audit for P1000/Ponsse/D71055/hydraulic/valve/physical-I/O
   terms — no hits.
7. Every `Ledger.ndjson` line parsed through `ConvertFrom-Json` — passed.
8. `git merge-base --is-ancestor 5dc6812 HEAD` — passed.
9. GitHub repository metadata — true fork of `jarikomppa/emu8051`, MIT license
   and default upstream history preserved.

The first strict Clang invocation omitted the repository's established
`-Wno-unused-parameter` flag and therefore rejected pre-existing disassembler
callbacks. Re-running with the same warning policy used by both Makefiles
passed; no product change was hidden by that correction.

## Focused coverage

Nine integrated test groups cover:

- explicit classic/8052/SAB variants and canonical Stage 0 SFR map;
- classic opcode/interrupt regression;
- upper indirect IRAM and stack behavior including SAB `SP=A2`;
- invalid SFR gateway inputs;
- exact 65536-byte raw CODE loader and error cases;
- counters, multi-cycle/timer boundaries and run/breakpoint control;
- interrupt-entry latency and vector stop ordering;
- exact instruction/SFR/MOVX/unsupported trace chronology and callback
  immutability;
- deterministic seeded reset/replay.

The independent final reviewer additionally compared all 256 opcodes between
raw tick/drain and bounded-step execution.

## Known environmental limitation

The upstream curses frontend full build was attempted before semantic changes
on Windows and openSUSE WSL. Both environments lack the external `curses.h`
development header. The core-only gate is verified; the missing dependency is
not reported as a successful full frontend build and is not a CPU defect.

## Stage boundary and first blocker

Stage 0 is verified. Stage 1 is not implemented. The first open implementation
blocker for firmware scheduling is the SAB80535 12-source/four-level interrupt
controller with true IEN0/IP0/IEN1/IP1/IRCON semantics; classic `IP=B8` must
remain isolated from the SAB variant.

