# SLC-034 — Reduced native outcome cost diagnostic

2026-10-29. Closed under the explicit human economy override. Master executed the
reduced diagnostic; no fresh reviewer or independent reproduction was requested
or performed. This does not accept the original full native/Unix measurement plan.
Scope and override: `NativeOutcomeEconomyCheckpoint.md` in the active Sprint.

## Result

One plain and one instrumented native pass each returned all 758 selected pages,
11,872 records. Saved checks passed exact response-byte equality, complete original
page-body equality, status/length/client identity, unchanged CPU/session/ring state,
and raw interval ordering, containment and aggregate reconciliation. The corpus
contains 5,936 canonical records and 5,936 retention markers; all remain present.

| CPU scope | Plain | Instrumented |
| --- | ---: | ---: |
| Native request total | 23.218 ms | 27.975 ms |
| Full fixture process | 73.366 ms | 78.013 ms |
| Synthetic ring seeding | 8.323 ms | 8.132 ms |
| Fixture state checks | 29.543 ms | 28.836 ms |

Instrumented request phases: allocation/parse/extraction 3.652 ms, schema
1.261 ms, dispatch 19.821 ms, response assembly/free 0.920 ms. Dispatch **includes**
page validation/copy 1.292 ms and canonical serialization 16.373 ms. Do not add
parent and child totals. These single short passes include clock overhead; they
do not support scaling profiled proportions to live operation or subtracting
separate consumer measurements to estimate Unix cost. Plain mean request CPU was
about 30.6 microseconds/page. Full process totals include setup, seed, state checks
and artifact retention; they are not native request totals.

The measured native request cost does not justify a producer optimization cycle
at this time. Actual Unix/frontend and end-to-end scheduling costs remain unknown.
No production pacing fix, cold-reset pass or sustained stability is demonstrated.
No product source, runtime artifact pin, API, retention or lag policy changed.

## Essential correctness and retained failures

Both original variants compiled successfully. The first correctness attempt failed
before a result because the fixture used a string-only predicate to test JSON
boolean success. A separate retained fixture copy corrected only that predicate.
The next attempt reached case ten and exposed an erroneous fixture expectation:
duplicate JSON keys cause accepted028 to close without a reply, not return an
INVALID frame. A matching instrumented attempt confirmed the same behavior.

To avoid expanding fixture work, the reduced gate checked the ten retained results
per variant: eight page fullnesses plus malformed JSON and duplicate-key closure.
All twenty passed exact bytes/body/error-close/state and timing-tree checks before
the corpus pair ran. The full 26-case matrix did not pass and the other sixteen
cases per variant were not exercised. Original failed runs remain preserved.

## Evidence and limits

External evidence root:
`/home/warloc/git/emuSA80535-N-artifacts/audits/SLC-034/economy/`.
`native-corrected/checked.json`, both `.records` files and `processes.ndjson` retain
the measured pair; `correctness-native-corrected/` retains the reduced gate.
`build-evidence/` and `build-corrected-evidence/` retain every compiler stage.
Original setup failure and two short owned GDB diagnostic logs remain present.
GDB diagnostics have timeout bounds but no separate CPU accounting; they are not
measurement results or proof of the original complete accounting contract.

Build child CPU was 14.196 seconds initially and 7.035 seconds for the fixture-only
correction. Measured child CPU was 0.151379 seconds total; both exited zero and were
reaped. Correctness stops exited 73 and were reaped. Native writers enforced their
prewrite byte limits; measured retained files were 7,953,805 and 8,575,173 bytes.
Build reservations covered file extents, not internal compiler rewrite volume.

No Unix server, QEMU, consumer lab, userlab attachment, CPU advance, replay campaign
or repeated corpus pass occurred. Synthetic observed ring windows reproduce page
payloads; they are not checkpoints or proof of native event generation. Prior
source reviews remain historical preparation evidence. Further work requires a
new concrete cost target; no follow-on implementation is active.
