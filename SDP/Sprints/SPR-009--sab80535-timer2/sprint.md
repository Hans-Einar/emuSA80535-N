# SPR-009 — SAB80535 deterministic Timer2

Status: active
Sprint ID: SPR-009
Started: 2026-09-02
Steering authority: Issue #17 SLC-015
Frozen master baseline: `bc86d2633b6057529e6fd1e666896c24d72822aa`
Consumer evidence: Ponsse PR #25 HEAD `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9`

## Goal

Implement the reached generic Siemens SAB80535 Timer2 timer-function path for
REQ-014: live TL2/TH2 counting, fosc/12 and fosc/24 prescaling,
reload-disabled overflow, sticky TF2 and accepted interrupt-controller service.

## Authorized scope

- canonical Siemens T2CON masks required by reached behavior;
- T2I1:T2I0=01 timer function and 00 stopped state;
- deterministic T2PS /12 and /24 timing;
- live 16-bit TL2/TH2 software writes and software reload sequences;
- reload-disabled FFFF->0000 overflow and sticky IRCON.TF2;
- repeated overflow event count and immutable diagnostics;
- existing ET2/EAL/priority/preemption/vector-002B/RETI integration;
- complete accepted regression and cross-platform matrix.

## Non-goals

- external-counter or gated Timer2 inputs;
- CRC auto-reload or T2EX external reload/EXF2 production;
- compare/capture behavior;
- automatic Timer2-to-P5.4 coupling;
- P1000 board/connector/output semantics;
- physical/live I/O or frozen `emu-debug` protocol changes.

## Planned iteration

- **ITR-015 / SLC-015:** reached reload-disabled Timer2 timer-function path.
- A bounded corrective Slice is created only if fresh review opens findings.

## Exit criteria

- all Issue #17 verification classes pass;
- Siemens T2CON, /12-/24, reload-disabled and TF2 semantics are independently
  reconciled against Edition 08.95;
- exact `5555 -> overflow` fixture produces 43691 Timer2 increments;
- repeated overflow remains observable while TF2 is sticky;
- software stop/live-write/reload/restart and real vector 002B fixture pass;
- all accepted suites pass Windows GCC, strict Clang, WSL GCC and WSL
  ASan+UBSan, including frozen debug/DAP integration where available;
- fresh Reviewer approves an exact product HEAD and Master verifies it;
- traceability, handoff and Ponsse #26/#47 checkpoints are complete;
- focused implementation PR is opened and left unmerged;
- no unauthorized reload/capture/target/live/protocol scope enters the diff.
