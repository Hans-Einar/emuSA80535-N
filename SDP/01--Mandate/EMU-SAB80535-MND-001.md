# EMU-SAB80535-MND-001 — Generic SAB80535-N emulator mandate

Status: active  
Mandate ID: MND-001  
Established: 2026-08-31

## Purpose

Evolve the preserved `jarikomppa/emu8051` instruction engine into a reusable,
deterministic Siemens SAB80535-N emulator that can execute unmodified firmware
images and can be embedded behind generic callbacks.

## Authority and provenance

- upstream repository: `https://github.com/jarikomppa/emu8051`;
- preserved upstream baseline: `5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8`;
- target fork: `https://github.com/Hans-Einar/emuSA80535-N`;
- CPU/peripheral evidence: Ponsse PR #25 commit
  `7891aceb2b713659ea9936b9743a5aee73579ae9`;
- cross-repository architecture and bootstrap contract: Ponsse Issue #26 and
  PR #27.

## Non-negotiable boundaries

Allowed scope is generic instruction execution, SAB80535 SFRs and peripherals,
virtual time, trace/debug interfaces, generic pin/interrupt injection and
generic CODE/XDATA integration callbacks.

Forbidden scope includes P1000 ROM-address behavior, Ponsse protocol policy,
P1000 XDATA topology, D71055 board composition, connector/signal names,
hydraulics, valves, live serial transports and physical machine actuation.

The P1000-specific board model remains in
`Hans-Einar/ponsse/p1000/simulator`.

## Success condition

The repository provides reviewed, reproducible stage gates from deterministic
core execution through the ROM-required SAB80535 peripherals, while retaining
classic upstream behavior and ancestry.

## Traceability

MND-001 is informed by STU-001 and STU-002, realized through REQ-001..REQ-016 and
constrained by ARCH-001..ARCH-010.

## Runtime architecture addendum — 2026-10-27

The user's architecture clarification adds generic orchestration, external
board-module loading and independently attachable debugging to the target
program. These are separate components/build targets in this repository;
the CPU engine retains its portable boundary. Board implementations remain
in their owning external projects.

Optional Unix/TCP debug-control transports and host administrative signals
are authorized target capabilities, not physical machine-I/O backends.
Simulation remains deterministic and virtual by default; no listener opens
implicitly. Remote debugger access requires explicit configuration and
authentication/protection. SLC-019 establishes design only, through STU-004,
REQ-018..REQ-024, ARCH-011..ARCH-018 and DES-101..DES-110. Existing protocol1.0,
CPU acceptance and historical authorities remain intact; implementation and
package publication require their recorded successor gates.

