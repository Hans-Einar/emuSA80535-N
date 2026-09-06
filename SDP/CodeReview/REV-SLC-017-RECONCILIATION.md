# REV-SLC-017-RECONCILIATION — Independent current-master reconciliation review

Date: 2026-09-06
Reviewer role: fresh independent SDP Reviewer; no implementation or merge role
Status: complete
Disposition: **approved for Master integration; no reconciliation finding**

## Authority and exact reviewed range

This bounded review follows repository `AGENTS.md`, the current ITR-017 /
SLC-017 reconciliation contract, and both Issue #14 Steering comments:

- [Merge-readiness gate, comment 5517725977](https://github.com/Hans-Einar/emuSA80535-N/issues/14#issuecomment-5517725977).
- [Stage 4 rebaseline, comment 5559152334](https://github.com/Hans-Einar/emuSA80535-N/issues/14#issuecomment-5559152334).

Both comments were fetched and read independently. The latter explicitly pins
the same current master used by this merge and requires Stage 0 through Stage 4,
including ADC and Timer2, to survive alongside the accepted debugger work.

| Checkpoint | Exact commit |
|---|---|
| Reviewed candidate | `c21d63f121f6305afd006a145a96c8dddae8b55e` |
| Reconciliation merge | `74dca67aafebdddc82ade3e5d6864a1457ea75bb` |
| Merge first parent: accepted takeover plus Master contract | `1ba90778aaa7a155bc68d8f913531d61414f8b1e` |
| Merge second parent: exact current master | `b43fe36b0965b6ac8628677bb6fcc16513d1f567` |
| Accepted prior PR #16 head | `1e588d28fb168a7c5a42c4c7dc4b51f84d29d1ed` |
| Accepted six-correction product commit | `d956177add44dda9efbd6d9e372a9c0a6d40f777` |
| Common branch ancestor | `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b` |

The review covers both parent-to-candidate diffs, the accepted-PR-to-candidate
diff, the merge resolutions reconstructed with `git show --remerge-diff`, and
the subsequent `74dca67..c21d63f` documentation correction. Ancestry checks
confirm that both parents, the accepted prior PR head and the accepted product
correction are ancestors of the reviewed candidate.

The existing holistic and corrective reviews, frozen `EMU-DEBUG-DES-008.md`,
current sprint/handoff, and Worker audit were read. Worker assertions were
independently checked against committed blobs; they were not used as proof.
Concurrent Master documentation integration after this candidate is outside
this exact reviewed range. No product file was written by this Reviewer.

## Findings

No blocking, high, medium or low reconciliation finding was identified. No
accepted SLC-017 correction was lost, no newer master source or regression test
was overwritten, and no unauthorized implementation scope entered the merge.

One inherited traceability observation is explicitly distinguished from a new
finding: `Relations.yaml` references `REV-SLC-016-F001`, while `CurrentIndex.yaml`
does not register that individual finding. The identical omission and both
relations already exist at accepted head `1e588d2`; this reconciliation did not
introduce or enlarge it. It does not invalidate the collision-preservation
checks below and is not grounds to reopen accepted SLC-016 behavior.

## Product and master preservation

An independent complete `git ls-tree -r` comparison classifies the candidate's
155 tracked paths as 87 identical to both parents, 25 identical to the debugger
parent only, 29 identical to master only, and 14 that match neither same-path
parent. No path in either parent is absent from the candidate.

Every master C/header/test source is retained byte-for-byte. The only C/header/
Python/JavaScript test paths differing from master are the nine already accepted
standalone debugger module and focused-test additions. In particular, `core.c`,
`emu8051.h`, Stage-0/IRQ/timer/UART/port-MOVX tests, external-edge tests, ADC tests
and Timer2 tests match exact master. Critical retained blobs include:

| Master path | Retained Git blob |
|---|---|
| `core.c` | `1ec658039e2f20173c7a178deef036e48b5615a0` |
| `emu8051.h` | `1bdc0b6a46871ea848c890581fca0a8616ab8fdf` |
| `tests/test_stage3_adc.c` | `e7ac5c96364c2d5796719f115c94c67f1631f22b` |
| `tests/test_stage4_timer2.c` | `ec0a15e69c19386ac27a1ec01c4f546bb031e1fc` |

Master README, peripheral design documents, sprint histories and canonical
REV/VER-SLC-013..015 files are also exact imports. Existing opcode, loader,
disassembler, `emu_debug.c/.h`, server, facade/process tests and DAP integration
test content is unchanged from the applicable baselines.

All six corrections are preserved by equality of all five product/test blobs
changed in `d956177`, not merely by presence of that commit in ancestry:

| Accepted correction | Preserved implementation and regression evidence |
|---|---|
| F001: reject forged reset/load through ordinary ingest | `emu_debug_runtime.c`; `test_bounds_and_invalid_ingest` |
| F002: retain lifecycle marker using entry enabled state | `emu_debug_trace.c`; `test_lifecycle_gate_entry_snapshot` |
| F003: invalidate CODE-capable address-only selectors on load | `emu_debug_runtime.c`; `test_lifecycle_and_code_invalidation` |
| F004: reject suppression-losing replacement until flush | `emu_debug_trace.c`; `test_suppression_replacement_requires_flush` |
| F005: retain session trace-ID history until clear-session | `emu_debug_trace.c/.h`; `test_trace_id_lifetime_and_registry_bound` and `test_trace_id_reuse_resets_only_on_clear` |
| F006: bounded UTF-8 validation | `emu_debug_trace.c`; `test_utf8_metadata_validation` |

The event/watch, trace and composed-runtime modules and focused tests also all
match accepted PR head `1e588d2`. Their tests remain invoked by the reconciled
Makefile. Source equality establishes preservation; fresh behavioral regression
and sanitizer evidence remains the separate Master verification gate.

## Build reconciliation

The reconstructed merge reports exactly ten conflicts: `tests/Makefile`, the
three traceability files, and the six colliding review/verification files.
No CPU or debugger implementation source needed conflict resolution.

The test Makefile was inspected against both parents. Independent forced dry
runs feed each committed Makefile to `make -C tests -f - -B -n` with both
`OS=Windows_NT` and `OS=Linux`. The candidate's `test` compile/run pairs are the
exact union of the parent pairs, with no duplicate suite and with each parent's
relative execution order preserved:

Stage-0, IRQ, timers, UART, port/MOVX, external edges, ADC, Timer2, event/watch,
trace router, and composed runtime — eleven suites in each OS branch.

`all`, separate focused targets, and `debug-test` resolve successfully in both
branches. The debugger facade and Python process recipes are retained. Parsed
cleanup inventories equal the exact union of the parent inventories: 53 named
Windows cleanup files and 29 Linux cleanup files. Cleanup was only dry-run;
the Reviewer did not remove or rebuild concurrent Master test artifacts.
The root Makefile remains byte-identical to the accepted debugger parent.

## SDP identities, evidence and ledger

Independent duplicate-key-rejecting PyYAML parsing passes for both YAML files.
Expanding the registry ranges produces 283 distinct IDs without overlap.
All master registry entries, including requirement and accepted peripheral
states, are retained verbatim. All 157 master relations survive. After applying
the documented debugger aliases and expanding ranges, every debugger-parent
relation survives as well. The only added relation beyond the parent union is
the reconciliation audit relation. The older planned `REQ-013..REQ-015`
aggregate is correctly superseded by master's individual current states;
REQ-012 retains master's satisfied state.

The 48 debugger aliases explicitly qualify the parallel sprint, iteration,
slice, review, verification and DES-064..097 collisions. Master keeps canonical
IDs and evidence paths. The active debugger sprint uses `DEBUG-SPR-008`, and
SLC-017's composition points to `DEBUG-SLC-015` and `SLC-016`. Frozen design
references remain source-scoped debugger references, with the registry and
relation graph resolving their qualified identities. Accepted debugger slice
states and the six resolved holistic findings are retained.

All six colliding debugger evidence documents survive at `REV/VER-DEBUG-*`
paths. Four are byte-identical to the originals. Independent line comparison
confirms only five trailing-space removals at REV-DEBUG-SLC-013 lines 3–7 and
two at REV-DEBUG-SLC-014 lines 3–4. The synopsis
`doc/DEBUG_TRACEPOINT_DESIGN.md` has only two such removals at lines 3–4.
There is no other textual change in those three files. Exact original bytes
remain available in the accepted history and initial merge.

All 226 candidate ledger lines parse as JSON. The first 225 lines reconstruct
both parent byte streams exactly: common lines 1–130, master suffix 131–182,
then debugger suffix 183–225 (debugger-parent original lines 131–173). The
three independently calculated SHA-256 values match the Worker audit:

- common: `6ae4664fb7264bd9e9000b7410b30901e723b6b5b834442f496468e6a9269dac`;
- master: `c4648180f830e2ded62fec73a6a076e4a560fa98f389a505e10cae22693fc89b`;
- debugger: `4fae0e13d4792529e1fad33c87a0501cb55134b6a7ec6847b73881dfeda1a97f`.

The 31 duplicated historical event IDs are precisely EVT-2026-09-02-138..168,
each occurring twice across the two preserved suffixes. They are expressly
disambiguated by source block and original commit/line, including in the new
RECON-002 event. No event was silently discarded or rewritten. Consumers must
honor this provenance rather than assuming historical event_id alone is unique.
The ledger preserves history; the qualified registry supplies current identity.

## Frozen design and scope

`EMU-DEBUG-DES-008.md` retains accepted blob
`df05e81a36d544f0b9c89e7e6ae0f81d28460d81`. Its eight decisions remain coherent:
one emulator-owned semantic model; sized/versioned public wrappers; caller
ownership and atomic operations; exclusive `after_record_sequence` paging;
session/epoch/stale/loss metadata; later CLI/protocol exposure; DAP projection;
and separately authorized producer/safe-boundary integration before frontends.

The registry still classifies DES-090..097 as target design. The raw current
runtime remains internal, and the merge does not implement the frozen future
wrapper or external cursor. New master CPU/peripheral content is an exact
upstream import, with no debugger producer or safe-boundary hook added. No
frontend, CLI, wire extension, DAP, sink, source-map, physical-I/O or P1000
implementation appears in the reconciliation.

## Independent verification and disposition

Reviewer tools: Git 2.43.0.windows.1, GNU Make 4.4, Python 3.14.0 and PyYAML
6.0.3. Checks used committed candidate/parent objects to avoid conflating
concurrent Master documentation edits with the reviewed candidate.

Passed: ancestry, full tree/blob comparisons, both-parent/remerge conflict
audit, both-OS forced Makefile dry-run union and cleanup checks, duplicate-safe
YAML parse, range/alias/relation preservation, NDJSON parse and exact parent
ledger reconstruction. Plain `git diff --check` passes from each merge parent
and accepted prior PR head to `c21d63f`.

Before finalizing this report, Master completed and supplied
VER-SLC-017-RECONCILIATION and SLC-017-RECONCILIATION-EVIDENCE.json. The Reviewer
read them and independently verified all 81 accepted steps per environment
against their raw log byte counts, SHA-256 hashes and successful exit codes.
All accepted static-analyzer logs are empty. Both actual Makefile log hashes
match their manifest records and each log contains all eleven suite passes.
The evidence binds to exact `c21d63f`; the corrected native Clang harness and
initial failed harness attempts remain disclosed. Valgrind is recorded as
NOT_AVAILABLE under the where-available rule. These are inspected Master-run
results, distinct from the Reviewer-executed structural checks above.

**Approved for Master integration.** No corrective Worker pass is requested.
Master must integrate the completed review and verification into sprint/handoff,
CurrentIndex, Relations and Ledger and recheck final product/test/build identity
before declaring merge readiness. SharedUI is not applicable to this C runtime
reconciliation. Publication of a verified, mergeable exact final head and
`READY_FOR_STEERING_MERGE` remains Master's responsibility; PR #16 merge remains
Steering's responsibility.

## Final SDP integration follow-up — Master record

The same independent Reviewer inspected the final SDP-only integration after
the candidate review, including the actual evidence manifest and raw logs.

Evidence-helper observation `REV-SLC-017-EVIDENCE-F001` was opened and resolved
before publication: the initially added reproduction script trusted mutable
checkout inputs while labeling them with `--head`. It now requires an exact
full commit SHA, extracts a fresh Git archive with canonical line endings,
and compiles/executes only the extracted snapshot. Master checked all 155
snapshot files against committed inputs on both platforms and symbolic HEAD
rejection; both canonical archive hashes equal
`66de3bc399339272dc5d9a3febdb5953def2ad91e5d872f5db53432610cd807e`.
The executed product matrix was already independently source- and log-audited;
this supplemental tool correction does not invalidate it or change product code.

The independent Reviewer approved the final SDP integration with no remaining
blocker. Current statuses, latest Stage-4 authority, retained ledger prefix,
new completion events and unchanged non-SDP candidate content were checked.
This addendum records that follow-up without changing the original candidate
review range. The final exact remote head/master/mergeability check remains
the Master publication gate, and PR merge remains reserved for Steering.
