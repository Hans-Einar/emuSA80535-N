# EMU-SAB80535-REQ-001 — SAB80535 emulator requirements baseline

Status: active target baseline  
Requirement set: REQ-001..REQ-016
Established: 2026-08-31

## Stage 0 — deterministic core boundary

- **REQ-001 — Provenance and regression:** preserve upstream ancestry, MIT
  license/copyright and classic instruction behavior; provide a reproducible
  core-only build/test gate.
- **REQ-002 — Explicit variant:** select classic 8051/8052 or SAB80535 without
  treating the classic `REG_IP=B8` definition as SAB80535 semantics.
- **REQ-003 — Memory:** load an exact 65536-byte raw CODE image, retain strict
  CODE/XDATA separation and provide 256-byte indirect IRAM for SAB80535.
- **REQ-004 — Deterministic time:** expose monotonic 64-bit instruction and
  machine-cycle counters; default SAB oscillator is 11059200 Hz and one
  classical machine cycle is 12 oscillator clocks.
- **REQ-005 — SAB SFR foundation:** register canonical addresses for IEN0 A8,
  IP0 A9, IEN1 B8, IP1 B9, IRCON C0, CCEN C1, T2CON C8, CRCL CA, CRCH CB,
  TL2 CC, TH2 CD, ADCON D8, ADDAT D9, DAPR DA, P6 DB, P4 E8 and P5 F8.
- **REQ-006 — Run control:** support bounded run-N, breakpoint and run-until-PC
  with explicit stop reasons rather than unbounded host loops.
- **REQ-007 — Diagnostics:** expose deterministic instruction/SFR/MOVX trace
  hooks containing virtual time and PC where applicable; unsupported reached
  resources must be diagnosable.
- **REQ-008 — Deterministic reset:** allow a fixed power-on IRAM seed while
  documenting that hardware RAM is undefined rather than architecturally zero.

## Stage 1 — scheduling and serial blockers

- **REQ-009:** implement 12 distinct pending/enabled/in-service interrupt
  sources, four priorities from paired `IP1.x:IP0.x`, fixed polling order,
  higher-level preemption and RETI in-service release.
- **REQ-010:** implement Timer 0 mode 1 and Timer 1 mode 2 at deterministic
  machine-cycle timing, including the verified 8977-tick and 19200-baud cases.
- **REQ-011:** implement mode-3 9-bit UART with TB8/RB8, shared RI/TI interrupt
  and in-memory I/O only.

## Stages 2–5

- **REQ-012:** model P1/P3/P4/P5 latch versus resolved pins,
  quasi-bidirectional behavior, read-modify-write latch semantics, timestamped
  INT0..INT6 edges and generic bank-aware MOVX callbacks.
- **REQ-013:** model DAPR-started ADC conversion, 15-cycle normal completion,
  ADDAT/BSY/IADC and injectable AN0..AN7.
- **REQ-014:** model the reached Siemens Timer 2 fosc/12, overflow, software
  reload and P5.4 behavior without substituting generic 8052 semantics.
- **REQ-015:** provide deterministic in-memory capture/replay comparison and
  promote other peripherals only when evidence demonstrates need.

## Generic debug runtime

- **REQ-016 — Headless debug protocol:** provide a buildable Linux/Windows
  no-curses child-process runtime implementing the frozen `emu-debug` 1.0
  commands and required capabilities at DAP PR #4 exact HEAD
  `36639b48ddb2ffbafa14c00da794fe1734f7483b`. The runtime must use bounded
  UTF-8 NDJSON stdio, deterministic synchronous execution and atomic public
  snapshots, preserve all accepted CPU/peripheral behavior, and open no
  physical or target-specific I/O.

## Cross-cutting acceptance rules

- No P1000-specific implementation names, addresses or board behavior may
  enter the generic CPU source.
- No live serial output, physical GPIO or machine-control backend may be added.
- Every Stage has focused tests, independent review under `SDP/CodeReview`,
  verification evidence under `SDP/Verification`, a commit and updated Sprint
  handoff.

## Traceability

REQ-001..REQ-015 derive from MND-001 and STU-001. REQ-016 derives from
MND-001, STU-002 and Issue #6. Stage 0 requirements
REQ-001..REQ-008 are assigned to SPR-001.

