# Emulator debug-control API requirements

**Contract:** `emu-debug` protocol `1.0` frozen and verified baseline
**Consumers:** `emuSA80535-DAP` adapter and `emuSA80535-N` headless runtime
**State:** Implemented and independently verified on current emulator default
`d9f80eba172dd9d7281aaa9e5cfef461b6b9709b` (runtime merge `1a6aa397…`); no
contract change was required

## Authority and factual baseline

The current accepted `emuSA80535-N` default is `master` at
[`d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`](https://github.com/Hans-Einar/emuSA80535-N/tree/d9f80eba172dd9d7281aaa9e5cfef461b6b9709b).
Accepted Issue #6 / PR #9 runtime merge `1a6aa397…` is an ancestor; the delta is
three documentation-only P1000 boundary files, with all product/build/test blobs
identical. Current master implements the complete frozen headless contract and
passes independent DAP real-runtime acceptance on Windows and Linux. The earlier
`a20815e` baseline below is retained as historical pre-implementation evidence.
[PR #1](https://github.com/Hans-Einar/emuSA80535-N/pull/1) head
[`62f40127e1aa3b24e9d8d54c2458e847bfe86488`](https://github.com/Hans-Einar/emuSA80535-N/tree/62f40127e1aa3b24e9d8d54c2458e847bfe86488)
merged as Stage 0
[`0cf6792b794070bcbbb1bfdddc30eb9cdc4c3723`](https://github.com/Hans-Einar/emuSA80535-N/commit/0cf6792b794070bcbbb1bfdddc30eb9cdc4c3723),
and [PR #3](https://github.com/Hans-Einar/emuSA80535-N/pull/3) merged Stage 1
as the historical 2026-08-31 refreshed HEAD `a20815e`.

At historical baseline `a20815e`, the merged core exposed deterministic
variant/reset support, exact 64-KiB
raw loading, bounded run/run-until-PC, exact instruction step, typed stop
results, one pre-execution core breakpoint, `decode()`, immutable
instruction/SFR/MOVX trace records, and Siemens IRQ state plus a record-only
request/accept/release observer. It then had no buildable no-curses headless
debug executable, NDJSON server or version handshake, stable atomic debugger
snapshot/accessors, `decodeCode` wire contract, atomic replacement breakpoint
table, child scheduler/pause integration, or cross-platform process lifecycle
tests.

At the original 2026-08-31 study/review cut, `master` was `5dc6812` and PR #1
head `62f4012` was correctly recorded as candidate/unmerged. That observation is
dated historical evidence. No adapter may directly consume `struct em8051`
fields across the process boundary merely because the underlying core seams are
now merged.

## Boundary and framing

The adapter launches a separate emulator executable in explicit headless-debug
mode, without a shell. The child's stdin/stdout carries UTF-8 NDJSON: one JSON
object per line. stdout is protocol-only; structured human diagnostics go to
stderr. This stream is not DAP and never carries DAP `Content-Length` frames.

Each request has a positive session-unique integer `id`, `type: "request"`,
`command`, and optional `arguments`. Each response echoes `id` and `command`,
has `success`, and carries either `body` or a structured `error`. Server events
use `type: "event"`. Unknown fields are ignored within protocol major 1;
unknown required commands/capabilities fail explicitly.

The initial maximum line size, timeouts, execution-chunk maximum,
disassembly-count maximum, and breakpoint maximum are returned by `hello`.
Implementations must bound all allocation and work derived from input.

## Version and capability handshake

The first command is:

```json
{"type":"request","id":1,"command":"hello","arguments":{"protocol":{"major":1,"minor":0},"requiredCapabilities":["rawCode64k","deterministicReset","snapshotBasicRegisters","decodeCode","replaceCodeBreakpoints","boundedRun","stepInstruction"]}}
```

A success returns protocol major/minor, emulator product/version/commit, CPU
variants, named capabilities, and numeric limits. Major mismatch is fatal. A
minor mismatch is acceptable only when all named required capabilities are
present and message semantics are compatible. Product version/commit is
diagnostic and must not substitute for the protocol version.

The Slice-1 required capability names above are frozen. Raw CODE reading is not
required: `decodeCode` is the only Slice-1 disassembly consumer, while DAP
`readMemory` remains near-term. Adding optional capabilities is backward
compatible; renaming/removing a required capability or changing its semantics
requires a protocol-major change or an explicit compatibility design.

## Minimum needed for Slice 1

### Lifecycle and image

`load` accepts an absolute image path, format `raw-code-64k`, and expected
SHA-256. The server reads exactly 65,536 bytes, rejects shorter/longer content,
and returns the actual digest. It populates CODE only and does not infer P1000
or other firmware semantics.

`reset` accepts an unsigned 32-bit seed and 16-bit entry address. It performs a
deterministic cold reset, sets the configured entry state under an emulator-
owned rule, and returns stopped at an instruction boundary with reason `entry`.
Repeating the same accepted build, image, seed, and command sequence must
produce the same architectural state and stop sequence.

`terminate` stops execution, closes emulator-owned resources, returns an
acknowledgment when possible, and exits. EOF from the adapter also triggers
bounded cleanup. Headless mode must not initialize curses or physical host I/O.

### State and atomic snapshot

The child does not mirror the adapter's logical DAP state. Its independent
lifecycle/command state is `starting`, `idle`, `run-active`, `command-active`,
`terminating`, or `terminated`. Because commands are serialized, a successful
synchronous `run` response changes `run-active` back to `idle` at an
instruction boundary. The child is not executing between requests, even while
the adapter remains logically running and intends to send another chunk.

`getState`, `reset`, `stepInstruction`, every architectural stop result, and
every yield return one atomic instruction-boundary snapshot:

```json
{
  "state":"idle",
  "resultKind":"architectural-stop",
  "reason":"entry",
  "pc":0,
  "registers":{
    "a":0,"b":0,"psw":0,"sp":7,"dptr":0,
    "r":[0,0,0,0,0,0,0,0]
  },
  "variant":"sab80535",
  "instructionCount":0,
  "machineCycleCount":0
}
```

All numeric register values have fixed architectural widths. R0–R7 correspond
to the bank selected by the snapshot PSW. PC and registers must come from the
same point in execution. Private pointers, padding, callbacks, and struct layout
are never serialized.

When processed at `idle`, `getState` returns the latest still-valid boundary and
labels its provenance as `architectural-stop` or `yield`; it never reports a
persistent child `running` state. While a command is active no second request is
processed. Load/reset or another executing command invalidates the older
boundary as described under continue/pause.

Child architectural stop reasons are `entry`, `breakpoint`, `step`,
`exception`, and `halt`; exceptions additionally carry a stable code and
message. `pause` is not a child stop reason in Slice 1: it is synthesized by the
adapter when it promotes a yielded boundary after a DAP pause request. The
internal reason `breakpoint` is likewise not the DAP wire string; the adapter
maps it to DAP `instruction breakpoint`. A request invalid for the current
child command/lifecycle state fails and leaves state unchanged.

### Disassembly support

The required `decodeCode` command accepts a uint16 reference CODE address,
signed byte offset, signed instruction offset, and positive instruction count
within a negotiated limit. It returns exactly that many ordered records. A
valid record contains uint16 address, positive byte size, `valid: true`, and
exact emulator decoder text. An invalid predecessor placeholder contains the
selected byte address, `size: 1`, `valid: false`, stable reason
`unknown-predecessor`, and display text `<invalid>`.

The byte offset is applied first without uint16 wrapping. Non-negative
instruction offsets decode forward. For a negative instruction offset, the
server traverses only a contiguous predecessor chain whose boundaries are
known from an architectural boundary plus forward decode results or from an
observed completed sequential instruction. It must not scan raw bytes and guess
which variable-length predecessor was intended. At the first unknown
predecessor, and for further unknown slots, it moves back exactly one byte and
emits the invalid one-byte placeholder. Known records remain valid and
authoritative decoder output; placeholders are explicitly non-authoritative.

The exact returned window must fit `0x0000`–`0xFFFF`. Otherwise the command
returns a structured range error rather than wrapping or returning a partial
array. The adapter must not duplicate private opcode tables. The current C
`decode()` function is feasibility evidence but is not a cross-process
contract. A raw CODE-read operation is not in the Slice-1 command or capability
set because it has no Slice-1 consumer; debugger `readMemory` remains a
near-term requirement.

### CODE breakpoints

`replaceCodeBreakpoints` accepts the complete desired set of unique uint16 CODE
addresses. It atomically replaces all prior debugger breakpoints and returns
accepted and rejected addresses with stable reasons. Empty input clears the
set. `hello.limits.maxBreakpoints` is at least one. Breakpoints stop before the
instruction at that PC executes and return reason `breakpoint` and the same PC.
`breakpoint` is the emulator protocol's internal reason; the DAP adapter must
emit the distinct standard reason `instruction breakpoint`.

This replacement contract matches DAP `setInstructionBreakpoints`, which
replaces the global set. It must not share a TUI global or require adapter
access to emulator storage.

### Continue and pause

`run` executes no more than its requested/negotiated instruction chunk and
returns either:

- `yield` with progress counters and an atomic instruction-boundary snapshot
  when the chunk ends without an architectural stop; or
- one architectural-stop snapshot for breakpoint, exception, or halt.

The adapter schedules repeated chunks for DAP continue and keeps its DAP state
running across `yield` boundaries, although the child returns to `idle` at each
boundary and executes nothing until the next request. Before sending another
chunk, the adapter checks pause and termination intent. A DAP pause is an
adapter-local intent: the adapter acknowledges it before the eventual stopped
event, awaits an active chunk or uses the most recent still-valid yield, sends
no next chunk, and promotes that boundary snapshot to a DAP pause stop. The
child protocol has no Slice-1 `pause` command and requires no concurrent request
multiplexing. The negotiated maximum chunk supplies the deterministic service
bound. Physical elapsed time is not part of the execution result.

A yield snapshot is valid only while the child is idle with the same loaded
image. Sending the next run, load/reset, timeout, malformed response, EOF,
disconnect, or exit invalidates it. Disconnect sets termination intent and
schedules no next chunk; any snapshot returned during cleanup is not exposed as
a DAP stop. A run timeout proves no safe boundary, so the adapter kills/reaps
the child and terminates instead of assuming a stopped state.

### Single instruction

`stepInstruction` is valid while the child is `idle` at a boundary; the adapter
sends it only while the logical DAP session is stopped. It completes exactly
one architectural instruction, including required machine cycles, and returns
at the next instruction boundary with reason `step`, unless a higher-priority
exception/halt outcome is explicitly returned. This is the server primitive
for DAP `stepIn` in Slice 1. It does not imply call-aware `next` or `stepOut`.

### Errors and termination

Every failure returns:

```json
{"code":"STABLE_MACHINE_CODE","message":"actionable text","retryable":false,"data":{}}
```

Minimum families cover invalid request/schema, invalid state, range, image
size/hash, unsupported variant/capability, breakpoint limit, execution
exception, and internal error. Malformed input produces a bounded error or
clean termination; it must not be printed as successful output.

Unexpected child termination and EOF are observable to the adapter. A clean
adapter `terminate`/disconnect must not orphan the emulator.

## Cross-repository blocker disposition

The IDs remain stable. Independent DAP verification against accepted emulator
default `d9f80eba…` reproduced every prerequisite through the unchanged frozen
protocol:

| Blocker | Status at current `d9f80eba…` | Accepted evidence |
|---|---|---|
| `EMU-BLK-001` | **SATISFIED** | Portable documented `emu-debug` executable builds without curses or physical host I/O. |
| `EMU-BLK-002` | **SATISFIED** | Bounded UTF-8 NDJSON server enforces schema, correlation, canonical keys, stdout isolation, and structured errors. |
| `EMU-BLK-003` | **SATISFIED** | Protocol 1.0 `hello` returns the exact required capabilities and numeric limits. |
| `EMU-BLK-004` | **SATISFIED / preserved** | Deterministic SAB80535 variant, exact 64-KiB load/hash, reset seed, entry stop, and replay pass real integration. |
| `EMU-BLK-005` | **SATISFIED** | Stable atomic public PC/basic-register/bank-selected R0–R7 snapshot passes facade and DAP tests. |
| `EMU-BLK-006` | **SATISFIED** | Exact-count `decodeCode`, transactional predecessor knowledge, placeholders, range, UTF-8, and no-wrap behavior pass. |
| `EMU-BLK-007` | **SATISFIED** | Atomic replacement/clear/limit reporting and pre-execution CODE stops pass. |
| `EMU-BLK-008` | **SATISFIED** | Bounded synchronous run/yield, idle boundary snapshots, repeated scheduling, and adapter-local pause pass. |
| `EMU-BLK-009` | **SATISFIED** | Exact one-instruction wire step and stable stop/error mapping pass. |
| `EMU-BLK-010` | **SATISFIED** | Windows/Linux terminate, EOF, malformed/oversize, crash, pipe cleanup, and no-orphan process tests pass. |

Stage-1 IRQ state and request/accept/release observation are also merged core
seams, but they do not reduce the Slice-1 blocker set because IRQ frames/state
remain near-term and are not required by the candidate first slice.

## Needed for later slices

- side-effect-free CODE, IRAM, SFR, and XDATA read operations and variant
  ranges for DAP `readMemory` and richer scopes;
- call/return/IRQ-entry/RETI event stream with call site, target/vector,
  return PC, priority, nesting, and monotonic sequence;
- interrupt state snapshot and exception detail;
- data/SFR/XDATA access events and configurable watchpoints;
- safe, explicitly enabled memory/register writes with ranges and invalidation;
- richer trace subscription with backpressure;
- attach to an already-running emulator over an authenticated/restricted local
  socket and independent lifecycle;
- performance counters beyond deterministic instruction/machine-cycle counts.

These later capabilities must be negotiated and are not accidental Slice-1
dependencies.

## Logical stack event requirements for later work

Events must distinguish `acall`, `lcall`, `ret`, `irqEnter`, `reti`, and reset.
Each event carries sequence, instruction address, resulting PC, architectural
return PC where applicable, and IRQ vector/priority when applicable. Nested
interrupt order must be explicit. If event loss or an unexpected RET/RETI is
detected, the adapter can mark its logical model degraded; the server must not
claim that arbitrary hardware-stack bytes are language frames.

## Safety and firmware neutrality

Headless debug mode cannot open serial, GPIO, CAN, field-bus, or machine-control
endpoints. Device models operate only inside emulator state. The protocol has
no P1000 commands, types, addresses, names, signal meanings, or hydraulic
semantics. Firmware-specific symbols enter only through the adapter's generic
symbol-map input.

## Contract verification expected in the emulator repository

- schema/unknown-field/malformed/oversize record tests;
- compatible and incompatible hello tests;
- exact-size image and hash tests;
- deterministic replay with fixed seed;
- atomic snapshot/register-bank tests;
- forward, known-predecessor, unknown-placeholder, range, and exact-count
  `decodeCode` tests;
- breakpoint replacement/clear/limit and pre-execution stop tests;
- bounded run/pause and exact-step tests;
- exception/crash/EOF/terminate cleanup tests;
- Linux and Windows stdio process tests proving stdout contains NDJSON only;
- a safety test proving headless debug performs no physical host I/O.

Current emulator commit `d9f80eba172dd9d7281aaa9e5cfef461b6b9709b`
and runtime implementation merge `1a6aa397993d3f24cef8d41248ae2928d352966a`
are captured in DAP `VER-001-002-003`. This file remains the frozen
cross-repository requirements contract and now has independent proof of Slice-1
availability.
