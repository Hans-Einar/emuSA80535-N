# VER-SLC-032 — Generic execution throughput audit

Status: Worker verification passed; independent review required.
Authority: SPR-014 / ITR-032 / SLC-032 PerformanceAuditSlice.md.
Traceability: MND-001, ARCH-014/015, REQ-027..029, DES-119..124.
Product baseline: accepted028 `8dd1230704cd6c460167b954f7e987c23e24ff86` and
accepted031 `59f7a5c769a4fbf9eb73d0d660e94c58534c1c9c`.

## Result and limits

Repeated isolated synthetic execution preserves exact cycles/instructions across
native/API/JSON/Unix layers and quantum1024/4096/8000. No producer timing defect
or runtime regression is established by this audit. Full Unix sessions with
canonical observation and user tracing unconfigured execute this synthetic loop
at about 2.3–2.4 million machine cycles/s at quantum4096/8000, versus the selected
921600 machine-cycles/s real-time target (11059200 oscillator Hz / 12).

This is a baseline, not an application performance guarantee or exoneration.
Application instruction mix, UART/IRQ workload, board boundary/access callbacks,
output history, journal persistence, GUI scheduling and host contention remain
additional costs. No lag limit, loss reporting, required outcome producer, timer
or delay behavior was changed. No consumer pin or existing runtime was changed.
A measured composed bottleneck may justify a separate reviewed follow-up.

## Reproducible evidence

Committed harnesses and instructions: `SDP/Verification/SLC-032-evidence/`.
Durable Worker evidence:
`/home/warloc/git/emuSA80535-N-artifacts/audits/SLC-032-study/worker/`.
`manifest.json` inventories exact metadata, input/config/module bytes, harness
snapshots, command lines, response samples, full measurement rows and summaries.
`metadata.json` records exact static-library hashes, compiler and harness flags.
Runtime source/archive build/install provenance is retained in
`SLC-028-ARTIFACT.json` and `SLC-031-ARTIFACT.json`.

Measured executables:

| Product | Executable SHA256 |
|---|---|
| 028 | `28d8a5c6d8321ef6447a90aa1e861d4b44b4964e7858f00e66459e58d6402171` |
| 031 | `eb308d9d7085454e09649f6a88d39d18be0b4d94ab1d4eb43290476b365c87ec` |

These are producer-local installed builds; independently compiled consumer
executables may have different hashes. Every RPC run records actual hello
runtimeCommit and base contract manifest. The harness checkout HEAD `3e2f249`
is audit-source provenance, not the product version. Native probes link the
installed031 CPU/runtime libraries, with exact hashes in durable metadata.
All test runtimes were created privately, explicitly terminated and reaped;
endpoint disappearance and clean stdout/stderr/exit were asserted.

## Workloads and exact checks

The only CODE image is `80 FE` (SJMP to itself), zero-padded to 65536 bytes.
It starts at PC0 with seed0 and default reset peripherals. The workload is
synthetic and makes no application reachability claim.

- Native facade: three repetitions for each of unbound canonical observer,
  bound counting observer, and bound existing event runtime. Each run executes
  2,000,000 cycles / 1,000,000 instructions, final PC0/safe boundary; bound modes
  assert exactly 2,000,000 begin/retire observations. Unbound mode still has the
  existing legacy decode-cache callback, so it is not a bare `tick` benchmark.
- Native session: three repetitions × two interfaces × three quanta. Each row
  executes 200 advances; actual cycles/instructions and every progress response
  are checked. Board boundary count equals cycles+1, including reset boundary.
  Mode0 calls native control3.advance; mode1 serializes and parses JSON through
  the in-process control facade. Both retain full session observation/context
  and trace infrastructure without configuring user trace output.
- Unix host: both028/031 execute three repetitions × three workloads × three
  quanta ×100 advances: 2700 advances, exactly 11,808,000 cycles and 5,904,000
  instructions per host, PC0 final. 5439 requests precede explicit terminate
  (request5440). Workloads are advance-only, resume/advance/pause/status, and
  advance with generic full-schema validation. Every reply is correlated, and
  every advance checks exact cycle/instruction/generation/safe-boundary facts.
  The first16 exchanges are retained as a bounded sample; the evidence does not
  pretend this is an exhaustive raw request transcript.

No UART/IRQ or trace-file benchmark is claimed; existing acceptance records
remain their behavioral evidence. The zero-trace path does not disable required
canonical input/UART outcomes or context production.

## Observed throughput

Values are medians of three rows in million completed machine cycles/s.
They are observations, never pass/fail thresholds.

| Layer/workload | Quantum | 028 | 031 |
|---|---:|---:|---:|
| Native facade, canonical unbound | — | not measured by Worker | 13.242 |
| Native facade, counting canonical observer | — | not measured by Worker | 4.077 |
| Native facade, canonical event runtime | — | not measured by Worker | 2.582 |
| Full native session advance | 4096 | not measured by Worker | 2.408 |
| Full native session in-process JSON | 4096 | not measured by Worker | 2.368 |
| Unix advance-only | 1024 | 2.051 | 2.062 |
| Unix advance-only | 4096 | 2.332 | 2.353 |
| Unix advance-only | 8000 | 2.398 | 2.428 |
| Unix resume/advance/pause/status | 4096 | 2.025 | 2.128 |
| Unix resume/advance/pause/status | 8000 | 2.212 | 2.261 |
| Unix advance + full schema validation | 4096 | 0.857 | 0.836 |
| Unix advance + full schema validation | 8000 | 1.268 | 1.254 |

The extra three control operations per iteration have measurable cost even with
no application history or disk journal. Full generic JSON Schema validation is
also costly, but this is an isolated diagnostic: current downstream control code
uses lightweight explicit checks, so the schema comparison does **not** explain
its reported lag. Small differences between028 and031 are below what these
uncontrolled-host observations can attribute to a product change.

Native timings use CLOCK_MONOTONIC plus CLOCK_PROCESS_CPUTIME_ID. Unix client
uses Python monotonic/process_time; its owned child uses `/proc/<pid>/stat`
user+system ticks, with recorded 100Hz resolution (10ms quantization). Source
checks confirm correct tick field indexing. Native process CPU and wall time,
all Unix client/child CPU deltas, host load and Python/compiler versions are
retained. Startup/build cost is outside measured windows. Worker timed jobs ran
sequentially; Reviewer was notified before starting separate timing. Other host
activity, CPU-frequency changes and operating-system scheduling were not pinned.

## Source findings and next decisions

At baseline031, `emu_session.c:761` executes one CPU cycle, trace tick, board/input
boundary and full snapshot per completed machine cycle. This preserves cycle
ordering and is not safe to coarsen merely for throughput. Native sessions bind
canonical observations regardless of user tracing. `emu_cpu_facade.c:140` and
`:344` decode captured instruction evidence at begin/retire; the legacy callback
at `:494` also decodes to maintain its history. `disasm.c:1284` initializes a
whole temporary CPU view to reuse the decoder safely. These are identifiable
cost candidates, not demonstrated defects or authorized optimization patches.

`emu_debug_runtime.c:658` projects canonical facts into the existing event
bus/router; required outcome retention and dynamic call/IRQ context must survive
any future optimization. `runtime/control.c:321` uses an idle poll timeout;
ready Unix sockets wake the poll, so it does not impose a fixed100ms RPC delay.
No product optimization is recommended solely from this synthetic baseline.
Prefer measuring per-operation composed costs, callback/journal work and pacing
scheduling before selecting the next bounded producer or consumer correction.

The separate read-only state/function-probe study and independent reset/recovery
proof are integrated by Master/Reviewer. Synthetic forced entry must remain
explicitly distinguished from preserved-state PC mutation and natural execution.
