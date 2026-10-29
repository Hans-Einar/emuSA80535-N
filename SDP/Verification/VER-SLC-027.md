# VER-SLC-027 — Persistent capture and asynchronous trace files

Date:2026-10-28. Disposition:PASS within explicit coverage boundary.
Product:740f60863bc52481e589f219ba54df8f5bd0fc82.
Fresh independent review:135be69fd2039020e52301dc20ef5ddcadfa79c6,
REV-SLC-027 approved. Exact artifact inventory:SLC-027-ARTIFACT.json.

Fresh Trace and File Workers implemented the active contract. Existing canonical
producer, matcher, gates and coalescing/router feed stable trace storage and the
bounded asynchronous file provider. Capture A persists across calls, returns,
pause, reset and reconnect; limits and explicit stop are observable. Capture B
remains unsupported. Observation coverage125 deliberately excludes full ACCESS;
explicit XDATA READ/WRITE and SBUF WRITE/RMW predicates are supported, with other
access coverage rejected atomically. Broad traces may retain partial facts.

Independent review resolved boolean/prefix/stride/stopDetail/canonical-space/
storage-exhaustion/writer-state findings. Its exact-candidate native, all-opcode,
legacy, control/outcomes, 181 wire3 and1116 wire2.1 checks passed. Independent
signed/unknown/IRAM-engine/prefix/odd-stride probes, held writer/queue/flush/loss/
termination and ASan/UBSan/leak checks passed. Both frozen SDK checks passed.
Review evidence and limitations are detailed in REV-SLC-027.

Master exported the exact reviewed product twice with byte-identical archives.
SHA256:dd7203ff3aff63f8305660ee1e5311b8d829efd5323d8a9c694803840ab0c7ef.
A fresh Git-less extraction built runtime, SDK and compatibility debugger and
installed into an isolated root. Installed-artifact checker passes. C++17 built
using only installed headers/libraries resolves observation, trace and control3
extension tables, coverage125 and actual trace facade error handling.

Installed Unix process tests, with board fixtures compiled only against installed
headers, validate109 trace responses plus55 file responses and202 actual JSONL
lines. These cover shared coalescing, calls, watches, limits, reset, ownership,
stable pages, reconnect, async barriers, rotation, reopen and termination.
Actual hello reports the exact product commit, traceConfig/tracePage/captureA,
and traceFile only when startup token registration is present. Archive, logs,
actual responses/files and independent review evidence are retained durably beside
the artifact under verification/.

Frozen SDK0.1/0.2/0.3, CPU API1, board ABI1 and prior artifacts remain unchanged.
File sinks use startup opaque tokens, private directories and exclusive0600
segments; no wire paths. Queue256, async error/loss and bounded shutdown are
explicit. Full ACCESS, invocation capture B, TCP/DAP and RPM publication are not
claimed. REQ-029 remains partial pending invocation capture; REQ-028 stays partial.
