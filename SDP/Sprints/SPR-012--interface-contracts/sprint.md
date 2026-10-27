# SPR-012 — Versioned interface contract bundle

Status: complete — contract/SDK delivery only
Sprint: SPR-012; Iteration: ITR-020; Slice: SLC-020
Authority: user requests complete shared interface documentation, public headers,
canonical include paths, and version/provenance tracking across consumers.
Baseline: d4497fa

Deliver a self-contained versioned contract bundle for CPU/controller, board
module, reusable debug service, external debugger transport and DAP mapping.
One process hosts CPU and startup-loaded board; separate board processes are
excluded. Preserve existing behavior, protocol1.0 and DEBUG design decisions.
No runtime/network implementation, external repository edits, SDP migration,
package publishing or remote mutation belongs to this Slice.

## Result

SLC-020 accepted at reviewed candidate 7d924cc through REV-SLC-020 and
VER-SLC-020. Bundle0.1.0 is immutable; target runtime implementation remains
future work. See Handoff and SLC-020-ARTIFACT.json for delivery identity.
