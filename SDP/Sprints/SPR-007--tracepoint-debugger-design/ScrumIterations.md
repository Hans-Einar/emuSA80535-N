# Scrum iterations

## ITR-013 — Tracepoint debugger specification

Status: complete
Iteration ID: ITR-013
Active Slice: SLC-013

### Slice contract — SLC-013

**Goal:** produce a source-grounded, implementation-ready design for generic
breakpoints, data watchpoints and non-stopping tracepoints usable through the
legacy interactive debugger, the stable C facade and a future negotiated
`emu-debug` protocol extension.

**Expected files:** a detailed design document under `doc/`, a focused SDP
study/design extension, this Sprint's implementation notes/handoff and the
minimum traceability index/relations/ledger updates required by the project.

**Required content:**

1. inventory existing instruction/SFR/MOVX/IRQ/timer/UART observers and current
   debugger/DAP capabilities;
2. distinguish breakpoint, watchpoint and tracepoint stop semantics;
3. define generic address spaces, event taxonomy, filters, change-only rules,
   conditions and bounded hit counters;
4. define immutable event records with sequence, instruction/cycle time,
   executing PC, old/new values where observable and explicit unknown fields;
5. define deterministic ordering across instruction, memory, peripheral and
   interrupt events;
6. define bounded ring/file streaming, overflow/loss markers, rotation and
   failure policy without host-wall-time effects;
7. propose legacy CLI commands and a backward-compatible negotiated protocol
   extension without requiring DAP changes in this Slice;
8. define lifecycle, reset/load invalidation, snapshot/replay and differential
   trace use;
9. identify required core instrumentation gaps, especially IRAM reads/writes,
   SFR reads, CODE fetch/call/return and XDATA backing paths;
10. provide staged implementation slices and focused verification matrices.

**Invariants:** observers are record-only; disabled tracing changes no CPU
behavior; execution remains deterministic and bounded; no target-specific or
physical-I/O behavior enters the emulator.

**Non-goals:** implementation, live endpoints, P1000 profiles, firmware
semantics, DAP product changes or unbounded trace retention.

**Required verification:** source-reference audit, terminology/schema review,
cross-platform path/lifecycle review, no-target/no-live audit and independent
Reviewer approval.

### Result

Complete. REV-SLC-013 approved the corrected documentation and VER-SLC-013
passed the available source, test, format and boundary gates. Product
implementation remains a future Sprint.

## ITR-014 — Multi-trace routing refinement

Status: complete
Iteration ID: ITR-014
Active Slice: SLC-014

### Slice contract — SLC-014

**Goal:** refine the accepted tracepoint design so multiple independently
gated trace sessions can subscribe to one canonical event bus.

**Required behavior:**

1. each trace session has stable ID, bounded tag/comment, destination and
   enabled state;
2. matched events carry a bounded, deterministic set of routed trace IDs;
3. trace-on/trace-off gates target one or more trace IDs with precisely
   defined before/after-current-event semantics;
4. each session chooses interrupt policy: include, suppress while any interrupt
   is active, or interrupt-only, with nested-depth correctness and explicit gap
   markers when suppression hides events;
5. watchpoints publish derived watch-match events on the same ordered queue and
   may stop, print, remain quiet, or route into trace sessions without recursive
   callback execution;
6. destinations include file, ring and interactive console; raw stdout is
   permitted only in a dedicated non-NDJSON CLI mode and never in headless
   `emu-debug` protocol mode;
7. configuration, routing lists, fan-out, tags and output remain bounded.

**Non-goals:** product-code implementation, target-specific profiles, DAP
changes, physical I/O or mutation from observers.

**Required evidence:** updated detailed/numbered design, fresh Worker and
Reviewer, format/source-boundary checks and Master verification.

### Result

Complete. The refinement added DES-080..DES-089. REV-SLC-014 approved the
design with corrections and VER-SLC-014 passed the documentation, format and
generic-emulator boundary checks. No product code was changed.
