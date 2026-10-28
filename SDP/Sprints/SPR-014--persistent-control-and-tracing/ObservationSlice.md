# SLC-026 — Canonical CPU production and operational outcomes

Status: active. Sprint SPR-014; Iteration ITR-026.
Authority: explicit continued producer Master execution, reconfirmed by the
coordinator after successful composed verification of the corrected runtime.
Baseline: 2b8603a5e0dbf62c27777c999836bf7d62ce6e27 (product d247e7f).
Requirements REQ-028 and the operational-outcome portion of REQ-029;
DES-120..122; frozen SDK0.3 observation/trace/control contracts are normative.

## Delivery boundary

Implement real CPU observation binding/delivery, instruction lifecycle including
peripheral tails, captured instruction bytes and shared decode, typed control
and IRQ facts, generic access attempts/results and UART frame lifetimes. Only
complete verified coverage classes may be advertised. A SAB80535-only binding
is permitted initially; unsupported variants return UNSUPPORTED without mutation.
Partial access instrumentation must be reported honestly and cannot advertise
COVER_ACCESS until implicit register/stack/peripheral accesses are covered.
Never invent read/write history from state diffs or device rereads. Preserve
legacy architectural behavior, callback ordering and disabled-observer costs.

Integrate the existing semantic event/watch/gate/router engine as the canonical
sequence owner. No second matcher. Carry uint64 generation and full canonical
metadata without conflating input/UART kinds. Controller owns parent stimulus
IDs, global CPU queue orders and board/CPU-stage admission/delivery outcomes.
Retain kinds14..27 in the fixed1024 operational ring independently of user trace.
Implement native outcome_page and control3 outcomePage, stable epoch/cursor/loss
semantics and conditional capabilities. Unimplemented trace/config/capture/file
operations stay UNSUPPORTED and must not be advertised. Future027/028 consume
these producers rather than installing another event path.

Preserve real safe-boundary source/stop identity; instruction retirement follows
all successful cycles, never the old pre-tail counter. Failed accesses preserve
failure and unknown values; an unsupported MOVX cannot retire. CALL/RET and IRQ/
RETI use bounded typed context identity and actual observed SP/return facts.
No filesystem I/O in CPU callbacks. Reset cancels queued inputs and terminates
active/held frames before generation change; identity counters never recycle.

## Required delivery policy clarification

Observation deliver preserves API1 apply_input semantics. REN-clear RX start
returns STATE; busy returns BUSY. Both allocate rejected frame identity and
emit rx.drop/rx.end, without a successful rx.start/accept. The existing active
frame survives a busy rejection. Controller emits CPU-stage input.failed with
original status/reason8 and applies existing required-due-input session fault
policy, without retry. This is not a fabricated architectural CPU fault or
instruction.abort on the preflight rejection. Late RI/SM2 drop after successful
start is an ordinary hardware outcome and does not retroactively fail delivery.
No policy change treating REN/busy as successful delivery is authorized here.

## Work ownership and compatibility

Fresh SDK Worker first snapshots accepted shared payload bytes and adapts modern
export/verification to reconstruct immutable0.2 and0.3 independently. Verify
exact old archives before releasing the raw-header mutation gate. Frozen native
contract headers/manifests remain unchanged. Raw same-build hooks may evolve;
CPU API1 and board ABI1 layouts stay unchanged. Source/install identity remains
the new runtime revision rather than a frozen SDK source revision.

Fresh CPU Worker owns core.c, opcodes.c, disasm.c, emu_cpu_facade.c, emu8051.h,
new private CPU observation seams and CPU behavioral tests. Fresh Controller
Worker owns emu_session.c/internal header, semantic debug runtime/event modules,
new private controller/outcome implementation, control3 projection and tests.
Coordinate Makefile/test registrations explicitly. SDK tooling ownership ends
before CPU/controller work begins. Workers must agree a small private delivery
seam; frozen observation API remains the consumer boundary. Master records
integration/evidence; separate fresh Reviewer assesses combined implementation.

## Acceptance evidence

Focused adversarial tests: begin/retire/abort across delays, captured bytes,
failed bus callbacks, calls/returns/IRQ, UART REN/busy/RI/SM2/replacement/timing,
board admission vs CPU delivery and parent-only failure, reset cancellations,
outcomes with tracing disabled, ring overwrite and all cursor states/capacity,
uint64 generation, unsupported variants and disabled-observation equivalence.
Run existing core, native control/session, Unix, legacy debug and SDK contracts.
Fresh review must identify unimplemented classes explicitly. Export reviewed
source reproducibly, build/install without Git, verify actual installed wire
capabilities/outcomePage and hand off exact source/archive identity. Preserve
V0 and corrected029 artifacts. No physical IO, board protocol, TCP/DAP, RPM
publication or SDP migration. Continue027 then028 after acceptance.
