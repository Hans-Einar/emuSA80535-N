# Handoff — Interface contracts

Updated: 2026-10-27
Disposition: SLC-020 accepted; SPR-012/ITR-020 complete; no active Slice

## Accepted deliverable

Contract bundle 0.1.0 lives at interfaces/0.1.0; canonical public headers at
lib/include/emusa80535. CPU/debug production sources use this include root;
legacy root headers forward. The bundle covers CPU/controller, board lifecycle
and bus/time/stimulus/inspection/output channels, session/debug facade, wire2,
DAP projection and host administration. New runtime surfaces remain unimplemented.

Reviewed candidate: 7d924cc27e48c599d5775904543070d92e413c26.
REV-SLC-020 approved at f911923; F001..F007 resolved; VER-SLC-020 passed.
SLC-020-ARTIFACT.json records exact source/manifest/archive identity and local
export path. The accepted payload is immutable, independently exportable and
verifiable. Consumer lock/copy instructions are in docs/11-versioning.md.
Native API1/board ABI1.0 and wire2.0 are separate from bundle0.1.0/product1.0.0
and retained legacy wire1.0. First native module profile is Linux x86_64 SysV LE.

## Cross-repository coordination

The user-authorized Ponsse agent reviewed consumer needs and prepared its own
SDK intake/adapter plan. Its input informed INT0..6, normalized ADC16, UART9
frame semantics, drive/release, initial phase limits, board-specific ordered
stimuli, pure inspection/current-bank peek and effective-pin capability limits.
Ponsse owns board payload schemas and its thin adapter; CPU/runtime remain here.
A full exact archive+source+hash pin precedes Ponsse's first adapter work, and
actual runtime and DAP implementation revisions are pinned separately. No
external repository source, lock or adopted SDK copy was created by this Slice.

## Next work

Record a new authorized implementation contract against bundle0.1.0 before
assigning a Worker. Start CPU cycle/access facade and common scheduler with a
synthetic board, preserving accepted CPU ordering and legacy run semantics.
Then dynamic loading, reusable session debugger, Unix/TCP, DAP attach and signals
must satisfy their stated implementation gates. Ponsse can independently intake
the frozen SDK and compile its adapter against a synthetic host. Any contract
correction discovered in implementation requires a new bundle version, not an
in-place edit of 0.1.0. Optional/unsupported capabilities must remain explicit.

SLC-018 packaging remains deferred pending integrated REQ-024 release evidence.
No packages uploaded or published. SDP kanban/sessions upgrade remains deferred.
