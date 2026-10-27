# SPR-014 — Persistent local control and canonical tracing

Status: active; only ITR-024 / SLC-024 is active
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
