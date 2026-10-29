# VER-SLC-028 — One observed dynamic invocation

Date:2026-10-28. Disposition:PASS within coverage125 boundary.
Product:8dd1230704cd6c460167b954f7e987c23e24ff86, including Context Worker9f48cfe.
Fresh independent review:2f748dbd222354b9665e0f09a19d788aa238c174,
REV-SLC-028 approved. Exact artifact inventory:SLC-028-ARTIFACT.json.

Fresh Context and Invocation Workers implemented the active contract. Mode1 B
binds only after an observed CALL retires; mode2 requires the known committed
current top CALL frame. Typed CALL/IRQ tracking includes nested and recursive
work regardless of trace filtering. Root RET plus retirement closes the extent;
caller continuation is excluded. Unknown/mismatch/overflow/fault/reset/load and
limits remain explicit incomplete outcomes. A fully observed child may recover
above unknown ancestry with parent0; returning it cannot recover old ancestors.

Per-destination bounded staging preserves original coalesced A+B route sets,
gates and order. CALL abort/overflow removes only candidate B routes, without
recounting A or fabricating successful binding. Limits,64 retained statuses,
explicit reset successor/rearm exhaustion and original binding generation pass.
Independent review found and resolved F001 failed-candidate suppression leakage
and F002 loss of pre-arm same-trace suppression. Selected flush after complete
start validation preserves old history; only failed candidate counters discard.
Native fixtures verify partial stage, BUSY barriers, prefixes/untouched tails,
mode2/typed context, source identity at staged stops, load and suppression.

Exact detached candidate passes core/native/trace/control/legacy suites,181
wire3 and1116 wire2.1 responses,109 persistent-A replies,55 file replies/202JSONL,
11 independent UART outcome replies and both immutable SDK checks. Independent
Reviewer exercises594 actual schema/semantic replies and76 JSONL lines, including
CALL-tail failure,300-source stage pressure, recursion, IRQ policies, reset/rearm
exhaustion, wrong SP/target/type, top-only binding and shared A+B file limits.
Native and CPU ASan/UBSan/leak checks pass. Review source assessment finds no
new file IO or per-event allocation in CPU/owner callbacks.

Master exported exact product twice with byte-identical archives, SHA256
0503bdf512adff41d97ac300ec46e0e60f642dda9aba22d444d46ca6b7a33f2f. A fresh Git-less extraction
built runtime, SDK and legacy debugger, then installed in isolation. Installed
checker and C++17 installed-header/static-library consumer pass. Actual installed
Unix invocation test validates173 responses and48JSONL, including mode1/mode2,
limits/reset, filtered closure, real fault, stop precedence and shared A+B file.
The board fixture is compiled only against installed headers. Actual hello
identifies exact source and captureB; file capability is startup-provider-only.
Durable artifact verification/ contains actual replies, JSONL and review evidence.

Frozen SDK0.1/0.2/0.3, CPU API1, board ABI1 and old artifacts remain immutable.
REQ-029 tracing/capture/file behavior is satisfied within explicitly advertised
coverage. REQ-028 remains partial: full ACCESS is not implemented. General IRAM/
CODE/SFR access selection remains unsupported; actual partial XDATA/SBUF facts
stay available. This gate does not claim real-time throughput, whole-machine
behavior, physical IO, TCP/DAP implementation or package publication. External
composed intake follows capability handoff without changing accepted V0 pins.
