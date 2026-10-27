# EMU-INTERFACE-DES-012 — Shared interface authority and distribution

Status: SLC-020 contract candidate
Design IDs: DES-111..DES-114; realizes REQ-025

## DES-111 — One canonical include tree

`lib/include/emusa80535` owns implemented legacy declarations and target
`contracts/` declarations. Root legacy headers only forward. Internal production
sources and external consumers use `<emusa80535/...>`. Build/install dependencies
include canonical headers; installed pkg-config supports namespaced and legacy
flat include spelling. C++ linkage is declared without changing C behavior.

## DES-112 — Versioned channel contracts

`interfaces/0.1.0` specifies CPU API1, native board ABI1.0/platform profile,
session/debug API1, control wire2.0, DAP projection and administrative channels.
Target declarations explicitly remain unimplemented. Existing stdio1.0 remains
unchanged, and source-scoped DEBUG-DES-090..097 semantic ownership/cursor rules
are preserved. Board-specific payload schemas remain in the owning external
project; the generic contract defines bounded ordered envelopes only.

## DES-113 — Immutable provenance and consumer copies

The bundle manifest hashes every payload, including canonical headers and
vendored reference contracts/licenses. Export from clean Git or SOURCE_COMMIT
archives adds source identity and a manifest-hash receipt; the archive hash and
export location live in a separate receipt. Consumer locks pin exact version,
source/manifest/archive hashes and copy location. Offline verification rejects
changed/missing/extra files and mismatched provenance, including optimized Python.
An accepted payload version is immutable. Recover old headers from its pinned
Git revision or complete export; never pair old docs with a newer include tree.

## DES-114 — Contract verification is not runtime acceptance

Isolated C99/C++17 consumers, strict schema/UTF-8/uint64 negative examples,
reproducible archive and tampered-copy checks verify this deliverable. Legacy
CPU/debug/protocol tests and staged installed SDK exercise include compatibility.
Synthetic scheduling/board/attach/DAP/signal gates remain required before claiming
implementation or integrated release under REQ-024. The consumer inventory
records intended owners and actual adoption status; no external adoption is
inferred from creating documents here.
