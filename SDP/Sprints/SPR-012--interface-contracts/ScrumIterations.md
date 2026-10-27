# ITR-020 / SLC-020 — Shared interface contracts and SDK

Status: active

## Worker contract

Read AGENTS, CurrentIndex, this Sprint/handoff, lifecycle MND/STU/REQ/ARCH/DES,
especially runtime STU004/REQ018..024/ARCH011..018/DES101..110 and frozen
DEBUG-DES090..097. Inspect actual CPU/debug source and sibling DAP protocol.

Create a self-contained versioned interfaces/ contract bundle, canonical
lib/include/emusa80535 public headers, and reproducible export/check tooling.
Use an explicit initial pre-release contract identity (0.1.0) independent of
legacy product/protocol versions. Define exact target interfaces rather than
only aspirational prose; clearly distinguish declarations from implemented APIs.
Keep unimplemented runtime declarations out of claims of linkable functionality.

Cover all channels: CPU/control API; native board ABI and lifecycle; execution
phases, IDLE, power-down unsupported policy, deterministic input ordering;
debug-service/session boundary; serialized Unix/TCP attach protocol; external
debugger/DAP role mapping and IDE DAP framing; host signals/diagnostics. Specify
ownership, errors, bounds, version negotiation, schema/example messages,
reset/pause/reconnect/disconnect/termination, trace paging/loss and side-effect
free inspection. No board semantic types, addresses or machine names in SDK.
Define CPU access context once, transaction exactly once, observation afterwards.

Canonical shared headers must be usable by CPU/controller and external C/C++
consumers from one include root. Preserve source compatibility for legacy APIs
using forwarding headers if needed; do not keep divergent copies. Build/install
must use/export canonical headers and docs consistently. Preserve existing RPM
prototype compatibility without publishing it. Header-only future contracts may
be compile checked but must not pretend to provide implementations.

Bundle manifest must identify independent contract/API/ABI/wire versions,
implementation support, source provenance and every exported file checksum.
Provide deterministic archive/export and offline verification; consumer lock
example records exact version, origin commit and hash/location. No mutable
latest reference or self-referential hash scheme. Explain how accepted versions
are immutable and how incompatible changes require a new version.

Include compile-only external C/C++ consumers and wire examples/schema checks
and documentation links/provenance verification. Run relevant legacy build and
protocol/facade tests for include/build changes. No new production runtime,
socket, signal handler, board simulator implementation or DAP source changes.

Worker owns interfaces/, canonical headers, bounded build/install changes,
contract verification tooling, relevant tests, README entry and implementationNotes.
May add lifecycle REQ025 and DES111..114 to describe bundle/version authority.
Master owns sprint/handoff, index, relations and ledger. Separate fresh Reviewer
must review contracts, headers, examples, provenance/export and compatibility;
resolve findings before acceptance. Worker commits own changes and reports
commands/results and residual implementation gaps.
