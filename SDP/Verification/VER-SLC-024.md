# VER-SLC-024 — Observation and persistent-control contract SDK

Date: 2026-10-28
Disposition: PASS, contract gate only
Reviewed source: `2133e7220284bb8dc1600ba98b2d842106a7dba4`
Review: REV-SLC-024 approved at `52682f9f985affb09da6910385f0316c390aab13`
Artifact identity: [SLC-024-ARTIFACT.json](SLC-024-ARTIFACT.json)

The Worker delivered target SDK0.3.0, control wire3.0, separately queried CPU
observation extension1 and trace facade1. The fresh Reviewer approved the exact
candidate after F001..F008 corrections. This accepts declarations, schemas,
examples, semantic fixture validation and delivery tooling. REQ-027..029 remain
implementation targets; no listener, CPU producer, capture or writer is supplied.

## Verification performed

Worker passed `make interface-target-check`, `make interface-check`,
`make runtime-session-test`, `make cpu-link-test` and runtime build/identity
checks. The session regression validates1116 actual wire2.1 responses. The
historical-header link fixture now enumerates headers from its immutable Git
pin instead of requiring newly added target headers to exist in old history.
The final successor changes only trace documentation and its manifest; the
Worker and independent Reviewer reran the target checker on that exact revision.
Reviewer adversarial checks and their limits are recorded in REV-SLC-024.

Master independently exported the committed source without Git or sibling
repositories, applied the exact two-file documentation successor and verified
its full source identity. `make -j4 emu-runtime sdk emu-debug` and staged
`make install prefix=/usr libdir=/usr/lib64 DESTDIR=...` passed. Both SDK0.2 and
SDK0.3 complete bundles are installed alongside canonical headers. Running
`packaging/check_install.py` against only installed files passed CPU, MOVX,
decode, debug/runtime linking and legacy protocol1.0/source identity. A separate
installed-only C++17 consumer of control3.h and CPU API1 linked and ran. No
unimplemented query symbol was linked or falsely advertised.

The rebuilt runtime identifies source2133e722, script1, bundle0.2.0, CPU API1,
board ABI1.0 and wire2.1. Target headers use separate version/availability macros.
Master ran `interfaces/tools/check_target_contracts.py` from the **installed**
SDK0.3 directory, without Git, to verify standalone C99/C++17 headers, all command
request/result/error and observation/storage shapes, semantic negatives,
reproducible export, copy/lock tampering and both version inventories. It passed.
These fixtures validate contracts, not the future runtime's behavioral obligations.

Master exported SDK0.3 from the exact isolated source, then reproduced the same
archive at its durable artifact path. Archive SHA256:
`405f22f7fc7e0a49d921715102102a4d667ba1ff84b1238494d76353feeb0f53`.
Manifest SHA256:
`ed3917b0d3a7300aca94171f1259eacf9861b0ee87376afa534690618da27ff0`.
SDK0.2 reconstruction is byte-identical to its accepted archive, SHA256
`5abd71f52925c8fcf299b8b8ed417279a1bc20f2a5f004425bc083f87e9cf91d`.
Frozen old directories and compatibility version.h remain unchanged. Historical
ledger prefix and source-scoped debugger aliases are preserved. Final Master
integration validates YAML paths/relations, append-only ledger and new IDs.

Durable build/install logs and checksums are under the artifact record's
verificationEvidencePath; the temporary isolated build is
`/tmp/slc024-master-f5df31a` (advanced to exact2133e722 before final build/export).
No RPM, network publication or SDP tool migration was performed.

## Acceptance boundary

SDK0.3 is frozen for consumer contract intake. Existing runnable V0 runtime/module
pins remain unchanged. The original agreed producer reply is unchanged; the
completion handoff records advance-or-step and reset/scope clarifications plus
frozen versions and archive identity. Planned SLC-025..028 are separate product
implementation gates. Their behavioral conformance matrix remains mandatory
before any new capability is advertised.
