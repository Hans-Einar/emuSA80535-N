# Handoff — First runnable system

Status: active SPR-013/ITR-022/SLC-022; SLC-021 accepted

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

## Current accepted CPU and active runtime

CPU API1 candidate0a77c2d is approved by REV-SLC-021/f9266f3 and VER-SLC-021.
Bundle0.2.0 is immutable; exact export is in SLC-021-ARTIFACT.json. BoardABI1.0
remains compatible with the external0.1 consumer; raw legacy CPU users rebuild.
Input ordering is per(cycle,admission ID), with controller owning identity and
generation. UART_FRAME applies only to SAB80535 mode3; effective-pin output is
not advertised. Pure post-fault board inspection is target wire2.1 capability.

RuntimeSlice.md is now the one active contract, with two disjoint fresh Worker
areas (session/debug, host/loader) and one fresh Reviewer. REQ-026/DES-115..118
define this local milestone without asserting full REQ-024 completion.
Ponsse reports its native board module accepted at source305cd0b, built against
SDK0.1/ABI1.0; its artifact and companion manifest are external integration inputs
and will be verified independently against the actual runtime. No external source
is copied into this repository. Its full-system integration awaits a runtime pin.
