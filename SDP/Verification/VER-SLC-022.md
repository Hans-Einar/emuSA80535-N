# VER-SLC-022 — Local composed runtime

Date: 2026-10-27
Disposition: PASS; reviewed candidate a03504bd186efe3a8882bd3f1cdb5c4cf463f30d
Review: REV-SLC-022 at f999ce9; F001..F009 resolved

The two Workers and separate Reviewer verified the opaque-CPU session, ordered
mixed input queue, exactly-once boundaries, IDLE wake, reset/generation, partial
fault preservation, controller leases, bounded native validation and wire2.1
responses. 1116 actual native responses pass the immutable schema. Old SDK0.1
shared modules exercise the actual loader, bus/port events, input order, replay,
fault inspection and descriptor rejection. ASan/UBSan with leak detection pass.
Legacy CPU, event/trace, debugger facade and protocol1 gates remain passing.

Independent review includes adversarial callback/JSON/identity/output-size probes,
C99/C++17 installed-only linkage and clean staged installation. The exact checks,
resolved findings and remaining limits are recorded in REV-SLC-022. The fixed
inspection metadata interpretation accepts both actual-length and ABI minimum-
capacity required counts; the preceding candidate957dd9c was not accepted.

Master independently exported the exact approved source in a clean checkout,
built the extracted archive without .git and passed native/session/host/schema
checks using the separately verified immutable0.1 SDK for fixture compilation.
The source archive hash agrees with independent Reviewer export. Both frozen
bundle directories and public contract headers are unchanged; ledger prefix and
source-scoped aliases remain intact. SLC-022-ARTIFACT.json records the durable
archive, exact identity and retained independent evidence hashes.

Master and Reviewer also exercised the accepted external board binary. A longer
Master ROM probe reaches a reproducible typed unsupported access, allows pure
post-fault inspection and repeats after reset. This preliminary interoperability
evidence is retained with the artifact; domain acceptance and the external
pinned build/launcher are the active SLC-023 gate. No full boot claim follows.
No RPM was rebuilt/published and SDP migration remains deferred.
