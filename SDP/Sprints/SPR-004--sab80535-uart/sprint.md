# SPR-004 — SAB80535 mode-3 9-bit UART

Status: active
Sprint ID: SPR-004
Started: 2026-09-01
Steering authority: Issue #2 SLC-007 checkpoint
Base: `master` at `c0cd6f26bd8984c9fed10eb81716619cb1bb96e6`

## Goal

Implement the deterministic Timer1-derived SAB80535/MCS-51 mode-3 9-bit UART
required by reviewed firmware evidence, including full-duplex in-memory receive
injection and shared interrupt integration.

## Authorized scope

- REQ-011 only;
- Timer1 overflow divide-by-16/32 phase and exact 19200 startup timing;
- separate transmit-write and receive-read SBUF storage;
- eleven-bit TX frames, TB8 latching, TI/STOP timing and back-to-back TX;
- deterministic in-memory 9-bit RX injection with REN/RI/SM2/RB8 rules;
- existing shared RI/TI controller vector 0023;
- immutable in-memory UART/frame diagnostics and focused tests.

## Non-goals

- Stage 2 GPIO/TxD/RxD edge or electrical modeling;
- ADC, Timer2 or any new timer producer;
- P1000 protocol/board/XDATA/D71055 policy;
- OS serial APIs, live transport, physical GPIO or machine control;
- dedicated Siemens internal baud-rate generator.

## Planned Slice

- **ITR-007 / SLC-007:** mode-3 9-bit UART.
- Corrective Slice(s) only if independent review opens findings.

## Exit criteria

- all 18 Steering verification classes pass;
- exact divider phase, TX/RX timing and shared interrupt behavior are proven;
- separate SBUF roles and full-duplex/back-to-back cases pass;
- Stage0/IRQ/timer/classic regressions pass cross-platform and under sanitizer;
- fresh independent review approves exact product HEAD and Master verifies;
- Issue #2/Ponsse checkpoints and focused PR are published;
- no unauthorized Stage 2/ADC/Timer2/target/live-I/O behavior enters the diff.

