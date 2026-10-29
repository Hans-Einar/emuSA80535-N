/* Linux host-owned asynchronous trace sinks. Copyright 2026 Hans-Einar. MIT. */
#ifndef EMU_RUNTIME_FILE_SINK_H
#define EMU_RUNTIME_FILE_SINK_H
#include "emu_trace_file_internal.h"
#include <stddef.h>
struct runtime_files;
/* Specs are TOKEN=/absolute/private/directory/stem. Directories are pinned
 * before any module/CPU starts; no output file is opened by creation. */
int runtime_files_create(const char *const *, size_t, struct runtime_files **);
void runtime_files_provider(struct runtime_files *,
                            struct emu_trace_file_provider *);
/* Requests closure and waits at most timeout_ms outside the execution owner.
 * On timeout the independent writer/context remain alive until process exit;
 * callers must report partial output and must not reuse the backend. */
int runtime_files_destroy(struct runtime_files *, unsigned timeout_ms);
#ifdef RUNTIME_FILE_TESTING
void runtime_files_test_hold(struct runtime_files *, int);
void runtime_files_test_hold_after_flush(struct runtime_files *);
void runtime_files_test_hold_capacity(struct runtime_files *, int);
void runtime_files_test_sequence(struct runtime_files *, uint32_t, uint64_t);
void runtime_files_test_fail_after(struct runtime_files *, uint64_t);
void runtime_files_test_fail_fsync(struct runtime_files *, int);
#endif
#endif
