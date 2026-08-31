# EMU-SAB80535-DES-001 — Stage 0 foundation design

Status: active target design  
Design items: DES-001..DES-010  
Established: 2026-08-31

## DES-001 — Backward-compatible initialization

Keep the upstream `reset()` entry point for classic callers. Add an explicit
variant initialization path whose selected descriptor survives reset. The SAB
descriptor owns 256-byte IRAM availability and its SFR reset table.

## DES-002 — Variant identities

Expose stable identities for classic 8051, classic 8052 and SAB80535. Opcode
execution may query capabilities through the selected variant; it must not use
preprocessor remapping of `REG_IP` to resolve the SAB conflict.

## DES-003 — Owned upper IRAM

The CPU object must safely provide the upper indirect 128 bytes for SAB80535
without requiring an embedding caller to allocate it. Direct addresses
`80..FF` remain SFR space; only indirect access reaches upper IRAM.

## DES-004 — Raw loader

`em8051_load_binary()` accepts only exactly 65536 bytes, reports I/O/size
errors, and does not alias or resize XDATA. Intel HEX loading remains available.

## DES-005 — Counters

Record 64-bit executed instruction count and machine-cycle count. Counters are
reset deterministically and updated once per architectural instruction using
the existing opcode tick return value.

## DES-006 — Run control

Provide one-instruction stepping plus `run(max_instructions)`,
`run_until_pc(target,max_instructions)` and breakpoint behavior. Return an enum
stop reason for limit, breakpoint, target, exception or halt-like conditions.

## DES-007 — Trace contract

An optional callback receives normalized records for instruction boundaries,
SFR writes and MOVX reads/writes. Each record includes machine cycle and PC;
memory records include address/value. A null callback has no behavioral effect.

## DES-008 — SFR access strategy

Stage 0 establishes one internal read/write gateway usable by direct and
bit-addressed opcode paths. The worker must audit exceptional accumulator/PSW
forms before claiming complete SFR observation. Tests cover representative
direct, bit and read-modify-write forms.

## DES-009 — Core-only test harness

Compile `core.c`, `opcodes.c` and required decoding/loading units without the
curses frontend. Tests use tiny synthetic machine-code images and deterministic
assertions. No external test framework is required.

## DES-010 — Stage boundary

Stage 0 may define SAB SFR addresses/reset values and variant hooks, but it
must not claim the Stage 1 12-source controller, UART timing or completed
behavioral peripherals. Those remain explicit target-state work.

## Traceability

DES-001..DES-010 refine ARCH-001..ARCH-006 and constrain SLC-001.

