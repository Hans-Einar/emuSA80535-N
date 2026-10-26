# Implementation notes — SLC-018

## Master preparation

- Resolved current authority through CurrentIndex rather than historical
  foundation handoff; updated AGENTS.md accordingly.
- Reconciled Timer2 handoff/sprint with confirmed PR #18 integration.
- Kept all historical ledger events and source-scoped DEBUG aliases intact.
- Established the bounded package contract before assigning implementation.
- Confirmed local builder AlmaLinux 10.2 x86_64; repository host is openSUSE
  Leap 15.5, reached through `ssh warloc@beinerud`.
- Existing client repository config targets
  `https://beinerud.ddns.net/repo/AlmaLinux-10.2-apps/` with RPM and metadata
  signature checks enabled. Existing publication procedure is being inspected.

## Worker result

Implemented generic `sdk`/POSIX `install` targets, a static library of the eight
accepted core/debug units, five installed headers and pkg-config metadata.
The RPM split is `emusa80535` (headless runtime) and `emusa80535-devel`
(same-build static SDK); no shared ABI or advanced debugger completion is
claimed. Added a clean-HEAD deterministic archive helper and an installed-only
consumer that exercises instruction execution, XDATA callbacks, disassembly,
snapshot facade and standalone runtime linkage. The install check also tests
protocol 1.0 and exact source identity.

Initial Worker checks on AlmaLinux 10.2 x86_64:

- `make -j4 sdk emu-debug`: passed.
- Fresh executable rebuild and `make install` to
  `/tmp/emusa80535-worker-stage`, prefix `/usr`, libdir `/usr/lib64`: passed.
- `python3 packaging/check_install.py ... --commit <HEAD>`: passed.
- All eleven default suites plus facade and NDJSON process suite: passed.
- Dirty-source archive creation: rejected as required.
- `git diff --check`: passed.

The first installed-consumer attempt exposed an example ordering mistake:
variant initialization cold-clears CODE. The example now copies its synthetic
program after initialization. No emulator source change was required.
Clean committed-source RPM build/check and independent review follow.
