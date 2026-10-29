# REV-SLC-026 — Canonical observations and operational outcomes

Disposition: APPROVED for the bounded SLC-026 contract.
Date: 2026-10-28. Reviewer: fresh independent observation_reviewer.
Reviewed and independently tested product:
`af73adf974731afe32fdb2be806e1557a0804f7c`.
Active contract: SPR-014 / ITR-026 / SLC-026, ObservationSlice.md.
Master activation baseline:06839fe; accepted previous product:d247e7f.
No product fixes were made by the Reviewer.

## Findings and resolution

All findings below are resolved in the reviewed candidate.

- REV-SLC-026-F001: native outcome validation initially read an undersized
  four-byte prefix beyond its declared size and did not enforce complete native
  session-ID/record-slot rules. Prefix validation now rejects before reading
  version/flags; IDs, known reserved fields, nested prefixes and strides are
  validated before copying. Caller sizes, buffers and unknown tails are retained.
  Independent ASan/UBSan probes cover tiny prefixes, malformed later slots,
  CAPACITY without partial copying and extended output strides.
- REV-SLC-026-F002: observation query/binding initially inspected unknown trailing
  bytes and repeated unbind returned STATE. Unknown tails are now untouched and
  unbind is idempotent. Existing API1 RX delivery while bound obtains its actual
  input order and autonomous parent0, preserving valid correlation.
- REV-SLC-026-F003: unmatched/overflow control facts initially used context0 and
  frameKind3, violating frozen event validation; mismatched RETI could inherit a
  CALL target byte as an IRQ source. Unknown transitions now get distinct tokens,
  opcode-derived type, cleared context knowledge/SP/parent and explicit reason;
  known matching frames retain identity. Master records this narrow representation
  clarification in ObservationSlice.md, without modifying frozen contracts.
  Rebinding after unobserved progress invalidates prior context knowledge. IRQ
  routing depth changes after RETI effect while frame closure waits for retire.
- REV-SLC-026-F004: peripheral-tail failure requires attempted-phase cycle N+1
  while the completed CPU counter remains N. Private phase tracking now emits
  ordered abort/fault evidence without fabricating completed progress. API1 reset
  abandoning a pending instruction emits its abort before ending UART frames.
  Independent TX-end callback failure and partial LCALL stack-failure probes pass.
- REV-SLC-026-F005: stopDetail and controller input admissions needed real
  execution identity. Failed-attempt and retired-step identities are now retained
  separately from trace storage. Callback admissions use the captured pending
  instruction/begin/PC and actual callback cycle; no CPU callback reentry is used.
  Autonomous parent0 remains distinct from board stimulus identity. The retained
  native test exercises TX-end scheduling an input during an instruction tail.
- REV-SLC-026-F006: observation identity exhaustion initially allowed malformed
  context0 CALL/IRQ facts and some device effects before rejection. Final af73adf
  preflights CALL/IRQ allocation before stack effects and suppresses invalid
  unknown-return facts on token exhaustion; frame/access exhaustion also rejects
  before device effects. UINT64_MAX regressions cover ACALL, LCALL, IRQ entry,
  unmatched RET, mismatched RETI, UART frame and access identities. No ID reuse.

## Independent verification

The final candidate was checked in a detached exact-commit worktree. Core
Stage0..Stage4, CPU facade/observation, semantic event/watch/router/runtime,
native session, control3 C/C++, CPU ABI linking with historical headers, legacy
emu-debug facade/process, script host, real Unix control and real Unix UART
outcome tests all passed. Native suites validated 181 actual wire3 responses and
1116 actual wire2.1 responses. The UART process test validated 11 actual responses
against the frozen schema and semantic validator with tracing unavailable,
including RX/TX correlation and reset history retention.

Independent temporary public-API probes, rebuilt on the exact final source with
ASan/UBSan and leak detection, additionally passed:

- all 256 opcodes with observers on/off: architectural snapshot equivalence and
  exactly one begin plus one retire/abort for each attempted instruction;
- RX acceptance, REN-clear rejection, busy rejection preserving the active frame,
  late RI/SM2 drops and exact 160/176-cycle receive sampling/end phases;
- TX-end callback IO failure: no retired instruction, completed CPU counter
  unchanged for the failed cycle, nondecreasing attempted-phase observation time;
- oversized native prefixes with nonzero sentinel tails, repeated unbind,
  captured CODE across FFFF wrap, incomplete LCALL operands after stack failure,
  and unmatched RETI represented without invented known context;
- 2500 outcome events causing 3976 storage insertions, 2952 overwrites and 1024
  retained records including typed loss markers; all cursor states, metadata-only
  reads, capacity and malformed-later-slot atomicity, and exhaustion nonmutation.

All 1736 actual CPU observation records generated by these probes pass the frozen
observation schema and target_wire semantic validator; retained outcome pages
also pass semantic validation. Evidence is under `/tmp/slc026-review/`, notably
accepted-native.log, accepted-core.log, accepted-debug.log,
accepted-process-sdk.log, accepted-adversarial.log,
accepted-outcome-adversarial.log and accepted-semantic.log. The two independent
probe sources and emitted records are retained there for Master artifact copying.

An earlier Git-less test attempt could not run historical Git-object-dependent
ABI helpers; the final detached worktree runs passed. This was a test-environment
limitation, not a product build failure. Master separately owns exact archive
export/build/install and installed capability verification before acceptance.

## SDK and compatibility

Reviewed prerequisite d084e603 snapshots accepted shared raw headers/tools;
accepted manifests and stable CPU API1/board ABI1/observation/trace/control layouts
remain unchanged. Reviewer independently exported that commit to a Git-less
source tree and reproduced both exact accepted SDK archives with their original
SDK source pins while leaving the runtime SOURCE_COMMIT unchanged:

- 0.2 SHA256 5abd71f52925c8fcf299b8b8ed417279a1bc20f2a5f004425bc083f87e9cf91d;
- 0.3 SHA256 405f22f7fc7e0a49d921715102102a4d667ba1ff84b1238494d76353feeb0f53.

Both interface checks also pass on final af73adf. Historical 0.1 directories and
all frozen contract directories are unchanged. New immutable raw-header copies
retain pre-existing upstream whitespace by design; diff whitespace checks pass
outside those exact snapshots. Do not normalize immutable payload bytes.

## Availability and remaining scope

SAB80535 native observation coverage is 125: instruction, control, IRQ, lifecycle,
input and UART. Other variants reject binding. Explicit MOVX and SBUF facts exist,
but complete implicit register/stack/peripheral access instrumentation does not;
ACCESS coverage is correctly withheld. The actual new session capabilities are
observations and uartOutcomes, with outcomePage/native outcomes usable independently
of user tracing. The existing semantic engine owns canonical sequencing; the
operational ring does not introduce another matcher.

Trace configuration/general paging, capture A/B and file output remain unavailable
for later Slices. Conservative context loss currently persists until reset;
recovery of a newly observed child invocation above unknown ancestry is explicitly
retained for SLC-028, not claimed implemented here. Complete REQ-028/029 satisfaction
and composed application exchange are not established by this bounded review.
No physical I/O, external board model, TCP/DAP or publication changes were made.
