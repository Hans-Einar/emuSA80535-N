# SLC-034 build-accounting amendment

2026-10-29. Preparation only; subject to fresh harness review before compiler
execution. Complements reviewed plan9d7bdc46 and activation ca0c360. This amendment
explicitly overrides the plan's build-only cumulative-write wording as described
below; correctness and measurement cumulative-write budgets stay unchanged. No correctness
or measurement run is approved here. Master resolves the build supervisor choice;
Worker freezes exact argv/tool hashes/output paths before review.

The build trusts the frozen installed GCC/binutils tools and their documented
single-output behavior for the selected options. It is not containment against a
malicious compiler, kernel or arbitrary filesystem writer. No ptrace filesystem
interceptor or new cgroup subsystem is required for this bounded measurement.

Use direct, serial cc1, assembler and linker invocations, with no GCC/collect2
driver spawning hidden tool descendants. No LTO, compiler plugins, dumps, profiling
output, debug side files, dependency outputs or extra generated outputs. Metadata
queries may resolve exact tool/include/library paths without compilation; freeze
those commands and results too. Clear output-affecting tool environment variables.
No tool child is expected to spawn a descendant; detect unexpected descendants
as a failed build assumption, stop and retain evidence. Do not describe this as
adversarial hard process-tree containment.

The source closure pins project and generated sources, project headers, corpus,
direct tool executables and direct linker inputs. Transitive installed system
headers and the Python runtime are trusted environment dependencies; retain their
include roots and toolchain/package provenance, without claiming a fully pinned
system dependency closure. No broad system-header snapshot is required.

There are16 C units per variant: one fixture/session/corpus unit,15 other units
including the generic frontend. Each variant has16 cc1 stages,16 assembler stages,
one linker stage. CPU allocation maxima per variant:

- fixture cc1:10 seconds;
- remaining15 cc1 stages:3 seconds each;
- each of16 assembler stages:1 second;
- linker:4 seconds.

Sum75 seconds per variant,150 for both, within the original180-second build-role
budget. Hard RLIMIT_CPU per direct tool uses these integer maxima. Charge actual
completed user+system CPU and preallocate each concurrently active allowance;
no tools run concurrently. Thirty seconds remain unassigned, not automatic retry
authority. Failed/terminated stages consume their attempt and stop; Master may
assign an explicit bounded remaining-budget correction after fresh review.
External300-second hard-wall supervisor plus10-second cleanup applies to the
complete build; parent accounting/logging CPU is recorded separately.

Known output file-size limits: fixture assembly32MiB/object16MiB, each other
assembly2MiB/object1MiB, executable16MiB. Set RLIMIT_FSIZE to the exact file bound
per stage, use a distinct exclusive output path, and reject any other generated
file. Pre-reserve one assembly bound and twice each object/executable file bound
as conservative output-capacity credit. This totals156MiB per variant,312MiB for
both. Credit from failed attempts or replaced files is never refunded.

The512MiB build budget is a sum of reserved output-file extents across attempts,
source-generation writes and bounded captured logs/metadata. It is NOT a cap on
the number of bytes internally rewritten by assembler/linker syscalls or physical
storage I/O. The2x reservation is spare output credit, not a proved rewrite-volume
bound. RLIMIT_FSIZE enforces each expected regular file's length; frozen trusted
tool options define the output set and unexpected output is a build failure. This
explicit build-only scope correction avoids an unsupported cumulative-I/O claim.
Native fixture and measurement journals/results remain under true cumulative
prewrite accounting by their owned writer/launcher, as originally required.

Pre-reserve1MiB per tool stage for combined bounded stdout/stderr retention,66MiB
for both variants. Launcher captures output into capped buffers/files; on excess
stop that stage with retained truncation/failure status. Reserve4MiB additional
build/supervisor/failure metadata. Existing source preparation is charged at its
actual prewrite ledger cost (currently about23MiB), plus every later source revision.
The total must stay below512MiB before launching any tool; refuse on insufficient
remaining credit. All accepted source and failed preparation revisions remain.

Reviewer must inspect actual build ledger, tool options, output paths, file write
assumptions and cleanup before build approval. Successful binaries then receive
hashes in the separate exact correctness/measurement freeze. This amendment
changes no product/runtime interface, pin or simulation behavior.
