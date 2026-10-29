# VER-SLC-020 — Shared interface bundle verification

Date: 2026-10-27
Disposition: PASS — contract/header bundle and legacy include compatibility
Reviewed candidate: 7d924cc27e48c599d5775904543070d92e413c26
Independent review: REV-SLC-020, approved at f911923; F001..F007 resolved

## Contract and compatibility evidence

- Worker and independent Reviewer passed `make interface-check`: isolated C99
  and C++17 public-header/consumer compilation, wire schema and UTF-8/uint64
  negative fixtures, reference/link checks, deterministic archives and rejected
  modified/missing/extra payload, wrong provenance/locks and optimized Python.
- All eleven CPU/peripheral/event/trace/runtime suites and debugger facade and
  protocol1.0 process suites passed. Product/build candidate 24f1268 is unchanged
  by the final README/manifest and malformed-command contract corrections.
- Master independently compared all fifteen changed existing C implementation
  files with activation baseline 6f1ff9f: only public include paths changed.
  Canonical legacy declarations retain upstream notices and gain C++ guards;
  root headers forward rather than duplicate definitions.
- Master built an exact Git source archive of 24f1268 without .git, with its
  SOURCE_COMMIT. `make -j2 all emu-debug sdk CPPFLAGS=-DSLC020_CONSUMER_BUILD=1`
  passed, including the legacy curses executable. `make install DESTDIR=...`
  with prefix=/usr and libdir=/usr/lib64 correctly used SOURCE_COMMIT without
  DEBUG_COMMIT override. `packaging/check_install.py` passed external SDK
  compile/run, executable protocol1.0 and source identity from the staged tree.
  Evidence log: /tmp/emu-slc020-master-tj7z5pux/verification.log (ephemeral).
- Master exported the final 7d924cc source archive independently; its archive
  is byte-identical to Worker's clean-checkout export. Offline verification
  passed. Exact source, manifest/archive hashes and durable local artifact path
  are recorded in SLC-020-ARTIFACT.json. No sibling checkout is needed to export.
- Whitespace checks pass for newly authored content. The full relocation diff
  retains pre-existing whitespace in emu8051.h and the exact vendored DAP MIT
  license EOF, as explicitly reviewed in REV-SLC-020; no blanket clean-diff
  claim is made for those provenance-preserving copies.
- Master integration checks duplicate-safe YAML, NDJSON, all new indexed source
  paths, unchanged historical ledger prefix and DEBUG aliases, frozen manifest
  hashes, and no active implementation after closeout.

## Acceptance boundary

Accepted bundle 0.1.0 payload is immutable. Later changes require a new bundle
version, with incompatible changes also changing the appropriate API/ABI/wire
version. Local exports and consumer locks identify copies by source and hashes;
this does not assert remote publication or automatic adoption by another repo.

New CPU/controller, board, session/debug APIs are declarations; runtime wire2
and DAP attach are specifications. Their synthetic scheduler/module, transport,
DAP and signal runtime gates remain future work under REQ-024. Existing stdio1
behavior stays implemented and tested. No runtime feature, external simulator,
physical I/O, SDP upgrade or RPM publication was implemented by this Slice.
