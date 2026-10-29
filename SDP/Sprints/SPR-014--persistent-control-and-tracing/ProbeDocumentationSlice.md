# ITR-033 / SLC-033 — Accurate execution evidence and recovery guidance

Status: active. Sprint SPR-014. Follows accepted evidence audit032.
Authority: shared-user stabilization/research mandate2026-10-29 and concrete
source/consumer audit findings. Documentation only; accepted runtime unchanged.

Fresh Documentation Worker corrects runtime/README.md claim that every retired
instruction has complete captured operands/disassembly. Explain actual immutable
fetch-prefix semantics, including untaken conditional paths, separately labeled
static decode enrichment and begin/retire/abort distinction. No frozen bundle edits.

Add concise generic runtime/RESEARCH.md and README link describing existing
paused coherent scalar/IRAM/SFR/board-peek captures, current generation and exclusive
lease, trace A/B invocation binding and source ordering, observer limits, no public
set-PC/register/RAM/checkpoint API, and reset(entry) as full reset. Explain synthetic
bootstrap actual CALL versus unimplemented warm-state forced entry; provenance,
old/new state and stack/IRQ context and no natural-reachability claim are required.

Include ordinary paused/fault reset/reconnect recovery boundaries supported by032,
external pacing versus CPU fault, no implicit replay, independent host queues,
and reset failure/exhaustion caveats. Explain UART RX_ACCEPT known captured value
versus RX_DROP canonical unknown representation in current accepted runtime; fact
statusOK differs from input.failed status. Validate per-kind fields and generic
unknown-value rules; do not infer failed raw messages that were not retained.

Worker owns only runtime/README.md, runtime/RESEARCH.md and VER-SLC-033.md.
Fresh Reviewer checks source/frozen contracts, no overclaims/new API promises,
all document links and diff boundary; owns REV-SLC-033.md. No runtime needed for
this low-impact documentation correction; cite accepted030/032 behavior and
existing source, not invented new tests. Master accepts with traceability/handoff.
No product implementation, consumer edits, runtime pin update, existing-lab access,
physical IO, DAP/TCP, public-state-mutation API or immutable artifact changes.
