# SLC-032 isolated throughput probes

These are audit harnesses, not product changes or performance pass thresholds.
Run from the repository root against known installed static libraries/binaries.
Each RPC invocation creates a new private endpoint, synthetic image and module,
then terminates/reaps only its child and asserts endpoint removal. Never supply
an existing work directory. No command attaches to an existing runtime.

Native probes (same-build private test interfaces need the corresponding source):

```sh
cc -O2 -std=c99 -I. -Ilib/include SDP/Verification/SLC-032-evidence/native.c /absolute/install/lib64/libemusa80535_runtime.a /absolute/install/lib64/libemusa80535_cpu.a -o /tmp/owned-study/native
cc -O2 -std=c99 -I. -Ilib/include SDP/Verification/SLC-032-evidence/session.c /absolute/install/lib64/libemusa80535_runtime.a /absolute/install/lib64/libemusa80535_cpu.a -o /tmp/owned-study/session
/tmp/owned-study/native > /tmp/owned-study/native.jsonl
/tmp/owned-study/session > /tmp/owned-study/session.jsonl
python3 SDP/Verification/SLC-032-evidence/rpc.py --source "$PWD" --binary /absolute/install/bin/emu-runtime --output /tmp/owned-study/new-rpc-directory
```

The parent `/tmp/owned-study` must already exist; the RPC output directory must
not. Run timing jobs sequentially, without competing test jobs. The interpreter
requires the same `jsonschema` dependency as existing protocol conformance tests.
The source root must be a Git checkout; its HEAD identifies harness/test fixture
provenance, **not** the runtime source. `metadata.json` records the actual hello
runtimeCommit and executable SHA separately. Preserve build logs/source archive
provenance alongside the installed libraries. Never reuse arbitrary stale objects.

`native.c`: each of three repetitions executes 2,000,000 machine cycles of the
synthetic `80 FE` infinite SJMP, with 1,000,000 instructions and final PC0 at a
safe boundary. Mode0 is opaque CPU API without canonical binding (the legacy
decode-cache callback still exists); mode1 binds a counting canonical observer;
mode2 sends all canonical facts into the existing default event runtime. Modes1/2
assert exactly 2,000,000 begin/retire events. Mode2 is not a full session/trace
facade: `session.c` covers that separately.

`session.c`: reuses the existing generic board fixture, with the real full session
and canonical/trace machinery. Mode0 calls native control3.advance; mode1 calls
its in-process JSON request facade and parses the reply. Initialization is outside
timing; each combination runs 200 advances at quantum1024/4096/8000, three times.
Every advance asserts exact progress/instruction counters; board boundary calls
must equal completed cycles plus the reset boundary. No user trace is configured,
but canonical production, context semantics and outcome infrastructure remain.

`rpc.py`: the full host loads the independently built synthetic board `.so`.
Three repetitions cover 100 advances for each quantum1024/4096/8000 and each of:
advance-only; resume/advance/pause/status; advance with full generic JSON Schema
response validation. Every response is correlated and each advance checks exact
cycles, instructions, generation and safe boundary. Only the schema branch runs
the generic validator on every response; it is a diagnostic comparison, **not**
a claim that downstream applications use this validator. The resume/pause/status
branch has four requests per iteration but no application history/journal reads.
Total per host: 2,700 advances, 11,808,000 cycles and 5,904,000 instructions.

Native wall time uses CLOCK_MONOTONIC; native process CPU uses
CLOCK_PROCESS_CPUTIME_ID. Python uses monotonic/process_time; runtime child CPU
uses `/proc/<owned-pid>/stat` user+system ticks, quantized at the recorded clkTck
(100Hz on the measured host). Small child-CPU deltas have 10ms quantization error.
RPC measurements include JSON framing/decoding, deterministic checks and normal
OS scheduling. They exclude process/module startup. Offline 1,000-response full
schema validation separately isolates its client CPU cost. The stored response
sample is bounded; exact full-run counter assertions are in the harness and final
status, not an invented exhaustive raw transcript.

This loop does not exercise UART traffic, IRQ handlers, heavy board callbacks,
trace file output or an application GUI. Throughput does not establish peripheral
correctness, complete-system real-time capacity or equivalence of wall and virtual
clocks. No timer/delay/CPU/product behavior was modified to obtain these numbers.
