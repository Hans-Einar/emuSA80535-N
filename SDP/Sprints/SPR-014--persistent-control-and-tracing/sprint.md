# SPR-014 — Persistent local control and canonical tracing

Status: open; SLC-024/025/026/027/028/029 accepted; composed intake pending
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

Authorized follow-up: SLC-030 UART/reset audit accepted without runtime changes;
SLC-031 separately negotiated read-only breakpoint enumeration active. Consumer
concurred protocol before freeze; existing028 remains consumer pin.

SLC-030 audit and SLC-031 opt-in enumeration now accepted. No implementation
Slice active. REQ-028 full ACCESS gap remains explicit; later work needs its own
bounded contract. Consumer integration pins are independently selected.

2026-10-29: PerformanceAuditSlice.md activates evidence-only SLC-032 under new
user stabilization/research authority. New owned isolated tests allowed; existing
user processes/pins untouched. Measure producer/transport overhead and audit
probe/recovery contracts before any separately reviewed product correction.

SLC-032 accepted through REV/VER-SLC-032, Worker3f8233c and fresh review1ee99aa.
Generic throughput and ordinary reset/reconnect proof preserved; no CPU timing
defect or product optimization established. ProbeDocumentationSlice.md activates
SLC-033 to correct captured-prefix wording and document existing research/recovery
contracts, including RX-drop value validity. No new runtime pin needed.

SLC-033 accepted documentation candidate c6a2a8707403a11800091ba2aba644d4a43896ca,
fresh review432f5188c61840f6c53c6705f121ff4bf1f8e493 through REV/VER-SLC-033.
runtime/RESEARCH.md records existing observation, synthetic-entry and recovery
limits; README fetched-prefix overclaim corrected. No new runtime artifact/pin
or active implementation Slice. Master remains available for measured generic
defects under standing stabilization authority; consumer scheduling/recovery
and validation corrections remain externally owned.
