# REV-SLC-032 — Independent throughput and recovery audit review

Status: **APPROVED** for the bounded evidence-only SLC-032.
Reviewed candidate: `3f8233c4437dbefd7aaf6909c20b2bdec963f165`.
Baseline activation: `3e2f249f1808c82f25b9814d81205aeccbbf47ac`.
Reviewer: fresh `throughput_audit_reviewer_032`, separate from Audit Worker.
Date: 2026-10-29. Authority: SPR-014 / ITR-032 / PerformanceAuditSlice.md.

## Decision and scope

The candidate adds three generic audit harnesses, their reproduction guide and
VER-SLC-032 only. No product implementation, public header, frozen contract,
consumer pin or existing runtime was changed. Worker claims accurately separate
synthetic throughput from application behavior and avoid attributing a host pacing
failure to architectural CPU timing. No blocking finding remains for this audit.
This approval does not recommend an optimization, new ABI or real-time guarantee.

Read AGENTS, CurrentIndex, mandate/study/requirements/architecture/design, Sprint
and latest Handoff before review. Inspected every added harness and the native
CPU/session/control/observation paths it measures. Independently verified all26
Worker inventory hashes and byte equality of its retained harness snapshots with
the reviewed commit. Actual hello product identities and executable hashes match
accepted028/031, independently of the audit checkout HEAD. Same-build native
probes use the recorded installed static libraries; no arbitrary stale product
object or existing runtime is used.

## Independent reproduction

New private `/tmp/slc032-review` processes only; timed runs were sequential after
Worker explicitly completed its timed jobs. Durable evidence:
`/home/warloc/git/emuSA80535-N-artifacts/audits/SLC-032/review/`.
`manifest.json` inventories24 files, including separate native/session rows,
Unix metadata/rows/samples, an independently written recovery driver and its full
62-exchange schema-validated transcript. `reproduction-commands.txt` records exact
commands; `summary.json` retains median/minimum/maximum rather than thresholds.
GCC14.3.1, `-O2 -std=c99`; host scheduling/frequency/load were not controlled.

- Native facade:9 rows each for028 and031, covering unbound, counting-observer
  and existing event-runtime modes; every row asserts2,000,000 cycles,
  1,000,000 instructions, PC0/safe boundary and exact observation counts.
- Native session:18 rows each for028 and031, native control3.advance and
  in-process JSON, quanta1024/4096/8000, three repetitions. Exact returned cycle/
  instruction counts and board boundary count cycles+1 pass.
- Unix host:27 rows each for028 and031, three repetitions of advance-only,
  resume/advance/pause/status and isolated full-schema validation at all three
  quanta. Each host completes11,808,000 cycles/5,904,000 instructions,
  2700 advances,5439 requests before explicit terminate. Source identities,
  generation, safe boundaries, PC0 and counters pass. Child exit/stdout/stderr
  and endpoint removal are checked; all owned runtimes are reaped.
- Native clocks are monotonic wall and process CPU. Python client uses monotonic/
  process_time. Checked `/proc/<owned-child>/stat` fields11/12 after the comm
  separator: they are user/system ticks. Recorded100Hz quantization is explicit.
  Startup/compilation are outside timed regions; raw reply samples are bounded,
  not falsely presented as complete transcripts.

Representative independent medians, million completed machine cycles/s:

| Workload | Quantum | 028 | 031 |
|---|---:|---:|---:|
| Facade, canonical unbound | — | 13.442 | 13.653 |
| Facade, counting observer | — | 4.031 | 4.568 |
| Facade, event runtime | — | 2.574 | 2.800 |
| Native session advance | 8000 | 2.496 | 2.474 |
| Unix advance-only | 4096 | 2.373 | 2.318 |
| Unix advance-only | 8000 | 2.448 | 2.360 |
| Unix resume/advance/pause/status | 8000 | 2.227 | 2.178 |
| Unix advance plus full schema | 4096 | 0.857 | 0.799 |

The direction of the small028/031 Unix difference reverses the Worker's result;
no version performance regression/improvement can be inferred. A two-byte SJMP
loop excludes application instruction mix, UART/IRQ, substantial board callbacks,
output-history reads and GUI/journal work. Neither faster-than-target synthetic
throughput nor the artificial full-schema slow case identifies the composed lag
cause. Actual consumer validation is lightweight; the full-schema branch is a
counterfactual diagnostic only. Further measured composed profiling is justified.

## Independent reset/reconnect proof

The reviewer-authored `recovery_probe.py` launches each exact installed028/031
binary with a private synthetic board and CODE image; all62 responses validate
against frozen wire3.0. For each binary:

1. Advance1000 cycles, pause, queue a future board stimulus and set CODE breakpoint0.
2. Release controller, disconnect and attach a new controller: same session,
   generation1/cycle1000/paused; strictly later lease identity.
3. Reset returns generation2/cycle0/paused with no fabricated leaseId field.
   Old-generation resume returns STALE. Preserved breakpoint0 fires before any
   instruction. Clearing it and advancing3000 cycles yields board applied0:
   the pre-reset queued stimulus was cancelled, not replayed.
4. Queue an actual unsupported-mode UART input through the board fixture. Required
   CPU delivery faults at cycle3002; status remains responsive and resume returns
   STATE. Reset returns generation3/cycle0/paused; an explicit30-cycle advance
   succeeds and board applied0 confirms no implicit replay.

Source review confirms reset dispatch accepts paused/faulted sessions before the
normal faulted-command rejection (`emu_session.c:1342..1354`); control3 validates
session/generation/controller lease first. A host pacing failure does not itself
become a producer CPU fault. Keeping an owner recovery service alive is a host
policy; this proof does not claim to exercise that consumer service.

Reset success is conditional: identity exhaustion, CPU/board reset failure,
callback failure or retained fatal observation errors can still reject/fault a
reset. This is ordinary queued-input and paused-control recovery evidence, not
an unconditional recovery guarantee. Producer reset cancels only admitted queues;
it does not retract socket bytes or clear an external transport's backlog.

## Probe-contract observations for follow-up

Source-confirmed: public CPU API1 and wire3.0/3.1 have no preserved-state PC,
register/RAM write, checkpoint/restore or synthetic-call injection. `reset(entry)`
is cold reset plus entry selection. Paused state/memory reads and canonical trace
must be recorded with knownness, source/cycle and generation; a synthetic bootstrap
CALL cannot establish natural firmware reachability or full warm-state equality.

The live runtime README's complete-retired-disassembly statement is too strong:
untaken CJNE may not fetch its offset (`opcodes.c:1185..1206`), and
`em8051_decode_captured` deliberately leaves text empty for a partial prefix.
Frozen0.3 accurately requires actually captured bytes only. This pre-existing
README issue should be corrected in its separately bounded documentation gate;
no change to CPU semantics or frozen contracts is indicated by this audit.

Additional source-confirmed consumer contract caveat: RX_ACCEPT19 has known16-bit
newValue, whereas RX_DROP20 may correctly retain unknown value0/width8/unsigned
(`emu_cpu_facade.c:118..176`, frozen observation doc). A client requiring a known
receive payload for every drop is overstrict. Ordinary drop statusOK is separate
from required-input failure statusSTATE/BUSY. The exact recently reported failing
consumer page was not retained here, so this source finding does not prove which
individual live outcome triggered that consumer error. No producer change or
extra benchmark is warranted on that evidence alone.
