# Debug service/session boundary

`contracts/debug.h` supplies a callback-free facade for the host's debug service.
It uses bounded serialized wire2.1 request/response values instead of exposing
private debugger structs. It is distinct from the CPU and board callback APIs.
All record prefixes follow API1. The owner normally calls it at a completed safe
boundary; faulted snapshots explicitly identify an unsafe boundary. In a nonexecuting
faulted session, status, reset, detach and authorized terminate are permitted
even if safeBoundary=false. The optional wire2.1 boardInspectFault capability
(additionally requiring boardInspect) also permits pure bounded module inspection
in that state, including an unsafe CPU boundary. No pump, resume, step, CPU decode,
CPU memory peek or board XDATA peek is permitted after fault.

`debug_open` admits an authenticated connection ID (nonzero uint64, never reused
within a session), maximum8 clients. Host assigns maximum role OBSERVER or
CONTROLLER and may_terminate 0/1 from endpoint authorization. Opening grants no
controller lease. Client-supplied role cannot exceed this host-authorized role.
No opaque caller/context pointer or callback is retained.

`debug_request` borrows one UTF-8 JSON request without newline, length1..65535.
Response is one UTF-8 JSON response without newline. Output capacity must be
65536 or greater **before** parsing or mutation; smaller returns CAPACITY with
required=65536, written=0 and no side effects. Other native validation failures
write no response. Well-formed native calls always return OK with a serialized
wire success/error, written byte count and required=written. No trailing NUL is
part of written. The caller owns response storage; accepted configuration is
copied. Invalid/malformed requests never partially apply mutations. Run-time
execution/board faults are explicitly state-changing failures as documented.

The same facade handles local CLI, socket clients and DAP projections. There is
one emulator-owned breakpoint/watch/trace matcher. No CPU pointer, event ingest,
observer registration, ring pop or callback is exposed to clients. New wire2.1
CODE points must integrate the existing semantic model rather than add a second
matcher. Existing stdio1.0 continues through its own compatibility wrapper.

`debug_close` releases the connection and its controller lease, preserving
running/paused/faulted session state. A dropped connection cannot terminate,
reset, resume or retry a request. Close never retains a response pointer.
A controller lease is acquired by attach, at most one at a time. Observers can
query state and optional read/trace operations but cannot mutate. Request IDs
are per connection, increasing uint32; exhaustion requires a new connection.
There is no retry deduplication cache. Lost response means outcome unknown:
reconnect, inspect generation/state and reconcile; never replay blindly.

## Trace compatibility

The authoritative semantics in [stable facade reference](../reference/debug-stable-facade.md)
and [model reference](../reference/debug-model.md) remain binding. The old
same-build trace runtime headers are not a stable ABI. Baseline wire2.1 does not
expose configuration of watches/routes/gates or claim those producers are
implemented. Optional `traceRead` defines pull paging only; richer configuration
needs a separately versioned contract and capability. No trace is pushed as
unsolicited NDJSON, and no ring read consumes records.

Page cursor is (sessionId,destinationId,ringEpoch,afterRecordSequence). Record
sequence is independent of canonical event sequence. Expired same-epoch cursor
returns retained records and exact lostBeforePage; ahead/wrong-session/wrong-epoch
returns no records with explicit cursor state. Zero maxRecords is metadata-only.
nextAfterRecordSequence is last returned sequence, otherwise requested cursor.
Counts and loss are uint64 strings on the wire. Overwrite counters saturate;
identities never wrap. Reset does not restart ring sequences; ring clear changes
epoch and restarts sequence; session clear creates new identity. Neither clear
operation is exposed by baseline2.1. See schema for exact page/record fields.
