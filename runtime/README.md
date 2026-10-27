# Local runtime script1

`emu-runtime` owns one CPU/session and one independently built native board.
Build offline from a pinned clean source checkout/archive with `make emu-runtime`.
Dependencies are a C99 compiler, GNU make, Linux x86-64 libc/libdl, Python3 for
build identity, and the included source. `make install-runtime DESTDIR=...`
installs the executable, CPU/session static libraries and these product docs.
This runtime implements native API1 session create/destroy/pump and debug
open/request/close, alongside CPU API1. Immutable SDK0.2.0 headers retain their
historical publication labels (`TARGET` and `EMUIF_TARGET_IMPLEMENTED=0`);
those frozen labels describe the bundle's publication, not this later runtime.
The exact runtime source pin shown by `--version`, identity and capability hello
is the implementation authority. Socket listeners and DAP attach remain absent.

The source archive must carry its exact `SOURCE_COMMIT` (the repository's
`packaging/make_source.py` generates it). Record and verify the archive SHA256
before unpacking. No external board implementation is included.

```
emu-runtime --variant sab80535 --clock-hz 11059200 --seed 0 --entry 0 \
  --image /absolute/image.bin --image-sha256 LOWERCASE_HEX64 \
  --board /absolute/module.so --board-sha256 LOWERCASE_HEX64 \
  --board-config /absolute/config.json --script /absolute/commands.ndjson \
  --board-inspect-fault
```

All valued arguments are required, unique and case sensitive. `--help` and
`--version` work alone without opening any input. Numeric arguments are decimal.
Image size is exactly65536 bytes. Configuration is a nonempty UTF-8 JSON object,
maximum16384 bytes; use `{}` for empty configuration. Identity hashes the exact
image/config bytes, including config whitespace. Module size is bounded to64MiB.
All file paths must be absolute regular files. The loader copies module bytes
into a private sealed Linux memfd, verifies SHA256, and dlopens those exact bytes
with local scope. `$ORIGIN`-relative module dependencies are unsupported; use a
self-contained module or system shared-library dependencies. ELF constructors
are trusted native code and can run during loading. Descriptor/ABI/callback and
required CPU-capability checks happen before session creation/execution.
Board ABI1.0 modules built against immutable SDK0.1.0 remain compatible.

`--board-inspect-fault` is optional, default off. It asserts that the integrator
has reviewed the exact pinned module's promise of pure bounded post-fault
inspection. The module must advertise INSPECT. The host records this policy in
identity and enables `boardInspectFault` before opening its local controller.
This is host policy, not a new ABI bit or an inferred capability.

The script is LF-terminated NDJSON. First line is exactly the object
`{"format":"emu-runtime-script","version":1}` (key order/spacing may vary).
Remaining lines are at most256 command objects; each line including LF is at
most65536 bytes, total file at most8MiB. No blank lines, comments, BOM, CR,
duplicate keys, unknown fields, malformed UTF-8 or noncanonical uint64 strings.
The complete file and static ranges are validated before loading any module or
executing any command. JSON bounds: depth16,8192 values,256 object members and
1024 array elements per record. Board command depth is limited to15 because the
native wire arguments wrapper adds one level. Payload/request object compact JSON is <=4096
bytes. Board-owned semantics are validated by the session at admission time.

Supported operations (exact shapes in `script1.schema.json`):

- `{"op":"state"}` returns current snapshot.
- `{"op":"run","cycles":100}` resumes, pumps1..100000 machine cycles and
  pauses only if still running. Pump safe drain can add at most16 cycles.
- `{"op":"pause"}` pauses coherently; already paused succeeds without time.
- `{"op":"step","maxCycles":100}` executes one instruction, or bounded
  IDLE/stop under API1, while paused; safe drain can add at most16 cycles.
- `{"op":"reset","seed":0,"entry":0}` restores original image and advances
  generation, including explicit recovery from fault.
- `{"op":"boardInput","schema":"synthetic.inputs.v1","cycle":null,"payload":{}}`
  admits an opaque stimulus; null resolves current+1, otherwise cycle is a
  canonical uint64 decimal string strictly in the future. Returns resolved order.
- `{"op":"boardInspect","schema":"synthetic.status.v1","request":{}}`
  performs opaque pure inspection; after fault requires the explicit promise.
- `{"op":"replaceCodeBreakpoints","addresses":[0,32]}` atomically replaces
  up to1024 unique uint16 CODE addresses while paused.

Stdout contains one `identity` followed by one `result` per operation, no other
records. Each record including LF is at most65536 bytes. `output1.schema.json`
is authoritative. `identity.runtime` is the exact wire2.1 hello body, including
runtime source commit, immutable bundle manifest digest, capabilities and limits.
`identity.module` records descriptor id/version/buildId, verified artifact digest
and `inspectFaultPromise`. Top-level variant/clockHz/seed/entry and actual image/
config hashes record startup choices. `snapshot` is the exact wire2.1 status body.

Every result has zero-based `index`, `op`, boolean `success` and current `snapshot`.
Successful `body` is the corresponding wire2.1 command body (state uses status;
boardInspect's script `request` maps to wire `payload`). Run instead returns
`{"cycles":"N","instructions":"N","outcome":"budget"|"stop"}`.
Every run result additionally has `progress:{"cycles":"N","instructions":"N"}`,
including on failure. `budget` means local pump completion; it is not firmware
halt. The final snapshot may have reason `pause` due to the host's explicit pause.
Earlier breakpoint/architectural/fault stops are preserved. Failed results have
`error:{"code":"STATUS","message":"text","retryable":false}` (BUSY may be
retryable). Counters/generations/cycles are decimal strings; uint32 and smaller
values are JSON numbers. Run partial fault progress is retained even when no
instruction completed. Fault snapshots remain inspectable and may be unsafe.

Faults do not terminate script processing: state, permitted board inspection and
reset remain available; execution without reset fails. Exit0 means all operations
succeeded; exit2 means startup/static validation failed before CPU execution and
stdout is empty; exit3 means an operation/runtime/output failure, even after
recovery. Diagnostics use stderr. Native crashes are OS failures, not typed
results. No retries, sockets, DAP attach, administrative signal handling,
physical I/O or wall-clock pacing are implemented by this host.

For replay compare architectural snapshot/time/module evidence, retaining both
identity records. Session IDs vary between processes; lifetime admission/access
IDs survive reset and are not architectural reset values. Identical initial
seed/image/config/commands are deterministic subject to the module contract.

The native session library admits at most8 concurrent clients and8192 lifetime
connection IDs. IDs cannot be reused; lifetime exhaustion returns CAPACITY before
mutation. The script host uses only one connection. Host tests additionally need
Python `jsonschema` and Git history for the immutable0.1 fixture; an unpacked
verified SDK0.1 include tree can instead be supplied as `EMUIF_SDK01_INCLUDE`.
Run `make runtime-host-test` after building. No test dependency is needed to run
the installed executable.

Native consumers: authorized terminate after an unsafe partial fault applies
termination and closes clients, returning native OK with written=required=0.
Wire2.1 cannot represent a terminated unsafe snapshot, so no success acknowledgement
is fabricated. Zero response here means connection closure; ordinary safe
termination returns its conforming acknowledgement. Script1 has no terminate
operation and is unaffected by this native lifecycle case.
