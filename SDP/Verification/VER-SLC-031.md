# VER-SLC-031 — Opt-in CODE breakpoint enumeration

Date:2026-10-28. Disposition:PASS. Product:
59f7a5c769a4fbf9eb73d0d660e94c58534c1c9c (Runtime53106b2, Contract59f7a5c).
Fresh independent review:904ff730cbfb6565d1bcc54f8766582c41c56ae7,
REV-SLC-031 approved. Exact runtime/sidecar pins:SLC-031-ARTIFACT.json.

Consumer concurred the explicit3.1 proposal before implementation. Attached
observer/controller can read authoritative CODE bitmap with same-snapshot
sessionId/generation and sorted unique integer addresses0..65535, max1024.
Enumeration advances no CPU cycles/instructions, stop revision, breakpoint skip
state, trace/outcome counters or file output. It is not CAS; consumer must retain
exclusive controller lease for snapshot/temporary replace/restore guarantees.

Old3.0 remains exact0.3 hello/manifest/capability/schema and rejects the new command
as before. New minor3.1 and wire-only0.4 require explicit hello; no fallback.
CPU/board/native APIs and CPU/UART/reset behavior are unchanged. Trace files opened
through3.1 still use frozen0.3 metadata. No running user lab or consumer pin changed.

Independent exact-candidate proof:82 Unix enumeration replies plus60 active
trace/file replies, idle/power-down probes, native capacity/owner/prefix and
ASan/UBSan/leak checks. Worker104 actual Unix replies/10base JSONL reproduced.
Tests cover empty/unsorted replace/sorted read/clear/reset, upper endpoint and
1024 entries, running/paused/faulted, observers with held controller lease,
foreign session/unattached/malformed/terminated, exclusive temporary replacement,
restore and exact breakpoint skip-once invariance. Old181 wire3 and1116 wire2.1
native replies, Unix/script/outcome/A/B/file regressions pass. Frozen0.3 archive
re-export retains405f22f7fc7e0a49d921715102102a4d667ba1ff84b1238494d76353feeb0f53.

Wire-only sidecar has12 exact inventoried payloads. Worker71 focused checks and
Reviewer49 independent negatives pass, including strict integer-vs-bool/sort/
identity/frame/unknown fields, poisoned import isolation, tamper/unsafe paths,
standalone copy and deterministic export. Baseline validators are exact immutable
0.3 copies; no old manifest, tool registry or public header is modified.

Master exported exact source and sidecar twice byte-identically. Runtime archive:
9186cfebe71bafa94fe3e51e672d8ef4614fb2c8428b4f6d06c94cfd3af07245.
Sidecar archive:084218bcfc7cde3f43d716087cfce05268a9b9bd8547fd730a71467242a63c92.
Sidecar manifest:80932dd18f883438074b37c167828484b8cf6f95ce74a309526b6b709c60383a.
Fresh Git-less build/install passes installedSDK/debug checker, C++17 installed
headers/static-library consumer and actual installed104 replies/10JSONL. Module
fixture compiles only installed headers. Actual3.0 and3.1 hellos identify exact
source and their respective immutable manifests; capability stays profile-scoped.
Standalone and installed sidecar71 checks pass. Real acceptance lock selects the
exact same sidecar bytes even under a simulated different runtime DEBUG_COMMIT.

interfaces/control31.lock.json freezes accepted contract source/manifest/archive
outside the hashed payload, avoiding self-reference. Later runtime provenance
remains separate; copied receipts cannot be rewritten. Durable runtime artifact
verification/ contains logs, actual replies/files and hash-verified review proof.
REQ-030 satisfied; full ACCESS remains absent under REQ-028. No UART enhancement,
physical IO, new consumer frontend, TCP/DAP or RPM publication is claimed.
