# SPR-002 — SAB80535 Stage 1 interrupt controller

Status: complete
Sprint ID: SPR-002
Started: 2026-08-31
Steering authority: `Hans-Einar/emuSA80535-N#2`

## Goal

Replace classic interrupt assumptions for the SAB80535 variant with a faithful
12-source/four-level Siemens controller while preserving accepted Stage 0 and
classic 8051/8052 behavior.

## Authorized scope

- REQ-009 interrupt-controller portion only;
- IEN0/IP0/IEN1/IP1/IRCON source, enable, priority and request structure;
- deterministic pending/enabled/in-service arbitration;
- correct vector/poll/preemption/nesting/RETI semantics;
- documented hardware-clear versus software-clear request behavior;
- immutable IRQ diagnostics;
- focused synthetic and classic-regression tests.

## Explicit non-goals

- new Timer0/Timer1 counting or overflow fidelity beyond pre-existing behavior;
- Timer1 baud scheduling or any new UART transmit/receive timing;
- 9-bit UART behavior;
- GPIO pin/edge electrical semantics beyond a generic synthetic IRQ request;
- ADC conversion, Siemens Timer2 counting/reload or P5 behavior;
- P1000 ROM-address policy, XDATA decode, D71055, protocol or physical I/O.

Later Timer/UART slices in Issue #2 are not authorized by this Sprint.

## Planned Slice

- **ITR-004 / SLC-004:** Siemens interrupt controller.
- Corrective Slice(s) only if independent review opens findings.

## Exit criteria

- all 12 sources, vectors, enables, priority pairs and clearing classes have
  focused deterministic tests;
- pending survives individual/EAL masking and becomes serviceable correctly;
- equal-level poll order, higher-level nesting, RET versus RETI and in-service
  stack behavior pass;
- IRQ trace exposes deterministic controller state;
- classic Stage 0 and 8051/8052 interrupt regressions pass on Windows and WSL,
  with sanitizer evidence;
- final independent review approves exact product HEAD;
- Master verification and traceability records pass;
- Issue #2 receives checkpoints and a stacked implementation PR is opened or
  updated without including later Timer/UART behavior.

## Verified result

All technical exit criteria are satisfied by cumulative product commits
`529602d800e36e87f495ef088f17e67ba3659a1a` and corrective
`85fa6e1f8318c691598b9a2ae191d1bd94b436c8`, through REV-SLC-005 and
VER-SLC-005. The verified branch is published as stacked PR #3:
`https://github.com/Hans-Einar/emuSA80535-N/pull/3`.

SPR-002 is complete. Later Timer0/Timer1 and UART slices remain unauthorized.
