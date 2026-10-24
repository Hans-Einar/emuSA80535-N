# SLC-017 current-master reconciliation audit

Status: reviewed and verified — bounded reconciliation accepted
Date: 2026-09-06
Final evidence: REV-SLC-017-RECONCILIATION and VER-SLC-017-RECONCILIATION
approve/pass exact candidate `c21d63f121f6305afd006a145a96c8dddae8b55e`.
Historical Worker-only pending statements below describe that earlier stage.
Scope: Issue #14 Steering comment 5517725977; PR #16 is not merged.
Latest authority: Stage-4 Steering rebaseline comment 5559152334,
https://github.com/Hans-Einar/emuSA80535-N/issues/14#issuecomment-5559152334.
It confirms the exact master used here and preserves Stage0..Stage4 including
ADC and Timer2 without introducing additional feature scope.

## Exact inputs and method

- Accepted PR #16 baseline: `1e588d28fb168a7c5a42c4c7dc4b51f84d29d1ed`.
- Accepted six-correction product commit: `d956177add44dda9efbd6d9e372a9c0a6d40f777`.
- Merge first parent (baseline plus Master contract): `1ba90778aaa7a155bc68d8f913531d61414f8b1e`.
- Exact fetched current master / merge second parent: `b43fe36b0965b6ac8628677bb6fcc16513d1f567`.
- Common branch ancestor: `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`.
- Worker ran `git merge --no-ff --no-commit b43fe36b0965b6ac8628677bb6fcc16513d1f567` and
  resolved the ten actual conflict paths listed below. The resulting merge
  preserves both ancestries. Exact merge commit:
  `74dca67aafebdddc82ade3e5d6864a1457ea75bb`. A subsequent docs-only
  whitespace correction is recorded below; the final reviewed HEAD is recorded
  by Master review/verification.

## Conflict inventory and resolution

| Conflict path | Resolution |
|---|---|
| `tests/Makefile` | Union of both existing Windows/Linux executable variables, build rules, default `all`/`test` prerequisites, execution recipes and cleanup lists. Master external-edge/ADC/Timer2 and accepted event/watch/trace/runtime suites all remain runnable. No new suite or product behavior. |
| `SDP/Traceability/CurrentIndex.yaml` | Master canonical entries and requirement/acceptance states retained; debugger-only entries unioned with scoped aliases for colliding IDs. The debugger-parent stale planned `REQ-013..REQ-015` aggregate is superseded by master's individual `REQ-013`, `REQ-014`, `REQ-015` states. Active work is `DEBUG-SPR-008` / `ITR-017` / `SLC-017`; acceptance of debugger `DEBUG-SLC-015`, `SLC-016` and `SLC-017` remains intact. |
| `SDP/Traceability/Relations.yaml` | Union of master relations and qualified debugger relations. Duplicate identical relations omitted. Holistic review range expanded into its three explicit slice targets to avoid a mixed-namespace range. |
| `SDP/Traceability/Ledger.ndjson` | Exact common prefix plus master suffix plus exact debugger suffix; no historic record or ID rewritten. Explicit block provenance below disambiguates duplicate historic event IDs and scoped entity IDs. New implementation event appended. |
| `SDP/CodeReview/REV-SLC-013.md` | Current-master bytes retained at the canonical path; debugger-parent content preserved at `SDP/CodeReview/REV-DEBUG-SLC-013.md`, with only 5 inherited trailing Markdown hardbreaks removed for the required whitespace gate (see exception below). Historic labels in the preserved document remain source-scoped. |
| `SDP/CodeReview/REV-SLC-014.md` | Current-master bytes retained at the canonical path; debugger-parent content preserved at `SDP/CodeReview/REV-DEBUG-SLC-014.md`, with only 2 inherited trailing Markdown hardbreaks removed for the required whitespace gate (see exception below). Historic labels in the preserved document remain source-scoped. |
| `SDP/CodeReview/REV-SLC-015.md` | Current-master bytes retained at the canonical path; debugger-parent bytes preserved at `SDP/CodeReview/REV-DEBUG-SLC-015.md`. Historic labels in the preserved document remain source-scoped. |
| `SDP/Verification/VER-SLC-013.md` | Current-master bytes retained at the canonical path; debugger-parent bytes preserved at `SDP/Verification/VER-DEBUG-SLC-013.md`. Historic labels in the preserved document remain source-scoped. |
| `SDP/Verification/VER-SLC-014.md` | Current-master bytes retained at the canonical path; debugger-parent bytes preserved at `SDP/Verification/VER-DEBUG-SLC-014.md`. Historic labels in the preserved document remain source-scoped. |
| `SDP/Verification/VER-SLC-015.md` | Current-master bytes retained at the canonical path; debugger-parent bytes preserved at `SDP/Verification/VER-DEBUG-SLC-015.md`. Historic labels in the preserved document remain source-scoped. |

## Identity and evidence provenance

The parallel histories independently assigned the same SDP IDs. Unqualified
canonical IDs remain those on current master. The debugger source scope is
the accepted takeover history and all `EMU-DEBUG-DES-007.md`,
`EMU-DEBUG-DES-008.md`, `SPR-007--tracepoint-debugger-design/` and
`SPR-008--debug-trace-runtime/` text, plus the preserved debugger reviews and
verification records. Original references inside those historical bodies and
ledger rows must be interpreted through this mapping, never as master ADC,
external-edge or Timer2 evidence. The source-scoped labels and frozen prose
are intentionally not rewritten; the registry and relation graph carry the
disambiguated identities. Existing unique sprint directory paths stay intact.

| Original debugger scope | Current registry identity | Master canonical meaning |
|---|---|---|
| `SPR-007` | `DEBUG-SPR-007` | External edges sprint |
| `SPR-008` | `DEBUG-SPR-008` | ADC sprint |
| `ITR-013`, `SLC-013` | `DEBUG-ITR-013`, `DEBUG-SLC-013` | External edges iteration/slice |
| `ITR-014`, `SLC-014` | `DEBUG-ITR-014`, `DEBUG-SLC-014` | ADC iteration/slice |
| `ITR-015`, `SLC-015` | `DEBUG-ITR-015`, `DEBUG-SLC-015` | Timer2 iteration/slice |
| `REV-SLC-013..015` | `REV-DEBUG-SLC-013..015` (individual IDs/files) | External edges / ADC / Timer2 reviews |
| `VER-SLC-013..015` | `VER-DEBUG-SLC-013..015` (individual IDs/files) | External edges / ADC / Timer2 verification |
| `DES-064..DES-089` | `DEBUG-DES-064..DEBUG-DES-089` | Overlapping master external-edge/ADC/Timer2 design IDs |
| `DES-090..DES-097` | `DEBUG-DES-090..DEBUG-DES-097` | Overlap with master Timer2 `DES-090..DES-095`; debugger frozen set kept together |
| `ITR-016..017`, `SLC-016..017`, and their review/verification IDs | Unchanged | No master collision |

`CurrentIndex.yaml:legacy_debugger_aliases` enumerates every individual alias.
Historical abbreviations of a range inherit the source scope of the document.
The accepted debugger semantics called SLC-015..017 and DES-090..097 by
Steering retain exactly those meanings; qualification is registry provenance,
not redesign, replacement, renumbering of the frozen source or reacceptance.

## Historical ledger preservation

The first 225 output lines are immutable historical input records. Use the
line block together with `event_id` to identify a historical event; some
parallel-history event IDs are equal. Appended events use distinct RECON IDs.
Physical append order is provenance order, not a new global timestamp order.

| Output lines | Source and source lines | SHA-256 of exact original block bytes |
|---|---|---|
| 1–130 | `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b` lines 1–130 | `6ae4664fb7264bd9e9000b7410b30901e723b6b5b834442f496468e6a9269dac` |
| 131–182 | `b43fe36b0965b6ac8628677bb6fcc16513d1f567` lines 131–182 | `c4648180f830e2ded62fec73a6a076e4a560fa98f389a505e10cae22693fc89b` |
| 183–225 | `1ba90778aaa7a155bc68d8f913531d61414f8b1e` lines 131–173 | `4fae0e13d4792529e1fad33c87a0501cb55134b6a7ec6847b73881dfeda1a97f` |

Duplicate historical event IDs across the two suffixes (31):

- `EVT-2026-09-02-138`
- `EVT-2026-09-02-139`
- `EVT-2026-09-02-140`
- `EVT-2026-09-02-141`
- `EVT-2026-09-02-142`
- `EVT-2026-09-02-143`
- `EVT-2026-09-02-144`
- `EVT-2026-09-02-145`
- `EVT-2026-09-02-146`
- `EVT-2026-09-02-147`
- `EVT-2026-09-02-148`
- `EVT-2026-09-02-149`
- `EVT-2026-09-02-150`
- `EVT-2026-09-02-151`
- `EVT-2026-09-02-152`
- `EVT-2026-09-02-153`
- `EVT-2026-09-02-154`
- `EVT-2026-09-02-155`
- `EVT-2026-09-02-156`
- `EVT-2026-09-02-157`
- `EVT-2026-09-02-158`
- `EVT-2026-09-02-159`
- `EVT-2026-09-02-160`
- `EVT-2026-09-02-161`
- `EVT-2026-09-02-162`
- `EVT-2026-09-02-163`
- `EVT-2026-09-02-164`
- `EVT-2026-09-02-165`
- `EVT-2026-09-02-166`
- `EVT-2026-09-02-167`
- `EVT-2026-09-02-168`

Both full parent ledgers are reconstructible byte-for-byte: common + master
block yields current-master Ledger; common + debugger block yields the first
parent Ledger. Source paths and entity IDs in the debugger block use the
debugger mapping above. No historical event has been silently replaced.

## Imported versus reconciled files and preservation evidence

The following files differed between parents and are imported byte-for-byte
from current master; they are upstream integration, not worker implementation:

- `README.md`
- `SDP/05--Design/EMU-SAB80535-DES-005.md`
- `SDP/05--Design/EMU-SAB80535-DES-007.md`
- `SDP/05--Design/EMU-SAB80535-DES-008.md`
- `SDP/05--Design/EMU-SAB80535-DES-009.md`
- `SDP/CodeReview/REV-SLC-013.md`
- `SDP/CodeReview/REV-SLC-014.md`
- `SDP/CodeReview/REV-SLC-015.md`
- `SDP/Sprints/SPR-007--sab80535-external-edges/Handoff.md`
- `SDP/Sprints/SPR-007--sab80535-external-edges/ScrumIterations.md`
- `SDP/Sprints/SPR-007--sab80535-external-edges/implementationNotes.md`
- `SDP/Sprints/SPR-007--sab80535-external-edges/sprint.md`
- `SDP/Sprints/SPR-008--sab80535-adc/Handoff.md`
- `SDP/Sprints/SPR-008--sab80535-adc/ScrumIterations.md`
- `SDP/Sprints/SPR-008--sab80535-adc/implementationNotes.md`
- `SDP/Sprints/SPR-008--sab80535-adc/sprint.md`
- `SDP/Sprints/SPR-009--sab80535-timer2/Handoff.md`
- `SDP/Sprints/SPR-009--sab80535-timer2/ScrumIterations.md`
- `SDP/Sprints/SPR-009--sab80535-timer2/implementationNotes.md`
- `SDP/Sprints/SPR-009--sab80535-timer2/sprint.md`
- `SDP/Verification/VER-SLC-013.md`
- `SDP/Verification/VER-SLC-014.md`
- `SDP/Verification/VER-SLC-015.md`
- `core.c`
- `emu8051.h`
- `tests/test_stage2_edges.c`
- `tests/test_stage2_ports.c`
- `tests/test_stage3_adc.c`
- `tests/test_stage4_timer2.c`

The only reconciled build/product file is `tests/Makefile`;
`doc/DEBUG_TRACEPOINT_DESIGN.md` receives only the two inherited trailing
Markdown hardbreak removals documented below. In particular `core.c`,
`emu8051.h`, newer peripheral tests and README import the master blobs;
`opcodes.c`, `emu_debug.c/.h`, server/protocol code and all accepted standalone
runtime modules/tests receive no implementation edit. The merge adds no
CLI/DAP/wire/CPU producer/sink/source-map/physical/product feature.

The following accepted files match the accepted PR baseline by Git blob ID
(which also verifies retention of the six product corrections). These were
checked with `git hash-object --path PATH PATH` against
`git rev-parse ACCEPTED:PATH` after resolution:

| Preserved path | Git blob ID |
|---|---|
| `SDP/05--Design/EMU-DEBUG-DES-007.md` | `c4a22e04b0f13b20dd29d56e0e70e30a9cc0c556` |
| `SDP/05--Design/EMU-DEBUG-DES-008.md` | `df05e81a36d544f0b9c89e7e6ae0f81d28460d81` |
| `emu_debug.c` | `c8112f695d6bf3fd0e64df0d5216c258fd8bdccc` |
| `emu_debug.h` | `d7418d6efed146bd22cdbeec899082a7ea490d79` |
| `emu_debug_event.c` | `c2b6f7e4d9f544039304f66b2830f85aeb1c8f3c` |
| `emu_debug_event.h` | `360c1e4869616788d1163fbd7904c370e1c8f82f` |
| `emu_debug_runtime.c` | `a7f99e17440ae615a4ba36979c2c249149c76bae` |
| `emu_debug_runtime.h` | `40f9c3ddacc48ea8cbb7706ec8ea0c698ad6b13f` |
| `emu_debug_server.c` | `4b5b32b4774843c50d1691a7bb5c15d7aa3db4bc` |
| `emu_debug_trace.c` | `4f0f47679cd339694c0f872b0078891e367b37e6` |
| `emu_debug_trace.h` | `930072331d0e5f41416f2f7d3b751480d4928bce` |
| `tests/test_debug_event.c` | `2189e89fdedb4a33f83e6f3be2f1b72393a00a41` |
| `tests/test_debug_facade.c` | `d4b46cabf54702ca4b22c50f88e09248bcd7c550` |
| `tests/test_debug_runtime.c` | `d977b62c123892b9b35d37e9c7efca0107390515` |
| `tests/test_debug_trace.c` | `567f0ecda474c6172bc330499e3ec07099c3d140` |
| `tests/test_emu_debug_process.py` | `1bef88c7dc00af0b63620bb0a1df6fb78d6b5232` |

Original debugger evidence blobs (REV-013/014 have the whitespace-only
normalization exception below; the other four copies retain these exact IDs):

| Debugger-qualified path | Original debugger Git blob ID |
|---|---|
| `SDP/CodeReview/REV-DEBUG-SLC-013.md` | `81ae89c7c866cf41192fd2c2583a03ef86031601` |
| `SDP/CodeReview/REV-DEBUG-SLC-014.md` | `a13b914fd40ffd73a0d6043e5f19f0cbb7714ef0` |
| `SDP/CodeReview/REV-DEBUG-SLC-015.md` | `672afc4c327a2d42a644e873f808c829f9dc0e33` |
| `SDP/Verification/VER-DEBUG-SLC-013.md` | `93bba6bc0b11269fb864123fe8d1fa1d6925a86f` |
| `SDP/Verification/VER-DEBUG-SLC-014.md` | `07eebb7691d96cd25e33e8216cd340d9f6b45330` |
| `SDP/Verification/VER-DEBUG-SLC-015.md` | `6d7554042dc49a687c652d42ce42d5b4795b5046` |

Additional changes are reconciliation bookkeeping and the authorized nine-line
whitespace normalization only: this audit, implementation notes, current
handoff, registry identities/status, relations and one appended ledger event. Current-master design and evidence content remains unchanged;
accepted debugger design bodies including DES-090..097 remain unchanged.

## Worker verification

Initial Windows GCC strict C99 focused smoke passed via the reconciled Makefile:

```text
make -C tests debug-event-test debug-trace-test debug-runtime-test CC=gcc
  CFLAGS="-O2 -g -std=c99 -pedantic -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion"
debug event/watch tests passed
debug multi-trace router tests passed
debug dispatcher/runtime tests passed
```

Each executable was freshly compiled in this run. Full regression,
sanitizer/environment evidence and disposition belong to the fresh
Master verification and independent reconciliation review. This worker record
alone does not claim merge readiness or authorize merging PR #16.

Worker structural checks passed: duplicate-key-rejecting YAML parsing, NDJSON
parsing, exact reconstruction of both parent ledger byte streams, every master
registry entry and relation retained, accepted debugger slice states retained,
absence of conflict markers, and the eleven-suite Makefile union including
Windows/Linux variable names. Linux `make -C tests -B -n test OS=Linux`
enumerates all eleven build-and-run pairs.

The initial merge preserved all six historical evidence copies byte-for-byte.
Plain `git diff --check` treated seven old Markdown hardbreaks in two copies
as newly added whitespace, and the broader PR-versus-master diff also exposed
two such inherited lines in the existing design synopsis. Master explicitly
authorized only these nine whitespace removals to satisfy both plain gates;
this supersedes byte-identical preservation for those three files only.

| Path | Exact normalization |
|---|---|
| `SDP/CodeReview/REV-DEBUG-SLC-013.md` | Remove trailing spaces from lines 3–7 (five lines). |
| `SDP/CodeReview/REV-DEBUG-SLC-014.md` | Remove trailing spaces from lines 3–4 (two lines). |
| `doc/DEBUG_TRACEPOINT_DESIGN.md` | Remove trailing spaces from lines 3–4 (two lines). |

The prose, labels, ordering and all other content remain unchanged. Comparison
of every line after `rstrip()` matches the accepted parent content exactly;
original bytes remain available in the accepted parent Git blobs above and in
the initial merge. This normalization does not modify frozen DES-090..097,
any ledger byte, any product source or any test. The four other relocated
review/verification files remain exact byte copies. Plain whitespace checks
against exact master and the prior accepted PR baseline pass after this
bounded correction. No test rerun is needed for trailing Markdown whitespace;
the full new Master verification still follows on the final exact HEAD.
