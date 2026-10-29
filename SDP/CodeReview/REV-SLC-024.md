# REV-SLC-024 — Persistent control and observation contract gate

Date: 2026-10-28
Reviewer: fresh independent `observation_contract_reviewer`
Disposition: **APPROVED** for contract-only SLC-024
Reviewed candidate: `2133e7220284bb8dc1600ba98b2d842106a7dba4`
Contract baseline: `f371023`
Bundle: `0.3.0`; target control3.0; observation extension1; trace facade1
Manifest SHA-256: `ed3917b0d3a7300aca94171f1259eacf9861b0ee87376afa534690618da27ff0`

## Scope and conclusion

Read the active index, Sprint contract/handoff, mandate, runtime source study
and architecture, applicable requirements/design and source-scoped
DEBUG-DES-064..097. Inspected the new public declarations, schemas, generated
fixtures, semantic validator, checker, version/export integration and complete
candidate change inventory. Reviewed the evolving implementation independently;
approval is pinned to the committed complete candidate above.

The bundle freezes bounded target interfaces while preserving the implemented
CPU API1, board ABI1, SDK0.2/wire2.1 identity and existing runtime source. Missing
target query symbols and zero implementation markers explicitly mean unavailable.
The native trace table's partial convenience surface and control3 JSON bridge
are documented without promising additional native functions. New records use
sized fixed-width prefixes and distinct observation access masks. No private CPU
cast, parallel matcher, physical I/O or external board implementation is added.

Control contracts distinguish external advance/step from resume, preserve
lease/generation barriers and bounded progress, and retain the existing safe
stop priority. Observation contracts distinguish attempted effects from full
instruction retirement, parent board outcomes from CPU delivery, and UART
acceptance from admission. Trace contracts preserve canonical identity separately
from storage sequence, explicit loss, enabled state, invocation binding and
destination coalescing. File contracts provide host-owned tokens, bounded queues,
explicit failure and asynchronous completion barriers outside CPU callbacks.

No remaining blocking findings were identified for this declaration/schema gate.

## Findings resolved before approval

| Finding | Correction verified in candidate | Status |
|---|---|---|
| REV-SLC-024-F001 | Early draft retirement preceded peripheral/delay completion. `docs/02-observation.md` now keeps the instruction pending through its complete timing tail and aborts on failure; legacy instruction count remains distinct. `docs/04-trace.md` confirms closing return only at retirement and closes a candidate CALL abort incomplete/fault, including before invocation binding. | Resolved |
| REV-SLC-024-F002 | Initial IRQ mapping excluded supported INT2..INT6 and access tags risked confusion with CPU API1 directions. Observation headers/docs/schema now define source0..11, reject reserved sources, and use separate OBS_ACCESS constants. | Resolved |
| REV-SLC-024-F003 | Initial CPU admission API competed with session queue/order ownership and omitted parent-only board failures. The extension now delivers a controller-assigned correlated input at its due cycle; inputStage distinguishes board outcomes from CPU outcomes, including autonomous parent0 inputs. Existing ABI1 callbacks gain no unnegotiated kinds. | Resolved |
| REV-SLC-024-F004 | Stop priority and externally queryable watch evidence were incomplete. Control docs restore breakpoint-before-watch priority; stopDetail retains source/instruction identity and bounded simultaneous watch matches independently of trace storage, including entry/step reasons. | Resolved |
| REV-SLC-024-F005 | One-record overwrite rings and invisible stop-on-full drops could conceal loss. Rings require capacity at least2, freeze pending marker behavior, and expose separate saturated overwritten/dropped counters in native/wire page metadata. Clear, epoch, exclusive cursor and invalid cursor states are explicit. | Resolved |
| REV-SLC-024-F006 | Reset rearm allocated an undiscoverable scope and delayed CALL binding could clone/reorder shared-destination records. Capture status now exposes successorCaptureId/rearmStatus; bounded staging preserves original gates, coalesced trace IDs and source order per destination. Scope filtering remains separate from enabled state; incomplete/terminal scopes never silently resume ordinary tracing. | Resolved |
| REV-SLC-024-F007 | Closed capability requests contradicted unknown-capability negotiation; draft validation also rejected allowed escaped NUL inside opaque board JSON. Required capabilities now accept bounded future names for UNSUPPORTED negotiation, successful hello requires mandatory capabilities, and C-string restrictions do not alter opaque payload semantics. | Resolved |
| REV-SLC-024-F008 | Initial semantic checks missed failed retirement, unknown fields, nested file records and cursor/generation correlations. Target validator/checker now exercise these cases, full IRQ mapping, board/CPU stages, failed access outcomes and required capability dependencies. Independent adversarial reproductions verify the identified gaps are closed. | Resolved |

The profile explicitly limits configuration conditions to old/new scalar
comparisons; broader expressions/delta remain unavailable. Native convenience
operations not present in trace_api use the serialized control3 bridge. These
declared limits do not assert that all historical target debugger features are
implemented.

## Independent verification

- `make interface-target-check` passed against the exact committed candidate.
  It checks every target header in isolated C99/C++17 compilation, native prefix
  and width assertions, every command request/success shape and common errors,
  all28 observation kinds and four stored record kinds, semantic negatives,
  deterministic standalone exports, consumer-lock/tampering negatives and
  exact accepted SDK0.2 archive reconstruction.
- Independently constructed invalid observations with successful-retirement kind
  plus FAULT status, or unknown PC plus nonzero value, were rejected. Nested file
  event generation mismatch, an ahead cursor mislabeled current, missing
  mandatory hello capability and reserved IRQ source12 were rejected. Supported
  IRQ source11, autonomous CPU input parent0 and opaque payload escaped NUL
  remained valid.
- `git diff --exit-code f371023 2133e72 -- interfaces/0.1.0
  interfaces/0.2.0 lib/include/emusa80535/contracts/version.h` passed. Old bundle
  directories and implemented version identity are unchanged.
- The candidate whitespace check passed excluding byte-preserved historical
  payload snapshots. The final candidate tree was clean before adding this
  review record.
- The agreed external reply still hashes to
  `08c26d70c3198c13da4312f0e31154ee3d80d684c4df541744e886f65cddad00`;
  its contents were not revised by the contract corrections.

These are contract/compile/export checks, not runtime producer, listener,
capture or file-writer tests. The conformance matrix retains those behavioral
obligations for later separately authorized implementation Slices. Master owns
the exact-source build/install verification, durable artifact pins, traceability,
Slice acceptance and external handoff. This review does not authorize those
future implementations, RPM publication or SDP migration.
