# EMU-DEBUG-DES-007 — Generic breakpoint, watchpoint and tracepoint design

Status: active target design
Design items: DES-064..DES-089
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

## DES-080 — Independently gated trace sessions

A trace session is a bounded configuration object with stable nonzero uint32
`trace_id`, enabled flag, UTF-8 tag of at most 64 bytes, UTF-8 comment of at
most 256 bytes, one destination reference and one interrupt policy. IDs are
not reused until `clear session`; deleting a trace invalidates its routes.
Trace enable/disable changes neither point definitions nor counters. The
implementation has advertised fixed maxima for traces, tag/comment bytes and
routes and validates a replacement configuration atomically.
Ordinary tracepoints name a bounded sorted trace-ID route set: their predicate
selects events and the route set selects sessions. A session does not
implicitly subscribe to every event, and a gate changes state without creating
an implicit route.

## DES-081 — Canonical events and routed views

The sequencer creates each architectural or derived event exactly once. It
then computes one ascending, duplicate-free bounded `trace_ids` set. Storage
does not clone an event per trace: a destination receives one routed view with
the canonical event identity and the subset of enabled trace IDs routed to
that destination. Sessions sharing a destination therefore coalesce. Different
destinations may encode the same canonical event, but retain the same session,
generation and sequence. Matching order is point ID, watch action, trace ID,
then destination ID; overflow is rejected at configuration time, never
silently truncated.

## DES-082 — Trace gates

`trace.on` and `trace.off` points target a bounded sorted set of trace IDs and
have explicit `before` or `after` current-event timing. All predicates and the
initial enabled state are evaluated against pre-event configuration.
Before-actions apply in ascending gate ID and their final state controls the
current source event and its derived watch events. After-actions apply in
ascending ID only after that routing completes and control the next event.
Consequently, an already-enabled `on-after` trace still receives the current
event, while an already-disabled `off-after` trace does not. Last action in
each ordered phase wins; the after phase determines subsequent state. Gates
never alter queued records, and a no-op gate creates no implicit route.

## DES-083 — Interrupt policies and suppression gaps

Each trace chooses `include`, `suppress-during-interrupt`, or `interrupt-only`.
The sequencer tracks architectural interrupt depth, incrementing after an
`interrupt.enter` event is routed and decrementing after an `interrupt.exit`
event is routed. Thus outer enter is evaluated at depth zero and outer exit at
depth one; nested boundaries use their actual nonzero depths. `include`
accepts all otherwise-route-eligible events. `suppress-during-interrupt`
accepts depth-zero events plus the outer enter and outer exit boundary records,
but suppresses all activity strictly between them, including nested boundaries.
`interrupt-only` accepts both outer boundaries and everything between them,
including nested activity, and rejects other depth-zero events.

Only events that passed their point/watch route, enabled-state gate and every
non-interrupt filter are otherwise-route-eligible and count as suppressed;
disabled or unrouted activity does not. For every continuous interval, the
destination emits one
`trace.suppression` summary before that trace's next retained event (or on
flush/close) with trace ID, policy, first/last suppressed source sequence,
count, entry depth and maximum depth. This is a visibility gap, distinct from
capacity loss, and is bounded/coalesced without recursive emission. Each trace
maintains its own interval even when destinations are shared.

## DES-084 — Watch matches are derived ordered events

A matched watchpoint produces one newly sequenced `watch.match` event after the
source event has completed fan-out. It contains `watch_id`,
`source_event_sequence`, matched address/value summary and actions. Actions
are independently combinable: request boundary stop, write the interactive
console, remain quiet, and route to a bounded sorted trace-ID set. `quiet`
suppresses the watch's own console output, not stops or trace routes. A watch
may route to many traces and a trace may accept many watches. Dangling routes
are rejected atomically.

If `console` and `quiet` are both set, `quiet` wins for console output but does
not cancel stop or route. Derived events are appended to a bounded pending list
and dispatched after the callback unwinds, in ascending watch ID. They cannot
match ordinary tracepoints, watchpoints or gates and cannot recursively enqueue
another `watch.match`; only the explicit watch route selects traces. Routing
uses the enabled state left by source before-gates and interrupt policy because
source after-gates run only after the derived queue drains. The source remains
independently routable.

## DES-085 — Destinations and stdout isolation

Destination objects have stable uint32 IDs and type `ring`, `file`,
`interactive-console`, or `raw-cli-stdout`. Each trace references exactly one;
many traces may share one destination. Ring and file bounds follow DES-072 and
DES-073. Interactive console output is paged/rate-bounded and may report
drops. Raw stdout is permitted only when the process starts in a dedicated
human/raw CLI mode with the protocol server disabled; it is bounded
human-oriented text, not protocol NDJSON. `emu-debug` NDJSON mode
never accepts or advertises a stdout trace destination; its stdout remains
protocol-only. Diagnostics use stderr.

## DES-086 — Lifecycle and limits

Reset preserves trace IDs, enabled states, routes, destinations, counters and
pending suppression state and applies DES-074 generation rules. Image load
disables CODE-addressed tracepoints, gates and watches but does not renumber
traces. Deleting a watch removes its routes. Deleting a destination is rejected
while referenced. Deleting a trace is rejected while referenced by a
tracepoint, gate or watch unless one atomic transaction removes those
references too. File failure disables only its destination, not the trace or
execution. `clear session` closes destinations, discards pending derived and
suppression records and resets ID allocation. Advertised maxima cover traces,
watches, gates, destinations, routes per point/watch, trace IDs per routed
event, pending derived events and encoded record bytes.

## DES-087 — Multi-trace facade and protocol

The sized/versioned C facade atomically replace/lists trace sessions,
destinations, gates, watches and routes; enables/disables a bounded set of
trace IDs; and returns routed pages carrying canonical event plus trace-ID
set. Protocol extensions mirror these operations only after capability
negotiation. The protocol never pushes trace output unsolicited. Unknown IDs,
duplicate IDs, invalid UTF-8, oversize metadata, route overflow and incompatible
destination mode fail without mutation.

## DES-088 — Interactive grammar

The CLI supports `trace create`, `trace on`, `trace off`, `trace list`,
`trace delete`, `gate add`, `watch add ... actions`, `route add/remove` and
destination creation/listing. Commands show IDs, bounded tags, policy,
destination, enabled state and counters. Commands execute only at safe
boundaries; a gate hit during execution uses DES-082 semantics rather than the
time at which the UI next refreshes.

## DES-089 — Multi-trace verification

Golden tests cover two traces sharing and not sharing destinations, stable
sorted route IDs, all four gate timings, conflicting gates, nested interrupt
policies, suppression summaries on resume/flush, watch source correlation,
quiet/console/stop/route combinations, non-recursive derived events, deletion
and atomic replacement, every advertised bound, destination failure, and raw
stdout rejection in NDJSON mode. Disabled or unmatched tracing remains
architecturally equivalent to tracing absent.

## Traceability

DES-064..DES-089 are informed by STU-003, refine ARCH-007..ARCH-010 and govern
future implementation slices after SLC-013.
