# ITR-030 / SLC-030 — UART reset contract audit

Status: active. Sprint SPR-014. Baseline accepted028 product8dd1230;
Master state72ebca4. Authority: explicit shared-user consumer coordinator request
for urgent bounded UART/reset audit and native actual proof, then a separately
opt-in read-only CODE breakpoint enumeration capability. No live lab access.

This Slice is evidence-only: a fresh Audit Worker runs private synthetic CPU and
session probes against exact accepted028 source; a separate fresh Reviewer
reproduces and checks conclusions. No product behavior, public contract, pin,
existing process, consumer repository or firmware changes. Read AGENTS,
CurrentIndex, Sprint/Handoff, lifecycle01..05 and frozen0.3 observation/control/
trace contracts. Consumer lifecycle documents are context only, outside product.

Verify wrong UART mode, REN disabled, busy RX, RI/SM2 completion loss separately.
Distinguish raw CPU API1 status, CPU observation facts, session queued-delivery
fault policy and host observed-not-admitted policy. Test queued CPU and board
input reset cancellation, active RX/TX/holding-slot end/reset outcomes, old/new
generation ordering, nonreused input/frame/source/storage IDs, ring/trace/file
retention and fresh-cycle0 semantics. Do not equate admission with reception.

Worker owns isolated harness/evidence under /tmp/slc030-audit and proposed
SDP/Verification/VER-SLC-030.md; Reviewer owns SDP/CodeReview/REV-SLC-030.md and
independent isolated proof. Master sends immediate source-based guidance marked
pending native verification, then accepts final audit and consumer handoff.

No UART enhancement is presumed necessary: preserve old rejection/fault semantics.
If existing API cannot express cold arrivals without fault, recommend host-local
observed-not-admitted records with precise reason and clock/generation identity,
not fabricated physical RX drops or delayed replay. Explicitly identify limits
on queued board payloads and externally owned TX ring semantics.

A concurrent read-only design study may propose enumeration negotiation/schema
and read-only semantics, but product implementation waits for a separate active
Slice and proposal communication. Frozen0.3 and old pins remain immutable.
