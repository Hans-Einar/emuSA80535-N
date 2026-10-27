# Interface bundle 0.2.0

Status: CPU API1 is implemented in `libemusa80535_cpu.a` (also included in
`libemusa80535.a`). SLC-021 review and verification record its acceptance.
Session/debug APIs, board loader, socket service and DAP attach remain targets.
This pre-1.0 bundle is independent of product1.0.0 and legacy wire1.0.

| Channel | Contract | Canonical header / wire | Availability |
|---|---|---|---|
| CPU ↔ controller | [CPU API](docs/02-cpu.md), [time](docs/04-time.md) | `emusa80535/contracts/cpu.h` | Implemented API 1; same-build C ABI |
| Controller ↔ board | [Board ABI](docs/03-board.md) | `emusa80535/contracts/board.h` | Target ABI 1.0; native Linux x86-64 |
| Host ↔ session | [Session](docs/05-session.md) | `emusa80535/contracts/runtime.h` | Target API 1 |
| Debug service ↔ session | [Debug facade](docs/06-debug-service.md) | `emusa80535/contracts/debug.h` | Target API 1; current debugger owns its CPU |
| External client ↔ debug service | [Wire 2.1](docs/07-wire.md), [schemas](schemas/runtime-2.1.schema.json) | Strict bounded UTF-8 NDJSON | Target; distinct from implemented stdio1.0 |
| IDE ↔ DAP adapter | [DAP mapping](docs/08-dap.md) | Standard DAP Content-Length frames | Existing launch1.0; attach2.1 target |
| OS ↔ host | [Administration](docs/09-host.md) | Configuration, signals, diagnostics | Target; do not send SIGUSR1 to old runtime |
| Existing callers | [Legacy compatibility](docs/10-legacy.md) | `emusa80535/emu8051.h`, `emu_debug*.h` | Implemented, same-build API |

Start with [roles and common rules](docs/01-overview.md). The [version and copy
workflow](docs/11-versioning.md), [consumer inventory](consumers.json), and
[acceptance matrix](docs/12-conformance.md) describe distribution and adoption.
`manifest.json` hashes every distributed source payload file. `SOURCE_COMMIT`
and `BUNDLE-RECEIPT.json` are generated when exporting an exact clean revision.

Source include root: `lib/include`. Example:

```c
#include <emusa80535/contracts/board.h>
```

All six target headers and five legacy headers are shipped together; no sibling
repository is required to read or compile this bundle. Only CPU target symbols are implemented. Other target symbols must not be
linked or advertised as available.
