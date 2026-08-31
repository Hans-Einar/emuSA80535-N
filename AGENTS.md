# Repository execution rules

This repository follows the Standard Document Procedure (SDP). The
authoritative mandate, requirements, design, active Sprint, review evidence and
traceability state live under `SDP/`.

## Roles and execution loop

- The Master identifies and records the one active Sprint, Iteration and Slice
  before product-code work begins.
- Product-code implementation is performed by a fresh Worker against the
  active Slice contract.
- A separate fresh Reviewer inspects the resulting diff and evidence.
- The Master integrates review, verification, traceability and handoff state.
- A Slice is not accepted until its review and verification records exist.

## Product boundary

This repository contains only generic 8051/SAB80535 CPU and peripheral
emulation. P1000 ROM addresses, protocol types, board decode, NEC D71055
devices, machine signal names, hydraulics, valves, physical serial I/O and
physical machine control are forbidden.

All simulation time and I/O must be deterministic and virtual or in-memory by
default. Preserve upstream Git history, MIT licensing and copyright notices.

## Required reading

Before implementation, read the active contract in
`SDP/Sprints/SPR-001--sab80535-foundation/` and the lifecycle documents in
`SDP/01--Mandate` through `SDP/05--Design`.

