# SPR-011 — Runtime, board-module and debugger architecture

Status: active
Sprint ID: SPR-011
Started: 2026-10-27
Authority: current user request to establish runtime/component boundaries,
loadable peripheral modules, remote debugging and process status signals.
Baseline: de4eb56

## Goal

Explain the preserved upstream architecture and the current fork, then record
a coherent target architecture and staged contracts before further product
implementation. Keep CPU, orchestration, external board models and debugger
transports independently buildable and separately owned.

## Scope

ITR-019 / SLC-019 is documentation/design only, with fresh Worker and Reviewer.
No product source/build changes, simulator changes, protocol activation,
network listener or RPM publication are authorized by this Slice.

The primary recommended deployment is a generic runtime process containing
the CPU engine and a startup-loaded external board module. Transport-neutral
debug service supports future independent attach over Unix sockets and
explicit TCP. A process-separated board transport is an optional later
extension, not a second implementation required now.

Exit: source-grounded study, requirements/architecture/design, explicit
implementation boundaries and compatibility gates, independent review and
Master traceability/handoff. Package publication remains deferred.
