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
see `04--Architecture/EMU-RUNTIME-ARCH-003.md` and its Sprint handoff. SPR-013 / SLC-023 is the active external integration verification contract. SPR-012 / SLC-020 delivers accepted contract
bundle0.1.0 at `../interfaces/0.1.0/README.md` and canonical public headers at
`../lib/include/emusa80535`. See VER-SLC-020 and SLC-020-ARTIFACT.json for review,
verification and exact local export identity. REQ-025 is satisfied. SLC-021 implements CPU API1 and selects immutable bundle0.2.0;
see VER-SLC-021 and SLC-021-ARTIFACT.json. SLC-022 implements the reviewed controller/loader/local host; see VER-SLC-022
and SLC-022-ARTIFACT.json. External composed-system verification is active under REQ-026, while full REQ-018..024 completion, network/DAP and publication
remain future. New work uses the selected immutable contracts and capability limits.

The next debugger feature work remains Gate B: canonical CPU producers and
safe-boundary stop application, followed by the stable facade/paging and
versioned protocol extension. DAP Issue #6 in `emuSA80535-DAP` depends on that
contract. REQ-015 capture/replay also remains planned.

An external simulator can already instantiate a CPU and load a raw image.
The accepted local composition has a generic runtime own that CPU and load an
external board module. The native reusable debug-service/session boundary is
implemented; network transports and independently attaching debugger/DAP clients
remain future work. Stock `emu-debug` still owns its own CPU and preserves its
protocol1.0 workflow. Legacy embedding remains a supported compatibility path.

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
