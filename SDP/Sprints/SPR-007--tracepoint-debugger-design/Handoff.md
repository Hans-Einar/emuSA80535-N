# Handoff

## Current objective

SPR-007 is complete. Use the accepted design to activate the first bounded
product Slice; do not implement all layers in one change.

## Safety boundary

Documentation only. No physical I/O, target-specific behavior or emulator
product-code change is authorized.

## Next step

Activate implementation Slice A from `doc/DEBUG_TRACEPOINT_DESIGN.md`: raw
event schema, deterministic sequencing and bounded fan-out with legacy callback
compatibility and golden ordering tests.

## Worker handoff

- study: `SDP/02--Study/EMU-DEBUG-STU-003.md`;
- numbered design: `SDP/05--Design/EMU-DEBUG-DES-007.md`;
- implementation specification: `doc/DEBUG_TRACEPOINT_DESIGN.md`;
- traceability additions: STU-003 and DES-064..DES-079.

Important review points are the phase ordering around interrupt entry, unknown
old-value rules, ring loss-marker capacity, file failure policy, reset/load
survival and protocol 1.0 backward compatibility.

REV-SLC-013 and VER-SLC-013 accepted those design points. The process-level
Python suite must be rerun on a supported Python version in the first product
Slice.

## Pull request

- PR: https://github.com/Hans-Einar/emuSA80535-N/pull/11
- Branch: `codex/tracepoint-debugger-spec`
- Design commit: `f279b94`
- Scope: documentation and SDP records only

## Accepted refinement handoff — SLC-014

The accepted documentation-only refinement adds DES-080..DES-089 and the
multi-trace sections in `doc/DEBUG_TRACEPOINT_DESIGN.md`. Implementation must preserve:

- canonical-event identity versus per-destination routed views;
- exact on/off before/after semantics, including conflicting gates;
- nested interrupt depth and suppression-summary boundaries;
- ordered non-recursive `watch.match` derivation and source correlation;
- bounded many-to-many watch/trace routes and deletion lifecycle;
- file/ring/console destinations and strict NDJSON stdout isolation;
- facade, CLI, protocol negotiation and golden-test coverage.

REV-SLC-014 and VER-SLC-014 accepted this refinement. No emulator product code
or DAP code was changed in this Slice.
