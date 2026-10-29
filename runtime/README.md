# Local runtime script1 and persistent control3

`emu-runtime` owns one CPU/session and one independently built native board.
Build offline from a pinned clean source checkout/archive with `make emu-runtime`.
Dependencies are a C99 compiler, GNU make, Linux x86-64 libc/libdl, Python3 for
build identity, and the included source. `make install-runtime DESTDIR=...`
installs the executable, CPU/session static libraries and these product docs.
This runtime implements native API1 session create/destroy/pump and debug
open/request/close, alongside CPU API1. Immutable SDK0.2.0 headers retain their
historical publication labels (`TARGET` and `EMUIF_TARGET_IMPLEMENTED=0`);
those frozen labels describe the bundle's publication, not this later runtime.
The exact runtime source pin shown by `--version`, identity and capability hello
is the implementation authority. The explicit Unix frontend implements control3
wire3.0 with SDK0.3.0; the existing script frontend retains wire2.1/SDK0.2.0.
The frozen target headers keep their historical availability markers. Query
`emusa80535_control_query_v3` or the live wire hello to discover implementation.
DAP attach is outside this host.

The source archive must carry its exact `SOURCE_COMMIT` (the repository's
`packaging/make_source.py` generates it). Record and verify the archive SHA256
before unpacking. No external board implementation is included.

```
emu-runtime --variant sab80535 --clock-hz 11059200 --seed 0 --entry 0 \
  --image /absolute/image.bin --image-sha256 LOWERCASE_HEX64 \
  --board /absolute/module.so --board-sha256 LOWERCASE_HEX64 \
  --board-config /absolute/config.json --script /absolute/commands.ndjson \
  --board-inspect-fault
```

All common valued arguments are required, unique and case sensitive. Select
either `--script` or the pair `--control-unix` and `--execution-mode`. `--help` and
`--version` work alone without opening any input. Numeric arguments are decimal.
Image size is exactly65536 bytes. Configuration is a nonempty UTF-8 JSON object,
maximum16384 bytes; use `{}` for empty configuration. Identity hashes the exact
image/config bytes, including config whitespace. Module size is bounded to64MiB.
All file paths must be absolute regular files. The loader copies module bytes
into a private sealed Linux memfd, verifies SHA256, and dlopens those exact bytes
with local scope. `$ORIGIN`-relative module dependencies are unsupported; use a
self-contained module or system shared-library dependencies. ELF constructors
are trusted native code and can run during loading. Descriptor/ABI/callback and
required CPU-capability checks happen before session creation/execution.
Board ABI1.0 modules built against immutable SDK0.1.0 remain compatible.

`--board-inspect-fault` is optional, default off. It asserts that the integrator
has reviewed the exact pinned module's promise of pure bounded post-fault
inspection. The module must advertise INSPECT. The host records this policy in
identity and enables `boardInspectFault` before opening its local controller.
This is host policy, not a new ABI bit or an inferred capability.

The script is LF-terminated NDJSON. First line is exactly the object
`{"format":"emu-runtime-script","version":1}` (key order/spacing may vary).
Remaining lines are at most256 command objects; each line including LF is at
most65536 bytes, total file at most8MiB. No blank lines, comments, BOM, CR,
duplicate keys, unknown fields, malformed UTF-8 or noncanonical uint64 strings.
The complete file and static ranges are validated before loading any module or
executing any command. JSON bounds: depth16,8192 values,256 object members and
1024 array elements per record. Board command depth is limited to15 because the
native wire arguments wrapper adds one level. Payload/request object compact JSON is <=4096
bytes. Board-owned semantics are validated by the session at admission time.

Supported operations (exact shapes in `script1.schema.json`):

- `{"op":"state"}` returns current snapshot.
- `{"op":"run","cycles":100}` resumes, pumps1..100000 machine cycles and
  pauses only if still running. Pump safe drain can add at most16 cycles.
- `{"op":"pause"}` pauses coherently; already paused succeeds without time.
- `{"op":"step","maxCycles":100}` executes one instruction, or bounded
  IDLE/stop under API1, while paused; safe drain can add at most16 cycles.
- `{"op":"reset","seed":0,"entry":0}` restores original image and advances
  generation, including explicit recovery from fault.
- `{"op":"boardInput","schema":"synthetic.inputs.v1","cycle":null,"payload":{}}`
  admits an opaque stimulus; null resolves current+1, otherwise cycle is a
  canonical uint64 decimal string strictly in the future. Returns resolved order.
- `{"op":"boardInspect","schema":"synthetic.status.v1","request":{}}`
  performs opaque pure inspection; after fault requires the explicit promise.
- `{"op":"replaceCodeBreakpoints","addresses":[0,32]}` atomically replaces
  up to1024 unique uint16 CODE addresses while paused.

Stdout contains one `identity` followed by one `result` per operation, no other
records. Each record including LF is at most65536 bytes. `output1.schema.json`
is authoritative. `identity.runtime` is the exact wire2.1 hello body, including
runtime source commit, immutable bundle manifest digest, capabilities and limits.
`identity.module` records descriptor id/version/buildId, verified artifact digest
and `inspectFaultPromise`. Top-level variant/clockHz/seed/entry and actual image/
config hashes record startup choices. `snapshot` is the exact wire2.1 status body.

Every result has zero-based `index`, `op`, boolean `success` and current `snapshot`.
Successful `body` is the corresponding wire2.1 command body (state uses status;
boardInspect's script `request` maps to wire `payload`). Run instead returns
`{"cycles":"N","instructions":"N","outcome":"budget"|"stop"}`.
Every run result additionally has `progress:{"cycles":"N","instructions":"N"}`,
including on failure. `budget` means local pump completion; it is not firmware
halt. The final snapshot may have reason `pause` due to the host's explicit pause.
Earlier breakpoint/architectural/fault stops are preserved. Failed results have
`error:{"code":"STATUS","message":"text","retryable":false}` (BUSY may be
retryable). Counters/generations/cycles are decimal strings; uint32 and smaller
values are JSON numbers. Run partial fault progress is retained even when no
instruction completed. Fault snapshots remain inspectable and may be unsafe.

Faults do not terminate script processing: state, permitted board inspection and
reset remain available; execution without reset fails. Exit0 means all operations
succeeded; exit2 means startup/static validation failed before CPU execution and
stdout is empty; exit3 means an operation/runtime/output failure, even after
recovery. Diagnostics use stderr. Native crashes are OS failures, not typed
results. Script1 has no retries, DAP attach, physical I/O or wall-clock pacing.

For replay compare architectural snapshot/time/module evidence, retaining both
identity records. Session IDs vary between processes; lifetime admission/access
IDs survive reset and are not architectural reset values. Identical initial
seed/image/config/commands are deterministic subject to the module contract.

The native session library admits at most8 concurrent clients and8192 lifetime
connection IDs. IDs cannot be reused; lifetime exhaustion returns CAPACITY before
mutation. The script host uses only one connection. Host tests additionally need
Python `jsonschema` and Git history for the immutable0.1 fixture; an unpacked
verified SDK0.1 include tree can instead be supplied as `EMUIF_SDK01_INCLUDE`.
Run `make runtime-host-test runtime-control-test` after building. No test dependency is needed to run
the installed executable.

Native consumers: authorized terminate after an unsafe partial fault applies
termination and closes clients, returning native OK with written=required=0.
Wire2.1 cannot represent a terminated unsafe snapshot, so no success acknowledgement
is fabricated. Zero response here means connection closure; ordinary safe
termination returns its conforming acknowledgement. Script1 has no terminate
operation and is unaffected by this native lifecycle case.

## Persistent Unix control3

Use the same required pinned image/module/configuration arguments, replacing
`--script` with `--control-unix /absolute/private/control.sock --execution-mode
externally-driven` (or `host-driven`). The parent directory must already exist,
be owned by the effective UID and have mode0700. The absolute UTF-8 path is at
most107 bytes. Ancestor symlinks and `.`/`..` components are rejected, as are
preexisting endpoint files, sockets or symlinks. The host pins the directory
before loading the module, creates a mode0600 stream socket and authenticates
each peer with Linux SO_PEERCRED before accepting input. Same-UID clients may
request controller access and termination. No listener exists in script mode.

The listener speaks only wire3.0: strict UTF-8 JSON objects followed by LF,
maximum65536 bytes including LF. It produces responses only on the socket and
never writes unsolicited protocol records or identity to stdout. The immutable
`interfaces/0.3.0/docs/03-control.md` and schema define exact request/response
shapes. Begin with hello(protocol3.0, contractVersion0.3.0), then attach with
sessionId:null and observer/controller role. Attach selects the existing session;
it never resets or reloads the CPU. Every mutation requires the current session,
generation and nonzero controller lease. A single controller is shared with the
native debug facade. Release, detach or disconnect relinquishes the lease;
acquisition grants a new lease. Reconnect requires fresh hello/attach and state
queries: requests may have executed before a lost response, so do not replay
mutations automatically.

In externally-driven mode, only explicit step or advance changes virtual time.
Resume changes the session to running without progress. Advance requires running
state and a budget1..100000 machine cycles; the session may use up to16 additional
safe-drain cycles. Its result reports actual cycles, legacy instructions and
completed instructions separately, including partial fault progress. Step retains
its bounded one-instruction semantics. In host-driven mode the sole owner pumps
up to64 cycles plus bounded drain between control iterations while running;
advance is unavailable. Paused and faulted sessions remain responsive without
pumping. No wall-clock synchronization or physical I/O is implied.

Available commands include status/control, CODE breakpoint replacement, decode,
pure memory reads, stopDetail and board admission/inspection when advertised.
SAB80535 sessions using the native producer CPU also advertise `observations`
and `uartOutcomes`. Their observation coverage is instruction, control, IRQ,
lifecycle, input and UART (native bitmask125). Instruction retirement follows
all successful timing cycles; failed attempts remain distinguishable from
completed instructions. The native observation binding currently rejects other
CPU variants. Explicit MOVX and SBUF access facts are produced, but implicit
register, stack and peripheral accesses are not completely instrumented, so
full `access` coverage is not advertised. Trace configuration, trace paging,
persistent capture A, invocation capture B and bounded file sinks are available
on the canonical SAB80535 binding. File sinks require the optional startup
mappings below.

`outcomePage` and the native trace table's `outcomes` function read the same
fixed1024-record operational ring, independently of user tracing. It retains
input and UART events plus explicit loss records. Reads are nondestructive;
use the session ID, epoch and storage record sequence as the cursor, with at
most16 records per page. Overwritten records, expired/ahead cursors and wrong
session/epoch are explicit. Storage sequence differs from the canonical event
sequence, which also orders instruction and other execution facts. Native
queries validate sized records and require initialized output record prefixes.

Board admission, CPU scheduling/delivery and UART receive acceptance are
separate outcomes. A board stimulus has its own parent identity; each resulting
CPU input has its own queue order. Autonomous board scheduling has parent0.
RX/TX frame IDs retain their correlations through start, acceptance/drop,
holding replacement and completion. UART values preserve the ninth bit;
accepted nine-bit receive data uses a16-bit observed value. TI is a flag edge,
not full transmission completion. REN-clear or busy delivery preserves the
existing STATE/BUSY result and required-input session-fault policy; late RI/SM2
filtering is a UART drop rather than a failed delivery.

Reset cancels queued inputs and ends active/held UART frames before the new
generation. It preserves operational history and the ring epoch; identities
and canonical/storage sequences are not reused. Existing accepted runtime and
SDK artifacts remain immutable. The live hello carries the immutable0.3
manifest digest, actual runtime source pin, execution mode, bounds and only
implemented capabilities. `--version` also identifies the unchanged script1
compatibility profile. `make outcome-test runtime-outcome-test` verifies native
paging and actual Unix UART outcomes; the latter also supports `RUNTIME_BIN`
and `SDK_INCLUDE` overrides for checking an installed runtime.

One nonblocking owner loop serializes every request and CPU operation. At most8
clients are admitted. The session retains at most8192 lifetime connection IDs;
after exhaustion new connections are closed before any input or mutation. Each
client has one outstanding request and one output frame;
reads stop until the response has been written. Pipelined bytes remain in the
bounded kernel stream buffers until admission, so the admitted request count is
at most8 (within the protocol maximum32). Partial reads/writes are supported.
Incomplete hello/frames and incomplete output expire after5000ms measured from
the start, without extending the deadline for trickle traffic. Idle established
connections do not expire or stop the session. Slow readers cannot block other
clients or the owner. SIGINT/SIGTERM request orderly host shutdown; they never
become emulated interrupts. Terminate or shutdown removes only the created
socket inode. Replacing that path does not authorize deleting the replacement.
Native callback hangs/crashes remain host failures; control cannot interrupt a
hung trusted module.


Persistent trace files are opt-in startup mappings:

```sh
mkdir -m 700 /absolute/private/capture-directory
emu-runtime ... --trace-file trace=/absolute/private/capture-directory/run
```

Repeat `--trace-file TOKEN=/absolute/private/stem` at most8 times. Tokens contain
1..64 ASCII letters, digits, dots, underscores or hyphens; wire/native `sinkOpen`
receives only the token. The parent directory must already exist, belong to the
host UID and have mode0700. Ancestor symlinks and dot/dotdot path components are
rejected. The host pins that directory before loading a module. Segments use
`stem.0000.jsonl` through `stem.0015.jsonl`, exclusive creation and mode0600;
existing files, symlinks and FIFOs are never overwritten or followed. Use a new
registered stem when reopening after a successful capture. Mappings cannot be
changed through the control connection, and are unavailable in script1 mode.

File capability is advertised only when a provider is registered. Configure a
file destination with capacity256, then call `sinkOpen` with its token and
bounds. Open returns a pending operation before the worker opens the file or
writes its header. Poll `sinkStatus` until its completed operation ID catches up;
state1 is pending, state0 is ready, and state4 reports failure. An existing-file
error therefore appears asynchronously as IO/partialOutput with no truncation.
Preheader views count as loss. Every segment repeats the opening configuration,
its canonical SHA256 and immutable runtime, contract, image, module and board
configuration identity. See [the frozen file contract](../interfaces/0.3.0/docs/05-files.md)
for exact JSONL fields, limits and semantics.

A host worker writes records outside the CPU/session owner. Its bounded256-record
queue drops excess incoming views with explicit count/source-range loss records;
virtual CPU timing does not wait for disk. Flush snapshots accepted records and
pending loss at the request boundary, then fsyncs that prefix. Later records
cannot delay completion. Close additionally stops admission, writes a footer,
fsyncs and closes. Only one operation per destination may be pending. Segment
and total-record limits close with a complete CAPACITY footer; storage failure
disables only the destination and reports partial output. Neither case invents
a CPU hardware fault. Eight sinks may be active, with64 destination identities
retained for the session. Reopen preserves storage/operation identity sequences
and resets per-open counters; disconnect and CPU reset preserve open files.
Configuration replacement is BUSY while any sink is active.

Terminate returns BUSY while asynchronous closes remain pending. SIGINT/SIGTERM
request closure and allow at most2000ms for host sink cleanup. Failed output or a
blocked writer produces a partial-output diagnostic and nonzero exit; no claim
of a complete footer is made. Missing/truncated footers remain evidence of
partial capture. `make file-sink-test runtime-trace-file-test` checks deterministic
queue/barrier/error paths and actual Unix commands/JSONL against the frozen
schema. `RUNTIME_BIN` and `SDK_INCLUDE` select installed artifacts for the latter;
`FILE_SINK_REPORT` and `TRACE_FILE_REPORT` retain validation evidence.


`tracePage` reads ring destinations only; files return UNSUPPORTED, and clearing
a file destination returns INVALID. Coverage125 has incomplete ACCESS
instrumentation: explicit XDATA reads/writes and SFR99 writes/RMW can be selected
or watched, but broad access completeness is not advertised. Access predicates
must name a supported space/range/mask. Broad selectors (matchMask0 or PC-only)
can include available facts without asserting completeness. Instruction-begin
facts can contain only the fetched opcode and empty text. Retirement and abort
retain all bytes actually fetched by that attempt, which may still be an
incomplete instruction prefix: an untaken conditional path can omit an operand
fetch. Disassembly text remains empty when the captured prefix is insufficient.
Static decoding of the image may be shown separately as enrichment; it must not
replace captured execution evidence. A failed MOVX attempt never becomes a
retired instruction. See [execution research and recovery](RESEARCH.md) for
state captures, invocation probes and UART value-validity rules.

Capture A persists across calls, pauses, disconnects and resets, retaining
cumulative cycles and reporting the current generation. Its maxCycles interval
is half-open: `[start, start+limit)` excludes the exact deadline event. The view
that reaches maxRecords is included; capture counts precede sink delivery and
therefore include dropped views. A capture limit with stopCpu pauses at a budget
stop; ring stopOnFull pauses with pause reason. Simultaneous stop precedence is
fault, power-down, CODE breakpoint, watch, trace, then step. A silent IDLE
deadline has zero event/instruction identity; an event cutoff retains the actual
identity. Terminate pauses execution before asynchronous BUSY, and resume, step
or advance are refused until termination completes.


Invocation capture B is available with instruction/control/IRQ coverage. Mode1
arms for the next fully observed successful CALL to entryPc; binding occurs only
after CALL retirement. Its startSequence is that retirement fact, although the
staged CALL event precedes it in output. Armed time does not count toward its
cycle limit. Mode2 binds only a fully observed, already retired entry whose CALL
context is the current top frame; a CODE breakpoint at entry supports this mode.
Its startSequence is the latest canonical source at the explicit bind. A matching
PC or stack pointer alone never establishes context. No new architectural event
is invented for either command.

CALL, IRQ, RET and RETI context tracking runs with tracing disabled or filtered.
Nested/reentrant calls and IRQ work belong to the root extent. A successful root
RET and its retirement are included; caller continuation is excluded. Mismatch,
unknown ancestry, stack overflow, failed retirement, reset/load and limits close
B incomplete. A newly fully observed child above unknown ancestry can be captured
with parent0; returning it never establishes the older unknown parent. Mode2
rejects unobserved, provisional, IRQ and non-top contexts.

Next-call output is staged per affected destination for at most256 records,
including A/other traces sharing it. Original route sets, gates, counts and source
ordering are retained. Successful binding commits each coalesced view once;
failed CALL removes only candidate B routes and commits other traces once.
Storage IDs are allocated at commit. Candidate overflow closes incomplete before
binding. Provisional B record limits are applied at original routing time, so
maxRecords1 includes only the CALL B view: successful retirement binds and closes
incomplete/limit, while other traces still retain their own retirement views.
Failed provisional suppression is discarded only for the affected B trace.
Flush/close and configuration mutation return BUSY during an unresolved stage.

Reset ends old B status in its old generation. Explicit mode1 rearmOnReset creates
a new armed captureId and exposes successorCaptureId; allocation exhaustion is
rearmStatus EXHAUSTED without CPU fault or implicit retry. There are64 retained
capture statuses, never silently discarded. With stopCpu, successful return or an incomplete B terminal requests a safe
budget/paused stop; captureStatus distinguishes the cause. Actual faults,
power-down, CODE breaks and watch stops retain their higher priority. Reset
retains entry/paused and consumes any pending scope stop.
Capture B completion does not change trace enabled state; its terminal route filter stays until another capture starts.
A/configuration/file sinks and independent UART outcome history remain persistent.

`make runtime-invocation-test` exercises actual CPU/Unix capture and shared A+B
JSONL. RUNTIME_BIN/SDK_INCLUDE select an installed executable and installed-header
board fixture; INVOCATION_REPORT retains all validated responses and file rows.

Explicit wire 3.1 CODE breakpoint enumeration
------------------------------------------

A new connection may explicitly send hello protocol `{major:3,minor:1}` with
contractVersion `0.4.0` and required capability `readCodeBreakpoints`. The same
Unix endpoint and native `emusa80535_control_query_v3` JSON table support this
opt-in minor profile. There is no automatic version probe or fallback. Wire 3.0
clients retain their exact 0.3 contract, capability set and command recognition;
old endpoints reject a 3.1 hello with VERSION/close. No extra startup flag is
required; the new hello is the explicit opt-in.

`readCodeBreakpoints` takes exactly `{sessionId}` and returns exactly
`{sessionId,generation,addresses}`. Addresses are the full authoritative CODE
bitmap in ascending unique integer order 0..65535, at most 1024 entries. Empty is
`[]`; this is not a list of watches or tracepoints. The single execution owner
snapshots the identity, current generation and bitmap together. Attached observers
and controllers may read paused/running/faulted sessions without a lease or
expected generation. Terminated sessions are not readable. Wrong session is
STALE; unattached is STATE; malformed arguments follow INVALID/close framing.

Enumeration does not execute CPU cycles, alter stopRevision, stop state, lease,
breakpoint suppression or trace state. Reset retains the bitmap and changes the
reported generation. There is no breakpoint revision or compare-and-swap token:
hold the exclusive controller lease throughout save, temporary replacement, run
and restoration to exclude another controller. A lost lease/connection or
uncertain mutation result provides no automatic restoration/replay guarantee.

`interfaces/0.4.0` is a wire-only sidecar, installed alongside the frozen 0.3 SDK.
The new hello's manifest hash identifies that sidecar and its pinned 0.3 base.
CPU API 1, board ABI 1, native observation/trace tables and public headers remain
unchanged. Trace file headers continue to identify 0.3 and its base manifest even
when a 3.1 client opens the sink: trace files belong to the shared session's frozen
trace contract, not to a connection's negotiated control minor version.

`make runtime-control31-test` verifies actual Unix responses and unchanged 0.3
file metadata; `make -C tests control31-test` exercises the native JSON table.
RUNTIME_BIN/SDK_INCLUDE choose an installed binary/header fixture and
CONTROL31_REPORT retains actual request/response and JSONL evidence.

`make interface-control31-check` verifies the sidecar. Once frozen, the external
`interfaces/control31.lock.json` pins its source, manifest and deterministic
archive; check/export/install use that lock even when runtime HEAD advances.
Before freeze, candidate exports use the explicit runtime source commit.
