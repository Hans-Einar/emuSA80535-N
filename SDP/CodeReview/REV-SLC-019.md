# REV-SLC-019 — Modular runtime architecture review

Reviewer: fresh independent Reviewer, separate from Worker and Master
Date: 2026-10-27
Scope: documentation-only SLC-019; STU-004, REQ-018..024,
ARCH-011..018, DES-101..110 and mandate addendum
Initial candidate: `6705bfd1abdd754c08d54af2f14326e448a8e8d6`
Reviewed correction: `e52f6653178a4f338d26d4c01c9ad60605eb166b`
Contract baseline: `2218400`; preceding repository state: `de4eb56`
Disposition: approved architectural direction; finding resolved

## Finding REV-SLC-019-F001 — DAP disconnect mapping

Initial disposition: changes required; bounded lifecycle clarification.
DES-106 preserves running/paused state on runtime detach, but did not explain
the external DAP adapter's graceful disconnect policy. A direct mapping of
DAP disconnect to runtime detach could leave an attached target paused when
the DAP client expects execution to resume.

Evidence: pinned `@vscode/debugprotocol` 1.68.0 `DisconnectArguments`
declaration, inspected in the SLC-018 DAP validation tree, says unspecified
`suspendDebuggee` resumes execution and gates the argument through
`supportSuspendDebuggee`. The enclosing `DisconnectRequest` distinguishes
launch and attach termination. See the official
[DAP disconnect specification](https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Disconnect).

Required correction: distinguish adapter policy from the low-level primitive;
apply authorized resume before lease release where DAP requires it, preserve
the appropriate suspension option, and retain state on abrupt connection
loss. An attached target must not be implicitly terminated. No runtime API
or DAP implementation change is required in this Slice.

Resolution: independently reviewed `e52f6653178a4f338d26d4c01c9ad60605eb166b`.
ARCH-016 and DES-106 now separate graceful adapter policy from runtime detach,
require applied/acknowledged authorized transitions before lease release,
address the negotiated suspension option, and preserve abrupt-loss semantics.
REV-SLC-019-F001 is resolved; no remaining blocking findings.

## Independent checks

- Compared upstream `5dc681275151c4a5d7b85ec9ff4ceb1b25abd5a8`
  with accepted fork `c8d647a29d5a160637025fa387e31f15628c3ec1`.
  Upstream `emu8051.h` already exposes state, opcode/decoder tables and
  SFR/XDATA callbacks; the listed legacy host/UI files are byte-identical.
- Inspected current `emu_debug.c` ownership/create/reset, server protocol loop,
  `core.c` cycle advancement/tick/run control and MOVX access/observation.
  These support the study's owned-CPU, launch-only, IDLE/HALT and scheduling
  gap claims. The proposal correctly avoids treating observers as bus owners.
- Reviewed distinct CPU API, native board ABI and serialized wire contracts;
  one session/CPU/execution owner; common pause; timestamped admission/replay;
  bounded queues and snapshot provenance. Cycle phases, sleep clock policy and
  concrete interfaces explicitly require subsequent design and verification.
- Checked preservation of stdio1.0 lifecycle and DEBUG-DES-090..097. Module
  callback retention is explicitly separate from the callback-free debug
  facade. Native modules are trusted and can fail/block the host; no sandbox,
  rollback, hot-unload or unsupported performance guarantee is claimed.
- Reviewed private Unix endpoint/peer access and opt-in authenticated,
  protected TCP; framing is separate from DAP. Reviewed signal routing against
  [signalfd](https://man7.org/linux/man-pages/man2/signalfd.2.html) and
  [async-signal safety](https://man7.org/linux/man-pages/man7/signal-safety.7.html).
  Status is captured at safe boundaries, ordinary signals may coalesce, and
  diagnostics avoid protocol stdout. Current source has no proposed SIGUSR1
  handler; the study correctly distinguishes target behavior.
- Candidate diff contains only the six authorized documentation files;
  `git diff --check 2218400 e52f665` passes. No product regression tests were
  run because no product code/build changed. No external repository was
  changed, listener started or package published.

No other architectural blocker was found. Approval applies only to
architectural direction. It does not freeze ABI,
wire schema or CLI, accept product implementation, or authorize publication.
Master owns traceability, verification integration and final Slice disposition.
