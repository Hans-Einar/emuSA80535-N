# Handoff — Runtime architecture

Updated: 2026-10-27
Disposition: accepted documentation/design Slice SLC-019; no active Slice

The user accepts one orchestration process and an embedded CPU component,
wants clean independently buildable boundaries, favors loading variants of
external board libraries at startup, and asks to include TCP debugger access
and administrative signals. The target contracts are now independently
reviewed and verified. No runtime implementation is activated here.

SPR-010 / SLC-018 retains approved package prototype evidence, but is deferred
and unpublished. SLC-019 is complete and the active entries are null. SDP migration
remains deferred. No external simulator product files are in scope.

## Accepted direction and identities

- Source study STU-004, target REQ-018..024, ARCH-011..018 and DES-101..110.
- Worker proposal: 6705bfd; reviewed correction: e52f665.
- REV-SLC-019 approved; finding REV-SLC-019-F001 resolved; VER-SLC-019 passed.
- One runtime owner/session/timeline; pure CPU build target; independently
  compiled external board module selected at startup; one debug semantic model.
- Unix and explicit protected/authenticated TCP transports; administrative
  signals handled by the host at safe boundaries, never as simulated IRQs.
- Same-CPU attach, no reset/reload; graceful DAP policy is distinct from abrupt
  connection loss and the lower-level state-preserving detach operation.

## Next bounded work

Record a new contract to freeze the CPU scheduling/access API and the versioned
board ABI: exact phases, limits, error classes, ownership, reset and supported
clock/sleep behavior. Then implement/test a generic runtime and synthetic board,
dynamic loading, reusable session-bound debug service, transports/signals and
DAP attach in separately reviewed slices. Preserve legacy APIs and stdio1.0.

The external simulator agent contacted this thread with the user's request to
coordinate. Both sides agree that legacy logicboard.c is not the new module
ABI and the post-access MOVX observer cannot supply read values. The external
board currently has finite append-only diagnostic trace storage; its adapter
contract must distinguish diagnostic saturation from transaction failure and
define drain/loss behavior for long runs. No external code was modified.

The external agent reports Ponsse PR49's accepted board implementation on main
and an older, still-open PR27 with a Ponsse-owned runner and parallel board
interfaces. PR27 must be reconciled with the accepted board and generic host
direction rather than merged blindly. Its suggested first integration gate is
a synthetic MOVX read/write, reset, pause and replay exercise before ROM boot.
These are external coordination findings, not acceptance of external code.

Exact ABI signatures, wire commands, binary names and CLI flags remain unfrozen.
No plugin implementation should depend on illustrative names from chat. The
packaging prototype remains development evidence; integrated publication waits
for REQ-024 verification and an updated delivery contract.
