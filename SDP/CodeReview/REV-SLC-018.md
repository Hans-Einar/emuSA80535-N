# REV-SLC-018 — AlmaLinux package and embedding SDK review

Date: 2026-10-27
Reviewer: fresh independent Reviewer agent `/root/package_reviewer`
Disposition: **APPROVED**; no blocking or corrective findings
Slice: SLC-018 / ITR-018 / SPR-010
Baseline: `c8d647a29d5a160637025fa387e31f15628c3ec1`
Reviewed product/build/documentation candidate:
`70eb6bc2bc9db71a3420baf2a1b4a9ef08d92f05`

The later Worker evidence commit `a1c2e5f` changes only this Sprint's
implementation notes and does not change the reviewed package inputs.

## Scope and contract audit

Reviewed the complete baseline-to-candidate diff against REQ-017,
DES-098..DES-100 and the SLC-018 contract, including the Master's governance
cleanup. All 33 preexisting C/H files retain their exact baseline blobs.
The build/install changes preserve the existing CPU and protocol semantics;
the static library includes the accepted generic modules without curses.
The runtime and devel package split, exact dependency, pkg-config metadata,
license payload and source identity are consistent with the contract.

The documentation explicitly distinguishes a simulator-owned CPU from the
opaque debugger's separate CPU and does not claim board-attached debugging,
complete peripheral coverage, Gate B or REQ-015 completion. The installation
instructions identify the AlmaLinux 10.2 x86_64 target and `.el10_2` release
argument. No firmware image, board code or physical-I/O path is added.

CurrentIndex selects the recorded contract. Updated navigation and Timer2
handoff resolve stale next steps without rewriting the historical ledger or
the source-scoped DEBUG aliases. Both YAML files pass a duplicate-rejecting
parser; the entire baseline ledger is an unchanged byte prefix of the new
ledger, and every NDJSON record parses. Whitespace checking passed.

## Independent executable verification

Used a separate RPM tree `/tmp/emu-slc018-review-lf9gtnq6`, independent of
the Worker's build and without changing product files.

1. Rebuilt the Worker's source RPM with:
   `rpmbuild --rebuild --define '_topdir /tmp/emu-slc018-review-lf9gtnq6' --define 'dist .el10_2' /tmp/emusa80535-worker-rpm/SRPMS/emusa80535-1.0.0-1.el10_2.src.rpm`.
   Result: PASS. The distribution hardening/LTO flags were present. All eleven
   default suites, the debugger facade, the NDJSON process suite and the
   installed-consumer gate passed in this independent rebuild.
2. Extracted the rebuilt runtime and devel RPM payloads to a fresh staging
   directory. Re-ran `packaging/check_install.py` against those payloads with
   exact candidate SHA: PASS, including CPU/MOVX callbacks/decoder/facade/
   runtime linkage, protocol 1.0 and executable provenance.
3. Compiled an additional Reviewer-owned program against only the extracted
   SDK and pkg-config metadata. It loaded a synthetic exact 64-KiB image,
   rejected a short image without changing CODE, executed its first
   instruction and captured the expected accumulator/PC: PASS.
4. Compared all five installed headers and the installed license with the
   source files: byte-identical. Inspected RPM file lists, dependencies and
   empty runtime scriptlets: expected payload, no curses dependency or
   unexpected installation side effects.
5. Created an isolated clean checkout at the exact candidate. Generated the
   source archive twice: byte-identical. Its members are unique, its
   SOURCE_COMMIT matches the candidate and it has no `.bin`, `.hex`, `.rom`,
   `.rpm`, `.o` or `.a` inputs. The source helper rejected both an untracked
   file and a tracked Makefile change.

Unsigned source RPM SHA-256:
`c1afa35ae1640c072da21eacc11e7b229494dd25f7327a92afc998e096a4f72e`.
Source archive SHA-256:
`418640e97efbba0d701938385f834f86a923f7c67e315e5975be77fd0fbbde47`.
Rebuild log: `/tmp/emu-slc018-review-lf9gtnq6/rebuild.log`.
Temporary test logs/paths are supporting local evidence; the committed
spec/checks and published source RPM provide the reproducible inputs.

## Publication operation inspection

Inspected the Master's one-shot operation
`/tmp/emu-slc018-evidence/publish.py`, SHA-256
`45d6535de4cd888686aafeacfc745dd5fda7ac8402610256d85d61f2738cb934`.
It checks the exact candidate/version/artifact set and input hashes, uses the
existing server promotion lock, signing identity and metadata refresh helper,
verifies RPM signatures, refuses filename reuse and checks preservation of
previous package hashes. Private signing material remains on the server.
No blocking issue found for this owner-authorized one-shot publication.
If publication fails after exposing files, the Master must inspect the partial
state before retrying; the operation deliberately refuses an existing source
release directory rather than overwriting it.

## Acceptance boundary

This review approves the exact candidate above for Master verification and
publication. It does not certify a full-machine simulation, new debugger
features, Windows execution, or compatibility with other AlmaLinux releases.
The packaged SDK supports the documented raw CODE loader; the inherited
`load_obj` declaration belongs to the unshipped legacy TUI/HEX-loading path.
Signed publication, HTTPS retrieval and repository installation evidence must
be recorded by the Master in VER-SLC-018 before closing the Slice.
