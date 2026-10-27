# External debugger and DAP contract

Roles: IDE→DAP adapter uses standard DAP; adapter→runtime uses wire2.0. A CLI can
use wire2.0 directly. The adapter may include the same client library as a CLI;
a separate middle process is not required. The runtime never sends DAP messages.
Existing DAP launch using emu-debug stdio1.0 remains a separate supported mode.

DAP dependency baseline is @vscode/debugprotocol1.68.0, pinned by the sibling
adapter lockfile. DAP frames use ASCII `Content-Length: N\r\n\r\n` followed
by exactly N UTF-8 payload bytes. Count bytes, not characters. This is different
from the LF-framed runtime protocol. Standard message correlation uses seq and
request_seq; runtime IDs are independently allocated. See the official
[DAP overview](https://microsoft.github.io/debug-adapter-protocol/overview.html).

## Attach profile

Adapter-specific attach arguments: endpoint (unix absolute path, or host/port
with configured mTLS profile), expected sessionId optional, role controller
or observer, contractVersion `0.1.0`, runtimeProtocol `{major:2,minor:0}`.
Credentials/certificate paths are adapter configuration, never forwarded as
runtime JSON. Do not accept a firmware program path as an attach instruction.

Initialize DAP, establish protected runtime transport, hello, attach; return DAP
attach success only after matching session identity and capability negotiation.
Emit initialized when configuration requests can be accepted. Honor
configurationDone before adapter-initiated execution. Attach itself never pauses,
loads or resets. For an initially running session, setting breakpoints requires
an explicit pause/resume policy communicated to the client; do not silently
mutate running state through an operation defined as paused-only.

| DAP operation | Runtime mapping |
|---|---|
| threads | One synthetic CPU thread id1; never a second CPU instance. |
| pause | pause; successful response precedes one stopped(reason pause) derived from applied snapshot. |
| continue | resume; continued is emitted after applied acknowledgment. Poll status for later stops. |
| stepIn | step with negotiated maxCycles; map step stop or actual exception/breakpoint; IDLE cycle-budget becomes stopped(reason pause, description cycle budget). |
| setInstructionBreakpoints | replaceCodeBreakpoints after validating hexadecimal references and offsets. Whole replacement errors return unverified entries with explanation. |
| disassemble | decodeCode; preserve authoritative text and invalid predecessor placeholders. |
| readMemory | Only advertise supportsReadMemoryRequest with memoryRead; return base64 of returned hex bytes, exact range. No physical device read. |
| scopes/variables | Basic registers from one snapshot. Handles bound to session,generation,stopRevision and invalid after resume/reset/detach. |
| stackTrace | At most a synthetic current-PC frame unless verified call/IRQ stream available; never infer source-level call stack from arbitrary stack bytes. |
| restart | Only advertise if allowed; pause if running, then reset with configured seed/entry, report new generation. |
| terminate | Only advertise when host grants termination; explicit terminate command. |

CODE stop maps to DAP `instruction breakpoint`, watch to `data breakpoint` only
when supported, exceptions to `exception`, entry to `entry`, step to `step`.
Do not advertise data breakpoints, source breakpoints, next/stepOut, reverse
execution or rich trace configuration before their full contracts/producers
are implemented. Optional tracePage is a bounded emulator extension, not a
replacement matching engine or unsolicited raw trace stream.

## Disconnect and loss

For attached sessions, absent/false terminateDebuggee does not terminate host.
If terminateDebuggee=true, require granted privilege and applied termination;
otherwise fail explicitly. With no termination request, DAP's default
suspendDebuggee absent/false requires resume before detach when paused.
With negotiated supportSuspendDebuggee and true, apply pause before detach.
Wait for applied transition acknowledgment, then detach/release the controller.
Observers cannot promise a state-changing disconnect policy: only complete it
when current state already matches, otherwise report denied/unsupported.
Advertise supportSuspendDebuggee only when this policy is implemented.

Unexpected EOF/timeout loses client certainty, preserves host state and must not
implicitly terminate/reset/resume. Reconnect checks session/generation, discards
old variable/frame handles, queries actual state and does not replay mutations.
A lost response may mean a command applied; retry requires reconciliation.
Legacy launch1.0 owns its child and keeps its existing kill/reap cleanup policy.
