# Development state and navigation

Updated: 2026-10-27

Start with `Traceability/CurrentIndex.yaml` for the one active contract, then
read its Sprint, iteration/slice contract and handoff. Completed handoffs
preserve their historical delivery checkpoints; their old "next step" does
not override the current index or a later integration record.

## Integrated baseline

- Stage0..Stage4 CPU behavior is accepted within its bounded requirements.
  Timer2 PR #18 merged on 2026-09-06 as
  `b43fe36b0965b6ac8628677bb6fcc16513d1f567`; Issue #17 is complete.
- Headless `emu-debug` protocol 1.0 is accepted for the initial DAP workflow.
- Standalone debug event/watch/trace runtime PR #16 merged on 2026-10-24 as
  `c8b82aba78dd1adcd474eb3bcd69424009a6a035`; Issue #14 is complete.
  Its closeout is `c8d647a29d5a160637025fa387e31f15628c3ec1`.

## Current delivery and remaining boundaries

SPR-010 / SLC-018 produced reviewed AlmaLinux package prototypes, but publication
is deferred. SPR-011 / SLC-019 establishes the reviewed modular runtime target:
see `04--Architecture/EMU-RUNTIME-ARCH-003.md` and its Sprint handoff. No
implementation Slice is active. Target requirements REQ-018..024 remain
unimplemented, and exact interfaces must be frozen in the next contract.

The next debugger feature work remains Gate B: canonical CPU producers and
safe-boundary stop application, followed by the stable facade/paging and
versioned protocol extension. DAP Issue #6 in `emuSA80535-DAP` depends on that
contract. REQ-015 capture/replay also remains planned.

An external simulator can already instantiate a CPU and load a raw image.
The selected target composition instead has a generic runtime own that CPU
and load an external board module, with independently attachable debugging.
Stock `emu-debug` currently owns its own CPU; implementing the new reusable
debug-service/session boundary remains required. Legacy embedding remains a
supported compatibility path.

## History and future migration

CPU and debugger histories used colliding Sprint/Slice/design numbers.
`Verification/SLC-017-RECONCILIATION-AUDIT.md` and the source-scoped
`legacy_debugger_aliases` in CurrentIndex retain their meanings. Do not globally
rename historical IDs or rewrite historical Ledger entries.

GitHub issue bodies/comments remain linked historical authorities. New work
may derive authority from an explicit user instruction recorded in its Slice.
Before eventual migration, carry accepted decisions, review/verification
references, dependencies and exact source identities into the new tracking
model. SDP installation via `gh sdp` and kanban/session migration are deferred
at the user's request; this cleanup does not introduce a replacement schema.
