# Local first-runtime host contract plan

Status: planned implementation contract for successor SLC-022; not implemented
Authority: direct user runnable-system instruction and agreed Ponsse integration
ownership. This product-level script interface is distinct from the native SDK
and future socket wire protocol; runtime release artifacts carry its docs/schema.

## Invocation

```text
emu-runtime --variant sab80535 --clock-hz 11059200 --seed 0 --entry 0 \
  --image /absolute/firmware.bin --image-sha256 HEX64 \
  --board /absolute/module.so --board-sha256 HEX64 \
  --board-config /absolute/config.json --script /absolute/commands.ndjson
```

All paths are local to the runtime host. Image is exactly65536 bytes; hashes
are required and verified before execution. Config is a bounded UTF-8 JSON
object. Explicit variant/clock/seed/entry are recorded in identity. No physical
I/O or network listener. One process contains CPU/session and dlopen board.
The host must support --help and --version without loading a module/image.

## Script1 and output

First line: {"format":"emu-runtime-script","version":1}. Remaining lines are
bounded UTF-8 JSON command objects. At most256 operations, each at most65536
bytes including LF and total input at most8MiB. Reject malformed/unknown fields,
unsupported format and invalid static ranges before executing any command.
State-dependent failures are reported at their applied point. No blank lines
or comments. File-driven commands avoid wall-clock arrival influencing replay.

Initial operations:

- {"op":"state"}: current session snapshot and identities.
- {"op":"run","cycles":N}: N1..100000, resume, pump budget, pause if still
  running; return actual cycles/instructions and final snapshot. CPU safe drain
  may add <=16cycles. Budget completion is a local outcome, not a false firmware
  halt. Preserve earlier architectural/fault stop reasons.
- {"op":"pause"}: idempotent coherent pause, no virtual-time advancement.
- {"op":"step","maxCycles":N}: paused only; one instruction or bounded
  IDLE/stop outcome under the SDK semantics, then paused.
- {"op":"reset","seed":U32,"entry":U16}: explicit coherent reset/restore.
- {"op":"boardInput","schema":"module.schema.v1","cycle":"U64 or null",
  "payload":{}}: schema is module-owned; cycle is an actual canonical decimal
  JSON string or JSON null, not the illustrative literal above. Uses owner
  admission/current+1/order policy and returns resolved cycle/order.
- {"op":"boardInspect","schema":"module.schema.v1","request":{}}:
  bounded pure inspection, including explicit post-fault capability when present.
- {"op":"replaceCodeBreakpoints","addresses":[U16,...]}: whole replacement
  through the shared debugger semantic model, paused only.

Every operation maps through the same session/control semantics used by the
native debug facade. Host convenience run wraps resume/pump/pause; it is not a
new socket wire2 command. No second matcher, scheduler or direct board mutation.

Stdout is NDJSON: one type=identity record followed by type=result per operation,
with zero-based index, op, success and body or structured error plus current
snapshot when available. Define exact schema in SLC-022 before external driver
integration. CPU/session/generation/count values follow the SDK widths/decimal
string convention. Runtime commit, bundle/manifest identity, module/image/config
hashes and selected capabilities are explicit. Diagnostics use stderr.

A runtime/module fault leaves the session faulted and does not stop script
processing: subsequent state, permitted pure boardInspect and explicit reset
remain possible. Invalid subsequent execution fails without mutation. Exit0
means no operation failure; exit2 means startup/script validation failure before
execution; exit3 means one or more runtime/operation failures occurred, even if
later explicitly recovered. Native crashes are OS failures, not typed success.
Session identity may vary per run; reproducibility compares architectural state,
virtual timing and module evidence while retaining both identity records.

## Distribution and acceptance

Deliver a pinned runtime source archive/build recipe and executable, with SDK
version compatibility and build/source identity separate from the board module.
Ponsse owns exact artifact/bootstrap locks and full-system launcher/tests.
First synthetic .so gate proves bus/port/time/input/inspect/reset/fault semantics
before an external module is used. Real firmware results remain evidence-scoped.
Socket/DAP attach and rich trace features remain later stages of the same session.
