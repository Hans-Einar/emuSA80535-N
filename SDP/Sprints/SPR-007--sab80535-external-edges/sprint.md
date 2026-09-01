# SPR-007 — SAB80535 deterministic external edges

Status: active
Sprint ID: SPR-007
Started: 2026-09-01
Steering authority: Issue #7 SLC-013 checkpoint
Frozen master baseline: `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`
Consumer evidence: Ponsse PR #25 HEAD `6a22d8713b607308c94f02df17d35ddbe8a36d6a`

## Goal

Complete the external-edge portion of REQ-012 by connecting deterministic
INT0..INT6 virtual pin transitions through the accepted resolved-pin model to
the accepted Siemens interrupt controller.

## Authorized scope

- canonical INT0..INT6 external pin/line mapping for the SAB variant;
- machine-cycle-scheduled virtual line changes and deterministic replay;
- INT0/INT1 IT0/IT1 edge-versus-level behavior;
- Siemens I2FR/I3FR edge selection and fixed INT4..INT6 edge behavior;
- canonical request flags, masking persistence and existing arbitration;
- immutable record-only edge diagnostics;
- bounded length/saw consumer fixtures without target policy;
- full accepted regression and cross-platform matrix.

## Non-goals

- ADC or Siemens Timer2 behavioral implementation;
- P1000 board decode, NEC/D71055 devices, connectors or machine semantics;
- live GPIO/serial/CAN/USB, OS devices or physical machine control;
- direct vector/PC hooks or a second interrupt controller;
- any change to the frozen `emu-debug` protocol, command or capability set;
- oscillator-subphase fidelity not represented by machine-cycle scheduling.

## Planned iteration

- **ITR-013 / SLC-013:** complete the authorized external-edge vertical Slice.
- A bounded corrective Slice is created only if REV-SLC-013 opens findings.

## Exit criteria

- all 23 Steering verification classes pass;
- pin/flag/edge semantics are independently reconciled with Siemens authority;
- external requests use only canonical flags and accepted arbitration;
- P1000 length/saw fixtures pass without target semantics in product code;
- all accepted suites and debug-runtime regression gates pass on the required
  Windows/strict Clang/WSL/sanitizer matrix;
- independent review approves an exact product HEAD and Master verifies it;
- traceability, verification, handoff and Issue #7/Ponsse checkpoints are
  complete;
- a focused implementation PR is opened and left unmerged.
