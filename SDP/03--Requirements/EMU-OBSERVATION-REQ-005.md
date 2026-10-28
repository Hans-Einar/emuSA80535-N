# EMU-OBSERVATION-REQ-005 — Persistent control and observable execution

Status: REQ-027 verified SLC-025; REQ-028 partial coverage125 SLC-026;
REQ-029 operational outcomes verified, trace/capture/file remain targets
Authority: shared-user collaboration request and external concurrence2026-10-28

REQ-027: expose persistent local structured control of the existing session,
with advertised externally-driven advance/step, leases, bounded failure and
existing break/decode/memory operations; preserve accepted compatibility modes.
REQ-028: provide canonical instruction/access/control/IRQ/UART lifecycle and
admission/outcome identity with actual phase/cycles, value validity, partial
failure and explicit retention/loss. No admission acknowledgement masquerades
as successful receive; no unsupported instruction is reported retired.
REQ-029: reuse the accepted trace engine for persistent capture and one dynamic
invocation including nested work; provide bounded paging/file output, truthful
completion/context loss and independent UART outcome retention.

This gate freezes contracts, not these implementations. Full remote/DAP,
physical IO, application/board protocols, joint-clock equivalence, package
publication and SDK tracking migration remain outside this gate.
