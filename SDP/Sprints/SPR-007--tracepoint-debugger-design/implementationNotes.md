# Implementation notes

Status: complete; design reviewed and verified.

## Baseline

- emulator branch base: `master` at `d9f80eb`;
- design branch/worktree: `codex/tracepoint-debugger-spec`;
- existing generic runtime: `emu-debug` protocol 1.0;
- frozen DAP study authority: commit
  `36639b48ddb2ffbafa14c00da794fe1734f7483b`.

## Initial source findings

- the core already emits instruction, SFR-write and MOVX diagnostic records;
- separate immutable observers exist for MOVX context, Siemens IRQ events,
  Timer0/1 overflow and SAB mode-3 UART events;
- `emu_debug.c` currently consumes the one core trace callback internally to
  learn sequential predecessor boundaries;
- the debugger facade owns an atomic CODE-breakpoint replacement table;
- protocol 1.0 exposes no watchpoint, trace subscription, memory read or file
  sink command;
- the frozen DAP contract names richer trace/watch support as later work.

## Worker deliverables

- `SDP/02--Study/EMU-DEBUG-STU-003.md` records the exact source/DAP inventory
  and instrumentation gaps;
- `SDP/05--Design/EMU-DEBUG-DES-007.md` defines DES-064..DES-079;
- `doc/DEBUG_TRACEPOINT_DESIGN.md` is the implementation-ready generic
  specification for semantics, schemas, ordering, matching, storage, CLI,
  facade, optional protocol, delivery and verification.

## Design decisions

- preserve protocol 1.0 and add only named optional major-1 capabilities;
- introduce a deterministic event fan-out because the facade already consumes
  the single general core trace callback for decode predecessor state;
- stop watchpoints only at a completed instruction boundary while tracepoints
  never stop;
- represent unavailable old/new values explicitly rather than reading again;
- bound ring, JSONL, conditions, point count and protocol pages;
- rotate by record/byte count, not host time;
- keep target policy, physical I/O and product-code edits outside this Slice.

## Worker checks

- source references re-read against emulator HEAD `d9f80eb`;
- frozen DAP requirements read with `git show` at exact commit `36639b4`
  (the local DAP worktree HEAD was intentionally not treated as authority);
- repository target/live-term audit and `git diff --check` required before
  review handoff.

## Independent review

`SDP/CodeReview/REV-SLC-013.md` approved the corrected design. Review aligned
ordering with actual `tick()` delay/IRQ/IDLE behavior, defined safe watchpoint
stops, separated raw core events from debugger session envelopes, closed ring
marker and file-path edge cases, and clarified facade/protocol/TUI feasibility.

## Master verification

VER-SLC-013 passed the available tests and document/traceability checks. The
Python process suite remains mandatory for implementation work but was
unavailable here because this host provides Python 3.6.15.
