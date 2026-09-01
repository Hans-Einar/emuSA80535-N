# EMU-DEBUG-STU-002 — emu-debug 1.0 baseline and contract study

Status: accepted
Study ID: STU-002
Accepted: 2026-09-01
Steering authority: `Hans-Einar/emuSA80535-N#6`

## Frozen baseline

- PR #8 merged as `b8a8fe67f37af250cbe20e3ce3450edfe4ea5fdf`.
- Exact implementation baseline is current `master` at that merge commit.
- Accepted SLC-010 product commit
  `ba00ef17af57076b01c7f548e8996a7d36a5c591` is an ancestor of the baseline.
- Frozen DAP consumer is `Hans-Einar/emuSA80535-DAP` PR #4 exact HEAD
  `36639b48ddb2ffbafa14c00da794fe1734f7483b`.
- Frozen contract is that revision's
  `protocol/EMU_DEBUG_API_REQUIREMENTS.md` for `emu-debug` protocol 1.0.

The older emulator baseline quoted in Issue #6 and in the DAP contract is
historical evidence only. It is not the implementation baseline for SPR-006.

## Current-master revalidation

Current master retains the accepted Stage-0 deterministic variant, reset,
64-KiB CODE loader, decoder, bounded run, single-step and typed stop seams. It
also contains all accepted Stage-1 interrupt, Timer0/Timer1 and mode-3 UART
behavior plus the accepted SLC-010 port/pin/MOVX context behavior.

The frozen protocol does not conflict with those architectures. Its missing
parts are compatible extensions:

- a stable public debugger snapshot rather than serialization of private
  `struct em8051` layout;
- a debugger-owned replacement breakpoint table and decode history;
- a serialized bounded command facade over existing deterministic core calls;
- a standalone no-curses stdio executable with bounded NDJSON framing,
  SHA-256 validation and portable cleanup.

No DAP contract change or DAP product-source change is authorized or required
by this assessment. Any mismatch discovered during implementation or real
integration must be classified and reported rather than papered over.

## Frozen command and capability set

Commands: `hello`, `load`, `reset`, `getState`, `decodeCode`,
`replaceCodeBreakpoints`, `run`, `stepInstruction`, `terminate`.

Required capabilities: `rawCode64k`, `deterministicReset`,
`snapshotBasicRegisters`, `decodeCode`, `replaceCodeBreakpoints`,
`boundedRun`, `stepInstruction`.

The runtime uses UTF-8 NDJSON over child stdin/stdout, reserves stdout for
protocol only, bounds input/allocation/work, executes only during synchronous
commands, and returns atomic instruction-boundary snapshots. It must perform
no P1000 policy, target decode, live serial/GPIO, or physical I/O.

## Blocker baseline

At SLC-011 activation, `EMU-BLK-004` is satisfied by the accepted core.
`EMU-BLK-001..003` and `EMU-BLK-005..010` are active implementation and
verification obligations of SLC-011.

## Traceability

STU-002 informs REQ-016, ARCH-007..ARCH-010, DES-050..DES-063 and SPR-006.
