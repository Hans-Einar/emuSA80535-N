# Generic debugger tracepoint design

Status: implementation-ready proposal, documentation only  
Baseline: `emuSA80535-N` `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`  
Frozen optional consumer study: `emuSA80535-DAP`
`36639b48ddb2ffbafa14c00da794fe1734f7483b`

## Purpose and boundaries

Add deterministic breakpoints, data watchpoints and non-stopping tracepoints
to the generic emulator. They must be usable directly in the legacy debugger
and C facade; DAP remains optional. The design contains no firmware addresses,
board models, device names, physical endpoints or target policy. Trace
observers record facts only and cannot modify emulated execution.

## What exists today

The core already reports instruction begin, SFR writes, MOVX reads/writes,
Siemens interrupt request/accept/release, Timer0/1 overflow and SAB mode-3 UART
progress. These are separate callback slots and have different record schemas.
`emu_debug.c` consumes the sole generic trace callback to build sequential
decode predecessor history. The facade owns atomic CODE breakpoints and checks
them before execution. The TUI has one global PC breakpoint plus step/run and
a small snapshot history. Protocol 1.0 exposes CODE breakpoints but no data
watchpoint, trace storage, memory read or subscription.

The implementation must preserve all these behaviors. It must introduce a
single ordered event bus or an internal multiplexer rather than replace an
existing callback consumer. The core bus carries raw architectural records;
the debugger session, not the CPU, adds lifecycle identity and storage status.

## Definitions

| Point | Match time | Effect | Stop boundary |
|---|---|---|---|
| breakpoint | instruction boundary PC | no event retention required | before matched instruction |
| watchpoint | observed memory/register access | retain trigger detail and request stop | next safe execution boundary |
| tracepoint | any supported event | retain/stream event | never stops |

Multiple watchpoints may match one instruction. Execution finishes that
instruction and all pending machine cycles once, all trigger details are retained, and the lowest point ID is
the primary reason. Exceptions and halt outrank watchpoints. Breakpoints are
tested before execution and therefore cannot coincide with accesses from that
instruction. A safe boundary has `mTickDelay == 0` and no active opcode or
callback: normally instruction completion, interrupt-entry completion, or the
end of an IDLE cycle. A debugger mutation while stopped reports synchronously.

## Address spaces and accesses

Stable address spaces are `none`, `code`, `iram-lower`, `iram-upper`, `sfr`
and `xdata`. Address is uint16; implemented ranges are validated by variant.
Access kinds are `fetch`, `read`, `write` and `rmw`. RMW means the architectural
latch/read-modify-write path, not two synthesized events. CODE decode requests
are debugger operations and do not masquerade as CPU fetches.

## Event taxonomy

Stable top-level kinds:

- `instruction.begin`, `instruction.end`, `code.fetch`;
- `memory.read`, `memory.write`, `memory.rmw`;
- `control.call`, `control.return`;
- `interrupt.request`, `interrupt.enter`, `interrupt.exit`;
- `timer.overflow`, `uart.frame`, `uart.bit`;
- `exception`, `halt`, `reset`, `image.load`, `debug.mutation`;
- `watch.match`, `trace.suppression`, `trace.loss`, `trace.sink-status`,
  `trace.marker`.

Call subtypes are `acall` and `lcall`; return subtypes are `ret` and `reti`.
Records include call site, target/resulting PC and architectural return PC when
known. Interrupt records include generic source identity, vector, priority and
nesting where available. Existing timer/UART details are carried in a bounded
kind-specific payload. Variant-specific fields are permitted only under a
named generic extension object and cannot change base semantics.

## Canonical event record

The core record is a tagged, fixed-size raw architectural value. The debugger
wraps it in this session envelope. Reset/load/debug-mutation/loss/sink status
and markers are debugger events, not fabricated CPU events. JSON uses this exact
logical envelope:

```json
{
  "schema":"emu8051-trace-event",
  "version":{"major":1,"minor":0},
  "session":1,
  "generation":3,
  "sequence":184337,
  "instructionCount":26102,
  "machineCycleCount":79810,
  "phase":40,
  "kind":"memory.write",
  "executingPc":27116,
  "address":{"space":"sfr","value":232,"widthBits":8},
  "access":"write",
  "oldValue":{"known":true,"value":0},
  "newValue":{"known":true,"value":68},
  "detail":{}
}
```

Required rules:

- `sequence` starts at one for a new debugger session and never decreases or
  repeats, including across reset/load.
- `generation` increments on successful reset, load or snapshot restore.
- `executingPc` is the instruction responsible for an access; asynchronous
  events use the boundary PC and state this in `detail.pcRole`.
- `phase` is the normative ordering phase, not elapsed time.
- unavailable values use `{"known":false}`. Zero is never a sentinel.
- write `newValue` is the final architecturally stored or externally accepted
  value. `oldValue` is present only if captured without an extra access.
- values wider than JavaScript's exact integer range serialize as lowercase
  fixed-width hexadecimal strings; current architectural fields fit integers.

Loss record:

```json
{"schema":"emu8051-trace-event","version":{"major":1,"minor":0},
 "session":1,"generation":3,"sequence":190001,"instructionCount":27000,
 "machineCycleCount":82400,"phase":250,"kind":"trace.loss",
 "executingPc":null,"address":{"space":"none"},"access":null,
 "oldValue":{"known":false},"newValue":{"known":false},
 "detail":{"firstLostSequence":184338,"lastLostSequence":190000,
 "lostCount":5663,"reason":"ring-overwrite"}}
```

Kind-specific `detail` schemas are closed under major 1 (unknown fields may be
ignored, but producers use the named fields and types):

| Kind | Required `detail` fields |
|---|---|
| `instruction.begin` | `opcode:uint8`, `decodedSize:uint8-or-null` |
| `instruction.end` | `resultingPc:uint16`, `machineCycles:uint32` |
| `code.fetch` | `fetchIndex:uint8` |
| `memory.*` | `source:cpu-or-debugger`, `rmw:boolean` |
| `control.call` | `subtype:acall-or-lcall`, `targetPc:uint16`, `returnPc:uint16` |
| `control.return` | `subtype:ret-or-reti`, `resultingPc:uint16`, `expected:boolean-or-null` |
| `interrupt.request` | `sourceId:uint16`, `asserted:boolean`, `pendingMask:uint16` |
| `interrupt.enter` | `sourceId:uint16`, `vector:uint16`, `priority:uint8`, `nesting:uint8`, `returnPc:uint16` |
| `interrupt.exit` | `sourceId:uint16-or-null`, `priority:uint8-or-null`, `nesting:uint8`, `resultingPc:uint16` |
| `timer.overflow` | `timerId:uint8`, `tl:uint8`, `th:uint8` |
| `uart.frame` | `direction:rx-or-tx`, `stage:string-enum`, `data:uint8`, `ninthBit:boolean`, `accepted:boolean-or-null` |
| `uart.bit` | `direction:rx-or-tx`, `bitIndex:uint8`, `bitValue:boolean` |
| `exception` | `code:int32`, `messageId:string-enum` |
| `halt` | `mode:string-enum` |
| `reset` | `seed:uint32`, `entry:uint16` |
| `image.load` | `sha256:64-lower-hex` |
| `debug.mutation` | `operation:string-enum`, `origin:string-enum` |
| `watch.match` | `watchId:uint32`, `sourceEventSequence:uint64`, `actions:string-enum-array`, matched address/value summary |
| `trace.suppression` | `traceId:uint32`, `policy:string-enum`, `firstSuppressedSequence:uint64`, `lastSuppressedSequence:uint64`, `suppressedCount:uint64`, `entryDepth:uint8`, `maxDepth:uint8` |
| `trace.loss` | `firstLostSequence:uint64`, `lastLostSequence:uint64`, `lostCount:uint64`, `reason:string-enum` |
| `trace.sink-status` | `sinkId:uint32`, `state:string-enum`, `errorCode:string-enum-or-null`, `segment:uint32` |
| `trace.marker` | `markerId:uint32`, `label:string-max-128-UTF8` |

`null` is used only where this table permits it. String enums and label bytes
are bounded at validation/serialization. A minor version may add optional
detail fields but cannot reinterpret an existing one.

## Normative deterministic order

Events are emitted synchronously at canonical mutation points. The debugger
assigns session sequence when it accepts a raw or synthesized event. Ordering
follows the current `tick()` implementation:

1. With `mTickDelay != 0`, decrement delay, run that cycle's timer/UART
   producers, advance virtual time, and emit `instruction.end` only after the
   final cycle of the tracked instruction. Do not arbitrate or fetch an opcode.
2. At zero delay, synchronize/arbitrate interrupts before opcode fetch. An
   accepted interrupt performs stack/PC/controller mutations, emits entry,
   starts its delay and advances the first entry timer/UART cycle. The vector
   opcode is not fetched in that tick.
3. Without accepted interrupt and outside IDLE, emit instruction begin, actual
   CODE fetches and memory accesses in handler order, then advance the first
   timer/UART cycle. Remaining cycles use rule 1.
4. In IDLE, arbitrate first; otherwise advance one timer/UART cycle without
   instruction events. POWER DOWN generates no spontaneous cycle event.

Timer events preserve producer call order. In the current SAB80535 path a
Timer1 overflow drives the UART before the legacy timer-overflow observer, so
unified UART consequences precede the timer observer event. Interrupt request
is recorded when producer state changes; accept/entry occurs at a later
zero-delay boundary.

An existing producer event is not re-created later by the debugger. Legacy
observers and the bus each receive exactly one view from the same producer.
This order is golden-test material and changes require schema/design review.

## Point specification

```json
{
  "id":17,
  "type":"tracepoint",
  "enabled":true,
  "traceIds":[7,23],
  "events":["memory.write"],
  "address":{"space":"xdata","from":4096,"to":4351},
  "access":["write"],
  "pc":{"from":8192,"to":12287},
  "bitMask":255,
  "changeOnly":true,
  "skip":0,
  "hitLimit":10000,
  "condition":[["field","newValue.value"],["const",0],["ne"]]
}
```

Omitted selectors mean any. Ranges are inclusive and non-wrapping.
`changeOnly` requires known old/new values and matches only when
`((old ^ new) & bitMask) != 0`; an unknown old value does not match. `skip`
counts predicate matches that take no action. `hitLimit` counts actions and
atomically disables the point after the limit; zero means unlimited. All
counters saturate at uint64 maximum.

Conditions are bounded postfix bytecode. Allowed operations are `field`,
`const`, `eq`, `ne`, `lt`, `le`, `gt`, `ge`, `mask-any`, `mask-all`, `and`,
`or`, `not`. Fields are only immutable record scalar leaves such as
`newValue.value`. Loading an unavailable value produces a typed `unknown`;
ordered/equality/mask comparisons with `unknown` deterministically evaluate
false, while `newValue.known`/`oldValue.known` can be tested explicitly. Limits include
maximum 64 operations, stack depth 16 and no strings, loops, CPU reads,
allocation, function calls or division. Validation is atomic and rejects
unknown fields, stack errors and type mismatch.

## Multi-trace sessions and routing

A trace is an independently gated session, not another CPU observer. The CPU
still emits one immutable canonical event. A trace has a stable nonzero uint32
`traceId`, an enabled flag, a UTF-8 tag of at most 64 bytes, a UTF-8 comment of
at most 256 bytes, one interrupt policy and exactly one destination. IDs are
not reused before `clear session`. Many traces may share a destination.

```json
{"traceId":23,"tag":"control","comment":"mainline control flow",
 "enabled":true,"interruptPolicy":"suppress-during-interrupt",
 "destinationId":4}
```

The sequencer creates each architectural or derived event exactly once, then
computes an ascending, duplicate-free, bounded `traceIds` set. It does not
clone an event per trace. A destination receives one routed view containing
the subset eligible for that destination. Traces 7 and 23 sharing a file yield
one line with `"traceIds":[7,23]`; separate files receive views with the same
canonical session/generation/sequence and their respective subsets. Empty
route sets are not written.

Routes are bounded many-to-many relationships. A watch may route to several
traces and a trace may receive several watches. Duplicate or dangling routes
are rejected atomically. Implementations advertise fixed maxima for traces,
destinations, routes, trace IDs per event, pending derived events and metadata.

Ordinary tracepoints carry a bounded sorted `traceIds` route set. Their
predicate selects canonical events and that set selects the trace sessions
which may retain them. A session does not implicitly subscribe to every event.
Watch routes perform the same selection for derived `watch.match` events;
gates change enabled state but never create an implicit subscription.

Configuration schemas are sized and versioned. The C facade uses fixed arrays
or caller-owned slices, never unbounded pointers embedded in retained objects:

```c
struct em8051_trace_session {
    uint32_t struct_size, trace_id, destination_id;
    uint16_t version, tag_length, comment_length;
    uint8_t enabled, interrupt_policy;
    char tag[64], comment[256];
};
struct em8051_watch_route {
    uint32_t struct_size, watch_id;
    uint16_t version, trace_id_count;
    uint32_t trace_ids[EM8051_MAX_ROUTES_PER_WATCH];
};
```

The optional protocol mirrors these as bounded arrays, for example
`{"command":"setTraceEnabled","traceIds":[7,23],"enabled":false}` and
`{"command":"replaceWatchRoutes","routes":[{"watchId":12,
"traceIds":[7,23]}]}`. Responses echo accepted IDs and a configuration
revision. Validation failure returns one error and preserves the prior graph.

Deterministic processing order is: assign sequence; evaluate predicates in
point-ID order against pre-event state; apply before-gates; route source views
in destination-ID order; enqueue derived watch matches in watch-ID order;
drain them after the callback unwinds; then apply after-gates. Within routing,
trace IDs are ascending. Overflow is a configuration error, never truncation.

### Trace-on and trace-off gates

A gate contains a normal bounded predicate, `operation` (`on` or `off`),
`timing` (`before` or `after`) and a bounded sorted trace-ID set.

| Gate | State used for current matching event | State after event |
|---|---|---|
| `on-before` | enabled | enabled unless changed by an after-gate |
| `on-after` | state after all before-gates | enabled |
| `off-before` | disabled | disabled unless changed by an after-gate |
| `off-after` | state after all before-gates | disabled |

All predicates and the initial enabled state see pre-event configuration.
Before-gate conflicts apply in ascending gate ID; their final state controls
the current source event and its derived watch events. After-gate conflicts
then apply in ascending ID after the source and all derived events; their final
state controls the next event. Thus `on-after` does not force exclusion when
already enabled, and `off-after` does not force inclusion when already
disabled. A no-op gate records its hit/counters but creates no route. Gates
never alter queued records. Manual `trace on/off` commands run at a stopped
safe boundary and affect the next event.

### Interrupt inclusion policies

- `include` retains every otherwise eligible event.
- `suppress-during-interrupt` retains depth-zero activity plus the outermost
  enter/exit boundary records, suppressing the interval between them.
- `interrupt-only` retains outermost enter through matching outermost exit,
  including nested interrupts, and rejects depth-zero activity.

Depth is architectural. `interrupt.enter` is routed at the old depth, then
increments it; `interrupt.exit` is routed at active depth, then decrements it.
For suppression, outer enter at depth zero and outer exit at depth one are
explicit retained boundaries; nested enter/exit events are hidden. For
interrupt-only, both outer boundaries and everything between them are
retained. Nested exits therefore do not prematurely reopen a mainline trace.

Suppressed counts only otherwise-route-eligible events: an ordinary tracepoint
or explicit watch route selected the trace, it was enabled after before-gates,
and every filter except interrupt policy accepted the event. Unrouted or
disabled events do not inflate the count. Every continuous hidden interval
produces one `trace.suppression` summary
before that trace's next retained event, or on flush/close. It names trace ID,
policy, first/last suppressed source sequence, count, entry depth and maximum
depth. Each trace tracks its own interval even when destinations are shared.
Suppression is intentional filtering, unlike `trace.loss`, and summary
generation is bounded and non-recursive.

## Watchpoint actions and derived events

A watch separates its access match from its action triggers. The access match
selects address space, address/range and read/write access. Each optional
action trigger compares one immutable source-event value (`old`, `new` or
`delta`) with an integer constant using `eq`, `ne`, `lt`, `le`, `gt` or `ge`.

```json
{
  "id":12,
  "type":"watchpoint",
  "events":["memory.write"],
  "address":{"space":"xdata","from":29465,"to":29465},
  "valueType":{"width":8,"signed":false},
  "actions":[
    {"kind":"stop","when":{"value":"new","op":"ge","constant":200}},
    {"kind":"console","when":{"value":"new","op":"gt","constant":180}},
    {"kind":"route","traceIds":[23]}
  ]
}
```

`eq` means equal and the ordered operators mean strictly less/greater or
less/greater-or-equal. Width is 8, 16, 32 or 64 bits. Signedness is explicit;
implicit signed/unsigned mixing is rejected. Both operands are converted to
the declared type. `delta` is `new - old` in a checked signed value one bit
wider than the declared operand where representable. Unknown input evaluates
false, and configuration rejects out-of-range constants. An action without
`when` fires on every access match.

This lets route record every value while stop fires only at a threshold, such
as `new >= 200`. Action triggers are evaluated in listed order against the
same immutable source event, without CPU reads, and share the bounded condition
operation limit. Multiple matching stop triggers coalesce into one stop request
while all fired action metadata remains visible.

A watch for which at least one action fires emits one ordered `watch.match`
after source-event fan-out. It contains `watchId`, `sourceEventSequence`,
matched address/value detail and the fired actions. Actions combine
independently: `stop` requests the established safe
boundary; `console` prints a bounded interactive notice; `quiet` suppresses
only that notice; and `route` sends the match to a bounded trace-ID list. If
`console` and `quiet` are both set, `quiet` wins without cancelling `stop` or
`route`.

Thus a watch can print to the interactive debugger while a trace stores its
structured match, or remain quiet while a trace stores it. The original access
and derived match remain separate canonical records: every `watch.match` gets
its own sequence and names the source sequence. Matches enter a fixed-capacity
pending list in ascending watch ID and dispatch after the callback unwinds.
They cannot match ordinary tracepoints, watches or gates and cannot recursively
enqueue another match; only explicit watch routes select traces. They observe
the state left by source before-gates and interrupt policy. Source after-gates
run only after the derived queue drains.

## Trace destinations

Destination IDs are stable nonzero uint32 values:

| Type | Behavior |
|---|---|
| `ring` | bounded in-memory routed-event ring |
| `file` | explicit JSONL sink with deterministic rotation |
| `interactive-console` | bounded/paged output in the legacy debugger |
| `raw-cli-stdout` | stream in dedicated raw CLI process mode only |

`raw-cli-stdout` emits bounded human-oriented text, not protocol NDJSON, and is
rejected whenever `emu-debug` NDJSON mode is active.
Protocol stdout remains protocol-only; clients pull bounded ring pages or use
an explicit file destination. No watch or trace record is pushed unsolicited.
Diagnostics remain on stderr.

### Multi-trace golden scenarios

Required tests include: shared versus separate destinations; stable trace ID
and tag across reset; every on/off timing from initially enabled and disabled
states; conflicting before gates, conflicting after gates and before/after
conflicts; nested IRQs
under all three policies; suppression closure on resume, flush and close;
simultaneous watch matches ordered by watch ID; stop+console+route, quiet+route
and console+quiet+stop+route combinations; a routed `watch.match` that cannot
retrigger; atomic removal of traces referenced by tracepoints, gates or watches
and destinations referenced by traces; every configured bound; sink failure;
and rejection of raw stdout while NDJSON mode is active. Each golden record
asserts canonical sequence, source correlation, route order and explicit loss
or suppression rather than merely comparing line counts.

## Storage and backpressure

### Ring

The debugger owns a fixed-capacity ring allocated during configuration. A
capacity of zero disables retention. Overwrite mode requires capacity at least
two; capacity one supports only `stop-on-full`. Default overflow overwrites the
oldest complete records and coalesces loss into one pending interval. The first
ordinary record causing eviction may be retained. Before the following
ordinary record, the sequencer emits a loss marker. A marker-caused eviction is
folded into that marker in one bounded operation and cannot recurse.
Alternative `stop-on-full` requests a stop at the next safe execution boundary.

`readTrace(afterSequence, maxRecords)` returns a bounded ordered page plus
`oldestSequence`, `newestSequence`, `nextAfterSequence`, `more` and cumulative
loss counters. Reading never removes records. Clear is explicit.

### JSONL sink

Only an explicit debugger command opens a local file. It accepts an absolute
UTF-8 path, create-new or append-continuation mode, maximum records/segment,
maximum encoded bytes/segment and flush policy. Rotation uses counts only:
`name.000000.jsonl`, `name.000001.jsonl`. Each segment begins with a header
containing schema, emulator identity, image digest, session/generation and
previous segment digest/name where available.

The implementation canonicalizes the parent directory, rejects `.`/`..`,
device/special files and symlink or reparse-point traversal, uses exclusive
create-new by default and restrictive user-only permissions where supported.
Append-continuation validates the existing header, identity and final complete
line. A headless build may constrain paths to an explicitly configured trace
root; it advertises `traceFileV1` only when its path policy is active. Rotation
uses identical checks and never overwrites an existing segment.

Open, encode, write, flush, close or rotation failure disables only that sink.
It increments sink failure counters and attempts one `trace.sink-status` ring
record. It never stops or alters CPU execution unless the user separately
selected `stop-on-sink-failure`. Partial final lines are treated as a failed
segment during reading. File output is serialized with execution; there is no
unbounded background queue. In headless mode trace bytes go only to the opened
file: stdout remains protocol NDJSON and diagnostics remain on stderr.

## Lifecycle

| Operation | Points/traces/routes | Counters/ring | Sink | Generation |
|---|---|---|---|---|
| reset | preserved, including enabled states | reset marker, otherwise preserved | preserved | increment |
| load image | CODE points disabled; traces/routes preserved | load marker | preserved | increment |
| clear trace | preserved | cleared with next sequence unchanged | preserved | unchanged |
| clear session | removed; ID allocation reset | cleared | flushed/closed | new session |
| snapshot restore | restored definition/counters | restored or explicit continuation marker | closed until reopened | increment |
| terminate/EOF | irrelevant | bounded final flush | closed | unchanged |

Snapshot/replay is a later slice. A snapshot contains CPU and internal
peripheral state, image identity, virtual stimuli cursor, point state,
generation and next sequence. Host file handles and paths are not executable
state and are not reopened implicitly. Replay injects only recorded virtual
stimuli and compares emitted events. Differential trace compares normalized
records, can ignore declared fields, and reports the first missing, extra or
different record plus bounded context.

## C facade proposal

Names are illustrative but signatures define the ownership model:

```c
enum em8051_debug_status em8051_debugger_replace_points(
    struct em8051_debugger *, const struct em8051_debug_point *, size_t);
enum em8051_debug_status em8051_debugger_list_points(
    const struct em8051_debugger *, struct em8051_debug_point *,
    size_t capacity, size_t *required);
enum em8051_debug_status em8051_debugger_configure_trace_ring(
    struct em8051_debugger *, const struct em8051_trace_ring_config *);
enum em8051_debug_status em8051_debugger_read_trace(
    const struct em8051_debugger *, uint64_t after_sequence,
    struct em8051_trace_event *, size_t capacity,
    struct em8051_trace_page *page);
enum em8051_debug_status em8051_debugger_clear_trace(
    struct em8051_debugger *);
enum em8051_debug_status em8051_debugger_open_trace_file(
    struct em8051_debugger *, const struct em8051_trace_file_config *);
enum em8051_debug_status em8051_debugger_flush_trace_file(
    struct em8051_debugger *);
enum em8051_debug_status em8051_debugger_close_trace_file(
    struct em8051_debugger *);
enum em8051_debug_status em8051_debugger_get_trace_status(
    const struct em8051_debugger *, struct em8051_trace_status *);
enum em8051_debug_status em8051_debugger_replace_trace_sessions(
    struct em8051_debugger *, const struct em8051_trace_session *, size_t);
enum em8051_debug_status em8051_debugger_replace_trace_destinations(
    struct em8051_debugger *, const struct em8051_trace_destination *, size_t);
enum em8051_debug_status em8051_debugger_set_traces_enabled(
    struct em8051_debugger *, const uint32_t *trace_ids, size_t, bool enabled);
enum em8051_debug_status em8051_debugger_replace_watch_routes(
    struct em8051_debugger *, const struct em8051_watch_route *, size_t);
```

Every public struct begins with `uint32_t struct_size` and `uint16_t version`.
Caller owns input/output arrays. List/read report required count without
overflow. Replacement/configuration validates and allocates before committing;
failure leaves old state intact. The core remains unaware of files, JSON and
protocol.

## Legacy interactive commands

Examples define grammar, not target-specific aliases:

```text
break add 0x1234
watch add write xdata 0x1000-0x10ff change
trace add sfr write 0x80-0xff pc 0x2000-0x2fff limit 10000
destination create file 4 /absolute/path/run.jsonl
trace create 23 tag "control" destination 4 interrupts suppress
trace on 23
trace off 23
gate add 41 off after traces 7,23 pc 0x6a1a
watch add write xdata 0x7319-0x731a actions stop,route traces 23 quiet
watch add write xdata 0x7319 u8 stop when new ge 200 route traces 23
routes list
points list
point disable 17
point delete 17
trace ring capacity 65536 overwrite-oldest
trace show after 184000 count 200
trace clear
trace file open /absolute/path/run.jsonl records 1000000 bytes 67108864
trace file flush
trace file close
trace status
```

Commands operate only while the UI is at a safe boundary. Existing `k` single
breakpoint remains a compatibility shortcut represented internally as a
reserved breakpoint ID. Parse limits and error messages are shown before any
state change. The TUI renderer must page results; it cannot load an entire
trace file into memory. The current curses loop has no command editor, so Slice
G first adds a fixed-size modal input buffer and paged ring viewer; file
browsing and live scrolling are not required. UI pacing may use wall time, but
matching, sequence and trace timestamps never do.

## Backward-compatible emu-debug protocol

Protocol major remains 1. A later minor version optionally advertises:

- `debugPointsV1`;
- `traceRingV1`;
- `traceFileV1` (may be withheld in restricted builds);
- `multiTraceRoutingV1`.

Commands mirror the facade: `replaceDebugPoints`, `listDebugPoints`,
`configureTraceRing`, `readTrace`, `clearTrace`, `openTraceFile`,
`flushTraceFile`, `closeTraceFile`, `getTraceStatus`. Requests remain bounded
NDJSON and synchronous. `readTrace` has negotiated `maxTraceRecordsPerRead`;
point count, condition operations, ring capacity and path/rotation limits are
reported by `hello`. No unsolicited high-volume trace events are written to
stdout in v1; clients pull pages. Old clients request only the frozen seven
capabilities and observe unchanged behavior. A 1.0 client accepts a 1.x server
only through the frozen named-capability rule. Existing responses cannot change
meaning and no unsolicited trace event may appear. Unknown optional commands
fail explicitly. If deployed 1.0 parsers are not proven tolerant of extra
`hello` fields, advertise extension limits only after request-side minor or
capability opt-in.

With `multiTraceRoutingV1`, additional bounded commands atomically replace or
list trace sessions, destinations, gates and watch routes, and enable/disable
named trace IDs. `hello` reports every route/metadata/derived-queue maximum.
Routed pages contain canonical events plus sorted `traceIds`; they never push
events. Invalid IDs, UTF-8, destinations or over-limit graphs fail without
partial mutation.

DAP can later map instruction breakpoints to existing CODE breakpoints, DAP
data breakpoints to watchpoints, and custom trace/log points to tracepoints.
That adapter work must negotiate capabilities and is not required to use the
CLI or facade.

## Instrumentation plan

1. Introduce the fixed raw event record, debugger-session sequencer and bounded
   multi-subscriber fan-out; adapt existing records without removing public
   legacy callbacks. Registration/removal is rejected during dispatch,
   callbacks run in stable slot order, and recursive execution/mutation from a
   callback is invalid.
2. Funnel all lower/upper IRAM accesses through canonical helpers. Instrument
   direct, indirect and register-bank paths exactly once.
3. Instrument SFR normal reads, RMW latch reads and final writes, capturing old
   values before mutation. Preserve port pin-versus-latch semantics.
4. Instrument MOVX after legacy callbacks resolve the final read/write result;
   old write value is unknown unless the backing store path already has it.
5. Instrument actual CODE fetches and instruction begin/end.
6. Classify ACALL/LCALL/RET/RETI in opcode control-flow helpers and connect
   existing IRQ accept/release events to the common sequence.
7. Add matcher and pending boundary-stop handling in the debugger, not core
   opcode handlers.
8. Add ring, then JSONL, then facade/CLI, then optional protocol.

Disabled tracing must use a predictable fast branch and produce byte-for-byte
identical architectural outcomes. Performance goals are measured, not semantic:
benchmark disabled overhead, one nonmatching point, one matching point and
full instruction tracing. No sampling based on wall time is allowed.

## Implementation slices and verification

| Slice | Deliverable | Essential tests |
|---|---|---|
| A | event schema, sequence, fan-out | existing callback compatibility; sequence/order |
| B | IRAM/SFR/XDATA/CODE instrumentation | opcode access matrix; old/new/unknown; no duplicate events |
| C | control flow and IRQ ordering | calls/returns, RETI, nested/simultaneous IRQ golden traces |
| D | matcher and watchpoint stops | selectors, condition limits, action-specific eq/ne/lt/le/gt/ge triggers, signedness, change-only, skip/hit, priority |
| E | bounded ring/loss | wrap, coalesced loss, zero capacity, stop-on-full, allocation failure |
| F | JSONL/rotation | golden schema, exact count/byte rotation, partial write and disk-full simulation |
| G | C facade and TUI | atomic replacement, parser bounds, compatibility shortcut, paged display |
| H | optional protocol | hello negotiation, old-client regression, malformed/oversize requests, paging |
| I | snapshot/replay/diff | identical replay, first divergence, sink continuation, generation behavior |
| J | multi-trace routing | gate timing, nested IRQ policy, watch derivation, route/destination bounds |

Every slice runs current Stage-0/1, ports/bus, facade and process suites on
Linux and Windows, strict warnings and sanitizers. Dedicated audits assert:

- tracing disabled does not change CPU/memory/counters/stop results;
- shared-destination events are coalesced and trace IDs are sorted/stable;
- all four gate timings and nested interrupt suppression produce golden traces;
- watch derivation cannot recurse and preserves source-event correlation;
- watch action thresholds cover equality and ordered comparisons for
  signed/unsigned widths, unknown values, boundary constants and stop coalescing;
- callbacks cannot obtain mutable CPU storage;
- buffers, input, event rate and file rotation are bounded;
- reset/load/restore behavior matches the lifecycle table;
- stdout remains protocol-only;
- no network, serial, GPIO, CAN or other live endpoint is opened;
- absolute paths and UTF-8 behavior are portable;
- hostile JSON/condition/path input cannot allocate or execute unbounded work.

## Security and failure posture

Trace contents may expose firmware data, so file creation is explicit and
never enabled by image content. The protocol cannot select a relative path,
execute expressions, load plugins or name physical devices. Restricted builds
may omit `traceFileV1`. All counters and size arithmetic are overflow-checked.
Malformed records or sink errors are observable but never interpreted as CPU
commands. Tracepoints cannot write registers or memory; debugger mutation, if
ever added, is a separate capability and event kind.

## Acceptance definition

The subsystem is ready for reverse-engineering workflows when a user can set a
bounded address/event tracepoint, run without stops, retrieve a deterministic
ordered trace with explicit losses, set a data watchpoint and stop at the next
safe boundary with trigger detail, replay identical virtual stimuli, and
compare two traces—without DAP, target knowledge, physical I/O or unbounded
retention.
