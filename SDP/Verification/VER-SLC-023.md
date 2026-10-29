# VER-SLC-023 — External composed-system verification

Date: 2026-10-27
Disposition: PASS for the bounded local build/run milestone; local review and
Master acceptance remain separate.
Runtime: `a03504bd186efe3a8882bd3f1cdb5c4cf463f30d`
External integration: `9f673bb3660cb1a957a95110a69283a55ad990ba`
Authority: SPR-013 / ITR-023 / SLC-023, REQ-026, DES-115..DES-118

## Execution and recovery provenance

Fresh verification Worker `/root/integration_verifier` executed the checks below
before the daemon restarted. Its raw commands, stdout/stderr, build receipts and
summaries were preserved in the durable artifact directory recorded by
SLC-023-INTEGRATION.json. Recovery Worker `/root/integration_recovery_verifier`
read the active contract and independently checked all77 retained file digests,
the six frozen external product-file hashes, approved external review and saved
scenario summaries/output hashes. The evidence manifest remains
`cc19ecde3449fb98fa28eb5174bdc7536c39d3dd83455c3290cadbc29900e902`.
Completed tests were not rerun merely because the execution agent restarted.
No product source or external repository file was changed by either verification
role. The recovery check inventory and exact external review snapshot are
retained separately outside the generic repository.

## Verified results

- The runtime archive reproduced byte for byte from its accepted source commit.
  The archive was extracted without `.git` and built successfully; version and
  hello identity match the source and immutable SDK0.2 manifest.
- The frozen external integration was exported without `.git`. Its documented
  explicit local-archive bootstrap built the runtime and separate SDK0.1 board
  module offline. All three binary/companion artifacts matched the reviewed
  lock, and all8 module/SDK/board CTests passed.
- `verify-build` and all four documented launcher variants passed: synthetic,
  short original-image trial, bounded diagnostic trial and custom controls.
  Independent producer-schema validation passed20/10/48/9 records respectively.
  The diagnostic wrapper succeeds only for its expected result and retains the
  underlying runtime exit3; arbitrary failures are not accepted as trial success.
- All13 integration tests passed in an exclusive build directory. They cover
  actual composed execution plus malformed input, identity/source/binary/receipt
  rejection, output/evidence bounds and process cleanup.
- The synthetic trial proves actual CPU/module effects, timed input, pause,
  instruction step and same-session reset. Separate processes running the
  synthetic and original-image trials produce equal complete result streams
  after removing only sessionId. Lifecycle identities are retained explicitly.
- The original image reaches the externally defined unsupported transaction;
  subsequent run requests reject with STATE, zero progress and unchanged state.
  Pure inspection remains available under the reviewed explicit promise, and
  reset/repeat reproduces the diagnostic result. Without that opt-in, inspection
  rejects without mutation. Step and board input after fault remain rejected
  even when pure inspection is allowed.
- Incorrect image and module hashes reject before CPU execution. Source,
  immutable SDK, artifact and receipt identities remain independently checked.

Exact shell argument arrays and result codes are in `verification-commands.json`
and `launcher-commands.json` under the durable evidence directory. Instructions
for fresh builds and direct use of the verified default build are maintained by
the external owner in `p1000/simulator/RUN_V0.md`; SLC-023-INTEGRATION.json records
its location, pins and a minimal invocation. Machine-specific firmware addresses,
register/decode assertions and raw board results remain in external/outside-repo
evidence, especially external `SDP/Verification/PERIPH-RUN-001-evidence/`.

## External review and acceptance boundary

Fresh external review REV-PERIPH-RUN-001 explicitly approves frozen integration
9f673bb with no open material findings. Its complete saved content hashes to
`27cf6e6bbfb71c07c8f89c7bae19b67fd566fda92af8f44fea2c5a2555ea53b0`.
Each of its six source-file digests matches both the exact commit and current
external product files. A hash-addressed snapshot is retained with recovery
evidence, so this verification does not depend on a mutable external draft.

At initial recovery capture the external approval existed without its bookkeeping
commit. The external Master subsequently committed the approved review, passed
verification and completion record as
`4682bb89d075838bb8d836b165f1b2b2499e1dd2`. Recovery verification confirmed that
its review content matches the retained snapshot and all six frozen product
files remain byte-identical to9f673bb. Both the independent approval and external
Master acceptance are therefore present. Local fresh review determines Slice
acceptance; Master here owns final traceability.

## Limits

The accepted scope is a Linux x86-64 scripted local system using the documented
fixed toolchain. Other compiler/linker bytes require a reviewed lock update.
Full firmware boot, complete physical-board equivalence, remote debugger/DAP
attachment, administrative signals, richer trace and signed RPM publication are
not established by these trials. SDP migration remains deferred.
