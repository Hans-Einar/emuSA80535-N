# REV-SLC-025 — Persistent local control implementation

Date: 2026-10-28
Reviewer: fresh independent `persistent_control_reviewer`
Disposition: **APPROVED** for bounded SLC-025 implementation
Reviewed combined candidate: `45b92abda4c88f56e69fff39441503d13ab64a59`
Activation baseline: `a66b0ef`
Host implementation: `67f2980`
Frozen IFC-003 manifest SHA-256:
`ed3917b0d3a7300aca94171f1259eacf9861b0ee87376afa534690618da27ff0`

## Scope and conclusion

Read AGENTS, CurrentIndex, the active ControlSlice/Sprint/handoff, applicable
mandate, runtime study, requirements, architecture and design, frozen0.3
compatibility/control/conformance documents, schema/semantic validator and
baseline0.2 session/wire semantics. Independently inspected the evolving diff,
then reviewed and tested the committed combined candidate above. This Reviewer
made no product-code changes.

Control3 shares the existing sole session, CPU, scheduler, breakpoint primitive,
decoder and strict JSON utilities. Cross-profile controller exclusivity and
profile checks prevent bypass through wire2.1. Lease allocation, release,
generation/identity checks, connection roles, capability precedence and bounded
requests preserve the owner boundary. Native sized records retain caller
prefixes/buffers and trailing bytes. No new observation/trace producer, matcher,
board-specific behavior, physical I/O, TCP or DAP implementation is introduced.

External resume changes state without progress; only authorized step/advance
moves time. Advance reports actual cycle/legacy-instruction/retirement deltas,
including faults. Retirement is counted only after successful CPU timing-tail
completion, before a subsequent board boundary; failed CPU peripheral/bus work
does not retire an instruction. Power-down takes priority over CODE, step and
budget stops. Stop detail reports available applied-stop facts without invented
canonical event IDs or observation coverage.

The explicit Unix frontend pins a no-follow directory traversal, validates
ownership/mode, refuses existing endpoints, checks kernel peer UID before input,
and removes only its created socket inode. Its nonblocking owner loop bounds
clients, frames and output, preserves absolute timeout deadlines, and stays
responsive while paused, faulted or serving a slow peer. Disconnect preserves
the session and releases only that connection's lease. No listener opens by
default. Host-driven pumping and externally-driven operation remain distinct.

No blocking findings remain for this Slice. Approval does not establish the
deferred observation/UART-outcome/capture/file capabilities, full remote/DAP
support, composed external integration or release publication. Master retains
exact-source artifact/install verification, traceability and acceptance duties.

## Findings and dispositions

| Finding | Evidence and final disposition |
|---|---|
| REV-SLC-025-F001 | Initial generated validation omitted compact board-payload limits and target cross-field config/capture constraints before capability/authorization. A bounded semantic validator now rejects these malformed requests with INVALID and closes, including stale-identity/observer cases. Native and independent process negatives passed. Resolved. |
| REV-SLC-025-F002 | Any initial response initially cleared the hello deadline, including UNSUPPORTED negotiation. Host now queries actual negotiation state and retains the original deadline. Repeated failed hello and trickled incomplete frames still expire after5000ms while another observer remains responsive. Resolved. |
| REV-SLC-025-F003 | Shared native connection history has an8192-distinct-ID lifetime bound, separate from8 concurrent clients. The retained baseline resource restriction is explicitly documented in runtime README and Sprint handoff; CAPACITY occurs before mutation, reused IDs remain STALE, and arbitrary unused lower IDs remain permitted within capacity. No new monotonic-ID public requirement or unbounded retention was introduced. Assessed implementation limitation, not a frozen-contract blocker; external intake must retain this limitation. |
| REV-SLC-025-F004 | Independent live testing found valid frozen captureStart rejected because generated primitive checks used a string-only helper; quoted boolean spellings could pass. Dedicated JSON_PRIMITIVE comparison now handles booleans/numeric enums, with corrected semantic boolean checks. All unavailable command examples, quoted-primitive negatives and independent adversarial tests pass. Resolved. |
| REV-SLC-025-F005 | A draft special case emitted an unsafe terminated snapshot after fault, contrary to inherited baseline safe-boundary semantics. Both profiles retain the established close-only termination path when a truthful conforming final snapshot is unavailable. Native regression confirms termination and client release. Resolved. |
| REV-SLC-025-F006 | Worker inspection identified shared scheduler CODE/step/budget checks preceding power-down at a completed boundary. The documented stop priority is now enforced before those checks. Regression proves successor CODE breakpoint cannot hide power-down, with actual two-cycle/one-retirement progress after safe drain. Resolved. |

## Independent verification on the candidate

- `make control3-test runtime-session-test runtime-control-test
  runtime-host-test cpu-link-test` passed. This includes141 actual wire3
  responses through the frozen strict validator,1116 wire2.1 responses, an
  actual linked C++ table consumer, C99/C++17 compatibility callers, immutable
  old-header board loading, real Unix process controls and the script1 suite.
- Native coverage includes cross-profile leases in both directions, wrong-profile
  rejection, stale generation, lease handover,8192 lifetime-ID capacity, prefix
  size/version/flags/reserved fields, unchanged trailing output storage and
  pre-effect response capacity. Delayed MUL, post-completion board failure,
  MOVX failure and actual UART peripheral callback failure establish distinct
  retirement/progress behavior. Power-down priority and unsafe termination pass.
- Independently compiled the native test with AddressSanitizer and
  UndefinedBehaviorSanitizer, frame pointers and leak detection; execution passed
  with `halt_on_error=1` and no sanitizer diagnostics.
- A separate temporary real-process adversarial harness passed59 cases plus
  reconnect/lease growth, delayed-step progress and stable stopDetail checks.
  It tested unknown arguments for all30 recognized commands; all12 unavailable
  operations with stale identity/generation/lease; invalid capture relations,
  ring minimum/duplicate IDs, oversized payload, noncanonical/overflow U64;
  escaped duplicate keys, invalid UTF-8 and replayed request IDs. Malformed
  trustworthy requests closed after INVALID; valid unavailable operations
  returned UNSUPPORTED before identity/role checks.
- The Unix suite passed exact65536-byte fragmented framing, client bounds,
  partial frames, unread output/backpressure, repeated failed negotiation,
  absolute timeout/trickle behavior, disconnect persistence, exclusive leases,
  host/external modes, fault recovery, private directory/symlink/preexisting-path
  refusal and replacement-inode preservation at shutdown. Same-UID admission
  was also inspected directly at the SO_PEERCRED check before service open/read.
- `make interface-target-check interface-check debug-test` passed, including
  exact immutable SDK0.2 archive reconstruction,64 IFC-003 payload checks,
  standalone export integrity and legacy debug facade/process regressions.
  The frozen checker still describes its target artifact as unimplemented;
  runtime availability is independently established by live capability tests.
- `git diff --exit-code a66b0ef HEAD -- interfaces/0.1.0 interfaces/0.2.0
  interfaces/0.3.0 lib/include` and the candidate whitespace check passed.
  Frozen bundles, public headers and their implementation markers are unchanged.
  The working tree was clean before adding this review record.

Review reproduction logs were produced as
`/tmp/slc025-review-focused.log` and `/tmp/slc025-review-compat.log`; the separate
adversarial harness is `/tmp/slc025-review-adversarial.py`. These temporary paths
are supplementary evidence, not delivery artifacts or required consumer tools.
