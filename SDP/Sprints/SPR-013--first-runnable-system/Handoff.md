# Handoff — First runnable system

Status: active SPR-013/ITR-021/SLC-021

The direct user authorizes continued cross-repo implementation to a buildable,
runnable first system. Ponsse's agent owns SDK intake/board adapter/integration.
Master here accepted that division and activates CPU facade first; session,
loader and local control host follow under a separately recorded Slice.

Frozen SDK source7d924cc, contract0.1.0, nativeAPI1, boardABI1.0. Accepted package
identity is in SDP/Verification/SLC-020-ARTIFACT.json. Ponsse's initial planned
adapter requests SAB80535, oscillator11059200, empty config, deterministic reset,
no cycle0 CPU injection and mandatory callbacks without PEEK/effective-pin
requirements. Its later scoped work supplies opaque stimuli/inspection.
These external plans are coordination facts, not accepted external product.

First runnable gate can use a local CLI/harness; network and DAP are not required
for this initial gate. No full machine boot claim without evidence. Do not
publish RPMs or upgrade SDP. Preserve source-scoped DEBUG aliases and history.

Implementation finding: core callbacks need an opt-in checked-execution abort
predicate to propagate module failures before later effects. Master authorized
this generic legacy-header addition under SLC-021, creating bundle0.2.0 while
preserving0.1.0 and board ABI1.0. See the Slice amendment. Ponsse is notified that
its ABI1 board adapter can continue against its accepted0.1.0 pin.
