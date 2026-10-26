# Scrum iterations

## ITR-018 / SLC-018 — Package accepted emulator and embedding SDK

Status: active

### Contract

Implement DES-098..DES-100 / REQ-017 against baseline
`c8d647a29d5a160637025fa387e31f15628c3ec1`. The Master owns governance cleanup,
traceability, server inspection and final publication. A fresh Worker owns
build/install/RPM implementation and documentation, then a fresh Reviewer
reviews the entire bounded result independently.

Worker scope: Makefile, .gitignore, packaging/, generic embedding example or
package-consumer smoke test, README.md and this Sprint's implementationNotes.
Do not alter core.c, opcodes.c, disasm.c, binary_loader.c, emu_debug*.c/.h or
emu8051.h. Preserve existing build/test targets and Windows behavior.

Deliverables:

1. Buildable static embedding library containing accepted generic modules.
2. DESTDIR/prefix/libdir-aware installation of executable, library, headers,
   license/documentation and pkg-config metadata.
3. AlmaLinux 10.2 x86_64 spec with runtime and development packages, source RPM,
   normal distribution build flags, meaningful check gate and exact source SHA.
4. Reproducible archive/build instructions that reject uncommitted product
   inputs rather than claiming a clean source identity for them.
5. Generic external-consumer smoke test using only installed SDK metadata.
6. Documentation separating standalone CPU debugging from simulator-owned
   CPU composition; no claim that advanced trace/DAP integration is complete.

Acceptance: existing eleven default suites, C facade and NDJSON process tests;
RPM build/check; payload/dependency/license audit; staged or isolated installed
consumer compile/run; executable protocol/version provenance; no target asset
in source/binary package; clean whitespace; independent approved review and
Master verification. Test actual ROM only externally, read-only and bounded.
No complete-machine execution claim follows from loading/stepping a ROM.

Publication is authorized by the current user, but occurs only after review
and verification. Use existing repository layout/signing, preserve unrelated
packages, record final URLs/hashes and verify retrieval with signature checks.
