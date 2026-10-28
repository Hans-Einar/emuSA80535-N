# Bounded host JSONL sink

A file destination is an owner-authorized regular file selected by an opaque
ASCII destinationToken (1..64 bytes), never a protocol path. At startup the host
registers at most8 token-to-path mappings through its embedding configuration;
no wire command creates or changes mappings. Only same-UID private directories,
new files O_CREAT|O_EXCL|O_NOFOLLOW mode0600, and regular files verified by fstat
are allowed. No FIFO/device/socket/stdout/stderr or symlink traversal. Native
sink_open uses the same token policy. Invalid token => DENIED, existing file =>
IO without truncation; no directories are created by control commands. The host
embedding owns token/path setup; this contract defines no extra CLI grammar for
it. CPU/board callbacks only enqueue bounded immutable routed records.

One sink per file destination. sinkOpen requires configured kind=file and
capacity256, closed/no existing sink, current lease/generation, token and limits.
maxSegmentBytes is canonical uint64 string in65536..1073741824;
maxSegments1..16; maxTotalRecords1..uint64. File format is UTF-8 LF JSONL using
trace-file.schema.json. Each segment starts with header, contains record lines,
ends with footer if possible. Every segment repeats pinned protocol3.0
contractVersion0.3.0, runtimeCommit, contractManifestSha256, sessionId, image/module/config
hashes, traceConfigSha256, generation, destination and full opening config.
`traceConfigSha256` hashes canonical config: UTF-8 JSON, keys sorted recursively,
compact separators, no ASCII escaping, arrays in defined ID order. Header's
configuration is a snapshot at sink opening. To keep configuration identity
reproducible, this profile rejects replaceTraceConfig while any sink is open
(BUSY); close, replace, then reopen. Reset/load do not alter configuration.


Native event facts are rendered with event schema, including source generation,
sequence, instruction/access/input/frame/context IDs, actual cycles/phases,
captured bytes/text, values and status. Suppression, loss and sink-status records
are typed and independently storage-sequenced. Record encoding <=3072 bytes;
file header/footer lines <=65536. A source record that exceeds encoded bound
is rejected as explicit sink loss/IO status, never split/truncated silently.
Queue is fixed256 records including summaries; when full, drop incoming views,
coalesce pending lost count/source range, emit one loss record before the next
successfully queued ordinary view or on flush/close. Pending summary storage is
fixed metadata, not another unbounded queue. No protocol stdout trace output.

File work runs only in host sink service outside CPU callbacks and outside the
safe execution owner critical section. A slow disk never changes virtual timing;
queue overflow is explicit loss. Writer may block in host I/O, but API callbacks
and control requests never wait on disk. Sink failure disables only that
destination and records sinkStatus error=IO,partialOutput=true; it neither clears
trace.enabled nor raises a fake hardware exception or resets the CPU. A requested
stop-on-limit is a separate capture stop policy, never implicit disk failure.

Rotation uses ordered filenames `<registered-path>.0000.jsonl` through `.0015.jsonl`.
Registered path is a stem, not an already-open stream. All segments O_EXCL;
never replace older files. segment0 header counts toward maxSegmentBytes. Before
writing each record reserve4096 bytes for footer; rotate if adding it would
exceed maxSegmentBytes. Close prior segment with complete=true,status=OK, open
next with new header before the record. If header+record+reserved footer cannot
fit, fail IO/partialOutput. Header size validation occurs before opening first
file. No wall-clock-based rotation. On last allowed segment exhaustion or
maxTotalRecords, close complete=true,status=CAPACITY; discard subsequent records
with explicit lost count in sinkStatus. Footer records counts ordinary and
summary/status record lines in that segment; bytes in sinkStatus counts actual
written bytes including headers/footers across all segments. maxTotalRecords
counts all record lines, excluding headers/footer. Partial write bytes count
only bytes actually written; incomplete JSON line is valid partial-output evidence,
not a parseable footer. No claim that a footer always exists after I/O failure.

sinkFlush/sinkClose return immediately with a new monotonically increasing
operationId. completedOperationId is highest completed request for that sink;
requested != completed is pending, never proof of durability. Flush drains the
queue as of request, emits pending summaries, flushes OS buffers with fsync, then
completes without closing. New records after barrier may queue behind it. Close
stops new enqueue for this destination at its request boundary, drains accepted
records, writes footer, fsyncs and closes; completion means handle closed, or
failure terminal with partialOutput. Sink states0 open,1 flush-pending,2 close-
pending,3 closed,4 failed. At most one pending operation per sink; another =>BUSY;
close already closed succeeds returning last operation status. sinkStatus polling
is nonmutating and reports operation/completion IDs, queued count, bytes, record
count, lost count, segment, state, partialOutput,error. Failed operation sets
completedOperationId to requested operationId and error=IO, not success.
Open allocates operationId1 and is complete only after header is written;
the open request returns state1
(flush-pending) with completedOperationId0 until header write completes, then
state0/open completedOperationId1. No data is routed until that header completes;
earlier views count as explicit loss. Subsequent IDs never wrap. Validation
failure allocates no operation ID. A new successful open on the same destination
continues operation IDs and resets per-open byte/record/loss counters.

Disconnect does not close a sink. Reset/load queues lifecycle records and
preserves A/file configuration; B ends per scope rules. terminate requests close
for all sinks and reports BUSY until all complete or fail, then terminates;
no hidden indefinite synchronous wait. Shutdown with queued records or a failed
footer reports partialOutput and host diagnostic; it must not claim successful
capture. Consumers validate parsed prefixes and report missing footer as partial.
