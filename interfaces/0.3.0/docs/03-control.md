# Persistent control 3.0

The target Unix endpoint is explicitly enabled at host startup with
`--control-unix PATH --execution-mode externally-driven|host-driven`; these flags are targets and are not accepted by the current runtime; no listener
exists by default. PATH is absolute UTF-8 <=107 bytes, owner-selected, in a
0700 directory owned by effective UID. Host creates a SOCK_STREAM socket with
0600 mode, refuses preexisting paths/symlinks and removes only its own socket
inode at orderly shutdown. It checks SO_PEERCRED UID equals effective UID before
reading frames. No TCP, authentication override, arbitrary endpoint unlink or
raw byte transport is defined. Host-driven mode uses the existing single owner
pump and is mutually exclusive with `advance`; externally-driven mode advances
only through explicit `advance` **or `step`**. `resume` only changes state.
Host decides idle polling; wall-clock connection timeouts never advance CPU time.

Wire3 is a new major, selected per endpoint at startup. No same-connection
fallback to wire2.1, script1 or legacy1.0 is permitted. Baseline command fields
and semantics are preserved except explicitly changed hello, lease checks,
boardInput parent identity and replacement of tracePage. Shapes are frozen in
runtime-3.0.schema.json; the schema is authoritative for required fields and
closed-object discriminants, this document for state/correlation semantics.
Every integer wider than uint32 is a canonical decimal string bounded by
18446744073709551615; IDs/generation must be nonzero except explicit unbound
sentinels. FORMAT validation and cross-field checks in target_wire.py are part
of the contract; schema validation alone does not validate sequencing/state.

Framing inherits wire2.1: UTF-8 JSON object + LF, <=65536 bytes including LF;
no CR/BOM/raw LF, duplicate keys, nonfinite numbers, unknown fields/commands or
invalid UTF-8. Depth<=16, values<=8192, object members<=256,array elements<=1024.
At most8 clients,32 pending complete requests total, one outstanding request and
one complete output frame per client; IDs strictly increase from1 (no wrap).
Input queue admission order is execution order at safe boundaries. Excess
complete requests get BUSY without mutation. Output blocked/incomplete hello or
frame for5000ms disconnects. No asynchronous protocol events; records are polled.
Recognized command + valid trustworthy ID malformed input returns INVALID then
closes; otherwise close without response. Wrong hello version => VERSION then
close. Schema-valid state/role errors return correlated errors without closing.
BUSY alone has retryable=true. Requests may have completed before disconnect;
no automatic replay, rollback, persistent replay IDs or exactly-once delivery
claim. Reconnect gets a fresh connection, hello, attach and state queries.

## Connection, lease and state

hello is first and exactly once. attach follows and selects the existing sole
session without resetting/loading it. Observer never mutates. Controller attach
requires host authorization, creates a unique nonzero leaseId and is exclusive;
BUSY if held. Every mutation (including advance) requires current sessionId,
generation and leaseId. Validate in order: framing/schema, hello/attached state,
capability, session identity, generation, role/lease, operation state and limits,
then mutation. Wrong identity/generation => STALE; wrong/missing role or lease
=> DENIED. Native advance connection_id must identify the lease holder.
Lease IDs never reuse within session. releaseControl invalidates holder lease
and leaves attachment as observer; acquireControl requests a new lease and
returns BUSY if occupied. No observer takeover or implicit lease expiry.
Disconnect/detach releases lease while preserving session, execution state,
trace/capture/sinks and scheduled input. Externally driven running state remains
running but makes no progress. Host-driven execution may continue. Terminate
requires host termination permission and ends the session; it does not grant
arbitrary process signals or file access.

| Command group | Legal state and effect |
|---|---|
|status, traceConfig, tracePage, outcomePage, captureStatus, sinkStatus|observer/controller; any non-terminated attached session, faulted included|
|pause|running or paused; returns applied boundary snapshot|
|resume|paused or running, rejects faulted/power-down-unsupported; no external-mode progress|
|step|paused only; exactly baseline one-instruction semantics, maxCycles bound and safe drain, remains paused|
|advance|running + externally-driven only; budget1..100000 machine cycles plus <=16 safe-drain cycles; may stop early; no wall time mapping|
|reset|paused/faulted; clears fault, increments generation, paused; preserves general trace state, closes B binding|
|replaceCodeBreakpoints, boardInput|baseline state rules; additionally lease required|
|decodeCode, readMemory, boardInspect|baseline nonmutating rules/capabilities|
|replaceTraceConfig, clearTrace, captureStart/Stop, sinkOpen/Flush/Close|paused/running/faulted; atomic owner boundary; starting capture in faulted => STATE|
|detach/releaseControl/acquireControl|attached, no CPU progress|
|terminate|baseline permission; closes session only after requested sinks are closed or visibly failed|

advance result counts actual cycles, legacy instruction delta and retired delta;
cycle is absolute and safeBoundary describes return. partial=true means progress
is less than requested budget or safe boundary not achieved; never pretend all
budget elapsed. Stop priority is existing arbiter: architectural fault/exception,
unsupported power-down, CODE breakpoint, lowest-ID watch stop, pause/step, budget. Watch
stops apply only after source routing, derived matches and after-gates complete
and CPU reaches safe boundary; completed architectural effects are preserved.
Native stop_reason uses 0 budget,1 pause,2 breakpoint,3 watchpoint,4 fault,
5 power-down-unsupported,6 terminated. Native state uses CPU API1 constants.
If progress occurs then fault/boundary drain fails, advance returns OK with
state=faulted, reason=fault, actual partial progress; details are canonical fault
records. Validation errors do not progress. Native cycles/instructions are
per-call deltas, retired_instructions is also a delta. Budget expiry alone leaves
running state; user must pause explicitly. step's inherited maxCycles cap is not
permission for unbounded execution in external mode.

Mandatory capabilities: attachSession,status,control,codeBreakpoints,decodeCode.
`advance` requires externally-driven execution. Optional capabilities:
memoryRead; boardInput; boardInspect (+ boardInspectFault dependency);
observations; uartOutcomes; traceConfig; tracePage; captureA; captureB; traceFile.
traceConfig/tracePage/captureA require observations; captureB requires captureA
and instruction/control/IRQ coverage; uartOutcomes requires observations and
input/UART/lifecycle coverage; traceFile requires traceConfig+tracePage.
Hello advertises only implementation-tested capabilities; this artifact supplies
none. Unknown required capabilities return UNSUPPORTED; known capabilities not
available also return UNSUPPORTED. `requiredCapabilities` accepts at most32 unique bounded identifier strings;
an unrecognized name is handled by semantic negotiation
UNSUPPORTED rather than malformed-frame close. No consumer can assume all
extension classes from a valid 3.0 handshake.

boardInput returns parentStimulusId for accepted scheduling only, in addition to
its existing response. Delivery/frame outcome requires outcomePage. Raw bytes,
external serial sockets, parity interpretation and coordination of another
simulator's clock remain outside this contract. There is exactly one CPU
execution owner regardless of connected clients.

stopDetail is nonmutating and returns stopRevision,generation,reason,
sourceSequence,instructionId and all matched stopping watch IDs with their
canonical watch event sequence and source sequence (max64, ascending watch ID).
It is independent of trace retention and remains until the next applied stop;
revision0 has sourceSequence0,instructionId0 and no matches. Snapshot reason
cycle-budget maps to detail/advance budget; snapshot exception maps to fault.
The first source that requested a stop owns sourceSequence; every simultaneous
match at that boundary is listed. Any later fault wins reason while preserving
the triggering watch evidence. No fabricated source event for a manual pause.

stopDetail additionally preserves entry and step reasons (startup/reset and
single-step completion), unlike advance which cannot produce those outcomes.
Initial/reset entry has the actual nonzero stopRevision from the existing
controller; revision0 is only the no-stop sentinel.
