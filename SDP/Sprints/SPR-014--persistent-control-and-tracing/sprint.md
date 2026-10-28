# SPR-014 — Persistent local control and canonical tracing

Status: open; SLC-024/025/029 accepted; planned SLC-026 next
Authority: shared user's continued collaboration instruction, relayed and
confirmed by the external simulator Master on 2026-10-28; external planning
acceptance30fe45a. Baseline fe6203f; accepted runtime a03504b remains operational.

Deliver the generic side of persistent structured local control, explicit UART
outcomes and execution trace A (persistent) / B (one invocation) through the
existing semantic engine. External owners retain raw-byte transport, pacing,
board logic, parity mapping and application/operator interpretation.

Sequence: SLC-024 contracts only; planned SLC-025 persistent local control;
SLC-026 canonical producers and safe stops; SLC-027 paging/capture A/file sink;
SLC-028 invocation capture B; then composed verification. Future Slices are not
activated by this plan. Each implementation gets a fresh Worker and Reviewer.
No physical I/O, TCP/DAP implementation, RPM publication or SDP migration here.

SLC-024 acceptance: REV-SLC-024 / VER-SLC-024 and IFC-003 freeze the consumer
contract. The Sprint remains open for its planned bounded implementation gates;
CurrentIndex records no active Iteration/Slice until the next contract is set.

2026-10-28 continuation authority: shared user asks both projects to operate in
Master mode and execute the agreed implementation/integration sequence without
renewed approval. SLC-025 is activated by ControlSlice.md; later Slices retain
separate contracts and reviews before implementation.

Urgent ordering amendment2026-10-28: SLC-029 fixes a confirmed generic indirect
MOV operand defect before external UART integration. SLC-026..028 identifiers
and scope remain planned, and resume immediately after the correction gate.
