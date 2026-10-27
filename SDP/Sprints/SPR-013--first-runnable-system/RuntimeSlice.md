# ITR-022 / SLC-022 — Session, module loader and local host

Status: active
Authority: direct user first-runnable-system request, REQ-026, DES-115..118
Baseline: accepted CPU0a77c2d and immutable SDK0.2.0

Read AGENTS/current index, Sprint/Handoff/LocalHostPlan, lifecycle documents and
all interfaces/0.2.0 contracts/schemas before implementation. Exactly one Slice
is active. Two fresh Workers implement disjoint work areas against this same
contract; a separate fresh Reviewer inspects their integrated candidate.

## Work area A — runtime Worker

Own session/scheduler and native debug API implementation, new private shared
JSON/SHA/CODE-breakpoint helpers, bounded legacy refactoring needed to share them,
root Makefile/tests Makefile changes and session/API tests. Implement public
session create/destroy/pump and debug open/request/close exactly against API1 and
wire2.1. Use CPU facade only, no private CPU layout in controller. Own callbacks,
image/reset, mixed bounded queue, coherent stops, leases and identity/capabilities.

Mandatory hello/attach/status/control/CODE breakpoints/decode must work. Support
optional side-effect-free memoryRead, boardInput and boardInspect/boardInspectFault
when the board supports them; no traceRead, rich watches or fake capabilities.
Share/extract the existing CODE bitmap primitive and authoritative semantics with
legacy debugger rather than duplicate matching; advanced Gate-B work remains
future. Extract reusable private JSON/SHA helpers instead of divergent parsers/
hash implementations. Coordinate helper header/function signatures early with
host Worker. Existing protocol1.0 remains regression-tested and unchanged.

Implement native contract validation, no partial mutation on invalid requests,
actual partial faults and typed causes, after-fault inspection/reset, zero-copy
borrowing only for stated duration, no wrap/reuse, and bounded output handling.
No listener, process/thread scheduling or module loading in work area A.

Root Makefile owns runtime static library and includes host-owned runtime/targets.mk
once available. Agree artifact names libemusa80535_runtime.a and emu-runtime.
Link shared helpers exactly once; host consumes native public API and agreed
private utility headers. Master/Reviewer resolve any ownership conflict.

## Work area B — host Worker

Own runtime/ local CLI/loader source, script schema/docs/examples, runtime/targets.mk,
synthetic shared module fixtures and host/process tests (keep its test script
independent of root tests/Makefile until area A adds the invocation). Read
LocalHostPlan.md; lock exact script1 input/output envelopes there through product
docs before external driver handoff. No changes to area A source, root build or
frozen SDK files. Communicate required private JSON/SHA helper API to area A.

Load absolute module paths using ABI1, validate actual artifact hashes, descriptor
and mandatory/capability callback consistency, and keep library resident until
session destruction. Never execute CPU on malformed/invalid startup. Use one
process containing CPU and module; no physical I/O, sockets or external-repo edits.
CLI operations call the same session/debug semantics. Run wraps resume/pump/pause
with documented safe drain; state/inspection remain available after typed fault;
reset is explicit. Exact implementation/source/bundle/module/image/config identity
is emitted. Document unimplemented signals/network/DAP honestly.

## Shared acceptance

- Synthetic static and separately built .so boards, including an immutable0.1
  SDK consumer, exercise exactly-once MOVX, timed input, mixed future timestamps,
  port events, pause/step/reset, IDLE wake, faults and post-fault pure inspection.
- Complete native validation/prefix/version/output-size, module rejection/unwind,
  reentrancy/callback failure, queue overflow, stale generation, roles/leases,
  malformed JSON, uint64 precision, unknown commands, reconnect/no-retry and
  optional capability rejection tests. Enforce genuine wire2.1 schema outputs.
- Host validates all script syntax/static ranges before execution, emits bounded
  typed output/exit status, and preserves diagnostics after unknown board access.
  Repeat identical seed/image/config/commands gives equal architectural evidence;
  session identity and monotonic lifetime IDs are compared appropriately.
- Legacy CPU/trace/debug/protocol gates; installed and clean-source builds; external
  C/C++ linkage; sanitizers on meaningful runtime tests; deterministic source/export
  identity. No RPM or network publication. Do not edit immutable0.1/0.2 payloads;
  report a concrete required version correction before dependent implementation.
- Workers commit only their own files, coordinate shared helper/build boundaries
  and report evidence. Do not stage another Worker's changes. Master owns SDP
  index/Sprint/handoff/relations/ledger; Reviewer owns REV-SLC-022. Full external
  Ponsse/ROM verification follows a concrete reviewed runtime artifact handoff.

## Implementation clarification — post-fault inspection promise

ABI1 has no native capability bit representing the explicit post-fault promise
required by wire2.1. Preserve the frozen layout. Public session creation defaults
to no `boardInspectFault`. The same-source local host may use a private runtime
policy helper immediately after creation, before any client or execution, to
opt in when INSPECT exists. The host exposes `--board-inspect-fault`, records the
policy in its identity, and requires the ordinary pinned module hash. Selecting
this option asserts the integration's reviewed module promise; merely exporting
INSPECT is insufficient. This helper is an internal host policy, not a new public
SDK ABI. The external accepted board has supplied that promise; its launcher can
select the option. Tests cover default rejection, valid opt-in and late/invalid
policy changes. No immutable bundle is modified.

## Implementation clarification — signal callback scheduling

ABI1 excludes zero-delay feedback from synchronous board callbacks. Apply the
strictly-future scheduling floor to the supplied event cycle for signal_output
as well as boundary/access/port/stimulus callbacks. UART_FRAME can report
completion at t+1 while the CPU cycle(t) call is still returning; its earliest
reaction is t+2, not t+1. Track the active callback's timestamp independently
from the last completed session cycle. Reject equal/past admission before queue
or identity mutation. This resolves the unspecified signal-specific detail
consistently with the existing feedback rule; no frozen table/layout is changed.
