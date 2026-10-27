# EMU-INTERFACE-REQ-003 — Shared contract package

Status: REQ-025 satisfied by accepted bundle0.1.0 through REV/VER-SLC-020

- **REQ-025 — Versioned shared interfaces:** publish within the repository a
  self-contained, versioned specification and canonical public header package
  covering CPU/controller, native board module, debug service, external
  transport, DAP projection and host administration. Internal and external
  consumers use the same include tree. Each exported/copy artifact identifies
  exact source revision, contract versions, file inventory and hashes; consumers
  record an explicit lock and copy location. Implemented legacy APIs remain
  compatible; future declarations must not be reported as implemented features.

REQ-025 derives from the user's 2026-10-27 request and refines delivery of
REQ-018..REQ-024. It authorizes contract/header consolidation and verification,
not runtime implementation, external adoption, SDP migration or publication.
