# Control wire3.1 — wire-only extension0.4.0

This separate contract adds read-only enumeration of the actual CODE breakpoint
set. It changes no CPU, board, observation, trace facade or trace-file ABI.
Availability is discovered from a successful hello, not from artifact presence.
The native entry remains `emusa80535_control_query_v3`: its JSON bridge selects
the wire minor per connection. No new public header is needed; use the immutable
SDK0.3 headers and this sidecar. Both artifacts must be pinned by the consumer.

## Explicit negotiation and unchanged baseline

Connect to the same explicitly configured authenticated Unix control endpoint.
Send hello with `protocol:{major:3,minor:1}`, `contractVersion:"0.4.0"` and the
requiredCapabilities list. Require `readCodeBreakpoints` when this feature is
needed. The successful3.1 hello advertises that optional capability and returns
the SHA256 of this sidecar's raw manifest.json as contractManifestSha256.
Do not silently probe/fall back or replay mutations. Accepted older runtimes
reject3.1 hello with VERSION and close. A successful profile is fixed for that
connection; connect explicitly again to select another profile.

Existing3.0 connections retain exact protocol3.0/contract0.3.0, the old manifest
and capability set. Their unknown readCodeBreakpoints command remains unknown
and closes according to the old framing rule. Requiring that new capability in
old3.0 returns UNSUPPORTED. Existing wire2.1/script1/legacy1.0 are unchanged.
All inherited3.0 command, state, owner/lease, validation, limits and error rules
apply to3.1 except the hello identity and the new read operation described here.
Both minors may coexist on the same runtime/session.

The immutable baseline is SDK0.3 source2133e7220284bb8dc1600ba98b2d842106a7dba4,
manifest ed3917b0d3a7300aca94171f1259eacf9861b0ee87376afa534690618da27ff0,
archive405f22f7fc7e0a49d921715102102a4d667ba1ff84b1238494d76353feeb0f53.
See its docs02/03/04/05/06 for CPU observation, scheduling/control, traces,
files and conformance. Its native headers, schemas and payloads remain immutable.

## readCodeBreakpoints

Capability and command are both `readCodeBreakpoints`. It depends on the existing
mandatory `codeBreakpoints` capability. Arguments contain only sessionId:

```json
{"type":"request","id":3,"command":"readCodeBreakpoints","arguments":{"sessionId":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}
```

Successful response body contains exactly sessionId, generation and addresses:

```json
{"type":"response","id":3,"command":"readCodeBreakpoints","success":true,"body":{"sessionId":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","generation":"1","addresses":[0,16,65535]}}
```

This is the complete actual CODE bitmap, not a cache of the last replace request,
watch set or trace configuration. The list is strictly increasing and unique,
with0..1024 JSON integer entries in0..65535 inclusive. Booleans and floating JSON
numbers such as1.0 are not valid address entries. Empty list means the actual
CODE set is empty. There is no paging. The response is within the inherited
65536-byte frame bound even for all1024 addresses.

The single execution owner obtains sessionId, current generation and bitmap at
one serialized command boundary. Generation is canonical nonzero uint64 decimal
text and may have changed since attach/reset. It is not a revision or CAS token.
The response sessionId must equal the request sessionId; response id and command
must equal the request. Schema/semantic validation does not prove a snapshot came
from real execution; actual runtime tests establish that separately.

An attached observer or controller may read while paused, running or faulted,
and in other existing nonterminated attached states. No control lease or expected
generation argument is required. Wrong session yields STALE; unattached yields
STATE. Terminated sessions are not readable (STATE if the connection remains
addressable, otherwise existing close/NOT_FOUND behavior). Malformed/extra fields
follow the existing INVALID-and-close rules; unknown command/version handling
is unchanged. Read-only permission does not grant control rights.

Enumeration never progresses CPU time, changes stopRevision/state/lease, clears
breakpoint suppression, or changes traces/captures/output counters. Reset retains
the existing CODE set but increments generation. Existing replacement semantics
are unchanged, including its paused-state requirement.

To save, temporarily replace and restore a set, the consumer must retain the
exclusive controller lease throughout snapshot, replacement, intentional run
and restoration. An observer reading a set does not reserve it. There is no CAS,
breakpoint revision, rollback or disconnect recovery guarantee. On uncertain
mutation result or lease loss, do not automatically restore or retry. No hidden
reset, clearing, stepping or fallback is part of enumeration.

## Trace files still use0.3

The new0.4 manifest identifies control wire3.1 only. Shared-session JSONL headers
remain `contractVersion:"0.3.0"`, protocol3.0 and the baseline manifest hash,
regardless of which connection opens the sink. Never substitute a3.1 hello's0.4
manifest into a trace header. CPU API1, board ABI1.0 and observation/trace facade1
remain unchanged. Existing3.0 tracing and a separate3.1 enumeration connection
can coexist; an authorized3.1 controller can also use all inherited commands.

## Offline validation, installation and copying

`wire.py` loads the exact frozen baseline validators from baseline-tools after
checking their pinned hashes. It binds the frozen bundle dependency explicitly,
without importing an arbitrary module named bundle from the caller's sys.path.
It wraps inherited semantics with strict integer/order checks and provides:

- `validate(value, schema=None)` for complete3.1 frames plus semantics.
- `validate_frame(bytes, schema=None)` for strict LF/UTF-8/duplicate-key framing.
- `validate_exchange(request, response)` for response ID/command/session binding.
- `validate_file_line(value)` for unchanged0.3 JSONL shape/base manifest.

Python3 with jsonschema is needed for wire validation. The bundle helper uses
only Python's standard library and works from a copied/extracted sidecar without
Git, this repository, installed CPU binaries or sibling directories.

```sh
python3 bundle.py check --expected-manifest EXPECTED_SHA256
python3 bundle.py export --output /absolute/new/accepted-control.tar.gz --lock /path/control31.lock.json
python3 bundle.py export --output /absolute/new/control-0.4.0.tar.gz --source-commit EXACT40HEX
python3 bundle.py install --destination /absolute/new/interface-bundles/0.4.0 --source-commit EXACT40HEX
python3 check_contract.py
```

After acceptance the repository's external `interfaces/control31.lock.json`
pins schema `emusa80535-control-lock-v1`, bundleVersion0.4.0, sourceCommit,
manifestSha256 and archiveSha256. It sits outside this hashed sidecar to avoid
self-reference. `--lock PATH` selects that accepted contract source and verifies
both manifest and deterministic archive hash before any export/install write.
An explicitly conflicting source/hash fails; a missing or invalid supplied lock
never falls back. Later runtime builds use this lock, retaining exactly the same
accepted0.4 archive/source even when runtime SOURCE_COMMIT changes. Before the
first acceptance only, builds without the external lock use their candidate
source commit. Runtime build provenance is recorded separately from the pinned
contract. Exported/copied sidecars already carry immutable source receipts.

A release supplies trusted source/archive/manifest pins externally. `check`
without expected hash establishes payload/internal consistency, not provenance
or authenticity. Source copies do not contain SOURCE_COMMIT until export/install;
exported copies retain immutable SOURCE_COMMIT and BUNDLE-RECEIPT.json. Export
rejects existing output files; install accepts an existing identical verified
copy with the same provenance and refuses differing/unverified copies. The
helper never rewrites copied provenance. Archives are deterministic for the same
payload and source; timestamps and owner fields are fixed. Manifest inventory
excludes only its own bytes, generated provenance and ordinary Python cache
files. Unknown files, unsafe payload paths, symlinks and nonregular entries fail.

Examples are shape fixtures, not claims that a runtime executed those operations.
check_contract.py checks the schema, inherited examples, strict negatives,
identity correlation, exact inventory, tamper/path refusal, deterministic export
and standalone copied validation. Runtime implementation is separately tested
with actual native/Unix replies and installed artifacts before release acceptance.
