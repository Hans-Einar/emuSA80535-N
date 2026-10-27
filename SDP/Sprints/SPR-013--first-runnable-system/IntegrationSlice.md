# ITR-023 / SLC-023 — External composed-system verification

Status: active ITR-023 / SLC-023; verification only
Authority: direct user instruction to cooperate until a system builds and runs
Prerequisite satisfied: accepted SLC-022 candidate a03504b; SLC-022-ARTIFACT.json

The external simulator repository owns its integration Worker/Reviewer loop,
SDK/runtime locks, offline bootstrap, build orchestration, launcher, firmware
selection and board-specific assertions. This repository owns verification of
the generic runtime artifact and consumption of independently reviewed external
results. Do not copy firmware, board logic or machine-specific fixtures here.
No additional CPU/runtime changes are authorized by this verification contract;
a defect reopens its owning implementation contract with fresh review.

## Concrete handoff

Master supplies the accepted runtime source commit, reproducible source archive
and SHA256, SDK manifest/version, build target, local script1 input/output schema,
exact launcher options and implementation limits. Source archives include
SOURCE_COMMIT, runtime docs/schema/examples, licenses and build files. Ponsse
pins exact artifacts with explicit bootstrap; later builds work offline.
The external module's build identity and actual loaded-byte hash remain separate.

## Acceptance evidence

1. Rebuild the runtime from the pinned source archive without its Git checkout.
   Confirm emitted runtime commit and SDK manifest match the pins; build the
   separately pinned board against its SDK and verify its artifact identity.
2. Run the external deterministic synthetic firmware through the real CPU,
   real session and dynamically loaded board, including timed input, inspection,
   pause/step/reset and repeated runs. Compare architectural/time/board evidence
   while retaining session and lifetime identity differences explicitly.
3. Run the exact original firmware with bounded scripts. Record image hash,
   seed, clock, entry, module/config hashes, actual cycle/PC/stop/fault, and
   available post-fault board diagnostics. A reproducible unsupported stop is
   useful first-system evidence; it is not proof of complete machine boot.
4. Verify fault inspection defaults off and is explicitly selected only for the
   reviewed module promise; it does not permit mutation or further execution.
   Reset is explicit and repeated trials preserve deterministic results.
5. External fresh Reviewer approves its final integration commit and evidence;
   a fresh Reviewer here checks the source pin, commands, results and scope.
   Master records a durable concise verification/traceability/handoff record.

The first composed-system milestone is complete only after these concrete
build/run trials exist. Network/DAP attachment, administrative signals, richer
trace and signed RPM publication remain subsequent gates. SDP migration stays
deferred. No user relay is required between the two owners.
