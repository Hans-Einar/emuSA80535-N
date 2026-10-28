# Contract conformance and consumer handoff

This is a declaration/schema/export gate, not verification of target runtime
behavior. No example links missing target symbols. Runtime implementation must
pass the behavioral obligations below before advertising the corresponding
capability or coverage bit. Tests in check_target_contracts.py validate fixtures,
negative contracts, C99/C++17 native prefix/layout, and offline packaging only.

| Contract class | Gate evidence | Future implementation evidence required |
|---|---|---|
|Versions/native ABI|isolated headers; prefix/width checks; separate macros; legacy bundle exact hash|query size/version/flags/trailing bytes and capability coverage|
|Control/leases|every command request/result and common error schema; frame/U64/malformed/state/correlation negatives|Unix UID/path/timeout/queue bounds, disconnect persistence, no replay, lease handover|
|Execution facts|all28 kinds, phases, identity/known flags and value-width negatives|every opcode begin/retire/abort, delay failure, disabled-trace equivalence, actual byte capture|
|Input/UART|board/CPU-stage correlation negatives and UART IDs|board failure before CPU scheduling, required due failure, REN/busy/RI/SM2 drops, holding replacement, TI/stop/end timing|
|Trace/config/pages|limits/references/order/epoch/loss negatives; all four record kinds|single matcher/router ordering, atomic config, safe stops, uint64 generation, ring wraps and suppression|
|Capture A/B|scope request constraints, typed context declarations|nested/reentrant calls and IRQ, mid-function unknown, RETI vs RET, unwind/overflow/fault/reset/rearm, included closing retirement|
|File sink|header/record/footer schema and bounds|regular file policy, queue overflow, rotation, slow/failing I/O, partial footer, requested/completed barriers|
|Delivery|reproducible export, archive/copy/lock negatives; baseline reconstruction|exact reviewed source pin carried by actual producer release|

Run `make interface-target-check` and `make interface-check`. Run `make
runtime-session-test`, `make cpu-link-test` and current runtime identity checks
when build/tool metadata changes. New runtime behavioral tests are deliberately
not claimed here. SOURCE_COMMIT and BUNDLE-RECEIPT identify full source revision;
consumer.lock additionally binds repository, version, manifest, archive hash,
versions map and local copy path. Vendor only the verified complete archive.

Modern tool default remains0.2.0. Its immutable reconstruction has source pin
0a77c2d0da22ecb0bd37781b430e30444f779e39 and archive SHA256
5abd71f52925c8fcf299b8b8ed417279a1bc20f2a5f004425bc083f87e9cf91d.
Historical0.1 files and accepted archives are unchanged. New0.3 includes baseline
0.2 docs/schema/reference and frozen common-tool snapshots for independent
reconstruction; `--version 0.3.0` selects target export/verification. Do not refresh
accepted0.2. Only candidate0.3 manifest may be refreshed during authoring.

After separate review/verification and exact clean export, record its manifest,
source and archive pins in the consumer's own lock before dependent work.
Acceptance of this SDK does not activate runtime or external-board work, and
existing runtime/board consumer pins stay unchanged. No release/RPM publication,
external repository edits or physical transport is part of this gate.
