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
