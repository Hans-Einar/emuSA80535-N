# ITR-031 / SLC-031 — Opt-in read-only CODE breakpoint enumeration

Status: accepted through REV-SLC-031 / VER-SLC-031. Sprint SPR-014. Baseline accepted0288dd1230 plus
accepted evidence-only030 Mastera8c1e66. Authority: explicit shared-user coordinator request,
consumer concurrence2026-10-28 to communicated3.1 proposal. Read AGENTS,
CurrentIndex, Sprint/Handoff, lifecycle01..05, frozen0.3 contracts and this Slice.
Fresh Runtime and Contract Workers and separate fresh Reviewer; Master artifact gate.
No user lab/process access, consumer repo edits, UART/CPU behavior change or pins.

## Consumer-concurred protocol

Explicit per-connection hello protocol{major:3,minor:1}, contractVersion0.4.0
opts into a new wire-only extension. Successful hello uses its own manifest SHA
and advertises capability readCodeBreakpoints. Command is readCodeBreakpoints.
Frozen3.0 hello remains3.0/0.3.0, oldmanifest and exact oldcapabilities/schema.
Its unknown new command retains old rejection/close behavior; required newcap
returns UNSUPPORTED. No silent version fallback/probing or automatic replay.
No additional host flag is needed; explicit newhello is opt-in. Same endpoint,
authentication, single execution owner, leases and framing remain unchanged.

Arguments exactly {sessionId}. Successful body exactly {sessionId,generation,
addresses}. Identity is current32 lowercasehex session and canonical nonzero
uint64 decimal-string generation from the SAME owner-serialized snapshot.
Addresses are actual JSON integers (not booleans), inclusive0..65535, strictly
ascending unique, length0..1024. Return complete authoritative CODE bitmap list,
including empty[] and upper endpoint65535; no paging. Not a trace/watch listing.
Use wider index than uint16 for scanning full bitmap without overflow.

Attached observer/controller may read paused/running/faulted and other existing
nonterminated states. No lease or expectedgeneration needed for read. Wrong
session STALE; unattached STATE; terminated not readable (STATE if still an
addressable connection, otherwise existing closure/NOT_FOUND rules). Malformed
fields/framing follow profile INVALID/close policy. Capability/state/session
checks precede read consistently with existing dispatch. No CPU progress,
state/stopRevision/lease mutation, breakpoint suppression or trace counters change.
Reset preserves the existing bitmap but increments response generation.

No breakpoint revision or CAS is added. Consumer must hold exclusive controller
lease across snapshot/temporary replace/explicit run/restore to exclude another
controller. Disconnect/uncertain result invalidates automatic restore guarantees.
No hidden reset, clearing, run, retry or fallback; no claims about physical RX.

## Immutable wire artifact and implementation boundaries

Add interfaces/0.4.0 as a wire-only sidecar: complete3.1 schema, semantics/README,
examples, inherited exact validator references/copies, manifest and independent
check/export/install helper. Manifest identifies version/kind/protocol, immutable
0.3 base source/manifest/archive pins, unchanged native interfaces and trace-file
base contract, plus exact payload inventory/hash. Do not hash its own manifest
inside a payload/helper that embeds the final manifest hash (self-reference).
External lock/receipt supplies final source/manifest/archive pins after review.
Separate deterministic sidecar archive is delivered with existing exact0.3 archive;
no need to duplicate all native SDK payloads. License and provenance retained.

New hello0.4 manifest identifies wire extension ONLY. CPU API1, board ABI1.0,
observation/trace facade1 and emusa80535_control_query_v3 remain unchanged;
existing JSON request table selects minor per connection. No new public header.
Shared-session trace file headers remain frozen0.3/base manifest independent of
which connection opens the sink. Clearly document this; do not relabel files0.4.
Vendor exact baseline validator/tool payloads if needed and wrap their inherited
semantics; no second matcher. Verify inherited checksums rather than trusting
relative imports that accidentally find different installed/source modules.

Preserve interfaces0.1/.2/.3, canonical lib/include/contracts and old bundle.py
registry/export pins. Add private3.1 recognition/argument validation and profile
state without widening3.0. New response serializes actual bitmap at command time.
Owner request work is bounded; no CPU callbacks, new clock, socket or file side
channel. Install new sidecar alongside oldSDKs; Git-less export/build/install must
work using only archive contents. Current runtime help/version/docs may describe
new negotiated profile without changing old protocol response identities.

## Verification and review gate

Runtime Worker owns session/profile implementation, private validators, root/tests
Makefiles, runtime docs/targets and native/Unix tests. Contract Worker owns only
interfaces/0.4.0 schema/docs/examples/manifest/export+validation helper and focused
contract tests there. Coordinate manifest/validator/build/install seams before
shared work; Runtime Worker applies Makefile additions requested by Contract
Worker. No shared edits without agreement. Separate fresh Reviewer owns
REV-SLC-031 and independent fixtures. Actual native and Unix tests must verify
empty, unordered replace->sortedread, clear, reset samebitmap/newgeneration,
0/65535 and1024entries, observer whilecontrollerholdslease, paused/running/faulted,
foreignsession/unattached/termination, readonly counters/stopRevision/suppression,
exclusive save/temp/restore, malformed fields and integer-vs-bool/range/sort
semantic negatives. Validate actual frames with new fullschema and inherited
semantics; actual old3.0 and new3.1 clients coexist. Wrong version/capability and
old unknowncommand paths must remain unchanged. Tests use only private synthetic
owned runtimes and optionally installed headers/binary overrides with reports.

Run relevant legacy2.1/control3.0/breakpoint/UART-outcome/trace regression, native
sanitizer and exact oldSDK checksum/export checks. New helper verifies inventory,
tamper/unknown file/paths, deterministic export and copied standalone artifact.
Fresh exact-source review must approve contract AND implementation before freeze.
Master exports exact reviewed source and new wire artifact twice, builds/installs
Git-less, tests installed enumeration and oldprofile, records verification and
hands off exactsource/review/archive/manifest/caps. Accepted028/V0 unchanged.
