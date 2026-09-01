# SPR-007 — Generic tracepoint debugger design

Status: complete
Sprint ID: SPR-007
Started: 2026-09-02

## Goal

Specify a deterministic, bounded tracepoint and watchpoint subsystem that
extends the existing generic core observers, `emu-debug` facade/runtime and
legacy interactive debugger without making DAP mandatory.

## Authorized scope

- read-only study of the current emulator and frozen DAP consumer;
- generic debugger/trace requirements, architecture and wire/CLI design;
- deterministic in-memory and file-sink behavior, filtering and backpressure;
- breakpoint/watchpoint/tracepoint semantics and verification plan;
- documentation and SDP traceability only.

## Non-goals

- product-code implementation;
- P1000 firmware addresses, board devices or machine signal names;
- physical serial/GPIO/CAN or other live transports;
- mutation through trace callbacks;
- changing the frozen `emu-debug` 1.0 or DAP Slice-1 contract.

## Exit criteria

- current debug/trace seams and gaps are evidenced from source;
- one implementable generic design covers CLI, facade and optional protocol
  evolution;
- event ordering, value snapshots, filters, bounded storage, loss reporting,
  determinism and lifecycle are explicit;
- a fresh Worker authors the design, a fresh Reviewer approves it, and Master
  records verification and handoff.

## Result

SLC-013 produced STU-003, DES-064..DES-079 and the implementation-ready
`doc/DEBUG_TRACEPOINT_DESIGN.md`. REV-SLC-013 approved the design after scoped
corrections and VER-SLC-013 passed with one documented Python-toolchain note.
No product code was changed.
