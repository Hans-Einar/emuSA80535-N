# SPR-006 — emu-debug protocol 1.0 headless runtime

Status: active
Sprint ID: SPR-006
Started: 2026-09-01
Steering authority: Issue #6
Frozen master baseline: `b8a8fe67f37af250cbe20e3ce3450edfe4ea5fdf`
Frozen DAP contract HEAD: `36639b48ddb2ffbafa14c00da794fe1734f7483b`

## Goal

Deliver the complete generic `emu-debug` protocol 1.0 child-process runtime so
DAP PR #4 can use the real emulator unchanged.

## Authorized scope

- REQ-016 and `EMU-BLK-001..010` closure;
- stable snapshot/debugger facade;
- the nine frozen commands and seven required capabilities;
- bounded NDJSON, SHA-256 image verification and portable lifecycle;
- emulator-local unit/process tests and accepted-regression suites;
- exact DAP PR #4 real-runtime integration.

## Non-goals

- any DAP/VS Code product-contract change;
- external-edge continuation from Issue #7, ADC or Timer2;
- readMemory, source maps, watchpoints, stacks, attach/TCP;
- P1000/D71055/board/target policy or firmware inference;
- live serial/GPIO/CAN, physical I/O or machine control.

## Planned iteration

- **ITR-011 / SLC-011:** complete vertical headless runtime and contract tests.
- Corrective fresh-Worker slice only if REV-SLC-011 opens findings.

## Exit criteria

- every `EMU-BLK-001..010` is satisfied with exact evidence;
- all nine commands and seven capabilities pass frozen-contract tests;
- Stage-0/Stage-1/SLC-010 regressions pass Windows and Linux/WSL;
- strict compiler, sanitizer and Linux/Windows process gates pass;
- independent review covers protocol, semantics, safety and DAP compatibility;
- DAP exact HEAD runs against the real executable with no product changes;
- exact reviewed product HEAD is captured and an unmerged PR is opened.
