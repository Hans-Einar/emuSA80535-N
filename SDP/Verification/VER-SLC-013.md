# VER-SLC-013 — Deterministic external-edge verification

Status: passed
Verified Slice: SLC-013
Verified product/test HEAD: `3cfc4a8e9a5accb4a91df36b0b03119bc4d1de9b`
Verified product tree: `f691504135f3caf2f61dd3e85069f77b2a6fd7ec`
Verifier: Master
Verified: 2026-09-02

## Result

Passed. SLC-013 completes the external-edge remainder of REQ-012. Combined
with accepted SLC-010, REQ-012 is satisfied/current. No corrective Slice is
required after REV-SLC-013 approved the exact product HEAD with no findings.

## Master executable evidence

1. Windows GCC 12.2 `make core-test` — Stage0, IRQ, timer, UART, port and edge
   suites passed.
2. Windows GCC `make debug-test` — debug facade and child-process protocol
   suites passed.
3. Windows Clang 18.1.8 strict C99, `-Wall -Wextra -Wshadow -Werror
   -pedantic` — all six core suites, debug facade and process suites passed.
4. openSUSE Leap 15.5 WSL GCC 7.5 — all six core suites, debug facade and
   Linux process suite passed with Python 3.13.9.
5. WSL GCC ASan+UBSan, warnings-as-errors, frame pointers, leak/UB halt options
   — all six core suites, debug facade and process suite passed without report.
6. Frozen DAP exact HEAD `36639b48ddb2ffbafa14c00da794fe1734f7483b`
   was checked out into a detached temporary worktree, installed from its lock
   file and built unchanged. `tests/test_dap_real.mjs` passed real contract,
   fake-versus-real equivalence and F5 smoke against the current executable.
   The temporary worktree was removed afterward.

## Coverage

The focused suite and independent review cover every Steering class 1..23:
all seven independent pin/flag/vector sources; nonqualifying transitions;
INT0/INT1 edge re-arm and level persistence/release; both I2FR/I3FR selections;
fixed-rising INT4..INT6; EAL and source masking; enable-after-pending/inhibit;
equal polling, higher preemption, RETI waiting; latch immutability and resolved
read coherence; exact cycle/replay/trace neutrality; reset without spurious
edge; generic length and saw vector fixtures; complete regression/scope gates.

REV-SLC-013 additionally passed independent 64-entry fill, ring wrap/refill,
equal-cycle FIFO, multi-cycle instruction, mutable callback and reentrancy
probes on Windows and under WSL sanitizers.

## Identity, scope and traceability evidence

- Authorized master `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b` is an
  ancestor of the product commit.
- Product tree/parent match REV-SLC-013 exactly. The reviewed diff is exactly
  `README.md`, `core.c`, `emu8051.h`, `tests/Makefile`,
  `tests/test_stage2_edges.c` and `tests/test_stage2_ports.c`.
- Later branch commits are SDP-only; all product/test blobs remain identical
  to the reviewed product commit.
- `opcodes.c`, frozen `emu_debug.c`, `emu_debug.h`, `emu_debug_server.c` and
  frozen debug tests have unchanged blobs across the product diff.
- Case-sensitive added-line audit found no P1000/Ponsse/NEC/D71055/PCB3,
  ADC/Timer2 behavior, live GPIO/serial/CAN/USB, physical control, wall-clock,
  thread/socket/device or debug-protocol implementation.
- The only ADC token in added tests is the accepted interrupt-source enum used
  to derive an existing priority-pair index; no ADC behavior is added.
- Ledger parsing passed with 139 unique valid NDJSON events before this record;
  active relation/index chains for SLC-013 review and verification are valid.
- Emulator and DAP worktrees are clean after build and temporary-worktree
  cleanup.

## Accepted authority distinction

The Siemens physical level-mode restriction on writing IE0/IE1 differs from
the pre-existing accepted DES-012 synthetic request seam. SLC-013 separately
tracks hardware level assertions and is line-controlled. REV-SLC-013 confirms
this inherited synthetic test seam is outside the changed product behavior and
is not an SLC-013 finding.

## First remaining blocker

Open the focused implementation PR against `master`, leave it unmerged, and
publish the exact accepted CPU product revision to Issue #7 and Ponsse Issues
#26 and #47.
