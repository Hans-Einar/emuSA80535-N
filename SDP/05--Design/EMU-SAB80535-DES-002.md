# EMU-SAB80535-DES-002 — Siemens interrupt controller design

Status: active target design
Design items: DES-011..DES-018
Established: 2026-08-31
Issue authority: `Hans-Einar/emuSA80535-N#2`

## Authority and evidence boundary

This design implements only the SLC-004 interrupt-controller scope. Primary
behavioral authority is the Siemens *SAB 80515/SAB 80C515 8-Bit
Single-Chip Microcontroller Family User's Manual*, interrupt-system chapter
(figures 8-1 through 8-8 and table 8-2), reconciled with Ponsse PR #25 commit
`7891aceb2b713659ea9936b9743a5aee73579ae9` and its generated SFR ledger.

The P1000 ROM proves reached register writes and vector wrappers, but no P1000
address or board meaning is embedded in CPU behavior.

## DES-011 — Twelve stable source identities

The SAB variant exposes twelve ordered sources:

| Index | Source | Request flag(s) | Enable | Vector |
|---:|---|---|---|---:|
| 0 | INT0 | TCON.IE0 | IEN0.EX0 | `0003` |
| 1 | Timer0 | TCON.TF0 | IEN0.ET0 | `000B` |
| 2 | INT1 | TCON.IE1 | IEN0.EX1 | `0013` |
| 3 | Timer1 | TCON.TF1 | IEN0.ET1 | `001B` |
| 4 | UART | SCON.RI or SCON.TI | IEN0.ES | `0023` |
| 5 | Timer2 | IRCON.TF2 or IRCON.EXF2 | IEN0.ET2 / IEN1.EXEN2 source gate | `002B` |
| 6 | ADC | IRCON.IADC | IEN1.EADC | `0043` |
| 7 | INT2 | IRCON.IEX2 | IEN1.EX2 | `004B` |
| 8 | INT3 | IRCON.IEX3 | IEN1.EX3 | `0053` |
| 9 | INT4 | IRCON.IEX4 | IEN1.EX4 | `005B` |
| 10 | INT5 | IRCON.IEX5 | IEN1.EX5 | `0063` |
| 11 | INT6 | IRCON.IEX6 | IEN1.EX6 | `006B` |

SLC-004 represents Timer1 even though the observed ROM vector is blank. Timer
or UART generation behavior is not added in this Slice.

## DES-012 — Pending state and SFR truth

A SAB-owned 12-bit pending mask is distinct from enable and in-service state.
At architectural arbitration points it is synchronized with the canonical
request flags in TCON, SCON and IRCON. Disabling a source or EAL does not erase
its request flag/pending state. Software writes through the central SFR gateway
can set or clear request flags.

A generic synthetic `raise/set pending` API may set the canonical flag for
tests and future peripheral producers. It carries only CPU source identity and
does not model a physical pin, board or protocol.

## DES-013 — Enable and global gate mapping

IEN0 at A8 keeps `EX0,ET0,EX1,ET1,ES,ET2,WDT,EAL` in bits 0..7. IEN1 at B8
keeps `EADC,EX2,EX3,EX4,EX5,EX6,SWDT,EXEN2` in bits 0..7. Classic `IP=B8`
remains reachable only through the classic variant path.

EAL gates acceptance, not pending state. A pending disabled source becomes
eligible after its individual enable and EAL permit service.

## DES-014 — Paired four-level priority

The corresponding IP1.x:IP0.x bits form levels 0..3 for six pairs:

| Pair bit | Left source | Right source |
|---:|---|---|
| 0 | INT0 | ADC |
| 1 | Timer0 | INT2 |
| 2 | INT1 | INT3 |
| 3 | Timer1 | INT4 |
| 4 | UART | INT5 |
| 5 | Timer2 | INT6 |

Among all eligible requests, choose the numerically highest level. Equal-level
polling uses the source index order in DES-011. Thus `IP0=00, IP1=10` assigns
UART and INT5 level 2 and leaves the other observed pairs at level 0.

## DES-015 — In-service stack and RETI

The SAB controller owns an explicit stack of accepted `{source,priority}`
entries. Because only a strictly higher level may preempt, depth four is
sufficient. Equal/lower requests wait. `RET` only restores PC; it does not pop
this stack. `RETI` restores PC and releases exactly the top in-service entry.

Interrupt entry reuses the Stage 0 checked stack operations and two-machine-
cycle boundary. Failed entry must not install vector/in-service state.

## DES-016 — Request clearing matrix

On accepted vector entry:

- IE0/IE1 clear only when their TCON IT0/IT1 mode is transition-triggered;
- TF0 and TF1 clear;
- IEX2..IEX6 clear;
- RI/TI, IADC, TF2 and EXF2 remain set until software clears them.

No blanket pending clear is allowed. After entry, the pending mask is derived
again from the surviving canonical flags.

## DES-017 — Arbitration timing

Interrupt flags are observed at deterministic architectural boundaries with
instruction-boundary fidelity. Preserve Stage 0 one-machine-cycle `tick()` and
bounded-run semantics. Following RETI or a write to IEN0, IEN1, IP0 or IP1,
Siemens arbitration is inhibited until one further instruction executes, as
documented for the hardware-generated LCALL path.

No Timer/UART event scheduler is introduced here; SLC-004 only consumes
already asserted canonical request flags or synthetic source requests.

## DES-018 — IRQ diagnostics

Add record-only IRQ trace events that expose at least cycle, PC, event/source,
selected priority, pending mask, enabled mask and current in-service stack
summary/depth. Trace observation remains immutable and behavior-neutral.

## Traceability

DES-011..DES-018 refine ARCH-002, ARCH-003, ARCH-005 and ARCH-006; satisfy the
interrupt-controller portion of REQ-009; and constrain SPR-002 / SLC-004.

