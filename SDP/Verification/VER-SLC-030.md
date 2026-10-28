# VER-SLC-030 — Accepted-runtime UART/reset contract audit

Date:2026-10-28. Worker disposition:verified evidence; independent review and
Master Slice acceptance pending. Evidence-only: no product, frozen contract,
consumer pin, existing runtime process or existing lab has been changed.

Audited product:8dd1230704cd6c460167b954f7e987c23e24ff86 (accepted028).
Source worktree:/tmp/slc028-export-8dd1230, clean exact product source.
Actual installed runtime:/tmp/slc028-master/stage/usr/bin/emu-runtime;
SHA25628d8a5c6d8321ef6447a90aa1e861d4b44b4964e7858f00e66459e58d6402171.
This binary identity is the producer's local build, not a claim that a consumer's
separately built executable has identical bytes. Actual hello confirms source.

## UART admission, delivery and acceptance

The result is consistent with frozen0.3 docs02 and the actual API1 implementation:

| Condition at CPU application | CPU API result | CPU UART facts | Required queued session input |
| --- | --- | --- | --- |
| UART mode other than3 | UNSUPPORTED | no frame allocation/start/drop/end | input.failed then session fault; no retry |
| mode3, REN=0 | STATE | fresh frameId, drop/end reason1 REN; no start/accept | input.failed then session fault; no retry |
| mode3, receiver busy | BUSY | fresh rejected frameId, drop/end reason2 BUSY; existing frame retained | input.failed then session fault; no retry |
| supported start, RI already set at acceptance | OK on application | start then drop/end reason RI | ordinary hardware outcome, no delivery failure |
| supported start, SM2 rejects ninth bit at acceptance | OK on application | start then drop/end reason SM2 | ordinary hardware outcome, no delivery failure |

Valid direct CPU API1/observation-deliver calls returning mode/REN/busy rejection
do not themselves poison CPU execution: subsequent cycles/instructions succeed.
The controller deliberately applies stricter existing required-delivery semantics.
The queued failure is removed once, emitted as failed and faults the session.
Calling a rejection harmless just because RX drop/end exist would be incorrect.
Drop/end event status may be OK because the fact was produced normally; inspect
kind/reason, and distinguish the input.failed delivery status STATE/BUSY.

A boardInput acknowledgement is board stimulus admission, not CPU admission or
hardware reception. Board callback success may schedule a later CPU input; that
second input can fail later. A queued payload that has not yet reached the board
callback is cancelled at board stage without an invented CPU order/frame.

Source anchors:emu_cpu_facade.c apply_input/obs_rx_rejected/deliver_observation;
emu_session.c boundary/input_observation; frozen0.3 docs02 admission/UART lifetime.

## Reset, identity and history

Actual private session proof held active RX, active TX and a pending held TX,
plus a future board stimulus and two CPU inputs. Reset at completed cycle60:

- Cancels the three pending inputs in global scheduler admission order, before
  resetting CPU/board. Cancellation facts retain generation1 and cycle60.
  The board-stage cancellation has parent2/CPU order0; CPU cancellations have
  parent3/orders5,6. Canonical source sequences101..103 reflect this order.
- Ends active RX frame2, active TX frame1 and held TX frame3 with reset reason6,
  old generation1/cycle60, sequences104..106. Held TX end does not claim transmit.
- Emits reset lifecycle in new generation2 at cycle0 after those old outcomes;
  resets CPU and invokes board.reset. No replay of cancelled inputs occurs.
- Keeps sessionId and controller lease; public result remains entry/paused.
  Mutating commands must use new generation. Existing old-generation requests
  cannot silently apply to the new execution.
- Does not reuse source sequence, parent/input order, UART frame identity or
  destination storage sequence. Later generation2 starts include TX frame4 and
  RX frame6; later parent/order identities exceed the prior admissions.
- Preserves outcome epoch1/history, independent of enabled user tracing. Trace
  ring epoch and its previous records remain; newest grows98 to105 at reset.
- Preserves trace configuration, A capture and open file sink. B old status is
  incomplete/reset in generation1; explicit reset rearm yields new armed status
  in generation2. No implicit B rearm without the requested policy.

The file test closes normally with218 records, four segments,211927 bytes,
lost0, partialOutput=false, completed operation2 and errorOK. Record storage IDs
are contiguous and file content contains both generations with correct canonical
ordering. Files are schema/semantics validated, including headers and footers.
Outcome/ring histories still have bounded retention: later overwrite can expire
old cursors and must remain explicit. Reset does not grant unlimited retention.

The generic runtime owns its outcome ring and trace storage, not an external
board's opaque TX journal, display cache or signal history. Board.reset semantics
must be audited by that owner. No promise about its cursor reuse or retention is
inferred from producer outcome retention. Likewise the runtime does not own or
reset an external byte socket, kernel buffer, guest clock or host queue.

## Recommended current consumer policy

Keep accepted028 semantics unchanged. While the receiver is cold, outside its
supported mode, or host policy cannot assign a valid current delivery, record the
raw host observation as observed-not-admitted with the precise host reason.
Do not invoke the required-input delivery path merely to manufacture an outcome.
Do not label host policy as physical RX drop, frame acceptance or CPU delivery;
there is no CPU order/frameId for a byte never admitted. A host-local identity
may be recorded in its own namespace alongside session/generation/observation
interval and explicit clock uncertainty. Do not silently retain cold bytes and
replay them later when REN/mode changes or across a reset generation.

Paused virtual time is not a physical arrival timestamp. Bytes already unread in
an external/kernel buffer cannot retrospectively be assigned a certain arrival
cycle or reset generation. Retain unknown provenance or an explicitly declared
host discard policy/interval; do not invent exact physical timing. A readiness
snapshot is not a guarantee about a future queued delivery: firmware may change
mode/REN/busy before that due cycle. Existing fault/no-retry behavior still applies
if an admitted required CPU input later fails. Coordinate host admission policy
at the sole owner boundary and report any uncertainty, rather than changing CPU
semantics or injecting automatic reset/retry/configuration.

A future soft/nonfatal arrival API would need explicit opt-in negotiated contract,
review and release. This audit establishes no need to change existing semantics
and implements no enhancement. It does not prove physical UART behavior or
simultaneous clock equivalence with an external guest.

## Reproducible evidence

Worker harness directory:/tmp/slc030-audit.

- native.c plus run-native.sh compiles exact028 source with strict warnings and
  ASan/UBSan/leak checks. Eight reduced native event projections in native.jsonl
  cover direct/observed mode/REN/busy and late RI/SM2. Existing exact-source UART,
  reset/held-frame and RMW tests also run; native.log records PASS.
- board.c is an independently compiled synthetic ABI1 module; no consumer board
  or firmware participates. probe.py launches only new private Unix runtimes,
  validates actual responses with frozen schema plus semantic assertions.
- Final run:/tmp/slc030-audit/run-1793217838209451342;69 actual validated replies,
  summary.json, responses.json, file-segments.json, per-run argv, exact synthetic
  image/config and original generated JSONL retained. accepted-run.txt identifies
  this final run. Earlier private probe attempts are retained: corrected test
  expectations for cursor spelling and numeric reason constants, no product bug.
- Reproduction:compile board.c using installed include directory, then execute
  probe.py; each execution creates a new directory and exclusive output stems.
  run-native.sh documents exact native compile/execution commands. No existing
  lab attachment, process modification, firmware patch or consumer source edit.

Independent Reviewer maintains separate proof and final disposition in
REV-SLC-030.md. Master alone accepts this Slice and communicates the final audit.
