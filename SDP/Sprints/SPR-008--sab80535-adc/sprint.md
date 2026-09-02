# SPR-008 — SAB80535 deterministic ADC

Status: ready-for-pr
Sprint ID: SPR-008
Started: 2026-09-02
Steering authority: Issue #13 SLC-014
Frozen master baseline: `a4d24786eb86a55479adc4ef14d0f27424fb5705`
Consumer evidence: Ponsse PR #25 HEAD `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9`

## Goal

Implement the generic MYMOS SAB80535 single-conversion ADC path required by
REQ-013: DAPR start/restart, AN0..AN7, programmable references, exact virtual
timing, ADDAT/BSY/IADC and accepted interrupt-controller service.

## Authorized scope

- ADCON MX/ADM/BSY semantics required for ADM=0 single conversion;
- generic normalized AN0..AN7 injection and deterministic sampling;
- DAPR reference programming, start/restart and diagnostic invalid ranges;
- exact 15-machine-cycle MYMOS completion;
- ADDAT, BSY and canonical IADC lifecycle;
- existing EADC/EAL/priority/preemption/RETI integration;
- immutable ADC trace and bounded generic consumer fixture;
- complete accepted regression and cross-platform matrix.

## Non-goals

- Siemens Timer2, capture/compare or P5.4 alternate producer behavior;
- full continuous ADC auto-restart mode;
- P1000 analog circuitry, calibration, connector or board/NEC logic;
- physical analog input, live serial/GPIO/CAN/USB or machine control;
- any frozen `emu-debug` protocol, command or capability change.

## Planned iteration

- **ITR-014 / SLC-014:** deterministic MYMOS single-conversion ADC.
- A bounded corrective Slice is created only if REV-SLC-014 opens findings.

## Exit criteria

- all 28 Issue #13 verification classes pass;
- ADCON, DAPR, reference, timing and request semantics are independently
  reconciled with the Siemens manual;
- all accepted suites pass Windows GCC, strict Clang, WSL GCC and WSL
  ASan+UBSan, including frozen debug/DAP integration;
- independent review approves an exact product HEAD and Master verifies it;
- traceability, verification, handoff and Issue #13/Ponsse checkpoints are
  complete;
- a focused implementation PR is opened and left unmerged;
- no unauthorized Timer2/target/live/protocol scope enters the diff.

## Verified result

REV-SLC-014 approved exact product/test HEAD
`0bd39132b2eaffbfc5190e223b54743f17fc68fa` with no findings.
VER-SLC-014 passed the complete Windows/strict Clang/WSL/sanitizer/core/debug
and exact frozen-DAP matrix. SLC-014 is accepted and satisfies REQ-013.
SPR-008 is ready for its focused unmerged PR.
