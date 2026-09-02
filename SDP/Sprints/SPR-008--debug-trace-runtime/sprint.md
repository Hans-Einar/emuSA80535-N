# SPR-008 — Generic debugger trace runtime

Status: active — dispatcher/facade extension
Sprint ID: SPR-008
Started: 2026-09-02

## Goal

Implement the debugger-owned, processor-independent foundation from
DES-064..DES-089 without changing instruction execution or SAB80535 peripheral
semantics.

## Integration strategy

This is a stacked branch based on tracepoint design PR #11. It is isolated in
`/home/warloc/git/emuSA80535-N-debug-trace-runtime` and will be opened as a
separate PR. After PR #11 merges it can be retargeted or rebased to `master`.

## Boundary

- allowed: generic immutable debug event types, bounded fan-out/sequencing,
  matcher/watch action conditions and unit tests;
- forbidden: opcode behavior changes, SAB80535 peripheral implementation,
  P1000-specific addresses or semantics, and physical I/O;
- later integration: narrow producer calls from core access helpers into the
  accepted generic event interface.

## Exit criteria

Fresh Worker implementation, fresh Reviewer, strict builds and focused tests,
Master verification and a separate pull request.

## Result

SLC-015 delivered the standalone event bus, sequencer and conditional watch
matcher. Core producer integration and trace storage remain later Slices.

Steering authorized the next conflict-free debugger-only step. ITR-016 /
SLC-016 adds synthetic-event trace routing and bounded storage without core or
peripheral integration.

SLC-016 delivered and verified the bounded multi-trace router, gate/interrupt
policies and ring storage. The Sprint is complete.

Steering authorized ITR-017 / SLC-017 for the remaining conflict-free
composition layer and a subsequent holistic high-reasoning audit of the full
standalone debugger runtime.
