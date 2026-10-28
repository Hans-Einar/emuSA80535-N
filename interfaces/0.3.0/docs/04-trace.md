# Trace facade, pages and invocation capture

This is a sized projection of the existing matcher/router, not a new matching
engine. Baseline [debug model](../../0.2.0/reference/debug-model.md) supplies
point ordering, watch actions, gates, interrupt suppression and destination
coalescing. This profile supports the already implemented old/new scalar
conditions; checked delta expressions and the broader expression grammar are
not in traceConfig1 and must return UNSUPPORTED via a later negotiated profile.
Raw same-build em8051_debug_* structs/offset rings are not this ABI.

## Exact configuration

replaceTraceConfig atomically replaces the entire config at expectedRevision;
stale revision => STALE, including when generation is current. Revision starts0,
increments on accepted replacement, never wraps. Maxima:16 traces,8 destinations,
32 points,32 gates,64 watches,8 actions/watch,16 routes. Count/byte limits apply
before copying; canonical compact UTF-8 config <=16384 bytes. Native arrays use
caller pointers/stride/count; element_size >= sizeof corresponding element;
zero count permits NULL, nonzero does not. Every element prefix is validated.
Arrays and route IDs must be strictly ascending by object ID; duplicates,
dangling destinations/routes and unsupported selectors fail atomically. Trace,
watch,point,gate,destination IDs are separate uint32 namespaces. IDs retired by
deletion cannot be reused until new session. At most64 different IDs per each
namespace per session; exhaustion => EXHAUSTED. Replace preserves counters,
ring contents, enable state supplied in replacement, and capture binding for
unchanged trace IDs; removing a bound trace closes capture as explicit stopped.
Referenced destinations cannot be removed unless all references are removed in
that same transaction. File destination currently open cannot be removed: BUSY.

Selectors have matchMask bits1 kind,2 inclusive PC range,4 inclusive address
range,8 space,16 access mask. Unselected fields must be zero. Selected kind is
1..28; mask0 matches every eligible source event, kind0 is only the unused
sentinel. Address match requires an access event with known address; PC requires
known bit1. Access mask bits use OBS_ACCESS constants, not CPU API1 direction.
Address space uses CPU API1 space values. Selected range first<=last. Watch
access mask nonzero; watch space selects CODE/IRAM/SFR/XDATA; only successful
known access results are eligible for value comparisons. Attempt/failure facts
remain routable by ordinary points but cannot fabricate watch completion.

A condition is present=false (all other fields zero except width8), or compares
old0/new1 against constant with eq0/ne1/lt2/le3/gt4/ge5. Width1/8/16/32/64 must
match captured operand; signedness must match. Constants are width-bit two's
complement and fit width. Unknown operands evaluate false. Action kind stop0,
console1,quiet2,route3; only route uses traceIds, other route lists empty.
Console means bounded host interactive diagnostics; it is suppressed in protocol
mode, never protocol stdout. quiet wins over console. firedActions is a bitmask
of action indexes. Derived watch events cannot re-match points/watches/gates.

Every predicate sees pre-event state. Gates sorted by ID: on0/off1, before0/
after1. Before gates control source and derived events, after gates run last.
Source fan-out then ascending watch IDs then after gates then safe-stop arbiter.
Trace IDs coalesce per destination and remain sorted; no duplicate record per
route to the same destination. Interrupt policy0 include,1 suppress-between-IRQ
boundaries,2 IRQ-only. Entry routes at old depth, exit at current depth, then
depth changes; outer enter/exit retained under both restricted policies.
Only otherwise-eligible events count as suppressed; per-trace summary precedes
its next retained event or flush/close, with first/last source sequence,count,
entry/max depth. Suppression is not loss. All ordering is deterministic.

Destination kind0 ring,1 file. Ring capacity2..4096; capacity0 is intentionally
not supported by this profile—disable trace routes using enabled=false instead.
File capacity must be256 (host queue), stopOnFull=false; no extra file path in
config. Ring stopOnFull=false overwrites oldest; true preserves existing records,
drops incoming records with explicit loss and requests a safe-boundary stop.
No source callback performs file I/O. Enabling/disabling traces uses atomic
replacement retaining all other config. clearTrace changes epoch, empties ring,
resets per-epoch record sequence/loss to0, preserves config/counters elsewhere;
file or nonexistent destination => INVALID/NOT_FOUND. Pending suppression is
flushed before clear and then discarded with the old epoch; no old summary is
carried into the new epoch. Outcomes ring cannot be explicitly cleared.

## Stable paging and record identities

Each destination has epoch>=1 and a monotonic storage recordSequence>=1. Every
inserted ordinary event, suppression, loss or sink status receives a distinct
storage sequence. Storage identity is (sessionId,destinationId,epoch,recordSequence);
canonical identity is (sessionId,generation,event.sequence), never storage
identity. Non-event records have generation and source range, no invented event.
Reset/load preserve epochs/storage sequence and history. Explicit ring clear
increments epoch and restarts storage sequence. New session creates fresh IDs.
Sequence exhaustion rejects insertion and stops capture incomplete, never wraps.

tracePage includes destinationId; outcomePage implicitly selects0. Page limit
0..16; each encoded record <=3072 bytes and whole frame<=65536. Native max_records
and capacity limit the output; required is min(available,max_records), written
is number copied; remaining counts records after the last returned cursor.
capacity<required returns CAPACITY with metadata, zero records. Metadata-only
max_records=0 copies none, nextAfter=requestedAfter, more=(remaining>0).
Empty epoch oldest/newest/first/last=0. nextAfter is last returned or requested
cursor when empty. Reads are nondestructive. Header page `written`/`required`
are native buffer metadata; wire written is records.length and has no separate
capacity request. Same-epoch after<oldest-1 => expired, returns oldest page,
lostBeforePage=oldest-after-1. Cursor0 means before first ever record, so it also
reports expired after overwrite. after>newest => ahead, no records. Wrong session
or epoch => respective cursorState, no records. Those three invalid cursors use
remaining=0,more=false,nextAfter=requestedAfter,lostBeforePage=0; current metadata
still reports actual session/destination/epoch/oldest/newest. Unknown destination
=> NOT_FOUND. Cumulative overwritten saturates uint64, exact lostBeforePage does
not saturate. Loss-marker generation consumes a storage record and may itself
overwrite; counters and cursor gaps are authoritative even if marker is lost.

## A persistent and B one dynamic invocation

captureStart attaches exactly one capture per trace; another => BUSY while armed/bound; a new start replaces a terminal scope
while retaining its status. captureStop is idempotent and retains the terminal
route filter. Each start
allocates unique captureId, never reused. mode0 A starts immediately and keeps
routing across CALL/RET, pump returns, pauses/resumes until explicit stop or
maxRecords/maxCycles; limits1..uint64 count routed ordinary event views (not
suppression/status) and elapsed virtual cycles since start. enabled/gates still
control output; capture state does not silently change trace.enabled.
Capture reaching limit completes with state2/reason2; it does not pause CPU
unless stopCpu=true. A uses contextId=0,entryPc=0,rearmOnReset=false.

mode1 B arms for the next observed successful CALL whose target=entryPc; given
contextId must be0. It binds after CALL retirement, includes that CALL event and
retire, and tracks the resulting typed context stack. mode2 B binds an already
observed active CALL contextId (nonzero), entryPc=0; require current top frame is
that context and fully known. This supports stopping at a pre-execution CODE
breakpoint at function entry: use the observed CALL context, not next-call mode.
If already inside a context not observed from entry, or only a PC match exists,
return STATE. Never infer invocation identity from entry PC/SP alone.

Each binding allocates invocationId independent of captureId/contextId. Reentry
at same target creates a nested CALL context, never replaces root. Track all
CALL/RET and IRQ/RETI even when disabled, unrouted or IRQ-suppressed. Each frame
stores type, contextId,parent,expected return PC and expected SP. A matching
RET closes only CALL, RETI closes only IRQ; mismatched type/target/SP, unknown
prior frame or an unwind not represented by matching returns makes scope
incomplete. Maximum64 frames, next push overflows and closes incomplete.
The successful root RET event and its retired instruction are included; caller
continuation is excluded. No successful return until terminal retire. Fault
before retirement, fault inside nested work, unknown context, unwind/mismatch,
overflow or reset/load closes incomplete, never reason=return. Source ordering
makes terminal sequence explicit; output-filtered closing return still closes
scope, but cannot fabricate a retained record it filtered out. Capture status
startSequence/endSequence refer to canonical binding/terminal facts even when
not routed. startSequence=0 while armed; endSequence=0 before terminal.

state0 armed,1 bound,2 complete,3 incomplete,4 stopped. Reason0 return (only
complete B),1 explicit (stopped),2 limit (complete A; incomplete B),3 reset,
4 unknown,5 mismatch,6 overflow,7 fault,8 load. Bound/armed status reason0 is a
sentinel, not proof of return; inspect state. Reset closes old B binding with
state3/reason3 while trace configuration/enabled state persists. rearmOnReset=true
creates a new captureId after terminal old status, mode1 armed at same entryPc;
it is legal only for mode1. Old terminal status remains until session ends or
64 retained capture statuses are exhausted (new start/rearm => EXHAUSTED,
never discard silently). No implicit rearm for load. No implicit CPU reset when
capture ends. maxCycles includes paused time only if virtual time actually
advances (normally none); armed B timer starts at binding. A reset records new
generation and keeps A active with cumulative limits. Generation in B status
is binding generation, not overwritten by reset's new generation.

On ring overwrite, coalesce pending dropped source ranges into one loss record
before the next ordinary record; reserve space for both (minimum capacity2).
A pending loss record is emitted on explicit flush/close even without another
source event. Never recursively create a loss record for its own overwrite;
cumulative overwritten and cursor gaps include every overwritten storage record.
With stopOnFull, pending loss count stays in page.dropped (including metadata-only queries)
until space is cleared;
clear discards it as part of the old epoch.

B next-call stages at most the CALL source and its terminal instruction record
until successful retirement. Routing decisions and trace-ID sets are computed
at each original event using original gates/config; no retrospective predicate
evaluation. If CALL aborts, discard staged binding and remain armed; the abort
remains routable through ordinary trace configuration. Other source events
between CALL and retire are staged too, bounded to256 records/786432 bytes;
overflow closes the armed scope incomplete/overflow before any binding success.
Captured source order is preserved when the stage commits.

Every capture status includes successorCaptureId (0 without a successor). Reset
rearm stores the new captureId there, so clients can follow explicit rearm without
guessing IDs. A successor is mode1 armed, invocationId/contextId0, and has its own
new generation. Allocation failure leaves predecessor incomplete with successor0
and rearmStatus=EXHAUSTED in capture status; CPU state is unaffected, no
fabricated hardware fault and no implicit retry. rearmStatus=OK unless rearm
allocation failed. Native rearm_status uses the shared emuif_status values.

Page `dropped` is the saturated uint64 cumulative number of incoming views
discarded without storage insertion, separate from `overwritten` (inserted
records evicted). Clear resets both per-epoch counters. Outcome ring overwrites
and has dropped=0; it never silently drops without incrementing a counter.
Suppression/loss summaries emitted after reset use emission generation; their
source ranges are session-wide sequences and may span prior generations.
Explicit captureStop uses the last processed source sequence (0 if no source
has occurred) as endSequence; it does not invent an architectural event.

Capture is an additional per-trace route filter, independent of enabled/gates:
without a capture object, ordinary trace routing is unchanged; A bound permits
routing until terminal; B armed excludes ordinary events except staged candidate
CALL-through-retire; B bound permits nested work until terminal. Terminal capture
filters that trace out until a new scope is started. captureStop never silently restores ordinary
routing and does not toggle enabled. Deleting the trace removes its scope;
retained terminal statuses remain queryable until session end.

Staging is per affected destination, not per B trace: retain the full originally
coalesced trace-ID set and all chronological records for that destination until
CALL retirement. A and other traces sharing that destination are delayed within
the same fixed256-record bound. On successful binding commit each staged source
once with its original eligible set; on CALL abort remove only candidate B trace
IDs and commit the remaining views once in original order. No event cloning or
retrospective gates. Overflow closes B incomplete, removes its candidate route,
and drains other staged views under normal destination loss policy. Storage
recordSequence is allocated at commit in original order. Source sequence stays
unchanged. Stops/config replacement wait until this bounded stage resolves at
the safe instruction boundary; flush/close returns BUSY while it is unresolved.
