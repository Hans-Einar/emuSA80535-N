# VER-SLC-026 — Canonical production and operational outcomes

Date:2026-10-28. Disposition:PASS within explicit coverage boundary.
Final source:af73adf974731afe32fdb2be806e1557a0804f7c.
Fresh review:cfa586dc9e9dd51d775076d5f06701cc817cb6fd, REV-SLC-026 approved.
Exact artifact/capability inventory: SLC-026-ARTIFACT.json.

Fresh SDK, CPU and Controller Workers implemented the bounded Slice; independent
Reviewer resolved F001..F006. Native SAB80535 observation coverage125 includes
instruction/control/IRQ/lifecycle/input/UART. Full ACCESS is deliberately absent;
actual MOVX and SBUF write/RMW facts are available without completeness claims.
Other CPU variants retain compatibility execution and reject this binding.

Instruction lifecycle follows actual begin, captured operand reads and successful
full timing completion, or abort with partial effects. No unsupported MOVX is
reported retired. Controller uses the existing semantic bus/matcher/router for
canonical sequence and full uint64 generation. Board/CPU admission and UART
hardware acceptance stay distinct, with immutable parent/order/frame identity.
Operational outcomes survive disabled user tracing and reset in fixed1024
storage records, including explicit loss summaries and stable cursor metadata.

Worker and independent tests cover all256 opcode observer-on/off equivalence,
captured bytes, partial/tail faults, call/IRQ contexts, unknown/mismatch/overflow,
reset and uint64 identity exhaustion before reuse, all12 interrupt request
sources, UART REN/busy/RI/SM2, holding replacement, actual TI edges and frame
phases. Required due-input rejection retains prior fault/no-retry policy. Input
admissions within CPU callbacks use captured pending instruction context without
CPU reentry. Stop detail retains real abort/retire identity without user output.

Independent Reviewer validates1736 actual CPU records against frozen schema and
semantic rules; ASan/UBSan/leak probes pass. Final detached-source core/native,
181 wire3 responses,1116 wire2.1 responses, ABI/link, legacy debug, Unix control,
11 actual Unix outcome responses and immutable SDK checks pass. Ring probes
exercise all cursor states, metadata-only, capacity without partial writes,
overwrite including loss records, nested prefixes, larger strides and exhaustion.

Master exported the exact reviewed source twice with byte-identical archives:
a19b70db053c60d4396250baf72c7fbd9179cf7812444a582142dda76ab7fb93.
A fresh Git-less extraction built runtime, SDK and compatibility debugger, then
installed successfully into an isolated staging root. Installed-artifact checker
passes. An independently compiled C++17 consumer using only installed headers
and libraries resolves all three extension tables and coverage125. The installed
Unix executable, using a board fixture built only against installed headers,
passes the actual11-response outcome/schema/semantic test, RX/TX correlation,
independent readers and reset retention. Actual hello identifies the exact source.
Evidence is retained beside the durable artifact under verification/.

SDK0.1/0.2/0.3 payloads/manifests and prior artifacts remain immutable. Historical
raw headers/tools are snapshotted for exact Git-less reconstruction; original SDK
source receipts remain distinct from new runtime provenance. Source-scoped DEBUG
aliases and historical ledger prefix are preserved. This gate does not implement
trace configuration, general trace pages, A/B capture, file sinks, DAP/TCP or RPM
publication. REQ-028/029 remain partial until later explicit coverage/work gates.
