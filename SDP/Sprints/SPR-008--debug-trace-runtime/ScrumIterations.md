# Scrum iterations

## ITR-015 — Standalone event and watch matcher foundation

Status: complete
Iteration ID: ITR-015
Active Slice: SLC-015

### Slice contract — SLC-015

**Goal:** implement a standalone generic debugger runtime foundation that can
consume synthetic immutable events now and core-produced events later.

**Required implementation:**

1. fixed-width immutable event/value schema with explicit known, width and
   signedness fields;
2. debugger-owned uint64 sequencing and fixed-capacity stable-order observer
   fan-out with registration changes rejected during dispatch;
3. bounded watch table with access/address selectors;
4. per-action `eq`, `ne`, `lt`, `le`, `gt`, `ge` comparisons over old/new
   values, with explicit signed/unsigned behavior and unknown=false;
5. independently combinable stop, console, quiet and bounded trace-route
   results; multiple stops coalesce;
6. tests for ordering, bounds, reentrancy rejection, all comparisons, numeric
   edges, unknown operands, action independence and observer neutrality.

**Compatibility:** no edits to `core.c`, `opcodes.c`, existing peripherals,
`emu_debug` protocol or DAP in this Slice. New APIs must be additive C99 and
build with GCC and Clang warnings-as-errors.

**Non-goals:** core instrumentation, interrupt policy, trace gates, JSONL,
ring buffer, CLI/protocol commands and physical I/O.

**Required evidence:** Worker tests, fresh Reviewer report, Master verification
and traceability updates.

### Result

Complete. The standalone runtime and focused tests were implemented without
changes to CPU, opcode, peripheral, existing debugger protocol or DAP code.
REV-SLC-015 approved with corrections and VER-SLC-015 passed.

## ITR-016 — Multi-trace router and bounded ring

Status: complete
Iteration ID: ITR-016
Active Slice: SLC-016

### Slice contract — SLC-016

**Goal:** implement the next debugger-owned layer over synthetic canonical
events, without changing CPU, opcode or peripheral sources.

**Required implementation:**

1. atomically replaceable bounded trace sessions with stable ID, enabled
   state, bounded tag/comment, destination ID and include/suppress/interrupt-
   only policy;
2. bounded point routes and deterministic before/after trace on/off gates;
3. nested interrupt-depth tracking with precisely retained outer enter/exit
   boundaries and per-trace bounded suppression accounting;
4. canonical event routing with ascending duplicate-free trace IDs and no
   per-trace cloning;
5. fixed-capacity routed-event ring with explicit overwrite/loss accounting;
6. watch results from SLC-015 accepted as explicit trace routes without
   recursive watch evaluation;
7. focused tests for replacement atomicity, route ordering, gate conflicts,
   nested interrupts, policies, shared destinations, ring wrap/loss and all
   advertised limits.

**Compatibility:** additive C99 only. No edits to `core.c`, `opcodes.c`, SAB
peripherals, existing `emu_debug` protocol/server or DAP. File/console sinks,
CLI/protocol exposure and core event producers remain later work.

**Required evidence:** strict GCC and Clang, sanitizers, regressions, fresh
Worker and Reviewer, Master verification.

### Result

Complete. REV-SLC-016 approved after direct coverage for all four gate timings.
VER-SLC-016 passed strict compilers, sanitizers, full regressions and the
existing debugger process suite.

## ITR-017 — Derived-event dispatcher and in-memory facade

Status: active
Iteration ID: ITR-017
Active Slice: SLC-017

### Slice contract — SLC-017

**Goal:** compose SLC-015 and SLC-016 into one bounded debugger-owned runtime
that accepts synthetic canonical events and exposes an additive in-memory C
facade, still without touching CPU execution or peripherals.

**Required implementation:**

1. dispatcher subscribes to the event bus and processes each source exactly
   once without recursive callback execution;
2. evaluate watches in ascending ID order, route the source event, then create
   newly sequenced `watch.match` events in deterministic order, and apply
   source after-gates only after all derived events drain;
3. fixed-capacity pending derived-event queue with explicit overflow status and
   no silent partial fan-out;
4. aggregate stop requests and preserve primary lowest watch ID/source sequence
   for the caller to apply only at a future CPU safe boundary;
5. additive opaque in-memory facade for atomic replacement of watches, traces,
   destinations, points and gates, trace enable/disable, event ingest, status,
   stop consumption and paged ring reads;
6. reset/load/clear lifecycle semantics and deterministic counter behavior;
7. focused end-to-end tests for source/derived sequence identity, before/after
   bracketing, multiple watches, overflow neutrality, stop priority, ring pages,
   replacement atomicity, lifecycle and all advertised bounds.

**Compatibility:** no changes to `core.c`, `opcodes.c`, SAB peripherals,
existing `emu_debug.c/.h`, `emu_debug_server.c`, wire protocol or DAP. No file,
console or raw stdout sink. The facade consumes synthetic events only.

**Required evidence:** fresh Worker, fresh Reviewer, strict GCC and Clang,
ASan/UBSan, Valgrind, complete regressions and Master verification. After Slice
review, a separate fresh holistic Reviewer audits SLC-015..017 together for
correctness and improvement opportunities before acceptance.

### Issue #14 takeover acceptance pass

Status: active

Steering/Master authority is Issue #14. Work starts from exact preserved WIP
HEAD `356836637d5ff432d91fc508fd55b2f17b45cdb3` on the fresh branch
`codex/debug-trace-runtime-takeover`; the preservation branch is immutable.

The acceptance pass must:

1. perform REV-SLC-017-HOLISTIC as a fresh independent review of SLC-015,
   SLC-016 and SLC-017, including every review dimension named in Issue #14;
2. treat the existing REV-SLC-017 only as non-authoritative historical
   evidence and reproduce or reject every claimed correction independently;
3. correct blocking findings only within the existing standalone debugger-
   owned event/watch/router/runtime composition scope;
4. run and record VER-SLC-017 with exact tool versions and explicit
   environment limitations;
5. freeze the future stable sized/versioned request/response facade and final
   after-sequence page metadata in design documentation only;
6. leave CPU producers, CPU safe-boundary application, CLI, wire protocol,
   DAP, file/console sinks, source maps and product/physical I/O out of scope;
7. preserve one emulator-owned breakpoint/watchpoint/tracepoint semantic model
   for later CLI and DAP frontends.

Expected completion signal: REV-SLC-017-HOLISTIC contains no unresolved
blocking finding, VER-SLC-017 is reproducible and passed or honestly records a
blocking environment gap, the design seam is frozen, and SDP/traceability can
support a READY/NOT_READY decision without relying on chat memory.

### Holistic review result and corrective Worker contract

REV-SLC-017-HOLISTIC disposition: **corrections required**.

The fresh review opened six in-scope findings:

- `REV-SLC-017-HOLISTIC-F001`: ordinary ingest can counterfeit reset/load;
- `REV-SLC-017-HOLISTIC-F002`: same-marker before-gates can hide required
  lifecycle markers;
- `REV-SLC-017-HOLISTIC-F003`: load misses CODE-capable address-only
  selectors;
- `REV-SLC-017-HOLISTIC-F004`: replacement can silently discard active
  suppression intervals;
- `REV-SLC-017-HOLISTIC-F005`: trace IDs can be reused before clear-session;
- `REV-SLC-017-HOLISTIC-F006`: tag/comment accepts malformed UTF-8.

The next pass is corrective, not a new feature Slice. A fresh Worker must
resolve F001..F006 inside `emu_debug_event.*`, `emu_debug_trace.*`,
`emu_debug_runtime.*` and their focused tests only. Lifecycle markers shall be
routed to the traces enabled at entry to the lifecycle boundary; same-marker
gates may determine subsequent enabled state but may not erase that marker.
Address-only selectors are CODE-capable and must be invalidated on load.
Configuration replacement must reject an active suppression interval whose
trace would be deleted or change destination/policy until explicit flush.
Trace IDs may update while live but may not disappear and later be reused
before clear-session. UTF-8 validation must be bounded and locale-independent.

Non-goals remain unchanged: no CPU producer/safe-boundary integration, CLI,
wire protocol, DAP, file/console sink, source map or product/physical I/O.
After correction, a separate fresh Reviewer must inspect the correction diff
before VER-SLC-017.
