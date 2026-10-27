# REV-SLC-020 — Shared interface contract and canonical SDK review

Date: 2026-10-27
Reviewer: fresh independent `interface_reviewer`
Disposition: **APPROVED** for SLC-020 contract/header/tooling scope
Reviewed candidate: `7d924cc27e48c599d5775904543070d92e413c26`
Baseline: `6f1ff9f1a43258c49770a038a0444d95a16b9666`
Bundle: `0.1.0`
Manifest SHA-256: `2893403dac5f02f71a0faa9c203e3b08d46d9f619626d39105b9cf0382e28097`

## Scope and conclusion

Reviewed the complete candidate diff, active Slice contract, runtime direction,
frozen DEBUG-DES-090..097, canonical headers, twelve channel documents, wire
schema/examples, vendored references, export/copy tools and build integration.
The bundle distinguishes implemented same-build legacy interfaces from target
CPU/session/board/debug declarations and target wire2.0/DAP attach behavior.
It does not claim a working module loader, attach runtime, complete simulator,
stable legacy ABI or external consumer adoption.

The interfaces preserve one CPU/session owner, exactly-once bus transactions,
post-access observation, deterministic input ordering, bounded IDLE progress,
explicit unsupported power-down, coherent pause/reset and independent debugger
attachment. Opaque board stimuli/inspection stay generic; board-specific schemas
remain external. UART frame and effective-pin coverage are capability-gated.
Legacy stdio1.0 and the frozen semantic model/cursor rules remain separate and
authoritative. Header relocation preserves licensing and C behavior; C++ linkage
guards and namespaced includes are the only changes to legacy declarations/use.

No remaining blocking findings were identified in the reviewed scope.

## Findings resolved before approval

| Finding | Issue and accepted correction | Status |
|---|---|---|
| REV-SLC-020-F001 | Opaque CPU table originally lacked a decoding seam for mandatory wire decoding. The CPU facade now owns a concrete bounded decode request/output API and authoritative predecessor knowledge. | Resolved |
| REV-SLC-020-F002 | Safe-boundary-only wording conflicted with recovery/inspection after an unsafe execution fault, and step from a pre-execution breakpoint was ambiguous. Nonexecuting fault recovery/allowed commands and one-instruction suppression for resume/step are explicit. | Resolved |
| REV-SLC-020-F003 | Integrity validation originally used removable Python assertions and lacked aggregate archive bounds. Production checks now raise explicit errors under optimized Python, bound archive members/expanded bytes, reject unsafe paths, and validate nonfinite JSON/UTF-8 byte limits. | Resolved |
| REV-SLC-020-F004 | Loopback TCP was incorrectly sufficient to imply per-account tunnel authorization. TCP now requires mTLS including loopback; an authenticated tunnel may instead forward a protected Unix endpoint with peer permissions. | Resolved |
| REV-SLC-020-F005 | Trace cursor wrong-session handling conflicted with generic stale-request wording, and page count could exceed the frame byte bound. Explicit cursor-state exceptions and whole-record byte-bounded pagination now preserve the frozen cursor/loss contract. | Resolved |
| REV-SLC-020-F006 | Include migration changed the tests' default Make target, lost mandatory includes under caller CPPFLAGS overrides, and did not default source-archive installation to SOURCE_COMMIT. All three build/provenance regressions are corrected. | Resolved |
| REV-SLC-020-F007 | Unknown wire commands with valid IDs required an error envelope that the strict schema could not represent. Only recoverable valid IDs with recognized commands receive INVALID; other malformed input closes without response. Negative fixtures cover the chosen rule. | Resolved |

Source/consumer feedback about INT6, normalized 16-bit ADC input, logical 9-bit
UART frames, drive/release, initial-cycle limitations and due-time RX rejection
is reflected in the final contracts. No future queue admission promises knowledge
of transient RX state at its later application time. Vendored DAP licensing is
included byte-for-byte with pinned source provenance.

## Independent verification

- `make interface-check` passed on the final candidate: isolated C99/C++17
  headers/consumer, strict examples and negative schema/UTF-8/uint64/nonfinite
  cases, reference/link integrity, deterministic export and copied-payload
  tampering/omission/addition/provenance/optimized-Python checks.
- Independently compiled all eleven canonical headers as standalone strict C99
  and C++17 translation units; the final bundle check repeated header coverage.
- `make -C tests test` and `make debug-test` passed all eleven CPU/event/trace
  suites plus debugger facade and process protocol tests at candidate `24f1268`.
  Later changes through `7d924cc` are documentation, manifest and validator
  fixtures only, so these unchanged product tests were not repeated.
- Independently compared existing production C files with the activation
  baseline: changes are namespaced public includes only. Verified each vendored
  reference against the exact local Git object named in its provenance record.
- Independently verified actual clean-commit export reproducibility, complete
  extracted-copy verification, source/manifest/archive lock mismatch rejection,
  archive path traversal rejection and the 512-member limit. Final candidate
  bundle checks cover the final payload; Master additionally reproduced final
  archive SHA-256 `4771155cf60148a3cd0bc6eb31c26b13bee913f3507c9fc174706e06f4c407bb`
  from a clean source archive.
- Verified tracePage schema includes all DES-094 identity/cursor/range/count/loss
  metadata and current/expired/ahead/wrong-session/wrong-epoch states. Decimal
  uint64 values are format-checked with an explicit upper bound; module payloads
  and text have additional UTF-8 byte validation beyond JSON Schema character
  limits. Runtime state-transition conformance remains a future implementation
  gate, not a claim made by fixture validation.
- Master reports successful clean-source build/install with caller CPPFLAGS,
  SOURCE_COMMIT fallback, installed SDK/protocol checks and offline export in
  `/tmp/emu-slc020-master-tj7z5pux/verification.log`. This is integration evidence,
  distinguished from the independent checks above.

Whitespace check passes with two narrow source-preservation exceptions:
`lib/include/emusa80535/emu8051.h` retains upstream trailing whitespace/EOF, and
`interfaces/0.1.0/reference/dap-LICENSE` retains its original EOF. The full-range
unqualified `git diff --check` reports those copied bytes; no blanket pass is
claimed. Neither exception changes licensing text or executable behavior.

## Acceptance boundary

Approval permits Master to accept and locally distribute this exact contract
payload, complete traceability and record its consumer handoff. It is not
acceptance of target runtime implementations, external board/DAP adoption,
whole-machine execution, RPM publication or an SDP migration. Those require
their separately recorded contracts and verification gates. An accepted bundle
payload is immutable; subsequent changes require a new bundle version.
