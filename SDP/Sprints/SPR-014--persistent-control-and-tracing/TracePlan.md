# SLC-027 implementation preparation (not active)

SLC-026 remains the sole active Slice. This note prepares the next contract;
no Worker may implement027 before026 acceptance and Master activation.
The frozen0.3 docs04/05/06 and existing DEBUG-DES-007/008 are normative.

## Configuration and routing Worker boundary

Extend the existing event/watch/gate/router engine, using026 canonical ingress.
Expose atomic whole-config replace/list, ID lifetime/revisions, selectors,
conditions, actions and suppression through native trace facade and control3.
No parallel predicate implementation. Adapt signed width-bit constants at the
boundary to preserve legacy signed condition semantics. Reject unsupported
coverage selectors honestly. Maintain all28 canonical kinds without aliasing.

Use a routed-record delivery seam AFTER existing fan-out/coalescing and
suppression decisions. Preserve old same-build ring readers for legacy users;
new bounded stable storage owns epoch/record sequence/loss and safe stops.
Provide2..4096 ring sizes, explicit stopOnFull, clear and nondestructive pages.
Outcome destination0 stays independent and cannot be configured or cleared.
Capture A is a per-trace route filter, survives calls/returns/pause/reset,
maintains record/cycle limits, terminal status and explicit stopCpu behavior.
Prepare typed context/routing staging seam for028 without advertising captureB.

## Host file Worker boundary

Host owns token/path mapping and filesystem IO; session owns immutable routed
records, configuration identity and lifecycle requests. Agree a small private
same-build host adapter before implementation. Avoid putting host filesystem or
threads in CPU library. Trusted embeddings use the same registration mechanism.
An optional startup token grammar may be selected/documented by the Worker;
wire schema permits only opaque tokens and cannot configure paths.

A bounded writer may run on a host thread. It never accesses CPU/board/session
mutable state and never holds queue/status locks during write/fsync/open/close.
Owner-side work only validates bounded memory and queues immutable records or
barriers. Status snapshots are bounded and do not wait for disk. Startup validates
private directory policy; actual opens remain O_EXCL/O_NOFOLLOW0600 with fstat,
regular files only. No wire truncate, mkdir, stdout or physical device path.

Honor exact frozen headers/config SHA, segment names/limits/footer reservation,
256-record queue including summaries, explicit pending loss, partial byte counts,
rotation, operation IDs and requested/completed barriers. Open completes only
after header; pre-header views count loss. A failed sink disables that destination
only. Terminate requests closes, returns BUSY until complete/fail; no synchronous
indefinite wait. Disconnect/reset do not silently close file sinks.

## Review and acceptance preparation

Fresh Workers and separate fresh Reviewer. Require real wire config/page/capture
and exported file schema checks, signed/unknown/width1 watches, canonical order,
IRQ suppression, exact shared-route coalescing, stale/invalid atomic config,
retired IDs, reset retention, stop identity when records absent/overwritten,
ring limits/clear/metadata/cursors, persistent A across nested calls and pumps.
File tests include slow writer, queue overflow, failed/partial writes/fsync,
existing paths/symlinks/FIFO rejection, rotation/limits, busy replacement, opening
and flush/close operation barriers. Fault injection must stay test-local.
Run prior026, CPU, native/session, Unix, script, legacy and immutable SDK checks.
Export exact reviewed source and verify installed runtime capabilities and actual
trace file output before handoff. CPU API1, board ABI1 and frozen SDKs unchanged.

## SLC-028 follow-up: recovery after unknown ancestry

The026 producer conservatively keeps context unknown after an observation gap,
mismatch or unwind until reset.028 must test and decide recovery at a newly
fully observed CALL: that child's entry/SP/return may establish a known frame
with unknown parent0 without asserting anything about prior ancestry. A pending
B-next-call can then bind that actual invocation. Never permit active-context
binding before such observed entry, infer entry from PC/SP, or mark the old
unknown scope complete. This requires a fresh bounded CPU/context change and
independent review in028, not a new026 capability claim.
