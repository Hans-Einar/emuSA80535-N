# Proposed ITR-034 / SLC-034 — Native outcome request cost attribution

2026-10-29. Status: preparation proposal, execution NOT activated. Sprint SPR-014.
Authority: shared-user stabilization mandate, renewed through overall coordinator
after the consumer's failed cold-configuration gate. Producer Master owns this
measurement-only follow-up. Consumer closed its gate at cd24d1d and handed off
sole cross-project measurement ownership. CurrentIndex remains without an active
Iteration/Slice until fresh plan review corrections pass; activation permits
fixture preparation only, with execution behind the gates below.

## Objective and boundaries

Measure accepted028 outcomePage request parsing/schema validation, native page
validation/copy, canonical serialization, response assembly and local Unix wrapper
cost. Determine a supported next target or report an inconclusive/negative result.
No optimization, public API, ABI, wire profile, pin, page/retention limit, execution
semantics or required evidence policy changes. No consumer product edits, firmware,
board model, external lab attachment, physical I/O, QEMU, TCP or package publication.

Lifecycle authority: MND-001; STU-004; REQ-027..029; ARCH-011..018;
DES-119..124. SLC-032 is historical evidence, not permission to repeat its runs.
Measurement sources and opaque observed corpus stay in an isolated external audit
directory; generic contract/review/verification summaries may enter this repository.
Preserve upstream MIT attribution and every accepted artifact.

## Ownership and gates

1. Master records reviewed plan and cross-project handoff, then activates
   ITR-034/SLC-034 before assigning a fresh measurement Worker.
2. Worker prepares fixture/converter/launcher and retains every source revision.
   No native/Unix execution before a fresh independent Reviewer approves the exact
   source freeze and correctness-run commands. Compilation is permitted only in
   the activated preparation scope with the build limits below.
3. Reviewer inspects source, private instrumentation/seed seam, budget enforcement,
   byte/error oracles, cleanup and all fixture provenance. Exact build commands,
   compiler/linker/dependencies and resulting binary hashes are frozen. Functional
   checks run under a reviewed correctness gate; performance runs require their
   results plus a final exact pre-run manifest review. No mutable latest paths.
4. Worker and Reviewer execute their finite approved sets sequentially. Master
   audits saved raw outputs, independent reproduction, traceability and handoff.
   A failed gate consumes its attempted slot; retain failure, no automatic rerun.

Preparation alone does not approve a benchmark or establish performance. Worker
owns audit worker files and VER-SLC-034 draft; Reviewer owns audit review files and
REV-SLC-034. Master owns this contract, CurrentIndex and final handoff. No product
source in the working repository is assigned for editing.

## Exact baseline and corpus

Accepted028 source: `8dd1230704cd6c460167b954f7e987c23e24ff86`.
Immutable source archive SHA256:
`0503bdf512adff41d97ac300ec46e0e60f642dda9aba22d444d46ca6b7a33f2f`.
Producer documentation HEAD at proposal: `12c9d0c99dd32abd6a19e73b36975441f4f3a963`.
Use the accepted028 archive, never current031 sources or installed user processes.
Archive location and external corpus bindings are in the coordination handoff.

Primary workload: the previously reviewed 758 observed nonempty page bodies,
11,872 records: 5,936 canonical events and 5,936 retention markers, with 732 full
16-record pages and 26 partial pages. Preserve order, fields, identities, text,
width/known flags and page metadata exactly. Derive selection and typed values
independently. Full input hash and derived corpus hash precede execution. Actual
raw response envelopes were not retained: request IDs/envelopes are explicitly
synthetic. Do not claim native captured-wire equality or complete live-loop cost.

Fixture decoder maps every canonical field to its native type with range/shape
checks; no truncation, unknown-field omission or numeric/string coercion. Re-encode
and compare complete typed page bodies to input. Keep all observed records outside
tracked producer sources and treat application-valued fields as opaque data.

Only page windows survive, not whole historical rings. A private test-only adapter
may initialize the accepted ring/session fields before a request, outside timed
regions. Declare this as synthetic reconstruction, never checkpoint restoration
or native event-generation proof. Preserve oldest/newest/overwritten, epoch/cursor,
record positions and exact selected bytes; explicitly mark unseen filler slots.
Distinguish observed returned records, header-derived endpoint sequence metadata,
and unobserved filler. Native paging may read oldest/newest endpoint sequence
metadata reconstructed exactly from recorded headers; no primary query may read
or serialize fabricated record payload. No appending synthetic input or
clock advance to invent missing history. A separate labeled synthetic matrix covers
empty/current/expired/ahead/wrong-session/wrong-epoch and invalid requests.

If complete body reproduction is impossible from the retained fields, stop the
pre-run gate with the precise mismatch. Do not silently normalize IDs, page headers,
loss records or change the workload to make the oracle pass.

## Measurement layers

Use identical compiler/optimization settings for plain and instrumented fixtures.
The accepted native functions remain byte-for-byte source-identical in the plain
fixture; explicitly isolated measurement hooks are permitted in the instrumented
copy only. Freeze the complete patch and generated source. The private seed seam
is identical in both variants and outside request timing. No exported new API.

Native arm calls the existing control3 request entry on an explicitly initialized
owned session. Correct setup/open/hello/attach occurs outside measured requests.
Virtual cycle/instruction/generation/stop state stays fixed throughout page reads.
Measure total request plus nested boundaries for:

- request allocation, JSON parse and envelope extraction;
- wire argument/schema/semantic validation;
- dispatch residual and native emu_outcome_page validation/copy;
- emu_trace_page_json and full canonical record serialization;
- response-envelope construction, body copy and cleanup.

Page-copy time must include its validation unless a distinct internal span is
explicitly frozen; do not relabel total native page cost as memcpy alone. Invalid
request recovery/reparse and connection close are separate functional cases.

Unix arm uses the accepted runtime/control.c frontend in an owned fixture host,
explicit externally driven mode and private local endpoint. Private seeding is a
reviewed fixture seam, not an installed runtime capability. No CPU run/advance or
board code is necessary. A dedicated client sends one pre-encoded request and
fully consumes each response; strict validation and output retention are measured
separately. No pipelining. Socket bind directory mode0700 and normal same-UID peer
checks remain. Use no existing endpoint and no consumer control client.

Measure server-thread CPU and monotonic wall for read/parse/dispatch/write spans,
and client end-to-end wall plus client CPU separately. Kernel/scheduler waits cannot
be inferred by subtracting independently run native totals from Unix totals. Even
aligned server wall-minus-thread-CPU is waiting/preemption/uninstrumented overhead,
not pure Unix scheduler cost. Retain plain/instrumented pairs for observer cost;
no guessed subtraction or profiled-percentage scaling. All accepted request work,
including its allocation, parsing, validation, response copies and free, stays
inside total request timing. Only fixture allocation/seed/setup, hashing, external
oracle validation and artifact encoding are outside core spans; report surrounding
totals. Unix seeding latency remains part of enclosing client latency and must be
reported without treating it as scheduler cost. Clock resolution and short-span noise may require an
inconclusive category rather than another run.

## Byte, state and error oracles

For each primary query require exact plain-versus-instrumented native response
bytes/status/length/client-open state, and identical native-versus-Unix payload
bytes (Unix adds only its framing newline). Separately require complete parsed body
equality against the original observed page. Synthetic envelope fields are fixed
in advance; no runtime-dependent normalization in timed or correctness comparisons.
Retain ordered response digest and bounded full output for independent inspection.

Correctness matrix (at most 64 cases per role) includes malformed/duplicate keys,
unknown fields, invalid scalar types and u64 bounds, maxRecords 0/16/17, cursor
conditions, output capacity, missing attachment, monotonic request IDs, invalid
frame/partial close and cleanup failures. Mirror frozen028 errors and close/no-close
behavior against the plain accepted source, not a newly invented oracle. Include
record reserved-field validation and unknown observed values in mapper tests.
Check CPU state and ring unchanged across every read, while separately checking
expected client request-ID progression, final busy=false and legitimate error-close
state; the entire session/client struct is not expected byte-identical. Fix synthetic
session and request identities before setup consistently in both arms.
No memory-write interface or
readiness behavior is changed to construct fixtures.

## Finite resource and run limits

All limits are maxima, not targets. No overlapping Worker/Reviewer benchmark.

- Build preparation per role: at most two compiler-driver/build attempts per plain/profile
  fixture (four total), 180 aggregate child CPU seconds, 300 seconds hard wall,
  512 MiB cumulative generated/build bytes. All compiler/linker descendants count
  within each attempt and its resource bounds. Build failure is retained; further
  attempts require an explicit plan amendment, not a silent loop.
- Correctness per role: at most 64 functional cases, 20 aggregate child CPU seconds,
  60 seconds hard wall, 32 MiB cumulative outputs; external launcher must preserve
  hard-limit/signal status. No full timed-corpus repetition hidden as warmup/tests.
- Measurement per role: four passes, each 758 requests: native plain, native profile,
  Unix profile, Unix plain for Worker; reverse layer/order for Reviewer. Thus eight
  passes total. Include empty/error synthetic cases only in the correctness budget.
  Each role has 20 aggregate child CPU seconds, 90 seconds external hard wall,
  96 MiB cumulative measurement outputs. No extra warmup/full-corpus pass.
- One server child at a time and one owned client; no daemonized children, threads
  unjoined at completion or detached processes. At most two Unix server lifetimes
  per role for measured arms; correctness may use at most one additional owned
  server lifetime per role. Record exact executable/hash/PID/start identity and
  endpoints; final launcher reaps only its own process tree.
- Enforce cumulative byte credit before every output, counting timing/results/
  failures/launcher records and overwritten bytes without refund. Reserve failure
  and cleanup space outside work allowance. Keep resource/accounting guard overhead
  identical in matched variants and report it. Freeze actual reserve sizes before
  execution. OS hard CPU limits per child are insufficient for aggregate accounting;
  use a frozen allowance ledger that preallocates remaining CPU to every concurrent
  process (including compiler descendants and client), with enforced maxima summing
  within the role total. Use process-tree supervision with explicit monitored
  overshoot bounds if that cannot be enforced directly: the exact bound/method
  must be an approved plan amendment, never silently called a hard aggregate cap.
  The parent accounts reaped user+system totals and refuses further work on exhaustion.
  Client work runs in a separately budgeted child; supervisor/accounting overhead
  is recorded separately, not hidden inside the server CPU result.
- External wall supervisor allows 10 seconds additional cleanup; graceful close,
  then identity-checked termination of only owned children and wait/reap. Signal,
  timeout, incomplete file, failed oracle or unresolved cleanup stops the sequence.

Fixture generation/hash inspection is bounded offline preparation, not a benchmark;
freeze its maximum input/output sizes too. No broad process scanning/killing and no
userlab access. Every source revision, manifest, first failure and command remains
retained externally; no replacing an earlier failed snapshot.

## Acceptance and limits

Acceptance requires fresh source/harness review, finite correctness results,
independent measured reproduction, byte/state/error agreement, checked timing trees,
all bounds/cleanup accounted, and explicit negative/inconclusive results where
needed. Exact allocation/validation/serialization findings may support a successor
optimization proposal, never an unreviewed product patch. The corpus is selected
and partly reconstructed; no firmware timing, realtime stability or hardware claim
follows. Larger live workloads and asynchronous arrivals are not represented.
