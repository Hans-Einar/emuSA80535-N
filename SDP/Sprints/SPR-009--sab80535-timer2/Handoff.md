# Handoff

## Current Objective

SLC-015 is independently reviewed and Master-verified. The only remaining
Stage-4 action is Steering review of PR #18 and an explicit merge decision.
No new CPU Slice is active or authorized.

## Authoritative Source Documents

- repository `AGENTS.md` and lifecycle SDP;
- Issue #17;
- `SDP/05--Design/EMU-SAB80535-DES-009.md`;
- `SDP/CodeReview/REV-SLC-015.md`;
- `SDP/Verification/VER-SLC-015.md`;
- Siemens SAB80515/SAB80C515 User's Manual Edition 08.95;
- Ponsse PR #25 HEAD `19ef6ff45efa719612b70e70b8c31b9cb2ebb7e9` as consumer evidence only.

## Accepted Identity

- Frozen master baseline: `bc86d2633b6057529e6fd1e666896c24d72822aa`.
- Exact accepted product/test HEAD: `e388a007635acb4f964326f817a3d6eb049ccf6d`.
- Exact accepted product tree: `8eb677ad298a3a743adb9d51f464b90ba755b0cf`.
- Frozen DAP integration HEAD: `36639b48ddb2ffbafa14c00da794fe1734f7483b`.
- Implementation PR: #18, open and intentionally unmerged.

Commits after `e388a007...` are temporary CI cleanup and SDP/review/verification
evidence only. Product/test blobs must remain identical through the final PR
head; Steering must re-check this before merge.

## Acceptance Evidence

- REV-SLC-015: APPROVED, no findings.
- VER-SLC-015: PASSED.
- Worker run `33688691147`: GCC, strict Clang, ASan+UBSan, debug, DAP, scope.
- Master run `33694685888`: Linux exact-head full gate + Windows GCC/Clang.
- Exact `0x5555 -> 43691` case, sticky repeated TF2, controller/vector `002B`,
  live writes, stop/reload/restart, free-running `/24`, classic isolation and
  no automatic P5.4 all verified.

## Stop Boundary

Do not add external/gated Timer2 inputs, hardware reload modes, EXF2 producer,
compare/capture, automatic P5.4 coupling, target/board semantics, physical/live
I/O, `emu-debug` changes or REQ-015 work without a new Steering authorization.

## Traceability IDs

MND-001, STU-001, REQ-014, ARCH-002..ARCH-006, DES-086..DES-095, SPR-009,
ITR-015, SLC-015, REV-SLC-015 and VER-SLC-015 are current/accepted.

## Cross-repository handoffs

- Ponsse Issue #26 exact CPU convergence handoff: https://github.com/Hans-Einar/ponsse/issues/26#issuecomment-5517911661
- Ponsse Issue #47 peripheral-simulator dependency handoff: https://github.com/Hans-Einar/ponsse/issues/47#issuecomment-5517915220

Both consumers are instructed to pin exact accepted CPU product HEAD
`e388a007635acb4f964326f817a3d6eb049ccf6d` rather than the moving PR/SDP head.

## Final Stage-4 checkpoint

Issue #17 READY/Steering checkpoint: https://github.com/Hans-Einar/emuSA80535-N/issues/17#issuecomment-5517933325

The accepted product remains `e388a007635acb4f964326f817a3d6eb049ccf6d`. Any later branch commit is
traceability/evidence only and requires a final product-blob identity audit.
