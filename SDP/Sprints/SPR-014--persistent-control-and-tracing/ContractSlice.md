# ITR-024 / SLC-024 — Persistent-control and observation contract gate

Status: active, contract/SDK declarations only; no runtime implementation
Authority: shared-user collaboration and explicit external concurrence at
30fe45a. Read AGENTS/CurrentIndex, lifecycle01..05, this Sprint/Handoff, accepted
bundle0.2 and DEBUG-DES-064..097. Preserve historical aliases/ledger and old pins.

## Worker scope

Freeze a new self-contained versioned interface artifact (candidate0.3.0),
canonical additional public headers under lib/include/emusa80535, schemas,
examples, conformance matrix, export/verification tools and consumer handoff.
Existing0.1/0.2 directories/archives and boardABI1 layout/behavior remain immutable.
Do not implement CPU, session, loader, transport, file writer or DAP functions.
Build/tool metadata changes needed for correct independent contract delivery are
allowed; they must not mislabel the currently implemented runtime2.1/SDK0.2.
Keep that implemented profile distinct from the new target contract bundle;
no prototype client should believe new target symbols or capabilities exist.

Freeze exact commands, native tables/records, schema discriminants, capabilities,
limits, state/error/lease/connection rules and operation ordering. No unresolved
consumer-facing field may be called stable. Reuse existing typed native status,
common prefixes and existing semantics rather than a parallel matching engine.
A major control profile is acceptable if needed to avoid reinterpretation of
frozen2.1 trace records; explicitly document coexistence and migration. Select
exact numbers during this gate and validate them, rather than silently adopting
provisional version numbers from coordination.

## Required contracts

1. Persistent Unix structured-control endpoint, explicit startup/no listener by
   default, local ownership/peer authorization, bounded framing/clients/queues/
   output/timeouts. Externally-driven execution is advertised: only explicit
   bounded advance OR step advances time; resume alone changes state. Advance
   checks controller lease and generation, preserves native budget+safe-drain
   limits and actual partial progress. Disconnect preserves session and never
   silently replays requests. Controller handover stays explicit; raw-byte/QMP
   clock coordination remains external.
2. Separately queried/sized CPU observation extension with immutable record-only
   facts and no private CPU cast. Canonical instruction begin/retire/abort;
   captured opcode/bytes/disassembly authority; memory/access attempt/result,
   control CALL/RET and IRQ/RETI, lifecycle and UART facts. No device re-reads.
   Legacy instruction count is not renamed retirement. Capture actual failures
   and partial effects, and advertise only implemented/verified coverage classes.
3. Generic admission/delivery/UART outcome path: parent board stimulus, distinct
   CPU input order and frame IDs, actual cycles, reason/known flags, RX start/
   accept/drop/end, TX holding replacement, stop/TI/end. Admission is not delivery.
   Required due failure preserves existing fault/no-retry behavior; observations
   do not fabricate CPU hardware faults. Outcome retention works independently
   of user trace enable/filter state, with bounded cursor/expiry/loss and reset
   cancellation. Do not send new unnegotiated kinds into old ABI1 board callbacks.
4. Compose accepted trace/watch/gate/router semantics with uint64 session
   generation, source-event identity versus storage-record identity, explicit
   stable ring epoch/cursor/loss, atomic bounded configuration and safe-boundary
   watch-stop ordering. Existing same-build structs are not a public ABI.
5. Trace A persists across CALL/RET, pump returns and pause/resume until explicit
   stop/limit. Reset preserves general trace enabled/config state and records
   generation. Trace B binds a unique observed invocation/context, tracks nested
   calls/IRQs even when filtered, includes retired closing return, and excludes
   caller continuation. RETI is not RET. Reentry does not replace root token.
   Unknown prior frames, unwind/return mismatch, overflow and partial faults
   close as incomplete, never successful return. Reset closes invocation binding;
   explicit rearm is scope state, not an implicit change of trace enabled state.
   Capture completion does not stop CPU unless separately requested.
6. Host-owned bounded JSONL file sink with pinned identity/configuration header,
   sequence/phase/cycle/correlation, explicit suppression/loss/status/footer,
   deterministic rotation/limits, no callback I/O or protocol-stdout trace, and
   visible sink failure without a fabricated architectural effect. Paths and
   allowed destinations are host policy; remote callers cannot open arbitrary
   files. Define requested versus completed flush/close and partial-output status.

## Acceptance and ownership

Worker owns new bundle/headers/schema/tools/tests and bounded build metadata;
Master owns lifecycle/Sprint/traceability and handoff. Separate fresh Reviewer
checks exact diff, cross-channel coherence, source findings and verification.
C99/C++17 header consumers; schema examples for every command/result/event and
negative limits/version/malformed/U64/state/correlation cases; native size/prefix
layout checks; self-contained export, pin/hash/lock negatives; old bundle/archive
immutability; current runtime identity and legacy/header build compatibility.
No target implementation test is claimed when only declarations exist.
Export exact reviewed clean source; retain durable archive and checksum, then
notify external owner before new dependent consumer implementation. Current V0
runtime/board pins remain unchanged. Acceptance of this contract gate does not
activate all planned product implementation or claim connected-system behavior.
