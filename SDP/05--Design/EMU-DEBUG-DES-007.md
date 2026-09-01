# EMU-DEBUG-DES-007 — Generic breakpoint, watchpoint and tracepoint design

Status: active target design
Design items: DES-064..DES-079
Established: 2026-09-02

## DES-064 — Unified immutable event bus

Canonical core boundaries emit value-only raw events through one record-only bus.
Subscribers receive `const` records and opaque user context, cannot reach CPU
storage through the signature, and are invoked in registration order. The
debugger installs one subscriber and fans out internally to decode history,
stop matching, ring storage and sinks. Raw core events contain architectural
facts only; the debugger adds session identity and synthesized lifecycle/loss
envelopes. Registration is bounded and cannot change recursively during
dispatch; callbacks cannot call execution or mutation APIs. Existing legacy
callback slots remain single-observation compatibility paths.

## DES-065 — Stable event identity and time

Every emitted record has a session generation, monotonic uint64 sequence,
instruction count, machine-cycle count, phase ordinal and executing PC. Reset
does not reuse a sequence within a session; a new debugger session starts at
one. No host timestamp participates in ordering or matching.

## DES-066 — Event taxonomy

The stable taxonomy covers instruction begin/end, CODE fetch, memory
read/write/RMW, call/return, interrupt request/enter/exit, exception, halt,
timer overflow, UART frame/bit, reset/load, debugger mutation and loss/sink
status. Address space is an enum: CODE, lower IRAM, upper IRAM, SFR, XDATA and
NONE. Peripheral subtypes remain generic and variant-qualified.

## DES-067 — Value validity

Memory events carry width, old-value validity, old value, new-value validity,
new value and access kind. Reads normally have only new/final value; writes
have old and final value only when captured without extra architectural access.
Unavailable data is encoded as `known:false`, never a fabricated zero.

## DES-068 — Deterministic event order

Ordering follows `tick()`: pending delay cycles advance peripherals first; at
a zero-delay boundary interrupt arbitration/entry precedes any vector opcode;
otherwise instruction accesses precede first-cycle timer/UART effects and
remaining delay cycles. Instruction-end occurs only after all cycles complete.
IDLE has peripheral cycles without instruction events. Tests cover Timer1-to-
UART producer order, delay/IDLE cycles, simultaneous sources and nested IRQs.

## DES-069 — Point semantics

A breakpoint matches the next instruction boundary and stops before execution.
A watchpoint matches an observed access and requests a stop at the next safe
execution boundary (complete instruction/entry delay, or current IDLE cycle)
without stranding `mTickDelay`; its stop response identifies the triggering
event sequence. A tracepoint records a matching immutable event and never
stops execution. Observer callbacks never mutate CPU state.

## DES-070 — Match language

Each point has uint32 ID, enabled flag, event-kind mask, optional address-space
and inclusive uint16 range, access mask, PC range, bit mask, change-only flag,
skip count, hit limit and bounded condition bytecode. Conditions may compare
only fields already present in the immutable event with integer constants
using equality, ordered comparison, mask test and boolean AND/OR/NOT. No CPU
reads, callbacks, strings, allocation or side effects occur during matching.

## DES-071 — Hit accounting and stop priority

Observed, matched, emitted and dropped counts are uint64 saturating counters.
Skip consumes matches before action; hit limit disables the point after its
last action. At one boundary the deterministic stop priority is exception,
halt, pre-execution breakpoint, then lowest watchpoint ID. All simultaneously
matched watchpoint IDs and event sequences remain queryable.

## DES-072 — Bounded ring

The debugger owns a configured fixed-capacity ring. Capacity zero disables it.
On full, the default policy overwrites oldest complete records and accumulates
a pending loss interval. Before the next retained normal record it inserts one
loss marker with first/last lost sequence and count. The first evicting record
may precede the marker; the marker precedes the following ordinary record.
Overwrite mode requires capacity at least two and marker retention is bounded,
non-recursive. `stop-on-full` is an
explicit alternative debugger stop policy; allocation growth is forbidden.

## DES-073 — JSONL file sink

An explicitly opened sink writes UTF-8 JSON Lines with one header, event or
status object per line. Paths must be absolute. Rotation is deterministic by
configured record count or encoded byte count, never wall time; suffixes are
monotonic segment numbers. Write/open/flush/rotate failure disables that sink,
records one ring/status failure when possible, and never changes CPU behavior.
The sink may flush on instruction boundary, explicit command and close.

## DES-074 — Lifecycle

Point definitions survive reset by default; counters, pending stops, ring and
sink segment continue but receive reset/load markers and a new generation.
Image load disables CODE-addressed points until explicitly re-enabled, because
their meaning may be stale. `clear session` removes points, closes sinks and
clears retained events. Snapshot metadata includes image digest, reset seed,
generation, next sequence and point definitions.

## DES-075 — Snapshot, replay and differential trace

A deterministic snapshot captures architectural/core peripheral state plus
debugger metadata required to resume event identity. File handles are not
serialized; restored sinks require explicit reopen and a continuation header.
Replay uses recorded virtual stimuli and verifies sequence/event equality.
Differential comparison aligns on generation/sequence or explicit markers and
reports the first schema-aware divergence, including missing/extra events.

## DES-076 — Stable C facade

Opaque debugger APIs atomically replace/list points, configure/read/clear the
ring, open/flush/close file sinks, query counters and retrieve pending stop
detail. Inputs use sized structs with `struct_size` and version; outputs are
caller-owned buffers with required-count reporting. Failed validation mutates
nothing. Product structs and callback pointers never cross the facade.

## DES-077 — Legacy interactive debugger

The TUI receives a command line/popup command family: `break`, `watch`,
`trace`, `points`, `enable`, `disable`, `delete`, `trace ring`, `trace show`,
`trace clear`, `trace file`, `trace flush`, `trace close` and `trace status`.
Hexadecimal is explicit (`0x`); ranges are inclusive; command parsing is
bounded and errors do not mutate state. Existing single-key run/step and
breakpoint behavior remains available as compatibility shorthand.

## DES-078 — Optional protocol extension

Protocol major 1 may advertise optional capabilities `debugPointsV1`,
`traceRingV1` and `traceFileV1`. New serialized commands mirror the facade:
`replaceDebugPoints`, `listDebugPoints`, `configureTraceRing`, `readTrace`,
`clearTrace`, `openTraceFile`, `flushTraceFile`, `closeTraceFile` and
`getTraceStatus`. Protocol 1.0 clients see no changed required capability,
command, response or event behavior. Trace data is pulled in bounded pages;
unsolicited stdout streaming is deferred.

## DES-079 — Delivery and verification

Implement in slices: event/schema infrastructure; memory instrumentation;
control-flow/IRQ ordering; matcher/watchpoint stops; ring/loss; JSONL/rotation;
C facade/CLI; optional protocol; snapshot/replay/diff; performance hardening.
Each slice preserves existing tests and adds disabled-trace equivalence,
golden ordering, bounds, allocation-failure, malformed-condition, cross-
platform path, sink-failure, loss-marker, deterministic replay and no-live-I/O
tests. Benchmarks report disabled and enabled overhead without changing
acceptance semantics.

## Traceability

DES-064..DES-079 are informed by STU-003, refine ARCH-007..ARCH-010 and govern
future implementation slices after SLC-013.
