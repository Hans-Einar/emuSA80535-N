# SPR-013 — First runnable composed system

Status: active
Started: 2026-10-27
Authority: direct user instruction to continue collaborating with the Ponsse
agent until a complete system can be built and run; choose pinned dependency
and integration ownership without further handoff by the user.
Baseline: 4ec4658

## Outcome and division of work

One generic host process loads an exact ROM and a separately built external
board module. First v0 supports bounded run, pause, step, CPU state, board
inspection, reset/repeat and explicit unsupported/fault outcomes. It does not
claim full machine boot or protocol fidelity. Network/DAP attach may follow
this local first delivery; current stdio1.0 remains compatible.

Ponsse owns the complete-system integration/build/launcher and its board module.
This repository owns CPU facade, deterministic session/scheduler, generic
module loader and local host. Dependencies use explicit exact source/archive
pins and checksums, with offline builds after bootstrap; no floating submodule
or copied CPU implementation. Future releases can replace local artifacts with
the same identity discipline. SDP migration and RPM publication remain deferred.

## Sequence

1. ITR-021/SLC-021: CPU contract facade and synthetic callback conformance.
2. Planned ITR-022/SLC-022: session/scheduler, loader, local host controls and
   synthetic shared module; concrete runtime source/artifact/launch handoff.
3. Planned integration Slice: independently reviewed Ponsse module + exact
   runtime/SDK/ROM/config pins, deterministic run/reset and unsupported evidence.

Only one Slice is active. Every implementation gets a fresh Worker and separate
fresh Reviewer. Master records successor contracts and cross-repo integration.
Interface bundle0.1.0 remains immutable; changes require a new bundle version.
