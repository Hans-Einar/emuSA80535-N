# Planned remaining producer implementation boundaries

Status: SLC-026 accepted within coverage125; SLC-027 active; SLC-028 planned.
Authority: continued Master execution instruction2026-10-28. These notes prepare
later bounded contracts; they do not activate concurrent product work.

## SLC-026 — Canonical producers and operational outcomes

Implement the separate CPU observation query/deliver binding over the opaque
CPU, actual begin/retire/abort across delay and peripheral cycles, actual captured
CODE bytes and shared decode, typed memory attempts/results, CALL/RET and IRQ/RETI,
UART frame outcomes and reset/fault lifecycle. The old legacy instruction callback
runs before effects and its counter is not retirement. Failed MOVX never emits a
completed access or retired instruction. All advertised coverage classes require
complete verified coverage for their selected variant; do not synthesize missing
facts by rereading devices or diffing state and inventing accesses.

The controller owns board/CPU admission, parent IDs, input order, generation and
canonical sequencing. Board apply failures before a CPU input exists still get
parent-only outcomes. CPU delivery receives its already-assigned input order;
autonomous board callbacks may have parent0. Retain all operational outcomes in
the fixed outcome ring independent of user trace enable/filter. Implement its
stable paging with explicit overwritten/loss metadata in this gate so the external
coordinator can use UART outcomes before general trace capture/file delivery.
No new ABI1 board callback or signal discriminator.

Compose the existing event/watch/gate/router runtime, not another matcher.
Its same-build generation fields currently use uint32, records lack canonical
metadata and offset rings lack epoch/record identity; evolve/adapt these before
claiming uint64 coverage. Preserve old internal semantic tests and disabled-trace
architectural equivalence. Safe stops retain actual source/match identities even
when trace output is disabled or overwritten. General user-facing capture/file
functions remain the next gate; no unsupported capability is advertised early.

Core changes may require new same-build raw-header fields or internal hooks.
Public CPU API1 and board ABI1 layouts remain unchanged. Frozen SDK0.1/0.2/0.3
payloads and archives remain immutable: before changing any shared payload,
retain its exact accepted bytes for independent reconstruction, including tools
and raw legacy headers. Historical bundles need not describe new same-build
private storage. Do not silently refresh an accepted manifest. Verify standalone
old archive hashes and current source/install identity after such changes.

## SLC-027 — Stable trace configuration, capture A and file output

Expose the frozen sized trace facade/control projection over the existing
semantic engine: atomic configuration/listing, stable record epochs/pages/loss,
watch stop detail, explicit trace gates and persistent capture across calls and
returns. Keep operational outcomes separate from user visibility but on the same
canonical event source. File sinks use host-owned tokens, bounded record queues
and asynchronous completion outside CPU callbacks/owner execution. Test failure,
backpressure, rotation, suppression summaries and missing/partial footer honesty.
No fabricated capture-complete or architectural fault from a failed sink.

## SLC-028 — One dynamic invocation

Bind only an observed typed call frame. Track nested calls, IRQ contexts,
recursion and return validation even when output is filtered. Preserve the
frozen per-destination coalesced staging rule around successful CALL retirement;
include the matching retired return and exclude caller continuation. Unknown
context, mismatches, overflow, reset and faults end incomplete. Keep trace enable
separate from scope state and expose reset rearm successor identity. Tests must
cover shared A/B destinations so staging cannot clone/reorder unrelated output.

Each accepted gate supplies a clean source archive, review/verification record,
exact manifest/source/archive identity and actual capability inventory to the
external coordinator. It retains the raw-byte/QEMU/pacing and board model. After
individual gates, composed verification must demonstrate actual virtual exchange
and trace correlation without implying physical device behavior or joint-clock
synchronization. Existing V0 remains pinned and reproducible throughout.
