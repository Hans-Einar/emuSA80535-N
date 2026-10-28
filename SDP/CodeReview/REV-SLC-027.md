# REV-SLC-027 — Persistent trace and bounded file delivery

Status: APPROVED for the exact product candidate below; Slice acceptance remains
with the Master.
Reviewer: fresh independent persistent_trace_reviewer.
Active contract: SPR-014 / ITR-027 / SLC-027, TraceSlice.md.
Baseline: 412dd0f; accepted producer026 af73adf.
Reviewed product: `740f60863bc52481e589f219ba54df8f5bd0fc82`, including file backend
parent `c5ee9f7b251c5e07a10774787034294a79b47ea7`.

## Review boundary

Review the exact combined Trace/File Worker implementation against frozen SDK0.3,
existing semantic matcher/router and the active Slice. Product fixes remain with
Workers. Master independently verifies exported/installed artifacts and accepts.
No native ABI, wire schema or historical SDK mutation is permitted.

Master selected an explicit partial-access policy: allow XDATA READ/WRITE and
SBUF-only SFR WRITE/RMW predicates; reject unsupported CODE/IRAM/general SFR
coverage requirements atomically. Broad source selectors may retain actual
partial facts without advertising full ACCESS coverage. Synthetic matcher tests
must still verify canonical IRAM matching across the legacy lower/upper split.

Final disposition requires source review plus independent native prefix/config/
page/capture checks and actual Unix trace/file schema tests. Key risks include
whole-config atomicity, lifetime IDs, source/derived/gate order, shared-destination
coalescing, persistent A across reset/IDLE, loss versus suppression, asynchronous
open/flush/close barriers, file queue bounds, path refusal, rotation/partial IO
honesty and compatibility regression evidence.

## Findings and independent evidence

Independent temporary harnesses live under `/tmp/slc027-review`; they do not
modify product sources. Actual wire replies and JSONL are checked against the
frozen SDK schemas and semantic validator, rather than only matching strings.

Findings raised to Workers during development:

- Wire boolean parsing initially used a helper restricted to JSON strings,
  silently reading `true` as false. Reproduced with enabled traces and capture A
  receiving zero records. Worker corrected primitive parsing; independent wire
  tests then passed, including true stopCpu.
- Nested native action/condition prefix version errors were collapsed to INVALID
  instead of required VERSION. Worker corrected exact prefix error propagation and added used/unused
  nested prefix regressions.
- Native pagers dereferenced output slots at odd strides. Independently
  reproduced undefined behavior with `sizeof(record)+1` under UBSan; Worker
  changed new trace and existing outcome paging to aligned local copies. New
  trace paging now passes the same adversarial test under ASan/UBSan/leak checks.
- Initial stopDetail watch JSON used `eventSequence` instead of frozen `sequence`;
  canonical-space-only selectors needed the access-event guard; unused native
  action slots required initialized nested prefixes, not whole-struct zero
  comparison. Worker corrected these before integrated tests.
- Storage exhaustion must terminate an affected capture incomplete/overflow,
  including file storage. Ring handling was corrected; private file outcome
  propagation now closes affected capture state without a CPU fault; final
  exact-candidate verification passed. Ordinary file loss/error remains independent
  of CPU faults and capture stop policy.
- Writer failure selection read a mutable error outside its mutex; Worker
  snapshots the error into the task under lock. Header/rotation fit accounting
  now includes the JSONL record envelope. No disk operation holds the shared
  queue mutex or execution owner lock.

Passing independent checks:

- `review_wire.py`: 75 schema-validated responses covering A record limits,
  shared destination coalescing, continued second-trace output, reset and cycle
  persistence, eventless IDLE stopCpu limit, stale replacement atomicity,
  unsupported IRAM predicates, epochs and retired-ID refusal. Selected IRQ
  suppression flush/clear isolation also passes: clearing one destination does
  not flush another destination’s pending counters.
- `native.c`: canonical IRAM 0x7f/0x80 identity, signed eight/one-bit values,
  unknown and attempted access exclusion, strided input deep copy, output tails,
  CAPACITY metadata and all-or-nothing validation. ASan/UBSan/leak checks pass,
  including odd output stride after the correction.
- `file_adversarial.c`: held writer, pre-header loss, full 256-entry queue,
  flush snapshot excluding later records/loss, operation BUSY, reset-generation
  loss summaries, close, asynchronous existing-file refusal. ASan/UBSan/leak
  checks pass; 260 resulting JSONL lines validate against frozen file schema.
- `file_wire.py`: real host token registration, missing token denial, file page
  UNSUPPORTED and clear INVALID, config BUSY while open, UTF-8 canonical config
  SHA, reset generations, contiguous storage IDs, exact bytes and mode0600.
  128 actual JSONL lines validate.
- `termination.c`: held file writer with a host-driven native session. First
  terminate returns BUSY and freezes at cycle20; repeated pumps/resume/step
  cannot progress; release of the writer permits final termination.

The new observation/routing path performs bounded copies and queue operations;
no new event callback allocation or JSON serialization was found. File encoding
and write/fsync/close run in the separate writer. This is a source assessment,
not a throughput guarantee. ACCESS completeness and capture B remain withheld.


## Exact candidate verification and disposition

Reviewed the combined final source in a fresh detached worktree at
`/tmp/slc027-review/candidate`. The worktree remained clean after verification.
A fresh `emu-runtime --version` reports the complete reviewed product SHA above.
No files under `interfaces/` or `lib/include/emusa80535/contracts/` changed from
the activation baseline. Generic CPU/board ABI and frozen SDK payloads remain
unchanged. Raw same-build matcher structures carry only the internal canonical
projection/delivery additions; existing raw matcher/router/dispatcher regression
suites pass.

The following commands passed on the exact candidate:

- Fresh `make emu-runtime -j4`.
- `make -C tests -j4 trace-native-test control3-test outcome-test
  cpu-observation-test debug-event-test debug-trace-test debug-runtime-test
  runtime-session-test`: includes all-opcode observation equivalence, native
  trace/64-watch/ID exhaustion tests, C++ control table, 181 actual wire3 replies
  and 1116 actual legacy wire2.1 replies.
- `make trace-test runtime-host-test runtime-control-test runtime-outcome-test
  runtime-trace-file-test file-sink-test interface-target-check interface-check`:
  109 trace replies, 11 outcome replies, 55 file-control replies, 202 actual host
  JSONL lines and 440 backend JSONL lines validate; host/control/lease/framing/
  path/error checks and both frozen SDK checksum/layout/export checks pass.
- Independent `review_wire.py` and `file_wire.py` against that built binary:
  75 replies and 128 JSONL lines pass the cases documented above.
- Independent native and file adversarial harnesses rebuilt from exact source
  with ASan/UBSan, leak detection and UBSan halt-on-error: pass. This includes
  odd record stride, untouched tails, signed one-bit comparisons, queue loss,
  flush barriers and async existing-file refusal.
- Independent held-writer termination harness relinked against exact libraries:
  pass, with no CPU progression while closure is pending.
- `git diff --check`: pass.

Exact-run logs are `/tmp/slc027-review/final-build.log`, `final-native.log`,
`final-host.log`, `final-independent-native.log`, `final-independent-file.log`
and `final-independent-termination.log`; harness sources and actual response/
JSONL evidence are in the same directory. These temporary review artifacts are
separate from the Master's durable exported/installed verification artifacts.

All findings raised in this review are resolved in the exact candidate. The
explicit-stop suppression flush now preserves an incomplete/overflow result;
file storage refusal propagates through the private enqueue result and owner
status/progress polling without file-thread access to CPU/trace state. The final
representable storage ID remains valid; subsequent insertion is refused. Global
config replacement remains BUSY while any file sink is active, as frozen05
requires. Selected clear/stop/flush preserves unrelated pending suppression.

No remaining blocking finding. Approved for SLC-027 integration and independent
Master artifact verification. This approval does not claim SLC-028 invocation
capture B, full ACCESS completeness, real-time throughput, physical I/O, TCP/DAP
or whole-system completion. The implemented partial-access domain, ring-only
paging, A interval/limit behavior, termination freeze and file lifecycle limits
are documented in runtime/README.md. Capture B remains explicitly unavailable.
