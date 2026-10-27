# Runtime control protocol 2.1

Target protocol; current emu-debug implements only legacy stdio1.0. This protocol
is used by external debugger clients (including DAP's client library) over Unix
SOCK_STREAM or TCP. It is not DAP itself. Native records/pointers are never sent.

## Framing, negotiation and bounds

Each frame is one UTF-8 JSON object followed by LF, at most65536 bytes including
LF. Reject BOM, CR, duplicate keys, invalid UTF-8, embedded raw newlines, NaN,
Infinity and unknown properties. Object key order is irrelevant. Requests,
responses and each command body follow [the schema](../schemas/runtime-2.1.schema.json).
Integers up to uint32 are JSON numbers; **all uint64 values are canonical decimal
strings** (`0` or no-leading-zero digits, maximum18446744073709551615). Negative,
fractional and over-range forms are invalid. No JSON safe-integer loss is allowed.

First request must be hello for major2, minor1 and exact contract `0.2.0`.
Mismatch returns VERSION and closes. Mandatory capabilities are `attachSession`,
`status`, `control`, `codeBreakpoints`, `decodeCode`; optional capabilities are
`memoryRead`, `traceRead`, `boardInput`, `boardInspect` and `boardInspectFault`, advertised only when implemented and verified.
Unknown required capabilities fail UNSUPPORTED. Hello is side-effect free.
The response includes versions, runtime source commit, contract manifest hash,
all fixed numeric limits and available capabilities. Product version is diagnostic,
not compatibility authority. Baseline2.1 never negotiates silently down to1.0.

The client sends at most one outstanding request per connection, IDs strictly
increase from1. Host admits at most32 complete pending requests across <=8
connections. It serializes requests at safe boundaries in admitted queue order.
Full queue returns BUSY without mutation; per-client output retention is at most
one complete frame. A slow writer is disconnected after5 seconds of blocked
output. Incomplete hello/frame times out after5 seconds measured by host time;
this closes the connection, does not advance virtual time. No unsolicited events:
a running client polls status (recommended10Hz, at most one pending request).

Malformed frame with a trustworthy valid ID **and a recognized command** gets
INVALID then close. If either cannot be recovered, including an unknown command
string, close without response; do not invent an error envelope outside the
recognized-command schema. Native module failure is FAULT and leaves session faulted.
A request with valid schema but illegal state/role returns a correlated error
without disconnect. Error codes are the status names in [common rules](01-overview.md),
with message<=256 UTF-8 bytes and retryable false except BUSY. Client timeout
never proves a stop and must not kill an attached host.

## Commands

All successful responses echo id and command, success=true and the exact body.
Errors echo id/command, success=false and error. No command runs before hello.
attach is required before every other command. Requests reference expected
session identity; mutation requests additionally require generation. Wrong
identity/generation returns STALE before mutation (tracePage instead returns
its explicit cursor state for wrong session/epoch). `status` omits generation so
an observer can discover a reset. Detach preserves state and permits reattach
on the same connection. Connection IDs/authorization remain host-private.

| Command | Arguments and semantics |
|---|---|
| hello | Version/capability negotiation only. |
| attach | sessionId or null discovers the sole session; role observer/controller. Return snapshot/identities. Controller lease exclusive; BUSY if held, DENIED if unauthorized. No load/reset/pause. |
| status | sessionId; return current safe snapshot, module/image identity and stop revision. |
| pause | sessionId,generation; controller; running→paused at safe boundary, already paused is success. Return applied snapshot. |
| resume | same; paused→running, already running success; faulted/unsupported sleep rejects STATE. Response proves transition, not subsequent progress. |
| step | same plus maxCycles1..100000; paused only; execute up to one complete instruction or stop/budget plus safe drain. Remain paused. IDLE bounded by cycle budget. |
| reset | same plus seed uint32 and entry uint16; paused/faulted only; coherent reset/restore image, generation++. |
| replaceCodeBreakpoints | same plus <=1024 unique uint16 addresses; paused only; atomically replace whole set, reject invalid whole request. Stop before executing addressed instruction. |
| decodeCode | sessionId,generation,address uint16,byteOffset int32,instructionOffset int32,count1..256; paused only; exact predecessor/placeholder/no-wrap rules inherited from protocol1.0. |
| readMemory | optional memoryRead; same identity plus space,address,count0..256; paused only; side-effect-free whole range or UNSUPPORTED, never device read. |
| tracePage | optional traceRead; sessionId,destinationId,ringEpoch,afterRecordSequence,maxRecords0..128; non-destructive, no generation requirement; current or explicitly expired cursor metadata. |
| boardInput | optional boardInput; controller; sessionId,generation,cycle uint64 or null,schema,payload JSON object; admit deterministic board stimulus, return assigned cycle/order. Valid paused or running; apply only when time advances. |
| boardInspect | optional boardInspect; sessionId,generation,schema,payload; paused only by default; with boardInspectFault also nonexecuting faulted, including unsafe CPU boundary; return pure bounded module JSON inspection. |
| detach | sessionId; release lease, remain in current execution state; attached connection becomes negotiated. |
| terminate | sessionId,generation; controller AND may_terminate; acknowledge final terminated snapshot if possible, host then closes all clients and destroys session. |

Breakpoint/watch exception precedence follows the frozen model. Resume or step from a
pre-execution CODE breakpoint suppresses that same PC breakpoint once until one
instruction completes; it does not remove the breakpoint or suppress other
stops. Stop revision is a monotonic uint64 incremented on every new applied stop;
status polls with unchanged revision do not create duplicate DAP stopped events.
Reset increments revision; resume leaves it unchanged. Snapshot `reason` is the
last applied stop while running; state is authoritative. `safeBoundary=false`
only with faulted state. Atomic snapshots are not collections of independent reads.

## Transport authorization

Unix endpoint lives in a private0700 per-user runtime directory, socket0600;
validate directory ownership and peer UID before debug_open. Reject symlink
endpoint replacement; remove only the socket this host created. TCP is opt-in.
Profile2.1 TCP requires mutually authenticated TLS1.3 with explicit trusted
client identities, including loopback TCP. Loopback alone does not authenticate
a local account. An authenticated tunnel may instead forward to the protected
Unix endpoint, where peer UID/permissions remain enforced. A TCP tunnel does not
remove the TCP mTLS requirement in this profile. Never expose an unauthenticated
wildcard or loopback control listener.
Role and termination privilege are server configuration, not request claims.
Transport security precedes hello and does not alter simulation time. No password
or token fields are part of this protocol. Endpoint advertisement contains no
credentials. Diagnostics never share the NDJSON stream.

Trace record eventKind strings are fixed by schema, independent of native enum
numbers. `width` is bytes; old/new unknown values must be `"0"` with Known=false.
Summary records use eventKind=none, space=NONE, address/width/pc=0 and unknown
values; source range and text describe the summary. Event records have equal
sourceFirstSequence/sourceLastSequence; summaries may span a source range.
Text is diagnostic, never a command. Empty rings use zero oldest/newest/first/last.
`remaining` counts records after the returned page, `more` is remaining>0.
For wrong-session/epoch/ahead, records is empty and next cursor stays requested;
metadata names current session/epoch. For an expired cursor, exact lostBeforePage
is oldest-1-requested; zero for current or unmeasurable cross-identity cursors.
Each record is returned at most once per exclusive cursor traversal.

Snapshot controlRole/canTerminate report the connection's attached role and
host-granted termination privilege, so DAP capability advertisement is based on
server authority. All successful commands returning snapshots use those same
connection-specific permission fields. They never grant additional privileges.

Additional parser bounds: maximum JSON nesting16 (root depth1), at most8192
JSON values per frame, object members<=256 and arrays<=1024. Text limits in this
contract are **UTF-8 byte limits**, even when JSON Schema maxLength provides
only a character bound. Validate frame size, nesting, byte lengths and uint64
format in addition to the schema. Module payload serialized compactly must fit
4096 bytes. Error messages and trace text fit256 bytes; decoded text fits96.

A trace page returns fewer complete records if the65536-byte frame bound would
be exceeded, even if maxRecords is larger. more/remaining/next cursor reflect
that shorter page. Never split a record. If one eligible record cannot fit,
return CAPACITY without consuming/changing any cursor; implementations must
bound retained records so this cannot occur for conforming baseline records.

`boardInspectFault` is a wire2.1 optional capability and requires `boardInspect`.
It permits only the same pure, bounded module inspection while nonexecuting and
faulted, including an unsafe CPU boundary. It does not authorize CPU memory,
decode, execution, mutation or replay after fault. Modules must explicitly support
post-fault inspection and retain relevant last-access/status state. All other
fault restrictions remain unchanged. This bundle specifies but does not implement
a wire service or this optional capability.
