# EMU-DEBUG-STU-002 -- Issue #6 baseline and frozen-contract revalidation

## Authority

- Steering/Master authority: `Hans-Einar/emuSA80535-N` Issue #6.
- Emulator repository: `Hans-Einar/emuSA80535-N`.
- Required starting point: exact current `master` after PR #8 / SLC-010 is
  merged.
- Frozen consumer contract: `Hans-Einar/emuSA80535-DAP` PR #4 exact HEAD
  `36639b48ddb2ffbafa14c00da794fe1734f7483b`.
- Protocol: `emu-debug` 1.0, newline-delimited UTF-8 JSON over stdin/stdout.

The DAP contract is an external frozen input. It must not be weakened or
changed to accommodate the emulator.

## Revalidation evidence

Revalidated on 2026-09-01 before any Issue #6 Sprint, Iteration, Slice, Worker,
Reviewer or product-code work was started.

| Evidence | Exact result |
| --- | --- |
| `origin/master` | `88ccb2b45976c137820cdc56eec550d953bcf76d` |
| PR #8 state | `OPEN`; `mergedAt` is null |
| PR #8 HEAD | `fe62b9127c27e075a1353bcac1564f713491a70c` |
| Accepted SLC-010 product commit | `ba00ef17af57076b01c7f548e8996a7d36a5c591` |
| SLC-010 ancestry from `origin/master` | false |
| DAP PR #4 frozen HEAD | `36639b48ddb2ffbafa14c00da794fe1734f7483b` |
| Issue #6 checkpoint | https://github.com/Hans-Einar/emuSA80535-N/issues/6#issuecomment-5491928272 |

The accepted SLC-010 product commit exists on the PR #8 feature line but is
not an ancestor of current `origin/master`. Current master therefore cannot be
frozen as the Issue #6 implementation baseline while also satisfying the
explicit requirement to preserve SLC-010 Stage-2 semantics as regressions.

## Frozen protocol surface retained for the next revalidation

The exact DAP revision requires the commands `hello`, `load`, `reset`,
`getState`, `decodeCode`, `replaceCodeBreakpoints`, `run`,
`stepInstruction`, and `terminate`. Its required capabilities remain
`rawCode64k`, `deterministicReset`, `snapshotBasicRegisters`, `decodeCode`,
`replaceCodeBreakpoints`, `boundedRun`, and `stepInstruction`.

The contract also retains these non-negotiable properties:

- one bounded NDJSON request or response per line, with stdout reserved for
  protocol traffic and stderr used for diagnostics;
- exact 64 KiB raw-code loading with SHA-256 validation;
- deterministic reset and stable architectural snapshots;
- exact decode, atomic code-breakpoint replacement, bounded run, exact
  single-instruction stepping, and deterministic terminate/EOF cleanup;
- no DAP-side adaptation and no P1000 target policy, board logic, live serial,
  live GPIO or physical I/O.

The full `EMU-BLK-001..010` implementation disposition is deferred until a
valid emulator baseline can be frozen. `EMU-BLK-004` remains satisfied by the
already integrated generic deterministic core/loading surface; the other
blockers have not been implemented or verified for Issue #6.

## Regression contract

All accepted Stage-0 and Stage-1 behavior on current master remains mandatory.
Accepted SLC-010 Stage-2 port/pin/MOVX behavior is also mandatory, but is
absent from the current master ancestry. This missing prerequisite is the
baseline blocker; it is not permission to omit or recreate SLC-010 within
Issue #6.

## Master disposition

Status: `BLOCKED_BEFORE_IMPLEMENTATION`.

No Issue #6 implementation baseline is frozen. Active Sprint, Iteration and
Slice remain null. No Worker or Reviewer may be dispatched until Steering
either merges PR #8 to master or explicitly authorizes a replacement exact
baseline that contains the accepted SLC-010 semantics. Master must then fetch
and repeat the ancestry/contract revalidation before activating Issue #6.

See `SDP/Blockers/BLK-EMU-DEBUG-001.md`.
