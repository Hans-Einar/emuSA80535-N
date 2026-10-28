# ITR-025 / SLC-025 — Persistent local control implementation

Status: active
Authority: shared user's explicit instruction relayed by the coordinating
external Master on2026-10-28 to execute SLC-025..028 and composed integration
without repeated approval. Baseline b5f9bd3; frozen contract IFC-003 at2133e722.
Only SLC-025 is active. Fresh Workers implement; independent fresh Reviewer
checks exact candidate and evidence; Master accepts and hands off artifacts.

## Deliverable

Implement the frozen control3 table and wire3.0 persistent Unix frontend over the
existing sole session. Preserve CPU API1, board ABI1, SDK0.1/0.2/0.3 archives and
versioned directories, script1, native wire2.1 and legacy debugger1.0. Read AGENTS,
CurrentIndex, Sprint/Handoff, lifecycle01..05, interfaces/0.3.0 docs01/03 and
schemas, plus baseline debug/native semantics before editing product code.

Control3 query/open/request/close/advance must conform to the existing canonical
header without inventing a parallel CPU/session. Reuse existing debug operations
and strict parser/serializer; no duplicated breakpoint or matcher semantics.
One controller across both protocol profiles, unique monotonically allocated
lease tokens, generation checks and explicit acquire/release. No old-profile
connection may bypass a new-profile controller. Connection close releases only
its own lease; requests and authorization are serialized by the owner.
Implement hello/attach/status/control/step/advance/reset/detach/terminate,
CODE breakpoint/decode, stopDetail for available stop sources, pure memory read,
boardInput parent identity and board inspection where capabilities permit.
Unavailable observations/outcomes/trace/capture/file commands return UNSUPPORTED
and never advertise availability. Query coverage remains empty in this Slice.

Implement externally-driven and host-driven modes explicitly selected at startup.
Only advance or step moves time in external mode; resume does not secretly pump.
Advance requires running state, current controller lease/generation and bounded
1..100000 budget plus <=16 drain. Report actual progress on failure; never label
legacy instruction count as retirement. A minimal owner-side completed-instruction
tally may be added for required progress fields, based on verified completed
CPU timing boundaries; test delayed/peripheral and board-failure edge cases.
Full canonical observation production remains SLC-026. If existing facade facts
cannot prove retirement, report the concrete gap before exposing a false count.

The immutable native session config has no execution-mode field. Add a private
host integration setter, allowed only before any client/progress at startup;
new control3 sessions default externally-driven. This is same-build host plumbing,
not a new public ABI or a consumer requirement. Existing native pump remains
trusted owner API; wire advance always validates lease. Host-driven loop uses
bounded pumps between requests, keeping paused/faulted control responsive.

Unix transport: explicit --control-unix absolute path and --execution-mode,
mutually exclusive with --script. Existing image/module/config hash validation
and sealed module loading still apply. Socket path <=107 UTF-8 bytes in owned
0700 directory, socket0600, same-UID SO_PEERCRED, refuse preexisting/symlink paths,
cleanup only the created inode. Bound8 clients/32 pending requests/one outstanding
and output frame per client, framing65536, strict JSON and 5000ms incomplete/
blocked-output timeout. Never touch CPU from transport callbacks or physical I/O.
Use one owner loop; no background virtual progress in external mode. Startup
opens no listener unless requested. Idle connections do not terminate session.
No unsolicited protocol output. OS shutdown cleanup must not touch unrelated
paths or pretend to be emulated CPU input. No new user-signal status API is needed.

Current build compatibility identity remains0.2/2.1 for the existing profile.
Control3 hello carries the frozen0.3 manifest and actual implementation source;
manifest/header implementation markers describe the immutable contract artifact,
not runtime capability discovery. Runtime help/docs/version must distinguish
available frontends honestly. Do not mutate frozen files to change availability.
Any tools update affecting frozen payload requires immutable snapshot/version
handling and independent old-export hash checks; avoid unnecessary changes.

## Work boundaries and acceptance

Session Worker owns emu_session.c/internal seam, control3 validation/projection,
root Makefile library integration and native control tests. Host Worker owns
runtime/main.c, new Unix transport, runtime/targets.mk/docs and process tests.
They coordinate the private mode setter/state access and build seam explicitly.
Master owns SDP/artifact/coordination; Reviewer owns REV-SLC-025 only.

Require real native requests and Unix process tests, exact3.0 schema validation,
C99/C++ callers, old2.1/1.0/script regressions, malformed framing/version/U64,
lease handover/reconnect/stale generation, observer denial, no hidden progress,
exact bounded step/advance/fault progress, partial writes/slow peers/client limits,
path ownership/symlink/refusal/cleanup and synthetic module exercise. Sanitizers
for relevant native state/control. Build/install from exact source without Git;
export source archive with identity and capability inventory. Keep prior V0 pins.
No trace behavior, raw external byte coordinator, board-specific model, TCP/DAP,
RPM publication or SDP migration is implemented in this Slice. After acceptance,
activate the next bounded producer Slice under existing authorization and notify
the coordinator via the shared artifact directory.

## Reviewed implementation capacity clarification

The existing bounded lifetime registry remains8192 distinct native connection
IDs per session, shared across profiles, with at most8 concurrent clients.
CAPACITY is returned before mutation at exhaustion; arbitrary unused lower IDs
remain accepted within that bound. Do not add an unbounded lifetime set or a
new implicit public monotonic-ID requirement. Document the finite implementation
limit in runtime availability notes and the artifact handoff, and verify it.
Reviewer confirms frozen3.0 specifies concurrent limits without mandating all
uint64 lifetime identities be accepted. The local host assigns monotonic IDs,
but that private policy does not change the native API.
