# REV-SLC-028 — One observed dynamic invocation

Status: APPROVED for the exact candidate below; Master Slice acceptance remains separate.
Reviewer: fresh independent invocation_reviewer.
Active contract: SPR-014 / ITR-028 / SLC-028, InvocationSlice.md.
Activation:cc4947b; accepted027 product740f60863bc52481e589f219ba54df8f5bd0fc82.
Reviewed product:8dd1230704cd6c460167b954f7e987c23e24ff86, including CPU
context commit9f48cfe and the final Invocation Worker correction set.

## Review boundary

Review the combined Context and Invocation Worker implementation against frozen
SDK0.3 docs02/04/05/06, accepted canonical producer and existing matcher/router.
Master independently verifies exported/installed artifacts and accepts the Slice.
No frozen ABI/schema/SDK mutation, board knowledge, full ACCESS claim or additional
clock/transport is within this gate. Product corrections remain Worker-owned.

Independent harnesses under /tmp/slc028-review exercise actual Unix CPU execution,
validate every response against frozen schema and semantic rules, and use a
separately compiled generic board fixture for CALL-tail failure and staging
pressure. Further native/file probes inspect boundaries not externally callable
while the execution owner is running. Final disposition requires exact committed
candidate verification, regression evidence and resolved findings.

## Focused risks

Known top CALL mode2 eligibility after successful retirement; initial unobserved
returns versus observed-frame SP/type/target mismatch; known child recovery above
unknown ancestry without restoring unknown parents; nested CALL/IRQ/RETI and
recursive targets; filtered/disabled closing return; terminal retirement inclusion.

Per-destination staging must preserve original coalesced A+B routes and source
order, strip only failed candidate B on abort/overflow, and never recount A or
re-evaluate gates. Reset must retain old terminal status/generation and allocate
an explicit successor or report exhaustion without CPU fault. File staging may
not weaken027 asynchronous barrier, loss or termination semantics.

## Findings during development

F001: a provisional B CALL inside IRQ suppression that aborts could leave its
pending suppression summary in the old router. A later flush/new scope would
expose views belonging to a failed candidate. Worker identified the edge and
Reviewer independently reproduced it with native canonical ingress: pending
summary count remained1 after CALL abort. Worker added selected candidate discard.

F002: discarding all pending suppression for that trace also erased older ordinary
tracing suppressed before captureStart. Reviewer reproduced a pre-arm NOP begin/
retire summary disappearing after the later candidate CALL abort. Worker now
flushes that trace's preexisting summary after successful start validation and
before allocating the new scope. Failed start requests remain nonmutating. The
independent sanitizer fixture now proves old count2 survives while failed candidate
views disappear, and unrelated trace count3 survives selected discard/flush.

Both fixes pass independent ASan/UBSan/leak tests on the exact committed final
candidate. Worker regressions preserve both reproductions. Product fixes remain
Worker-owned; no Reviewer product changes were made.

## Preliminary independent evidence

Actual Unix harness /tmp/slc028-review/review_wire.py validates534 responses for
shared A+B ordered/coalesced records and original counters, mode2 top-frame
selection/rejection at nested CODE entry breaks, disabled and filtered closure,
initial unobserved RET followed by known child recovery, wrong RETI/SP/target,
CALL-tail board failure,300 admissions causing provisional stage overflow,
record-limit1,64 reset/rearm retention and exhaustion, all three IRQ policies,
recursive same-target calls and depth overflow. Additional40 actual file replies
verify shared A+B plus nested third capture, exclusive JSONL output, canonical
configuration, ordered unique source/storage identities and record-limit1.

Native harness /tmp/slc028-review/native.c independently verifies staging hides
all shared destination views until retirement; config, captureStop and close are
BUSY during unresolved stages; mode2 cannot bind provisional CALL; native request
prefix/version/flags and output tails; committed mode2 selection, root terminal
retire and explicit load without rearm. It includes F001/F002 adversarial cases.

## Exact candidate verification and disposition

Reviewed final8dd1230704cd6c460167b954f7e987c23e24ff86 in a fresh detached
worktree /tmp/slc028-review/candidate. Built runtime, SDK and compatibility
emu-debug there; actual --version identifies this complete product hash. The
worktree remained clean after checks. No frozen interfaces/ or public contract
headers changed from activation. CPU API1/board ABI1 and coverage125 remain.
New context frames arise only from actual observed entry; successful CALL becomes
mode2 eligible after retirement. Old unknown ancestry remains unknown after
known child return. There is no new per-event allocation, JSON work or file IO
in CPU callbacks; destination staging is fixed256 records. Existing router and
canonical bus remain authoritative for matching, order and suppression.

Passed on exact final candidate:

- make -j4 emu-runtime sdk emu-debug.
- make -C tests -j4 trace-native-test cpu-observation-test control3-test
  outcome-test runtime-session-test debug-event-test debug-trace-test
  debug-runtime-test, including all-opcode observer equivalence, CPU context
  recovery/IRQ timing, native prefixes/cursors/staging/suppression,181 actual
  wire3 and1116 legacy wire2.1 responses.
- make runtime-invocation-test trace-test runtime-host-test runtime-control-test
  runtime-outcome-test runtime-trace-file-test interface-target-check
  interface-check:173 invocation replies/48 JSONL, persistent tracing,11 outcome
  replies,55 file replies/202 JSONL, host/control/compatibility and immutable
  SDK/header/standalone export checks.
- Independent review_wire.py using the detached executable:594 actual frozen
  schema+semantic responses, including the preliminary cases and shared file
  normal/limit1 with a nested third capture. Final file evidence contains76
  JSONL lines with exact product provenance, ordered source/storage identity,
  original coalesced route sets and matching capture counters.
- Independent native.c rebuilt against exact source with ASan/UBSan, leak
  detection and halt-on-error: partial-stage atomicity/BUSY, uncommitted CALL
  and IRQ mode2 refusal, typed committed mode2, prefix/tails, root closure,
  load without implicit rearm, failed candidate suppression and preservation
  of both older same-trace and unrelated trace summaries all pass.
- Exact CPU observation suite independently rebuilt with ASan/UBSan/leak
  detection and halt-on-error: passes all opcodes and context recovery,
  mismatch/unknown/overflow/IRQ/CALL/RET timing cases.
- git diff --check and exact worktree cleanliness pass.

Master-approved runtime policy is explicit in InvocationSlice/runtime README:
mode1 startSequence is successful CALL retirement; staged CALL may precede it;
armed cycles do not count. Mode2 uses latest canonical source at explicit bind.
Staged counters are original-time, with no recount; maxRecords1 can bind and
immediately end incomplete/limit after retaining CALL alone. stopCpu applies to
terminal B return/incomplete: ordinary requested scope stop is paused/budget and
captureStatus supplies the precise reason. Independent successful-return test
stops at root retire with matching stopDetail; actual MOVX failure retains
fault reason. Existing reset entry CODE-break behavior retains its priority.

Exact logs: final-build.log, final-native.log, final-host.log,
final-independent-wire.log, final-independent-native.log, final-cpu-asan.log
under /tmp/slc028-review. Harnesses/native.c, review_wire.py, board.c and actual
independent-wire.json/final-independent-files.jsonl are available there for the
Master's durable evidence collection. No uncommitted product fix is required.

F001/F002 resolved. No remaining blocking finding. Approve SLC-028 integration
and Master artifact verification. This approval covers observed dynamic scope
and existing declared coverage; it does not claim full ACCESS completeness,
physical I/O, real-time performance, board correctness, DAP/TCP or publication.
