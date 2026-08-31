# SPR-003 — SAB80535 Timer0/Timer1 timing

Status: ready-for-pr
Sprint ID: SPR-003
Started: 2026-08-31
Steering authority: Issue #2 SLC-006 checkpoint
Base: `master` at `a20815e24778760a308130cf1f9aa6d0f55b6af3`

## Goal

Verify and narrowly correct shared Timer0 mode1 and Timer1 mode2 behavior for
the SAB80535 evidence path, including deterministic overflow events needed by a
future UART Slice, without implementing UART behavior.

## Authorized scope

- REQ-010 only;
- Timer0 mode1 live 16-bit counting, TF0 and interrupt integration;
- Timer1 mode2 TL1/TH1 auto-reload, TF1 and repeated overflow events;
- generic integer overflow counters/record-only event seam;
- machine-cycle/ISR/software-reload timing tests;
- classic 8051/8052 regression.

## Non-goals

- REQ-011 or any SBUF/TB8/RB8/RI/TI/frame/transport behavior;
- Timer1 divide-by-16 UART shifting;
- C/T counter inputs or GATE pin semantics;
- GPIO/INT edges, ADC, Siemens Timer2 or P1000 board behavior;
- live serial, physical GPIO or machine control.

## Planned Slice

- **ITR-006 / SLC-006:** Timer0 mode1 and Timer1 mode2 deterministic timing.
- Corrective Slice(s) only if independent review opens findings.

## Exit criteria

- all 16 Steering-required verification classes pass;
- Timer0 overflow is exact at 8977 eligible cycles from DCEF;
- Timer1 FD steady-state overflow events repeat every three cycles independently
  of sticky TF1;
- live SFR writes, controller vectors and software reload latency are covered;
- Stage0/controller and classic regressions pass on Windows/WSL/sanitizers;
- independent review approves exact product HEAD and Master verification passes;
- Issue #2 checkpoints and a focused implementation PR are published;
- no UART or other unauthorized behavior enters the diff.

## Verified result

All technical exit criteria are satisfied at product HEAD
`30cf42efa845d29a47a950eca7bbaf657490fbe6` through REV-SLC-006 and
VER-SLC-006. Sprint remains `ready-for-pr` until the real PR identity exists.
