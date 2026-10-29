# Interface bundle 0.3.0 — target contract

**Declarations and fixtures only.** No CPU observation, control 3.0, persistent
listener, canonical trace producer, file writer or invocation implementation is
supplied by this bundle. Current runtime remains wire 2.1 / contract 0.2.0;
`script1` and legacy debugger 1.0 are unchanged. Do not link the new query symbols
until a producer explicitly supplies and verifies them. Compile-only examples
intentionally do not call unavailable symbols.

Read [compatibility](docs/01-compatibility.md), [observations](docs/02-observation.md),
[control](docs/03-control.md), [tracing](docs/04-trace.md), [files](docs/05-files.md)
and [conformance/handoff](docs/06-conformance.md). Exact object shapes are in
[control schema](schemas/runtime-3.0.schema.json),
[observation schema](schemas/observation.schema.json) and
[file schema](schemas/trace-file.schema.json). Numeric tags map directly to
canonical target headers under `lib/include/emusa80535/contracts`.

Baseline [0.2 contracts](../0.2.0/README.md) and their reference documents are
included in this archive, with historical availability statements retained as
historical evidence. This document supersedes only the target 3.0 profile.
Do not reinterpret 2.1 tracePage replies as 3.0 records.

Export with `python3 interfaces/tools/bundle.py --version 0.3.0 export --output
/absolute/destination.tar.gz` from the reviewed clean commit. Verify archive and
create a consumer lock with the same `--version 0.3.0` selector. No external
repository or sibling files are needed. A bundle is compatible only after its
manifest, canonical archive hash and full source commit match the consumer lock.
