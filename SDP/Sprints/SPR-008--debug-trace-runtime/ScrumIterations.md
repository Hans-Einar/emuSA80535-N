# Scrum iterations

## Current contract — SLC-017 current-master reconciliation (2026-09-06)

Completed Sprint: DEBUG-SPR-008; Iteration: ITR-017; Slice: SLC-017.
Authority: Issue #14 comment 5517725977 and the current user instruction.
Reconfirmed by the latest Stage-4 Steering rebaseline, comment 5559152334:
https://github.com/Hans-Einar/emuSA80535-N/issues/14#issuecomment-5559152334.
It pins the same exact master below and explicitly preserves Stage0..Stage4,
including accepted ADC and Timer2. It authorizes no new feature scope.
This bounded corrective pass reopens merge readiness only, not accepted
SLC-015..017 behavior or DES-090..DES-097 design decisions.

- Merge exact fetched master `b43fe36b0965b6ac8628677bb6fcc16513d1f567`
  into `codex/debug-trace-runtime-takeover`, starting from accepted PR HEAD
  `1e588d28fb168a7c5a42c4c7dc4b51f84d29d1ed` plus this contract record.
- Resolve only drift conflicts. Expected surfaces are test Makefile and
  colliding SDP review/verification files, registry, relations and ledger.
  Preserve newer master CPU/peripheral/test behavior and accepted debugger
  modules/tests byte-for-byte wherever no integration correction is needed.
- Preserve both histories and all six corrections from product commit
  `d956177add44dda9efbd6d9e372a9c0a6d40f777`.
- Resolve pre-existing parallel-branch ID collisions explicitly: retain
  current-master canonical IDs/paths; qualify colliding debugger registry
  identities and evidence paths with DEBUG, documenting aliases to their
  original source-scoped IDs. Preserve original scoped SLC-015..017 and
  DES-090..DES-097 meanings. Keep all historical ledger records, with an
  explicit provenance/disambiguation mapping rather than rewriting history.
- No CLI/DAP/wire/CPU/sink/source-map/product/physical-I/O implementation.
  SharedUI is not applicable to this C runtime reconciliation.
- Fresh Worker owns the merge and conflict resolutions; a separate fresh
  Reviewer audits the exact reconciliation and both parent baselines.
- Required verification: strict GCC/Clang C99 focused event/watch/trace/runtime;
  all master Stage-0/IRQ/timer/UART/port-MOVX plus external-edge/ADC/Timer2
  regressions; debugger C facade; modern-Python emu-debug 1.0 NDJSON;
  Windows and WSL/Linux ASan/UBSan; available Valgrind; static analysis;
  diff whitespace, duplicate-safe YAML and NDJSON parse, traceability
  preservation, forbidden-scope and reconciliation-only diff audit.
- Required records: REV-SLC-017-RECONCILIATION and
  VER-SLC-017-RECONCILIATION; update sprint notes, handoff, CurrentIndex,
  Relations and append Ledger events before declaring merge readiness.
- Completion: exact new PR HEAD is published, exact master is its ancestor,
  GitHub reports mergeable, all required available verification passes and
  fresh review approves. Report READY_FOR_STEERING_MERGE; do not merge PR #16.

Traceability: MND-001, REQ-016, ARCH-007..ARCH-010, debugger
DES-064..DES-097, SPR-008 / ITR-017 / SLC-017, accepted SLC-015..016,
REV-SLC-017-HOLISTIC-F001..F006, REV-SLC-017-CORRECTIONS, VER-SLC-017,
REV-SLC-017-RECONCILIATION, VER-SLC-017-RECONCILIATION.

Status: complete; bounded reconciliation approved and verified.

### Current result

Fresh Worker merge `74dca67aafebdddc82ade3e5d6864a1457ea75bb` and bounded
whitespace followup produce exact candidate
`c21d63f121f6305afd006a145a96c8dddae8b55e`. Fresh independent
REV-SLC-017-RECONCILIATION approves that reconciliation with no new finding.
Master VER-SLC-017-RECONCILIATION passes strict GCC/Clang, complete Stage0..4,
facade/process, all twelve Clang sanitizer suites and actual merged Makefile
runs on Windows and WSL, plus static/repository gates. Valgrind remains
NOT_AVAILABLE under the where-available rule. Product/test/design preservation
proves accepted ADC/Timer2, SLC-015..017 corrections and DES-090..097 survived.
No corrective implementation pass or new feature is required. Registry,
relations, ledger and Handoff record completion; no sprint remains active.
Final publication checks must confirm the exact remote head is mergeable
against the pinned master before reporting READY_FOR_STEERING_MERGE.

The remaining sections are historical execution checkpoints. Their earlier
active/pending/topology statements are superseded by this current result.

## ITR-015 — Standalone event and watch matcher foundation

Status: complete
Iteration ID: ITR-015
Active Slice: SLC-015

### Slice contract — SLC-015

**Goal:** implement a standalone generic debugger runtime foundation that can
consume synthetic immutable events now and core-produced events later.

**Required implementation:**

1. fixed-width immutable event/value schema with explicit known, width and
   signedness fields;
2. debugger-owned uint64 sequencing and fixed-capacity stable-order observer
   fan-out with registration changes rejected during dispatch;
3. bounded watch table with access/address selectors;
4. per-action `eq`, `ne`, `lt`, `le`, `gt`, `ge` comparisons over old/new
   values, with explicit signed/unsigned behavior and unknown=false;
5. independently combinable stop, console, quiet and bounded trace-route
   results; multiple stops coalesce;
6. tests for ordering, bounds, reentrancy rejection, all comparisons, numeric
   edges, unknown operands, action independence and observer neutrality.

**Compatibility:** no edits to `core.c`, `opcodes.c`, existing peripherals,
`emu_debug` protocol or DAP in this Slice. New APIs must be additive C99 and
build with GCC and Clang warnings-as-errors.

**Non-goals:** core instrumentation, interrupt policy, trace gates, JSONL,
ring buffer, CLI/protocol commands and physical I/O.

**Required evidence:** Worker tests, fresh Reviewer report, Master verification
and traceability updates.

### Result

Complete. The standalone runtime and focused tests were implemented without
changes to CPU, opcode, peripheral, existing debugger protocol or DAP code.
REV-SLC-015 approved with corrections and VER-SLC-015 passed.

## ITR-016 — Multi-trace router and bounded ring

Status: complete
Iteration ID: ITR-016
Active Slice: SLC-016

### Slice contract — SLC-016

**Goal:** implement the next debugger-owned layer over synthetic canonical
events, without changing CPU, opcode or peripheral sources.

**Required implementation:**

1. atomically replaceable bounded trace sessions with stable ID, enabled
   state, bounded tag/comment, destination ID and include/suppress/interrupt-
   only policy;
2. bounded point routes and deterministic before/after trace on/off gates;
3. nested interrupt-depth tracking with precisely retained outer enter/exit
   boundaries and per-trace bounded suppression accounting;
4. canonical event routing with ascending duplicate-free trace IDs and no
   per-trace cloning;
5. fixed-capacity routed-event ring with explicit overwrite/loss accounting;
6. watch results from SLC-015 accepted as explicit trace routes without
   recursive watch evaluation;
7. focused tests for replacement atomicity, route ordering, gate conflicts,
   nested interrupts, policies, shared destinations, ring wrap/loss and all
   advertised limits.

**Compatibility:** additive C99 only. No edits to `core.c`, `opcodes.c`, SAB
peripherals, existing `emu_debug` protocol/server or DAP. File/console sinks,
CLI/protocol exposure and core event producers remain later work.

**Required evidence:** strict GCC and Clang, sanitizers, regressions, fresh
Worker and Reviewer, Master verification.

### Result

Complete. REV-SLC-016 approved after direct coverage for all four gate timings.
VER-SLC-016 passed strict compilers, sanitizers, full regressions and the
existing debugger process suite.

## ITR-017 — Derived-event dispatcher and in-memory facade

Status: active
Iteration ID: ITR-017
Active Slice: SLC-017

### Slice contract — SLC-017

**Goal:** compose SLC-015 and SLC-016 into one bounded debugger-owned runtime
that accepts synthetic canonical events and exposes an additive in-memory C
facade, still without touching CPU execution or peripherals.

**Required implementation:**

1. dispatcher subscribes to the event bus and processes each source exactly
   once without recursive callback execution;
2. evaluate watches in ascending ID order, route the source event, then create
   newly sequenced `watch.match` events in deterministic order, and apply
   source after-gates only after all derived events drain;
3. fixed-capacity pending derived-event queue with explicit overflow status and
   no silent partial fan-out;
4. aggregate stop requests and preserve primary lowest watch ID/source sequence
   for the caller to apply only at a future CPU safe boundary;
5. additive opaque in-memory facade for atomic replacement of watches, traces,
   destinations, points and gates, trace enable/disable, event ingest, status,
   stop consumption and paged ring reads;
6. reset/load/clear lifecycle semantics and deterministic counter behavior;
7. focused end-to-end tests for source/derived sequence identity, before/after
   bracketing, multiple watches, overflow neutrality, stop priority, ring pages,
   replacement atomicity, lifecycle and all advertised bounds.

**Compatibility:** no changes to `core.c`, `opcodes.c`, SAB peripherals,
existing `emu_debug.c/.h`, `emu_debug_server.c`, wire protocol or DAP. No file,
console or raw stdout sink. The facade consumes synthetic events only.

**Required evidence:** fresh Worker, fresh Reviewer, strict GCC and Clang,
ASan/UBSan, Valgrind, complete regressions and Master verification. After Slice
review, a separate fresh holistic Reviewer audits SLC-015..017 together for
correctness and improvement opportunities before acceptance.

### Issue #14 takeover acceptance pass

Status: active

Steering/Master authority is Issue #14. Work starts from exact preserved WIP
HEAD `356836637d5ff432d91fc508fd55b2f17b45cdb3` on the fresh branch
`codex/debug-trace-runtime-takeover`; the preservation branch is immutable.

The acceptance pass must:

1. perform REV-SLC-017-HOLISTIC as a fresh independent review of SLC-015,
   SLC-016 and SLC-017, including every review dimension named in Issue #14;
2. treat the existing REV-SLC-017 only as non-authoritative historical
   evidence and reproduce or reject every claimed correction independently;
3. correct blocking findings only within the existing standalone debugger-
   owned event/watch/router/runtime composition scope;
4. run and record VER-SLC-017 with exact tool versions and explicit
   environment limitations;
5. freeze the future stable sized/versioned request/response facade and final
   after-sequence page metadata in design documentation only;
6. leave CPU producers, CPU safe-boundary application, CLI, wire protocol,
   DAP, file/console sinks, source maps and product/physical I/O out of scope;
7. preserve one emulator-owned breakpoint/watchpoint/tracepoint semantic model
   for later CLI and DAP frontends.

Expected completion signal: REV-SLC-017-HOLISTIC contains no unresolved
blocking finding, VER-SLC-017 is reproducible and passed or honestly records a
blocking environment gap, the design seam is frozen, and SDP/traceability can
support a READY/NOT_READY decision without relying on chat memory.

### Holistic review result and corrective Worker contract

REV-SLC-017-HOLISTIC disposition: **corrections required**.

The fresh review opened six in-scope findings:

- `REV-SLC-017-HOLISTIC-F001`: ordinary ingest can counterfeit reset/load;
- `REV-SLC-017-HOLISTIC-F002`: same-marker before-gates can hide required
  lifecycle markers;
- `REV-SLC-017-HOLISTIC-F003`: load misses CODE-capable address-only
  selectors;
- `REV-SLC-017-HOLISTIC-F004`: replacement can silently discard active
  suppression intervals;
- `REV-SLC-017-HOLISTIC-F005`: trace IDs can be reused before clear-session;
- `REV-SLC-017-HOLISTIC-F006`: tag/comment accepts malformed UTF-8.

The next pass is corrective, not a new feature Slice. A fresh Worker must
resolve F001..F006 inside `emu_debug_event.*`, `emu_debug_trace.*`,
`emu_debug_runtime.*` and their focused tests only. Lifecycle markers shall be
routed to the traces enabled at entry to the lifecycle boundary; same-marker
gates may determine subsequent enabled state but may not erase that marker.
Address-only selectors are CODE-capable and must be invalidated on load.
Configuration replacement must reject an active suppression interval whose
trace would be deleted or change destination/policy until explicit flush.
Trace IDs may update while live but may not disappear and later be reused
before clear-session. UTF-8 validation must be bounded and locale-independent.

Non-goals remain unchanged: no CPU producer/safe-boundary integration, CLI,
wire protocol, DAP, file/console sink, source map or product/physical I/O.
After correction, a separate fresh Reviewer must inspect the correction diff
before VER-SLC-017.

### Corrective Worker result

Status: review pending
Product commit: `d956177add44dda9efbd6d9e372a9c0a6d40f777`

The fresh Worker implemented F001..F006 in `emu_debug_trace.h/.c`,
`emu_debug_runtime.c`, `tests/test_debug_trace.c` and
`tests/test_debug_runtime.c` only. Focused strict GCC/Clang, Clang ASan/UBSan
and the complete core regression suite passed in the Worker environment.
MinGW sanitizer libraries were unavailable; Clang supplied sanitizer coverage.

The iteration remains open for `REV-SLC-017-CORRECTIONS`, VER-SLC-017 and the
documentation-only stable facade/versioning/paging freeze.

### Correction review result

`REV-SLC-017-CORRECTIONS` independently approved exact corrective product
commit `d956177add44dda9efbd6d9e372a9c0a6d40f777` for Master verification.
All six holistic findings are resolved, no new product finding was opened, and
the diff remained inside the standalone debugger-runtime boundary.

SLC-017 now proceeds to VER-SLC-017. It is not accepted until the full
verification matrix and documentation-only facade/versioning/paging freeze
are complete and recorded.

### Master verification, design freeze and acceptance

VER-SLC-017 passed all available Windows and WSL/Linux gates. Strict GCC and
Clang, ASan/UBSan, full Stage-0/IRQ/timer/UART/port-MOVX regressions, focused
event/trace/runtime suites, existing debugger facade/process tests, static
analysis, diff, traceability and forbidden-scope gates passed. Valgrind was not
installed in either WSL distribution and is recorded honestly as an
environment note under the “where available” rule.

DES-090..DES-097 freezes the future external integration seam. The current C
surface remains internal; future external operations use fixed-width sized/
versioned wrappers and an exclusive per-ring `after_record_sequence` cursor
with session/ring epoch and loss metadata. CLI, protocol and DAP remain later
frontends over one emulator-owned breakpoint/watchpoint/tracepoint model.

Status: complete. SLC-017 is accepted. No blocking finding, verification gap
or Steering escalation remains. Takeover-PR merge is not authorized here.
