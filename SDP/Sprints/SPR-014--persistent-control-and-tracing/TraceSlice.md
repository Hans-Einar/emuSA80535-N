# ITR-027 / SLC-027 — Persistent capture and bounded file delivery

Status: active. Sprint SPR-014. Authority: standing explicit continued producer
Master execution through026/027/028 and joint virtual integration.
Baseline:306c5af, accepted026 product af73adf974731afe32fdb2be806e1557a0804f7c.
REQ-029, DES-122..124. Read AGENTS, CurrentIndex, Sprint/Handoff, lifecycle01..05,
TracePlan.md and all frozen0.3 docs (especially04/05/06) before implementation.
Fresh Workers implement; separate fresh Reviewer; Master verifies and accepts.

## Scope

Implement atomic trace configuration replacement/listing, stable trace paging,
clear/epochs/loss, watch stop detail, persistent capture A start/stop/status and
host file sink open/flush/close/status. Bind to026 canonical producers through
existing semantic event/watch/gate/router machinery. No parallel matcher. Leave
capture B unsupported until028; ensure next gate can add typed context staging.
Do not infer full ACCESS coverage from the partial026 MOVX/SBUF instrumentation.
Configuration selecting an unsupported coverage class must be handled honestly;
report exact limitations and verify supported explicit source selection.

TracePlan.md records mandatory bridge hazards: signed width-bit constants,
canonical IRAM spanning both legacy halves, callbacks after existing coalescing,
bounded stable storage, and future invocation recovery. Frozen schema/native
prefix and all semantic limits remain normative. Preserve old raw API semantics,
source-scoped DEBUG identities, ABI1 and accepted SDK/archive pins.

## Worker boundaries and private host seam

Fresh Trace Worker owns emu_session.c/private session integration, existing debug
runtime/event/router implementation, trace facade/control3 projection, new private
trace storage/capture modules, and focused native/wire tests. Own root/tests
Makefile integration and dependency registration, coordinating with File Worker.
Implement native sized arrays/outputs with unknown tails untouched, complete
prevalidation and no partial config writes. Keep stable source/stop identities
independent of retained records and user trace filters. Use complete actual wire
response validation, not only schemas on canned fixtures.

Fresh File Worker owns runtime/main/control integration, runtime/file sink code,
private host adapter header agreed with Trace Worker, runtime/targets.mk and
current runtime documentation/host tests. The host registers up to8 opaque token
mappings before execution; a documented optional CLI grammar may supply them.
No protocol path or runtime mutation of mappings. Generic native embeddings use
the same private registration/policy seam; no frozen public ABI change.

Agree the adapter before shared changes. CPU callbacks and the execution owner
only queue bounded immutable routed records or barriers. Disk open/write/fsync/
close runs outside the owner critical section and without holding queue locks.
A host writer thread may block on IO; control never waits on that disk operation.
The CPU library remains independent of OS/file threading. Do not retain caller
buffers. Snapshot metadata/config hashes on the owner before async IO.

Honor exactly256 queue records, pending loss summaries, requested/completed
operation IDs, opening/header barrier, rotation/footer limits, all bytes actually
written, partial output/error and no fabricated completion. Existing files are
never truncated; private same-UID directories and regular O_EXCL/O_NOFOLLOW0600
segments only. Native sink_open obeys the same token policy. Failed sink disables
only its destination. Trace-enabled state and architectural CPU state persist.
Terminate requests async close and reports BUSY until completion/failure; no
indefinite synchronous shutdown wait. Interrupted host shutdown reports partial
output if it cannot finish accepted data/footer.

## Acceptance

Fresh review must assess exact combined source and resolve findings. Tests cover
atomic config/stale revision/ID retirement, conditions/signedness/width1/unknown,
selectors/watches across IRAM boundary, before/after gates and source/derived
ordering, coalesced shared destinations, IRQ suppression and summaries, all page
cursor/capacity/epoch/loss states, stopOnFull safe stops, reset persistence, A
across nested calls/pump returns/pause/reset, limits and explicit stop.

File tests must use real file schema validation and deterministic failure/slow
writer fixtures: pending open/flush/close, queue overflow, failed/partial IO,
rotation/limits, malformed tokens/existing paths/symlink/FIFO refusal, config
replacement BUSY while open, disconnect/reset persistence and bounded shutdown.
File IO failures are host sink outcomes, not emulated hardware faults.

Run existing026/core/native/session/Unix/script/legacy and immutable SDK checks.
Export exact reviewed source reproducibly, build/install without Git, run actual
installed trace configuration/capture/file operations and validate resulting
JSONL. Hand off exact source/review/archive/capability inventory to external
coordinator. Preserve V0/029/026 artifacts. Continue028 immediately after gate.
No board protocol, physical IO, TCP/DAP implementation, RPM or SDP migration.
