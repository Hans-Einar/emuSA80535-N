# Handoff — AlmaLinux delivery

Updated: 2026-10-27
Disposition: package implementation/review and pre-publication verification
complete; publication held for architecture clarification. SLC-018 is not
accepted as a published delivery.

## Current checkpoint

Product/source candidate `70eb6bc2bc9db71a3420baf2a1b4a9ef08d92f05` builds
runtime, development, debuginfo/debugsource and source RPMs for AlmaLinux 10.2.
REV-SLC-018 is approved after independent SRPM rebuild. VER-SLC-018 records
passing installed SDK/protocol, external ROM repeat and frozen DAP checks.
Artifacts remain local and unsigned. Nothing has been uploaded, signed or
published on Beinerud, and no host package installation was performed.

The user subsequently questioned whether this partial runtime should be
released and clarified the intended independent debugger attachment to the
same CPU used for full machine simulation. Master holds publication while
that architecture is reconciled; the package tests establish packaging
readiness only, not that the desired integrated product is complete.

Historical evidence now explicitly reconciled:

- `doc/P1000/P1000_SIMULATOR_BOUNDARY.md` section 7 preferred an external
  simulator executable linking the generic CPU/debug facade and owning its
  board composition (`p1000-sim-debug`).
- `emuSA80535-DAP/protocol/EMU_DEBUG_API_REQUIREMENTS.md`, "Needed for later
  slices", explicitly defers attach over a restricted local socket with an
  independent lifecycle. The implemented first slice is launch-owned stdio.
- The current opaque C debugger creates its own CPU. External-host/debug
  composition and an attach-capable scheduler/service are not implemented.

The user's proposed alternative is a generic emulator host loading a board
module and optionally exposing a debugger endpoint. A versioned board module
API and Unix socket attachment are proposals to assess, not approved product
contracts yet. Preserve the accepted CPU engine and debugger matcher. Do not
start either plugin or attach implementation under the packaging slice.

The user requested governance cleanup and delivery to the existing Beinerud
AlmaLinux app repository. No SDP upgrade is authorized yet. Historical
CPU/debugger identities and ledger entries remain intact.

Current baseline contains accepted Stage0..Stage4 and standalone debug trace
runtime. Gate-B CPU producers/safe-boundary/versioned-wire work and REQ-015
remain future work. The external simulator is owned by another Codex.

The first ROM smoke probe successfully loaded the reference image, reset at
entry 0, stepped one instruction and then yielded after 10,000 more
instructions on the unmodified baseline. This establishes load/execute
readiness only; no board was attached. Detailed evidence belongs in VER-SLC-018.

Next: reconcile runtime ownership, board-module/embedding API, independent
debug-service lifecycle and DAP launch/attach semantics before deciding the
release boundary. Existing packages and review remain usable development
evidence. Public-repository verification is not run because publication is held.
