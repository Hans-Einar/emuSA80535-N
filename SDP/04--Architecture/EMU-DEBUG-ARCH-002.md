# EMU-DEBUG-ARCH-002 — Generic headless debug architecture

Status: active target architecture
Architecture items: ARCH-007..ARCH-010
Established: 2026-09-01
Steering authority: Issue #6 and DAP PR #4 exact HEAD

## ARCH-007 — Stable debugger facade

Expose debugger state and operations through explicit public values and
functions. Atomic snapshots contain architectural register widths, selected
R0..R7 bank, variant, instruction/machine-cycle counters and stable result
provenance. Private pointers, padding, callbacks and object layout never cross
the facade or wire boundary.

## ARCH-008 — Standalone protocol process

`emu-debug` is a separate no-curses executable that owns one CPU/session and
communicates only through bounded UTF-8 NDJSON on stdin/stdout. The legacy TUI
remains independent. Human diagnostics use stderr; the headless process does
not initialize or depend on curses or physical host I/O.

## ARCH-009 — Serialized deterministic lifecycle

Commands execute serially through starting, idle, command/run-active,
terminating and terminated states. CPU execution occurs only inside bounded
synchronous run/step commands. Every successful reset, get-state, yield and
architectural stop returns one same-boundary snapshot; invalid commands fail
without state mutation.

## ARCH-010 — Debugger-owned derived state

The process may own protocol/session state such as the replacement CODE
breakpoint set, negotiated limits and known decode predecessor boundaries.
This state remains generic, deterministic and reset/load-invalidated as the
frozen contract requires. It must not alter accepted CPU semantics or encode
firmware, board or DAP policy.

## Dependency direction

```text
DAP adapter child-process client
        -> emu-debug 1.0 NDJSON executable
        -> stable generic debugger facade
        -> accepted em8051/SAB80535 core API
```

The emulator repository does not depend on DAP TypeScript, VS Code, P1000 or
physical transports.

## Traceability

ARCH-007..ARCH-010 realize REQ-016, are informed by STU-002 and are refined by
DES-050..DES-063.
