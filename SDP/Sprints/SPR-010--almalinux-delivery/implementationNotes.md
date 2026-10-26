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

## Worker committed-source package result

Source commit: `70eb6bc2bc9db71a3420baf2a1b4a9ef08d92f05`.
The package implementation and accepted emulator sources are frozen at that
commit; subsequent evidence-only commits do not change the delivered product.

- `python3 packaging/make_source.py /tmp/emusa80535-worker-rpm/SOURCES`:
  passed from a clean tree. Repeating into another directory produced a
  byte-identical source tarball (`cmp` passed).
- `rpmbuild -ba --define '_topdir /tmp/emusa80535-worker-rpm'
  --define 'dist .el10_2' .../SPECS/emusa80535.spec`: passed with distribution
  hardening, x86-64-v3 baseline and fat-LTO compiler flags.
- RPM `%check`: all eleven suites, facade, process and staged installed-only
  consumer/provenance checks passed. No RPM build warning/error was reported.
- Extracted completed runtime/devel RPM payloads to a new temporary directory
  and reran `check_install.py`: passed, including static-library linkage after
  RPM postprocessing and executable commit/protocol identity.
- Runtime dependency/payload audit: libc only; no curses dependency. Source
  archive has 202 members and no `.bin`, `.rpm`, `.a`, `.o` or simulator tree.
- Plain legacy `make -j4`: passed with existing TUI source warnings; the curses
  frontend was built but is intentionally not distributed in these packages.
- CPU, loader, decoder and every debug `.c`/`.h` source remain byte-identical
  to baseline `c8d647a`; whitespace gate and working-tree status are clean.

Artifacts: `/tmp/emusa80535-worker-rpm/RPMS/x86_64/` contains runtime,
development, debuginfo and debugsource `1.0.0-1.el10_2.x86_64` RPMs. The source
RPM is under `/tmp/emusa80535-worker-rpm/SRPMS/`. Full build output is
`/tmp/emusa80535-worker-rpm-build.log`; legacy build output is
`/tmp/emusa80535-worker-legacy.log`. These local unsigned artifacts await fresh
independent review, Master verification and authorized signing/publication.
