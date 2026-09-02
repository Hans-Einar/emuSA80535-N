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
