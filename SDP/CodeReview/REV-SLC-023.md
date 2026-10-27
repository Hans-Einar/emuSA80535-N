# REV-SLC-023 — External composed-system verification

Status: approved; no unresolved findings
Date: 2026-10-27
Execution Reviewer: fresh independent Reviewer `/root/integration_reviewer`
Recovery Reviewer: fresh independent Reviewer `/root/integration_recovery_reviewer`
Authority: SPR-013 / ITR-023 / SLC-023, REQ-026, DES-115..DES-118
Generic runtime candidate: `a03504bd186efe3a8882bd3f1cdb5c4cf463f30d`
External integration candidate: `9f673bb3660cb1a957a95110a69283a55ad990ba`

## Scope and decision

Approve the bounded local composed-system milestone against VER-SLC-023 and
SLC-023-INTEGRATION.json, committed through `cf3f3a3` (initial record `dcc040e`).
The external fresh review and Master acceptance are committed at
`4682bb89d075838bb8d836b165f1b2b2499e1dd2`. All five acceptance-evidence conditions
in IntegrationSlice.md are satisfied; no unresolved finding remains. This is
verification of the accepted runtime consumed by
an independently built external module, not authorization for further CPU or
board implementation. No product source was changed by this Reviewer.

Read AGENTS.md, CurrentIndex, the active Sprint/Handoff/IntegrationSlice,
applicable mandate/study/requirements/architecture/design, the accepted SLC-022
review/artifact, and the external module review, integration contract, launcher,
locks, tests and RUN_V0 instructions. Historical SDP identities and source-scoped
DEBUG aliases remain untouched.

The execution and behavioral checks below were completed by the named execution
Reviewer before the daemon restart. Recovery review validates their retained
evidence and unchanged candidates; it does not present those earlier runs as
newly repeated tests.

## Exact inputs and build checks

The accepted runtime source archive independently hashes to
`1157a016d23462df28b078b53ce82bd5ffa4667e55a1b46033e46f21ed2d8757`.
An independent extraction without `.git` preserves SOURCE_COMMIT and the SDK0.2
manifest hash `7be6be79dac99753c051b9b1a257e0774a14bf1115d70553e1371a513e583e72`.
Both the default documented runtime target and the external pinned build succeed;
`--version` reports the exact source, script1, API1, boardABI1.0 and wire2.1.

The external source candidate was independently exported into a separate
directory without `.git`. Its documented explicit archive bootstrap builds the
runtime and separate old-SDK0.1 module offline and passes all eight existing
module/SDK/board checks. The normal module, companion identity and runtime
executable match their immutable external lock:

| Artifact | SHA256 |
|---|---|
| Runtime executable | `6d5150cdbddb03c9cd3ad94c242fdae9d84d3431f8edb427bd74c25e4e003568` |
| External module bytes | `05004801bd998bda337d293e1a4df58e9b8ca5b606633afe20f19cf5a76def83` |
| Companion artifact / module build ID | `94984489876107bd74c9179c359755ac1420dfec3835aed356ce87807776bc53` |

The runtime executable also reproduces independently from the accepted archive
in another directory using `CC=gcc` and
`CFLAGS=-O2 -pipe -Wall -Wextra -Wno-unused-parameter -Wshadow`. The omission
of debug paths is deliberate. This proves the recorded local toolchain build;
it does not establish identical binaries across compiler/linker versions.
The launcher explicitly rejects other bytes until a reviewed lock update.

Inspection confirms bounded archive intake, explicit local archive paths,
complete original-source and SDK verification, preserved immutable old SDK,
separate actual module and companion hashes, and exact image/config identities.
Launch uses absolute paths without a shell. Source, SDK, lock-bound receipt and
pinned artifact bytes are checked on every reuse/launch. The external Master
clarified the contract to permit an ignored default or caller-selected build
directory instead of requiring its name to contain the hash. Integrity depends
on verified contents; invalid existing builds are refused. This resolves the
review's contract-wording concern without a product correction.

## Independent behavior and rejection checks

The Reviewer reran the frozen synthetic launcher with its own exclusive build.
Actual CPU effects match the independent fixture oracle: cycle23/instructions14,
then timed input24, cycle64/instructions36, verified register readback and board
counts, unchanged pause snapshot and a step to cycle66/instructions37. Explicit
reset produces the same architectural/time/board evidence while advancing
generation and lifetime admission identity.

Four targeted negative tests pass independently: a mutable receipt cannot bless
altered runtime/module/companion bytes; corrupt archives reject before extraction;
wrong image/module hashes produce startup exit2 and no result stream; and child
process cleanup removes a descendant after its leader exits. The verification
Worker separately ran the complete thirteen-test integration suite, all four
documented scenarios and independent frozen-schema validation.

The repeat normalizer preserves the complete ordered result stream and adjusts
only declared lifecycle identity fields. Independent mutations of cycle,
registers, board input, diagnostics and loss flags each remain unequal after
normalization. The verification Worker's separate-process synthetic and original
firmware trials compare equal after removing only sessionId. Raw evidence is
retained alongside normalized assertions.

The original image's bounded trial reaches the exact real unsupported stop
recorded by the external acceptance oracle. Later queued run requests reject with
STATE, zero progress and unchanged fault snapshots; they do not execute further
instructions. Explicit pure post-fault inspection retains the original context,
and reset/repeat reproduces the result. The verification Worker additionally
checks that default-off fault inspection rejects without state changes, and
that explicit inspection permission does not permit step or board-input mutation
after the fault. Exact firmware PC, access/decode context and board-specific
diagnostics remain in the external `SDP/Verification/VER-PERIPH-RUN-001.md`
and `SDP/Verification/PERIPH-RUN-001-evidence/` evidence.

## Evidence and limits

Review execution root: `/tmp/slc023-review-xudzbjh2`.
Durable review logs, build receipt, raw synthetic results and commands:
`/home/warloc/git/emuSA80535-N-artifacts/integration/SLC-023/9f673bb3660cb1a957a95110a69283a55ad990ba/reviewer`.
Its `sha256.json` hash is
`2cd5142675d36df8a6b64b8215c71d6ff78b33e1620e9d08df1b1060acd31b66`.
No firmware, board implementation or machine-specific fixture is copied here.

This is a local Linux x86-64 laboratory using the pinned normal toolchain,
trusted native module and deterministic virtual input. Python3.6 compatibility
is grammar/API inspection; execution uses Python3.12. It establishes buildable,
runnable composition and an honest bounded firmware blocker. Full firmware boot,
physical hardware equivalence, remote/DAP attachment, administrative signals,
rich trace, RPM publication and SDP migration remain outside this acceptance.

## Recovery evidence validation

The daemon restart interrupted final integration of the records after the tests
had completed. The recovery Reviewer independently verified all 11 retained
execution-review files and all 77 verification-Worker files against their exact
manifest digests. All six external product files match both frozen commit
`9f673bb3660cb1a957a95110a69283a55ad990ba` and the file hashes in the external
fresh Reviewer's `REV-PERIPH-RUN-001.md`. Generic runtime product sources remain
unchanged from accepted candidate `a03504b`.

The external fresh review explicitly approves the frozen product and bounded
build/run evidence. Its content SHA256 is
`27cf6e6bbfb71c07c8f89c7bae19b67fd566fda92af8f44fea2c5a2555ea53b0`.
At initial recovery capture that approval was present on disk without its
bookkeeping commit. The external Master subsequently committed acceptance at
`4682bb89d075838bb8d836b165f1b2b2499e1dd2`. The recovery Reviewer independently
verified that the committed review exactly matches the retained snapshot and
that all six external product files remain identical to the frozen candidate.
Final VER-SLC-023 and SLC-023-INTEGRATION.json were reviewed, including their
recovery manifest and each retained recovery-file digest. The unchanged product
and intact execution evidence justify acceptance without repeating completed
behavioral tests. Master here owns final local traceability and Sprint closeout.
