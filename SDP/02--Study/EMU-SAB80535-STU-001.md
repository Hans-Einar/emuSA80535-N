# EMU-SAB80535-STU-001 — Upstream and ROM-evidence study

Status: accepted  
Study ID: STU-001  
Accepted: 2026-08-31

## Sources reviewed

- Ponsse PR #25 commit `7891aceb2b713659ea9936b9743a5aee73579ae9`:
  `EMU8051_IMPLEMENTATION_ROADMAP.md`,
  `SAB80535_EMULATOR_REQUIREMENTS.md`,
  `SAB80535_INTERRUPTS_TIMERS.md`,
  `EMU8051_SAB80535_REQUIREMENTS.md`, and generated
  `sfr-access-sites.csv`;
- Ponsse PR #27:
  `EMU_REPO_BOOTSTRAP.md`, `EMU_REPO_ISSUE.md`, and
  `PR25_EMULATION_STUDY_RECONCILIATION.md`;
- upstream source and README at commit `5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8`.

The reviewed generated inventory reports 789 probable-reachable SFR access
sites over 66 byte/bit identities. This repository treats that generated
evidence as authority and does not replace it with a competing hand-curated
P1000 inventory.

## Upstream findings

- The ANSI C instruction engine already separates CODE, XDATA, lower IRAM,
  optional upper IRAM and SFR storage.
- Upper 128-byte IRAM is optional through `mUpperData`; the SAB variant must
  make it an owned, reliable capability because the observed firmware sets
  `SP=0xA2`.
- Timer 0/1 and classic two-level interrupt behavior exist, but the core binds
  interrupt semantics directly to classic `IE=A8` and `IP=B8`.
- SFR callbacks are explicitly incomplete for accumulator/PSW-special opcode
  forms, so a complete trace cannot be claimed without an access audit.
- The UI build depends on curses. In the available Windows and openSUSE WSL
  environments the full upstream build stops at missing `curses.h`; the core
  source itself is independently compilable. A core-only test harness is
  therefore required and is not a semantic upstream change.

## Critical family conflict

Classic upstream interprets `IE=A8`, `IP=B8`. SAB80535 requires
`IEN0=A8`, `IP0=A9`, `IEN1=B8`, `IP1=B9`, `IRCON=C0`. Therefore SAB80535 must
be an explicit CPU variant. Reusing `REG_IP=B8` as SAB semantics would be a
structural correctness defect.

## Confirmed timing and state

- oscillator: 11.0592 MHz;
- classic machine-cycle rate: 921600/s;
- Timer 0 mode 1 reload `DCEF`: 8977 ticks, about 9.74067 ms before ISR reload
  latency;
- Timer 1 mode 2, `TH1=FD`, `SMOD=1`: exact 19200 baud;
- 12 interrupt sources with four levels and fixed equal-level polling order;
- DAPR write starts one ADC conversion at ADM=0, completing after 15 machine
  cycles;
- Siemens Timer 2 mode used by the evidence is not generic 8052 Timer 2.

## Study decision

Retain the upstream instruction engine and introduce variant-aware core
boundaries in staged, regression-tested slices. Stage 0 must first make the
core deterministic and embeddable without introducing Stage 1 peripheral
semantics.

## Traceability

STU-001 informs REQ-001..REQ-015, ARCH-001..ARCH-006 and DES-001..DES-010.

