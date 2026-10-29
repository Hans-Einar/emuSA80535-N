# REV-SLC-031 — Read-only CODE breakpoint enumeration

Status: APPROVED for exact combined product source
59f7a5c769a4fbf9eb73d0d660e94c58534c1c9c. Master artifact verification,
freeze and Slice acceptance remain separate.
Reviewer: fresh enumeration_reviewer. Active contract: SPR-014 / ITR-031 /
SLC-031, EnumerationSlice.md, activation0ff7e7fa1c131f0e55f0f29707f8c2b59b137441.
Runtime Worker commit53106b2c6297db384f8dc3f5dd9c27892cbc0437 precedes
Contract Worker commit59f7a5c769a4fbf9eb73d0d660e94c58534c1c9c.

## Reviewed boundaries

The new operation reads the authoritative CODE bitmap at the existing serialized
owner boundary. Its uint32 scan includes65535 without wraparound. It returns
only sessionId, current generation and ascending unique integer addresses;
existing replacement bounds limit the complete set to1024. Attached observers
and controllers need no lease to read. Wrong-session, unattached, faulted and
terminated behavior agrees with the Slice and existing connection rules.

Successful hello fixes minor3.0 or3.1 independently for each connection.
The new command is recognized only after a successful3.1 negotiation. Frozen3.0
hello identity/capabilities, unknown-command closure and required-capability
rejection remain unchanged. A failed hello does not opt a connection in.
Enumeration changes no CPU, stop revision, breakpoint suppression, trace state
or ownership. It supplies neither a revision/CAS token nor a disconnect restore
guarantee. The documented temporary replacement workflow correctly requires an
uninterrupted exclusive controller lease.

No CPU core, opcode, CPU facade, public header, frozen0.1/0.2/0.3 payload, legacy
snapshot or existing interface tool changed. There is no UART behavior change,
new clock, physical I/O or consumer-specific implementation. The native query
table remains emusa80535_control_query_v3. Wire-only0.4 and shared-session trace
file0.3 identities are explicitly separate.

## Independent exact-source verification

All final runs use a fresh detached checkout at the reviewed source under
/tmp/slc031-review/candidate. They launch only privately owned synthetic runtimes
and modules. No existing lab, consumer repository or runtime process was used.

- Independent Unix harness:82 actual validated replies cover empty/clear sets,
  unsorted replacement followed by sorted enumeration,0/65535,1024 upper-range
  addresses, reset-preserved bitmap and new generation, observer access while
  the controller holds its lease, exclusive temporary replacement/restoration,
  lease handover, paused/running/faulted states, foreign session, unattached and
  malformed requests, strict mixed3.0/3.1 clients and old profile rejection.
  Status, stop detail and trace configuration remain identical across reads.
  Reading at a CODE stop preserves the existing skip-once behavior on resume.
- Independent trace/file harness:60 actual replies prove active capture A,
  trace ring/page, outcome page, stop/configuration and cycle counters unchanged
  by repeated reads, both paused and externally-driven running. A sink opened
  through3.1 produces six actual JSONL rows with the frozen0.3 header and manifest.
  Enumeration preserves flushed file counters; the sink closes without loss.
- Independent idle/power-down harness:22 actual replies prove both CPU modes
  remain readable without progressing the owner or changing status.
- Independent native harness, repeated against final source with ASan/UBSan,
  leak detection and halt-on-UB, proves insufficient response capacity leaves
  hello/read retryable without mutation; a host-driven owner read itself does
  not pump the CPU; second hello cannot change the negotiated profile; terminated
  client identities cannot enumerate. No sanitizer finding.

The final Worker Unix suite was independently reproduced:104 validated replies
and10 base0.3 JSONL rows. Native control31/control3/C++ and session tests pass;
the legacy native session fixture validates1116 actual wire2.1 responses.
Existing Unix3.0 control/leases/reconnect/framing, script1 host, UART outcome,
persistent trace and invocation/shared-file suites pass. Persistent trace reports
109 replies; invocation reports173 replies and48 JSONL rows. Asynchronous sink
poll counts are evidence from this run, not a protocol-stable count.

Evidence: /tmp/slc031-review/final-*.log, independent-wire.json,
independent-trace-replies.json, independent-trace-file.json,
independent-modes.json, final-worker-wire.json and
final-native-asan-responses.jsonl. Independent sources are review_wire.py,
review_trace.py, review_modes.py, review_bundle.py and native.c in that directory.
manifest.json records their hashes and exact reviewed source. Early reviewer
fixture corrections and an initial parallel test build before its prerequisite
library existed were harness/setup issues; final clean-source runs pass.

## Contract and artifact integrity

Reviewed sidecar manifest SHA256:
80932dd18f883438074b37c167828484b8cf6f95ce74a309526b6b709c60383a.
Its12 payloads include complete3.1 schema, examples, documentation, exact frozen
baseline validators, unchanged trace-file schema, independent helper and tests.
Reconstruction/structural comparison confirms inherited3.0 schema variants are
unchanged except hello identity/capability and the new command/error alternative.

Independent helper verification passes49 semantic/framing/provenance/tamper
negatives, including bool/floating addresses, range/order/duplicates/count,
canonical nonzero uint64 generation, response/session correlation, duplicate
JSON keys, extra fields, inventory/path/symlink/tamper and receipt refusal.
A deliberately poisoned pre-imported bundle module cannot replace the pinned
baseline dependency, and the caller's module binding remains untouched.
Deterministic exports are byte-identical; installation and isolated Git-less
copied validation pass. Worker check_contract.py independently reproduces71
focused checks, including nonregular files and inherited schema reconstruction.

The external accepted lock design resolves the provenance issue raised during
review: later runtime revisions must not re-stamp the same0.4 contract archive
with their runtime source. The helper selects the locked contract source and
verifies manifest and canonical archive before writing. Independent tests prove
missing/conflicting/malformed supplied locks and wrong archive hashes fail,
with no destination/archive created. The Makefile selects the external lock
when present; pre-freeze candidate fallback is explicit. Master must create and
retain that lock at freeze and verify installed copies against it. Runtime source
identity remains separate. No manifest hashes itself through a payload.

Both existing interface checks pass, including baseline38 and target64 payload
checks and immutable reconstruction. Independent frozen0.3 export still has
archive SHA256405f22f7fc7e0a49d921715102102a4d667ba1ff84b1238494d76353feeb0f53
and its original source2133e7220284bb8dc1600ba98b2d842106a7dba4.

## Disposition

No blocking finding remains. Approve the exact combined implementation and
sidecar for the Master's reproducible export, Git-less build/install and actual
installed-binary verification gate. Do not describe the sidecar or runtime as
accepted/frozen until that gate and external lock are recorded. Existing028/V0
and consumer pins remain unchanged; consumers opt into a new pin and explicit
hello3.1 when they choose to use enumeration.
