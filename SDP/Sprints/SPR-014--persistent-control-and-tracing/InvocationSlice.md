# ITR-028 / SLC-028 — One observed dynamic invocation

Status: accepted through REV-SLC-028 / VER-SLC-028. Sprint SPR-014. Baseline Master49dedd4; accepted027 product
740f60863bc52481e589f219ba54df8f5bd0fc82. Authority: standing explicit user
execution through028 and virtual composed integration, without renewed approval.
REQ-029, DES-123; preserve partial REQ-028 coverage125. Fresh Workers implement;
separate fresh Reviewer checks exact combined source; Master accepts artifacts.
Read AGENTS, CurrentIndex, Sprint/Handoff, lifecycle01..05, TracePlan and frozen
SDK0.3 docs02/04/05/06 before implementation. Frozen headers/schemas immutable.

## Scope and ownership

Implement frozen capture B mode1 next successful observed CALL target and mode2
already observed fully known current top CALL. Reuse027 canonical trace machinery
and026 CPU context producer; no parallel matcher or second execution clock.
A fresh Context Worker owns emu_cpu_facade.c and CPU observation/context tests,
private producer seam changes agreed with Trace Worker, and no session/trace
implementation. A fresh Invocation Worker owns emu_trace_internal.c/h, session
integration/control3 capability, existing router seam as necessary, focused native
and actual Unix/file tests, Makefiles and current runtime documentation. Coordinate
shared headers/test registration before editing; do not overwrite peer changes.
Both commit only their owned changes. No broad performance/ACCESS/new transport
work, board knowledge, physical IO, frozen ABI/schema changes or publication.

## Context and scope correctness

Track typed CALL/IRQ frames regardless of user filters, enable state or IRQ
suppression. Root closure requires matching type/expected SP/return PC and terminal
successful retirement; include root RET plus retire, exclude caller continuation.
Nested calls, recursion, interrupts, RETI and faults must not confuse ownership.
Never derive identity from PC/SP alone. Bind mode1 only after CALL retires;
mode2 rejects unknown/non-top/non-CALL/unobserved entry with STATE. Each binding
allocates invocationId independently; retained status/capture IDs never reused.

Recover knowledge only at newly fully observed CALL entry above unknown ancestry:
new child may be fully known with parent0 while old unknown base remains unknown.
Returning that child cannot make unobserved ancestry known. Old scopes affected by
unknown/mismatch/unwind stay incomplete. Define same rule for newly observed IRQ
where justified, without allowing IRQ mode2 CALL binding. Frame bound64; overflow,
wrong RET/RETI/target/SP, unknown context, abort/reset/load produce explicit
incomplete reasons and never a fabricated completed return. Entry/terminal source
identity remains canonical even when filtered from output. Uncaptured operands
stay unknown, not decoded by reading future mutable state.

## Destination staging and persistence

Follow frozen04 exactly: candidate next-CALL routing stages per affected
destination, retaining original coalesced trace-ID sets and original event-time
gates/filters. Stage all chronological records between CALL and retirement,
including other/A traces sharing that destination, within256 records/786432bytes.
Commit once in source order after success; on abort remove only candidate B IDs,
commit other routes once, close B incomplete/fault with context/invocation0.
Overflow closes incomplete/overflow before binding and drains other views under
normal storage loss policy. Never re-evaluate predicates, duplicate A records,
recount A limits or allocate storage identity before final ordered commit.
Flush/close BUSY while stage unresolved; config/stop wait safe instruction boundary.
Private seams may evolve without changing public contracts or routing semantics.

Capture status/rearm/limits exactly frozen04: B limit incomplete, armed timer
starts binding, mode1 reset optionally creates successorCaptureId after old status
incomplete/reset; preserve old binding generation. Allocation failure rearmStatus
EXHAUSTED, successor0, no CPU fault or silent retry. Retain64 capture statuses.
No implicit load rearm. A/config/enabled/history remain unchanged across reset.
Terminal filters remain until new capture; removing trace closes explicitly.
Output suppression cannot prevent context completion. Storage exhaustion remains
incomplete/overflow. Preserve027 termination freeze and async file semantics.

## Acceptance and handoff

Fresh separate Reviewer verifies exact combined candidate and independent tests.
Required real CPU/native/Unix tests: nested/reentrant calls; mode2 at entry CODE
break; IRQ/RETI during scope and at CALL/RET tails; disabled/filtered/suppressed
closing return; unknown ancestry recovery; corrupted return/SP/type/unwind;
stack overflow; failed CALL retirement; partial MOVX fault; reset/load/rearm and
retention exhaustion; limits; shared A+B ring/file ordered coalescing, overflow
and abort staging; no duplicate IDs/records/limits. Validate actual responses and
JSONL against frozen schema plus semantics, not merely canned fixtures.
Run appropriate026/027/core/legacy/control/immutable SDK regression and sanitizer
checks. Export reviewed source reproducibly, build/install Git-less, exercise
actual installed B/capture/file with compiled installed-header fixture. Preserve
all accepted artifacts and pins; communicate exact review/source/archive/caps
through shared artifact and explicitly authorized existing coordinator thread.
No claim of whole-machine/DAP/physical correctness. Continue composed virtual
consumer intake after the reviewed capability handoff.

## Binding sequence clarification

Frozen04 binds mode1 only after successful CALL retirement. Its startSequence is
that retirement sequence and its cycle timer starts at that boundary; included
staged CALL records can have an earlier source sequence. This is the binding
fact, not a promise that startSequence is the first retained record. Mode2 uses
the last processed canonical source at explicit known-context binding, without
inventing a source. Staged B record views count exactly once after successful
binding and may immediately reach the record limit; A counters/gates retain
original source-time evaluation. Terminal RET retirement routes before closure.

Runtime stop policy: B stopCpu=true requests a safe stop on terminal return or
incomplete outcome as well as limits. Public budget/paused represents this scope
stop; captureStatus supplies the exact return/incomplete reason. Higher-priority
actual fault, power-down, CODE breakpoint and watch reasons retain precedence.
Reset retains the existing public entry/paused lifecycle result; captureStatus
reason3 identifies reset. With stopCpu=false, terminal
scope only closes its trace filter and ordinary execution may continue.
