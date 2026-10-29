# REV-SLC-030 — Independent UART/reset contract audit

Status: APPROVED for evidence-only SLC-030; Master acceptance remains separate.
Reviewer: fresh uart_reset_audit_reviewer. Active contract: SPR-014 / ITR-030 /
SLC-030, UartResetAuditSlice.md, activationfc85087.
Audited product:8dd1230704cd6c460167b954f7e987c23e24ff86 (accepted028).
This is an evidence-only audit, not review of a UART behavior change or new API.
No existing runtime, user lab, consumer repository or frozen contract was modified.

## Independent proof

Private native harness /tmp/slc030-review/native.c reuses accepted fixture setup
but adds independent assertions against exact accepted source in the clean
/tmp/slc028-export-8dd1230 checkout. Compiled directly from that CPU facade/core/
opcodes/decoder source, then separately under ASan/UBSan with leak detection and
halt-on-UB. Both runs pass; native event logs and parsed events are retained.

- Wrong UART modes0/1/2 with REN set return UNSUPPORTED before frame allocation,
  with no UART frame fact. Subsequent CPU execution succeeds.
- Mode3 REN0 returns STATE and emits same-cycle RX_DROP/RX_END, reason REN,
  fresh frame identity, no RX_START or acceptance. The direct CPU remains usable.
- Busy RX returns BUSY with a different rejected frame's DROP/END; previously
  active RX can subsequently complete. Rejection is not evidence that another
  frame started or succeeded.
- A successfully started RX later rejected for RI or SM2 produces ordinary
  UART DROP/END with no CPU fault. The API's initial successful start and eventual
  reception are separate facts.
- Direct API reset with active RX, active TX and a held TX ends all three old
  identities at old cycle/generation, reason RESET. A pending instruction tail
  can additionally emit ABORT before them. It does not claim the held frame was
  transmitted. New lifecycle RESET has new generation and cycle0; subsequent
  frames have fresh increasing IDs, and an already accepted old input order is
  stale after reset. Direct API binding-generation changes remain host-owned.

Independent session harness /tmp/slc030-review/session.py uses the exact installed
028 binary and a privately compiled generic ABI1 module. All50 actual replies
pass frozen schema and semantic validation. It verifies cold and REN0 required
queued input delivery faults the session, even though direct CPU API rejection
need not fault the CPU. It then verifies queued board/CPU cancellation in global
admission order (not scheduled-cycle order), TX active/held reset ends, retained
outcome history/epoch, new-generation reset in trace, persistent capture A,
increasing new identities and no invented RX frame for a cancelled queue item.
Board-stage facts retain parent identity and cpuInputOrder0; CPU-stage identity
is distinct. No legacy board order field is fabricated in canonical events.

The Worker's final69-response actual Unix/file audit was independently
recompiled and rerun under /tmp/slc030-review/reproduction, using a new owned
runtime only. It passes simultaneous RX/TX/holding reset, pending board and CPU
cancellations, canonical order, retained ring/file storage identities, capture A,
B reset successor, file completion and cold/REN0/busy failure cases. Reviewer
source and actual replies confirm cancellation precedes old-frame ends, which
precede the new reset source. File remains open across reset and closes without
loss or partial output. Worker evidence carries the full operation/record counts.

Evidence: /tmp/slc030-review/native.jsonl, native-asan.jsonl, native-events.json,
session-evidence.json, session.log, reproduction-final.log and reproduction/run-*.
manifest.json hashes the independent harness/logs and records exact source.
Initial private harness assumptions about an optional reset-tail ABORT and a
nonexistent boardInputOrder field were corrected; neither was a product defect.

## Contract conclusions and consumer policy

Accepted CPU API1 and frozen0.3 deliberately distinguish scheduling admission,
due delivery and UART acceptance. Session delivery errors preserve fault/no-retry
behavior. There is no current optional best-effort UART delivery operation that
turns wrong-mode/REN-disabled arrivals into ordinary accepted scheduler outcomes.
No silent change to that contract is warranted by this audit.

A host may truthfully journal observed-but-not-admitted socket bytes with explicit
policy reason and observed session/generation/cycle identity. Such records are
host observations, not producer UART DROP, accepted input, physical arrival or
successful reception. They must not allocate fictitious producer frame/input
identities or later replay the bytes as if they arrived after initialization.

Four boundaries are essential:

1. Paused is not equivalent to cold. Socket/wall-time observation while virtual
   time is frozen does not establish a physical arrival cycle in the MCU model.
2. A readiness inspection is not a reservation. Firmware can change mode/REN
   before an admitted future input is due; any later rejection still faults
   according to the current required-delivery contract. Do not auto-retry it.
3. Runtime reset cancels only runtime-admitted queue entries. Host/socket buffers
   are separately owned, requiring explicit old/new-generation disposition and
   no replay. Board payloads not yet applied never acquired CPU frame identity.
4. Producer outcome/storage identity continuity does not specify an external
   board's TX journal or transport cursor. Already transmitted/delivered bytes
   are not retracted by reset; held TX END with reset reason proves cancellation,
   not transmission. Consumer board generation/history policy must be explicit.

Physical RX behavior, electrical fidelity, whole-machine synchronization and
external board journal completeness are outside this evidence-only proof. No
physical input or machine control was accessed. Observation coverage remains125
without the ACCESS bit; accepted SDK and product pins are unchanged.

## Final disposition

Reviewed final Worker verification record at bd87426, VER-SLC-030.md, and its
accepted-run.txt-selected evidence. Independent reproduction of the final Worker
harness passes69 actual replies, including the added explicit trace-ring history
queries; original reviewer proof remains50 replies plus independent native
regular/sanitized cases. The final Worker native evidence agrees with the
independent direct API conclusions. All tests use immutable accepted028 source
or its isolated installed executable; no runtime product diff is proposed.

No blocking finding. The report clearly separates CPU status, queued session
fault policy, ordinary RX outcomes and host-local nonadmission. It does not
claim board TX cursor continuity, physical RX/drop equivalence or guest-clock
synchronization. Preserve current APIs/pins; any later nonfatal arrival behavior
requires its own explicitly opted-in contract and review. Approved for Master
integration and concrete consumer guidance. This approval does not cover the
separately proposed breakpoint enumeration extension.
