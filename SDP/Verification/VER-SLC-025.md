# VER-SLC-025 — Persistent local structured control

Date: 2026-10-28
Disposition: PASS
Accepted source:45b92abda4c88f56e69fff39441503d13ab64a59
Fresh review: REV-SLC-025 at9c0a3e80c261f9d34ef4e86742edf390a8452c46
Artifact pins: [SLC-025-ARTIFACT.json](SLC-025-ARTIFACT.json)

Session and Host Workers implemented separate owned files. The independent
Reviewer approved their combined committed candidate with F001/F002/F004/F005/
F006 resolved and F003 explicitly accepted as a documented finite implementation
capacity:8 concurrent clients and8192 distinct connection IDs over a session's
lifetime. No unbounded history or public monotonic-ID requirement was added.

Worker/Reviewer verification passed141 actual wire3 responses,1116 existing
wire2.1 responses, C99/C++17 compatibility/table consumers, real Unix mode/lease/
reconnect/pause/fault controls, strict framing/timeout/path/backpressure tests,
legacy script1 and debugger1.0, and59 independent adversarial process cases.
Native ASan/UBSan with leak detection passed. Actual UART peripheral failure,
delayed instructions, MOVX rejection and post-retirement board failure establish
truthful progress/retirement deltas. Frozen interface/header and archive checks
passed. Full details and findings are in REV-SLC-025.

Master independently created the exact clean source archive, extracted it
without Git and built/installed emu-runtime, all SDK libraries and emu-debug.
The installed-artifact checker passed CPU/bus/decode/runtime/debug consumers and
actual source identity. The initial checker invocation had an incorrect expected
commit argument; rerunning with the archive's actual SOURCE_COMMIT passed without
any product change. A separate installed-only C++17 consumer linked and called
the implemented control3 query with all four function pointers present.

The complete Unix process suite was then run against the **installed** executable
and passed. An additional installed-only ABI1 synthetic module and local handshake
probe recorded hello/attach(sessionId:null), paused startup, resume with zero
progress, exact8-cycle advance, pause and terminate. The live hello reports the
actual source, frozen0.3 manifest, wire3.0 and empty observation coverage. Its
recorded capability inventory distinguishes mode/module-dependent features.
Native and legacy script profiles remain compatible; --version names both.

Two independent source exports from the exact committed candidate were byte-
identical, archive SHA256:
6fdd3a0b1e6fb0922ccc1f74d8107fb060726f3ac0df3014bd14e8383ea2f361.
Durable archive, logs, real handshake and capability evidence are recorded in
SLC-025-ARTIFACT.json. The exact source worktree is /tmp/slc025-export-45b92ab;
Git-less build/install evidence originated in /tmp/slc025-master. Master checks
frozen directories/public headers, old archives, ledger prefix and debugger
aliases before acceptance. No RPM or network publication was performed.

Accept REQ-027's bounded persistent local control implementation. Observation,
UART outcomes, user trace configuration/pages, captures and file sinks remain
unavailable; their commands return UNSUPPORTED and are not advertised. Keep
accepted external V0 pins unchanged; this separately identified runtime is an
opt-in intake artifact. Next is canonical production and outcome retention under
a newly activated bounded SLC-026 contract, using the existing authorization.
