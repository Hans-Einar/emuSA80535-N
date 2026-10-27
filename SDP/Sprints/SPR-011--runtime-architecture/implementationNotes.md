# Implementation notes — SLC-019

Master recorded the user's architecture clarification and a bounded design-only
contract before Worker assignment. Package delivery SLC-018 is deferred with
its review/evidence preserved. Source baseline is de4eb56; CPU baseline remains
c8d647a and upstream ancestry is 5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8.

## Worker result

Created source-grounded STU-004 and target REQ-018..024, ARCH-011..018 and
DES-101..110, plus a bounded mandate addendum. Preferred composition is a
generic process with an embedded CPU and startup-loaded board module; the
three CPU API/module ABI/wire boundaries remain distinct. No signatures,
schemas or CLI names are frozen and no product implementation is claimed.

Source inspection confirmed unchanged upstream host/UI files and the separate
owned-CPU debugger. It also identified that existing run-N returns HALT for
IDLE/power-down even though tick can advance IDLE peripheral cycles: new
orchestration needs a cycle/access seam and explicit sleep-clock policy.
Module callbacks have explicit lifecycle retention; this does not weaken the
frozen external debug facade's callback-free contract.

Commands/evidence: `git diff --stat 5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8
c8d647a -- <core/UI files>`; source inspection of tick/run_control/reset,
MOVX callbacks and debugger create/protocol loop; sibling frozen DAP later-
slice attach requirement; primary Linux signal and DAP/runtime references
linked in STU-004. Validation is documentary (`git diff --check`, scope and
ID coverage); no product regression rerun is needed for these prose-only edits.

Fresh review and Master acceptance/traceability remain pending. No board code,
network endpoint, release upload or runtime behavior was changed.
