# Implementation notes

Status: active Worker preparation; product code not yet committed.

## Frozen baseline

- Branch: `codex/sab80535-timer2`.
- Exact master: `bc86d2633b6057529e6fd1e666896c24d72822aa`.
- Accepted SLC-014/REQ-013 ADC is merged on the baseline.
- Accepted `emu-debug` 1.0 protocol remains frozen.
- Consumer evidence: Ponsse PR #25 exact HEAD
  `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9` plus current `doc/P1000/`.

## Planned Worker product

- Generic Siemens Timer2 timer-function implementation only.
- T2CON input/prescaler masks and bounded Timer2 state in `emu8051.h`.
- Deterministic machine-cycle Timer2 tick in `core.c`.
- Live TL2/TH2, reload-disabled wrap, sticky TF2 and accepted controller path.
- 64-bit overflow count plus immutable overflow diagnostics.
- Focused Stage4 test suite and build wiring.

## Critical boundaries

- P5.4 is not hardware-toggled by Timer2; firmware may change it normally.
- No external-counter/gated producer, CRC/T2EX reload, EXF2 producer or
  compare/capture implementation.
- No P1000/NEC/board/connector/valve semantics.
- No physical/live I/O and no frozen debug-protocol change.

## Review separation

The upcoming product/test commit is the Worker result, not acceptance. A fresh
independent Reviewer must inspect the exact product HEAD before Master can
create VER-SLC-015 or accept REQ-014.
