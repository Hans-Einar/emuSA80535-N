# VER-SLC-018 — AlmaLinux package and embedding verification

Date: 2026-10-27 (Europe/Oslo)
Status: pre-publication verification passed; publication verification pending
Slice: SLC-018 / ITR-018 / SPR-010
Product/source candidate: `70eb6bc2bc9db71a3420baf2a1b4a9ef08d92f05`
Prior accepted baseline: `c8d647a29d5a160637025fa387e31f15628c3ec1`

## Build identity and scope

The Worker built `emusa80535` 1.0.0-1.el10_2 on AlmaLinux 10.2 x86_64.
Compiler: GCC 14.3.1 (14.3.1-4.4.el10.alma.2); RPM:
4.19.1.1-23.el10.alma.1. The source archive contains committed candidate
sources plus SOURCE_COMMIT. Repeated archive generation was byte-identical;
this is not a promise of byte-identical RPMs across build environments.

The Master confirmed no changes to the existing core, opcodes, decoder,
loader, public CPU header or debugger C/H implementation against the accepted
baseline. Product changes are build/install/package tooling and a generic
installed-consumer example. No external ROM or board implementation is
included. VS Code/DAP remains a separate product.

## Gates executed

| Gate | Result |
|---|---|
| Worker RPM build with distribution compiler/linker flags | PASS |
| RPM check: eleven default CPU/event/trace/runtime suites | PASS |
| RPM check: C debugger facade and NDJSON process tests | PASS |
| Staged and final extracted SDK consumer | PASS |
| Existing curses frontend build | PASS; inherited warnings only |
| Master DNF install into isolated AlmaLinux 10.2 | PASS |
| Master `rpm -V emusa80535 emusa80535-devel` | PASS |
| Installed-only pkg-config consumer compile/run | PASS |
| Installed protocol 1.0 and exact source identity | PASS |
| Frozen DAP real contract/equivalence/F5 smoke | PASS |
| Read-only external image load/step/bounded run/repeat | PASS; no board |
| Whitespace, duplicate-safe YAML, NDJSON | PASS |
| Historical ledger preservation | PASS; baseline byte prefix unchanged |
| Signed public repository installation | PENDING |

## Independent Master installation

The container image was the already-cached official AlmaLinux image:

`docker.io/library/almalinux@sha256:ba31c3299856068f77bc10574cad51b0a4f6a3dafb882668656e1639bee63db6`.

Its `/etc/os-release` confirms 10.2. In an ephemeral container, DNF installed
the two local RPMs with resolved distro dependencies and GCC/pkg-config.
`rpm -V` reported no discrepancies. Then:

```text
python3 packaging/check_install.py / --commit 70eb6bc2bc9db71a3420baf2a1b4a9ef08d92f05
```

passed using installed files only. The consumer executes synthetic CPU
instructions, one MOVX read/write pair, disassembly, snapshot capture,
debugger creation/destruction and standalone runtime construction/status.
The executable reports product 1.0.0, protocol 1.0 and the exact source SHA.

The current host application/RPM installation was not modified by this test.
The repository host runs openSUSE Leap 15.5 and is used only for storage,
signing and metadata publication, never as an AlmaLinux execution oracle.

## DAP compatibility

An isolated clone of `emuSA80535-DAP` was checked out at frozen commit
`36639b48ddb2ffbafa14c00da794fe1734f7483b`. `npm ci --ignore-scripts` and
`npm run build` completed with Node 22.23.2. The Master ran:

```text
node tests/test_dap_real.mjs <frozen-DAP-clone> <extracted-RPM>/usr/bin/emu-debug
```

Real contract, fake equivalence and F5-style adapter smoke passed. This is
the existing automated adapter gate, not a new interactive VS Code GUI test.
No DAP product source was modified.

## External image readiness

The user-provided reference image was read from the sibling project without
modification or copying it into this repository or any RPM. Size is 65,536
bytes, SHA-256
`d1b5702e931330269c75ff8f08c32e9f107bb01c5ee89d852eda4bf14bfa073e`.

Both the baseline executable and the packaged executable successfully
performed handshake, hash-checked load, seeded reset, one step, a further
10,000-instruction bounded run and termination. The packaged execution was
repeated in two fresh processes in the AlmaLinux container; all responses
were identical. It yielded with 10,001 instructions and 13,576 machine cycles.

No board or external XDATA device was attached. This proves raw-image load
and bounded CPU execution readiness, not whole-firmware correctness, board
boot completion or peripheral integration. Simulator composition remains an
external task; the opaque stock debugger cannot attach to that CPU yet.

## Unsigned artifact identities

Source archive SHA-256:
`418640e97efbba0d701938385f834f86a923f7c67e315e5975be77fd0fbbde47`.

Unsigned runtime RPM SHA-256:
`94f2a918cc707fa95ad670a76b4bc59f8e81785b4e0164c0c76c87e90641adce`.

Unsigned development RPM SHA-256:
`baec8d88406a5bd0fe8d28b7a1b521333e787c2b7985bcf96d310263c2b57a79`.

The signed publication manifest will record all final RPM hashes, including
source/debug packages, and their unsigned identities. Signing changes the
RPM envelope hash, not its payload or source identity.

## Remaining gate

Independent review must approve before publication. The Master must verify
signed artifacts and a clean public HTTPS/DNF installation with both RPM and
repository metadata signature checks enabled before closing SLC-018.
