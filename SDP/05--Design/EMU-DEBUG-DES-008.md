# EMU-DEBUG-DES-008 — Stable trace-runtime integration seam

Status: active target design
Design items: DES-090..DES-097
Established: 2026-09-02
Steering authority: Issue #14

## Purpose and current-state classification

This document freezes the future external seam after the SLC-015..017 holistic
audit. It is documentation/design only. It does not authorize CLI commands,
`emu-debug` protocol changes, DAP changes, CPU producers, CPU safe-boundary
stop application, sinks or physical/product-specific I/O.

The current `emu_debug_event.h`, `emu_debug_trace.h` and
`emu_debug_runtime.h` surfaces are same-build internal APIs. The opaque runtime
owns its state and is suitable for the SLC-017 in-memory tests, but its raw
records, enums, padding, `size_t` arguments and unversioned status/page structs
are not the stable DES-076/DES-087 ABI. `EM8051_DEBUG_RUNTIME_API_VERSION` does
not by itself make that surface externally stable.

## DES-090 — One emulator-owned semantic model

Breakpoint, watchpoint and tracepoint semantics remain implemented once in the
emulator. CPU producers and the safe-boundary stop arbiter will feed that
model. CLI, the optional `emu-debug` extension and DAP are projections over it;
none may own a duplicate matcher, counter, gate, route, interrupt-policy or
stop-priority implementation.

The accepted `emu-debug` 1.0 CODE-breakpoint behavior remains unchanged until
a separately authorized integration slice composes it with this model.

## DES-091 — Sized and versioned public wrappers

Every future externally stable request, response and array element starts with
these fixed-width fields in this order:

```c
uint32_t struct_size;
uint16_t api_version;
uint16_t flags;
```

Version 1 requires `api_version == 1` and `flags == 0`. Each operation defines
a version-1 minimum size. A smaller struct is invalid. A larger version-1
struct is accepted and unknown trailing bytes are ignored; incompatible field
meaning requires a new `api_version`, never reinterpretation of version 1.
The callee writes only fields present in the caller-advertised output size.

C enum storage size, C `bool`, implicit padding and native `size_t` do not
cross the stable record boundary. Stable discriminants/statuses are fixed-
width integer fields with frozen numeric values. Native pointers may identify
caller-owned buffers for the duration of one same-process call only and are
paired with fixed-width element-size, capacity, written and required counts.

## DES-092 — Ownership, lifetime and atomicity

The stable facade keeps an opaque runtime handle. The caller owns all request,
response and output buffers. The runtime validates version, size, counts,
UTF-8, IDs, references and capacity before deep-copying accepted configuration;
it never retains a caller pointer or a callback pointer.

Every mutation is all-or-nothing. Validation, version, stale-cursor, capacity,
busy and exhaustion failures have explicit fixed status values and leave
configuration, gates, rings, counters, stops, generation and sequences
unchanged unless the operation contract explicitly identifies a consumed
canonical source sequence.

Facade calls are serialized and made only at a stopped/safe boundary. The
external facade exposes no observer registration and no reentrant callback.

## DES-093 — Stable ring record identity and exclusive cursor

Paging uses `after_record_sequence`, not an offset and not the canonical event
sequence. It is an exclusive uint64 cursor scoped to one session, destination
and ring epoch. Zero means before the first retained record. Every inserted
ring record receives one nonzero monotonic record sequence, including ordinary
event views, suppression summaries, future loss markers and sink-status
records.

The record sequence is storage identity only. An ordinary routed record also
carries its canonical event `generation/sequence`; a suppression or loss
record carries its canonical source range. This separation keeps summaries
with no standalone event sequence page-able and prevents ties when several
summaries precede one retained source event.

Reset and load do not reset the ring record sequence. Explicit ring clear
changes the ring epoch and restarts its record sequence. Clear-session changes
the session identity and creates fresh ring epochs. Exhausting a record
sequence is a bounded error; it never wraps or reuses an identity.

## DES-094 — Required page request and metadata

A page request supplies at least:

- `session_id`;
- `destination_id` and `ring_epoch`;
- exclusive `after_record_sequence`;
- bounded `max_records` plus caller-owned element buffer metadata.

The response supplies at least:

- current session, destination and ring epoch;
- requested cursor;
- oldest/newest available record sequence;
- first/last returned record sequence;
- `next_after_record_sequence`, equal to the last returned record or the
  requested cursor when the page is empty;
- returned, required/remaining counts and `more`;
- cursor state (`current`, `expired`, `ahead`, `wrong-session` or
  `wrong-epoch`);
- saturated cumulative overwrite/loss counters and exact
  `lost_before_page` when an expired same-epoch cursor can be measured.

Reads are non-destructive. `max_records == 0` is a metadata-only query. A
same-epoch cursor older than retained history returns the oldest available page
with `expired` and explicit loss metadata; it is never reinterpreted as an
offset. An ahead, wrong-session or wrong-epoch cursor is explicit and cannot
silently select unrelated records.

The current SLC-017 offset page remains internal until this record identity and
metadata contract is implemented and verified in a separately authorized
slice. Configurable capacity, DES-072 loss markers and stop-on-full remain
future implementation work.

## DES-095 — Later CLI and protocol exposure

After CPU producer and safe-boundary integration is independently accepted,
the following operations are suitable for later bounded CLI and optional
`emu-debug` capability-gated exposure:

- query limits/status and list the unified debug points;
- atomically replace/list watches, traces, destinations, points and gates;
- enable/disable traces at a safe boundary;
- configure, read and explicitly clear a ring through DES-093/DES-094;
- query applied stop detail and lifecycle/configuration revision.

Raw `runtime_ingest`, event-bus subscription, router source begin/watch/end,
ring pop, CPU pointers and callback pointers are internal and never protocol
commands. Reset/load reuse the existing emulator operations and generate their
runtime lifecycle records internally. File, interactive-console and raw-stdout
destinations require separate sink authority.

Protocol major 1 and its seven required 1.0 capabilities remain unchanged.
Later commands require named optional capabilities and bounded pull paging; no
unsolicited trace stream may appear on protocol stdout.

## DES-096 — DAP projection boundary

Future DAP mapping is:

- CODE breakpoint -> DAP instruction breakpoint backed by the emulator's
  existing pre-execution CODE-breakpoint semantics;
- stopping IRAM/SFR/XDATA watchpoint -> DAP data breakpoint after canonical
  producers and safe-boundary stop application exist;
- non-stopping trace/log action -> DAP logpoint-like output where representable,
  with structured trace retrieval kept in an emulator extension;
- trace sessions, destinations, routes, gates, paging and interrupt policies ->
  emulator-specific versioned capability/requests, not duplicate DAP-adapter
  state.

DAP cannot reduce the richer emulator model to its lowest common denominator.
The adapter reports unsupported projections honestly and retains emulator-
specific controls behind negotiated extension surfaces.

## DES-097 — Ordered integration issues

The next prerequisite Slice should connect canonical CPU/access/control/IRQ
producers and apply pending watch stops at the existing safe execution boundary
without changing any frontend. Only after that Slice is reviewed and verified
should a CLI/protocol/DAP integration issue expose the DES-091..DES-096 seam.

Recommended frontend issue title:

`[DEBUG][CLI/DAP] Project the unified debug-point and trace-ring model through versioned frontends`

That issue should retain protocol 1.0 compatibility, implement the sized/
versioned facade and cursor contract first, then make CLI and optional protocol
commands call the same facade, and finally map supported operations into DAP.

## Traceability

DES-090..DES-097 refine ARCH-007..ARCH-010 and DES-064..DES-089. SLC-017 freezes
the decisions as target design; product implementation belongs to separately
authorized producer/safe-boundary and frontend integration slices.
