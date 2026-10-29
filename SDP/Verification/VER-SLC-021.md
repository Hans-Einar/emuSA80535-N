# VER-SLC-021 — CPU API1 implementation

Date: 2026-10-27
Disposition: PASS; reviewed candidate0a77c2d0da22ecb0bd37781b430e30444f779e39
Review: REV-SLC-021 approved at f9266f3; F001..F006 resolved

Worker and independent Reviewer verified every mandatory CPU facade operation,
shared authoritative decode/cache, sized/versioned buffers, ordered callbacks,
fault propagation/reentrancy/recovery, per-cycle input ordering/reset floor,
IDLE wake, power-down refusal, UART profile and sequence exhaustion. Reviewer
swept256 opcode slots with32 callback-triggering paths and observed no later
architectural mutation after rejection; illegal opcode fault retains its PC.
The facade suite passed ASan/UBSan. Full CPU stage/event/trace/runtime and legacy
debug facade/protocol1 tests passed against the committed candidate.

Clean-source CPU-only/library/debug builds, installed-only consumers, C99/C++17
linking, actual0.1-header shared module query/access and legacy-header CPU API1
execution passed. The raw legacy CPU struct grows and requires same-build
recompilation; this does not change native boardABI1.0 or CPU API1 table layout.
Reviewer evidence: /tmp/slc021-review-final-ch_wb7mx (ephemeral logs).

Master independently exported exact candidate source without .git and verified
archive identity equal to Reviewer's export. SLC-021-ARTIFACT.json records the
source/manifest/archive hashes and durable local path. interfaces/0.1.0 and its
accepted archive remain unchanged. Exported0.2 documentation/tools are self-
contained. New whitespace passes except the exact copied DAP license EOF.
Master checks traceability paths/YAML/ledger/aliases and immutable payload before
handoff. No new RPM build or network publication was performed.

Accepted: CPU API1 implementation and immutable bundle0.2.0 including target
wire2.1/post-fault inspection. CPU supports PORT_LATCH for all three variants and
UART_FRAME only for SAB80535 mode3; session must enforce applicability. Effective
pin output is not claimed. Session/controller, module loader, host, sockets and
DAP remain future work. SLC-022 activates the next bounded generic runtime step.
