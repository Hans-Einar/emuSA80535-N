# VER-SLC-014 — Deterministic MYMOS ADC verification

Status: passed
Verified Slice: SLC-014
Verified product/test HEAD: `0bd39132b2eaffbfc5190e223b54743f17fc68fa`
Verified product tree: `d6e6ee7f93d545f61f3b33abe08070cc0e0964c2`
Verifier: Master
Verified: 2026-09-02

## Result

Passed. SLC-014 satisfies REQ-013. REV-SLC-014 approved the exact product HEAD
with no findings, so no corrective Slice is required.

## Master executable evidence

1. Windows GCC 12.2 `make core-test` — Stage0, IRQ, Timer, UART, ports, edges
   and ADC suites passed.
2. Windows GCC `make debug-test` — frozen debug facade and process suites
   passed.
3. Windows Clang 18.1.8 strict C99, `-Wall -Wextra -Wshadow -Werror
   -pedantic` — all seven core suites and debug facade/process passed from a
   clean rebuild.
4. openSUSE Leap 15.5 WSL GCC 7.5 — all seven core suites and Linux debug
   facade/process passed with Python 3.13.9.
5. WSL GCC ASan+UBSan with warnings-as-errors, frame pointers, leak detection
   and halt-on-error — all seven core suites and debug facade/process passed
   without report.
6. Frozen DAP exact HEAD `36639b48ddb2ffbafa14c00da794fe1734f7483b`
   was checked out into a detached temporary worktree, installed from lock,
   built unchanged and passed 45/45 contract, fixture/hash and real contract/
   fake equivalence/F5 smoke against the current emulator executable. The
   temporary worktree was removed afterward.

## Coverage

The focused suite covers every Issue #13 class 1..28: reset; all channels;
every DAPR byte; virtual and exact cycle-15 timing; BSY protection; ADDAT/IADC;
EADC/EAL pending and enable-after-pending; vector/priority/preemption/RETI;
software-clear IADC; restart/no-ghost and changed-context latch; reference
ranges/zero endpoints/clipping/floor; long replay; multi-cycle/entry/ISR/IDLE;
BD/CLK/UART preservation; callbacks; observer neutrality; all accepted core,
debug and DAP regressions; classic isolation and forbidden scope.

REV-SLC-014 additionally passed an independent 5,963,941-state uint64 oracle
over valid reference/input combinations plus all 256 DAPR bytes, opcode timing,
cycle-14 restart, nested/direct callback mutation, BSY and reset under Windows
and WSL sanitizers.

## Identity, scope and traceability evidence

- Authorized master `a4d24786eb86a55479adc4ef14d0f27424fb5705` is an
  ancestor of the reviewed product commit.
- Product tree/parent match REV-SLC-014. The diff is exactly `README.md`,
  `core.c`, `emu8051.h`, `tests/Makefile` and `tests/test_stage3_adc.c`.
- Later commits are SDP-only; all product/test blobs remain identical to the
  reviewed product commit.
- `opcodes.c`, frozen `emu_debug.c`, `emu_debug.h`, `emu_debug_server.c`, debug
  tests and DAP integration test have unchanged blobs.
- Case-sensitive added-line audit found no P1000/Ponsse/NEC/D71055/PCB3,
  Timer2/T2CON/CCEN/capture-compare, target calibration, live GPIO/serial/CAN/
  USB, physical control, wall-clock/thread/socket/device or protocol behavior.
- Ponsse PR #25 remains at exact evidence HEAD
  `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9`.
- Ledger parsing passed with 158 unique valid NDJSON events before this record;
  the complete REQ-013/design/Slice/review/verification relation chain exists.
- Emulator and DAP worktrees are clean after build and temporary-worktree
  cleanup. The unrelated pre-existing Ponsse `MVP1/` content was untouched.

## Timing authority disposition

The Siemens manual provides no numeric MYMOS offsets for figure 7-32's generic
early BSY/IADC anticipation. Issue #13 explicitly fixes the externally visible
completion boundary. Worker, Reviewer and Master therefore accept atomic
ADDAT/BSY/IADC exposure at exact cycle 15 as the non-invented deterministic
SLC-014 abstraction.

## First remaining blocker

Open the focused implementation PR against `master`, leave it unmerged, and
publish the exact accepted CPU revision to Issue #13 and Ponsse Issues #26/#47.
