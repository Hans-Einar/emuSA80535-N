/* Private host backend adversarial tests. Copyright 2026 Hans-Einar. MIT. */
#define _GNU_SOURCE
#include "emu_sha_internal.h"
#include "runtime/file_sink.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define INIT(x)                                                                \
  do {                                                                         \
    memset(&(x), 0, sizeof(x));                                                \
    (x).struct_size = sizeof(x);                                               \
    (x).api_version = 1;                                                       \
  } while (0)
struct fixture {
  struct runtime_files *files;
  struct emu_trace_file_provider p;
  char directory[512];
  char specs[8][640];
};
static char config_json[1024], config_hash[65];
static const char hash[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char commit[] = "0123456789abcdef0123456789abcdef01234567";
static void wait_ms(void) {
  struct timespec d = {0, 1000000};
  nanosleep(&d, NULL);
}
static void start(struct fixture *f, const char *root, const char *name,
                  unsigned tokens) {
  const char *specs[8];
  unsigned i;
  memset(f, 0, sizeof *f);
  snprintf(f->directory, sizeof f->directory, "%s/%s", root, name);
  assert(!mkdir(f->directory, 0700));
  for (i = 0; i < tokens; i++) {
    snprintf(f->specs[i], sizeof f->specs[i], "t%u=%s/capture%u", i,
             f->directory, i);
    specs[i] = f->specs[i];
  }
  assert(runtime_files_create(specs, tokens, &f->files));
  runtime_files_provider(f->files, &f->p);
}
static struct emu_trace_file_metadata metadata(uint32_t dest,
                                               uint64_t generation) {
  struct emu_trace_file_metadata m = {0};
  snprintf(config_json, sizeof config_json,
           "{\"destinations\":[{\"capacity\":256,\"destinationId\":%u,\"kind\":"
           "1,\"stopOnFull\":false}],\"gates\":[],\"points\":[],\"traces\":[{"
           "\"comment\":\"\",\"destinationId\":%u,\"enabled\":true,"
           "\"interruptPolicy\":0,\"tag\":\"\",\"traceId\":1}],\"watches\":[]}",
           dest, dest);
  sha256_hex((const uint8_t *)config_json, strlen(config_json), config_hash);
  m.runtime_commit = commit;
  m.contract_manifest_sha256 = hash;
  m.session_id = "0123456789abcdef0123456789abcdef";
  m.image_sha256 = m.module_sha256 = m.config_sha256 = hash;
  m.trace_config_sha256 = config_hash;
  m.configuration_json = config_json;
  m.configuration_bytes = (uint32_t)strlen(config_json);
  m.generation = generation;
  return m;
}
static struct emuif_sink_request request(uint32_t dest, unsigned token,
                                         uint64_t generation, uint64_t records,
                                         unsigned segments) {
  struct emuif_sink_request r;
  INIT(r);
  r.destination_id = dest;
  r.generation = generation;
  r.max_segment_bytes = 65536;
  r.max_segments = segments;
  r.max_total_records = records;
  snprintf(r.destination_token, sizeof r.destination_token, "t%u", token);
  return r;
}
static struct emuif_sink_status status(struct fixture *f, unsigned dest) {
  struct emuif_sink_status s;
  INIT(s);
  assert(f->p.status(f->p.context, dest, &s) == EMUIF_OK);
  return s;
}
static struct emuif_sink_status wait_operation(struct fixture *f, unsigned dest,
                                               uint64_t op) {
  struct emuif_sink_status s;
  unsigned i;
  for (i = 0; i < 5000; i++) {
    s = status(f, dest);
    if (s.completed_operation_id >= op)
      return s;
    wait_ms();
  }
  assert(!"operation timeout");
  return s;
}
static struct emuif_sink_status open_sink(struct fixture *f, unsigned dest,
                                          unsigned token, uint64_t records,
                                          unsigned segments) {
  struct emuif_sink_request r = request(dest, token, 1, records, segments);
  struct emu_trace_file_metadata m = metadata(dest, 1);
  struct emuif_sink_status s;
  INIT(s);
  assert(f->p.open(f->p.context, &r, &m, &s) == EMUIF_OK);
  assert(s.state == EMUIF_SINK_FLUSH_PENDING &&
         s.completed_operation_id + 1 == s.operation_id);
  return wait_operation(f, dest, s.operation_id);
}
static struct emuif_trace_record event(unsigned dest, uint64_t sequence,
                                       uint64_t generation) {
  struct emuif_trace_record r;
  INIT(r);
  INIT(r.event);
  INIT(r.event.old_value);
  INIT(r.event.new_value);
  r.event.old_value.width = r.event.new_value.width = 8;
  r.destination_id = dest;
  r.generation = generation;
  r.kind = EMUIF_RECORD_EVENT;
  r.route_count = 1;
  r.trace_ids[0] = 1;
  r.event.generation = generation;
  r.event.sequence = sequence;
  r.event.cycle = sequence;
  r.event.instruction_id = sequence;
  r.event.begin_cycle = sequence;
  r.event.kind = EMUIF_OBS_INSTRUCTION_BEGIN;
  r.event.known = EMUIF_OBS_KNOWN_PC;
  r.event.byte_count = 1;
  strcpy(r.event.text, "NOP");
  return r;
}
static struct emuif_sink_status operation(struct fixture *f, unsigned dest,
                                          int close, uint64_t generation) {
  struct emuif_sink_status s;
  INIT(s);
  assert(f->p.operation(f->p.context, dest, (unsigned)close, generation, &s) ==
         EMUIF_OK);
  return wait_operation(f, dest, s.operation_id);
}
static void emit_events(struct fixture *f, unsigned dest, unsigned first,
                        unsigned count) {
  unsigned i;
  for (i = first; i < first + count; i++) {
    struct emuif_trace_record r = event(dest, i, 1);
    f->p.enqueue(f->p.context, &r);
  }
}
static void basic(const char *root) {
  struct fixture f;
  struct emuif_sink_status s;
  struct emuif_trace_record r;
  struct emuif_sink_request q;
  struct emu_trace_file_metadata m;
  start(&f, root, "basic", 2);
  runtime_files_test_hold(f.files, 1);
  q = request(1, 0, 1, 10000, 4);
  m = metadata(1, 1);
  INIT(s);
  assert(f.p.open(f.p.context, &q, &m, &s) == EMUIF_OK && s.operation_id == 1 &&
         s.completed_operation_id == 0);
  r = event(1, 1, 1);
  f.p.enqueue(f.p.context, &r);
  assert(status(&f, 1).lost == 1);
  assert(f.p.operation(f.p.context, 1, 0, 1, &s) == EMUIF_BUSY);
  runtime_files_test_hold(f.files, 0);
  assert(wait_operation(&f, 1, 1).state == EMUIF_SINK_OPEN);
  runtime_files_test_hold(f.files, 1);
  emit_events(&f, 1, 2, 300);
  s = status(&f, 1);
  assert(s.queued == 256 && s.lost == 46);
  INIT(s);
  assert(f.p.operation(f.p.context, 1, 0, 2, &s) == EMUIF_OK &&
         s.operation_id == 2 && s.completed_operation_id == 1);
  r = event(1, 302, 2);
  f.p.enqueue(f.p.context, &r);
  assert(status(&f, 1).lost == 47);
  runtime_files_test_hold(f.files, 0);
  s = wait_operation(&f, 1, 2);
  assert(s.state == EMUIF_SINK_OPEN && s.records == 257);
  s = operation(&f, 1, 1, 2);
  assert(s.state == EMUIF_SINK_CLOSED && s.records == 258 && s.lost == 47);
  INIT(s);
  assert(f.p.operation(f.p.context, 1, 1, 2, &s) == EMUIF_OK &&
         s.operation_id == 3);
  /* Reopen preserves file storage and operation identities, but resets
   * counters. */
  q = request(1, 1, 2, 10000, 2);
  m = metadata(1, 2);
  INIT(s);
  assert(f.p.open(f.p.context, &q, &m, &s) == EMUIF_OK && s.operation_id == 4 &&
         s.records == 0 && s.lost == 0);
  assert(wait_operation(&f, 1, 4).state == EMUIF_SINK_OPEN);
  r = event(1, 400, 2);
  f.p.enqueue(f.p.context, &r);
  s = operation(&f, 1, 1, 2);
  assert(s.records == 1 && s.operation_id == 5);
  assert(runtime_files_destroy(f.files, 1000));
}
static void limits(const char *root) {
  struct fixture f;
  struct emuif_sink_status s;
  unsigned i;
  start(&f, root, "limit", 1);
  assert(open_sink(&f, 1, 0, 3, 1).state == EMUIF_SINK_OPEN);
  runtime_files_test_hold(f.files, 1);
  emit_events(&f, 1, 1, 20);
  runtime_files_test_hold(f.files, 0);
  for (i = 0; i < 5000; i++) {
    s = status(&f, 1);
    if (s.state == EMUIF_SINK_CLOSED)
      break;
    wait_ms();
  }
  assert(s.state == EMUIF_SINK_CLOSED && s.error == EMUIF_CAPACITY &&
         s.records == 3 && s.lost == 17 && !s.partial_output);
  emit_events(&f, 1, 21, 2);
  assert(status(&f, 1).lost == 19);
  assert(runtime_files_destroy(f.files, 1000));
  start(&f, root, "closing-limit", 1);
  assert(open_sink(&f, 1, 0, 1, 1).state == EMUIF_SINK_OPEN);
  runtime_files_test_hold_capacity(f.files, 1);
  emit_events(&f, 1, 1, 1);
  for (i = 0; i < 5000; i++) {
    s = status(&f, 1);
    if (s.state == EMUIF_SINK_CLOSE_PENDING)
      break;
    wait_ms();
  }
  assert(s.state == EMUIF_SINK_CLOSE_PENDING && s.error == EMUIF_CAPACITY);
  emit_events(&f, 1, 2, 2);
  assert(status(&f, 1).lost == 2);
  runtime_files_test_hold_capacity(f.files, 0);
  assert(runtime_files_destroy(f.files, 1000));
  start(&f, root, "rotation", 1);
  assert(open_sink(&f, 1, 0, 10000, 2).state == EMUIF_SINK_OPEN);
  for (i = 0; i < 50; i++) {
    emit_events(&f, 1, 1 + i * 20, 20);
    s = status(&f, 1);
    if (s.state == EMUIF_SINK_CLOSED)
      break;
    s = operation(&f, 1, 0, 1);
    if (s.state == EMUIF_SINK_CLOSED)
      break;
  }
  s = status(&f, 1);
  assert(s.state == EMUIF_SINK_CLOSED && s.error == EMUIF_CAPACITY &&
         s.segment == 1 && !s.partial_output);
  assert(runtime_files_destroy(f.files, 1000));
}
static void failures(const char *root) {
  struct fixture f;
  struct emuif_sink_status s;
  struct emuif_sink_request r;
  struct emu_trace_file_metadata m;
  char path[700];
  FILE *file;
  struct stat st;
  start(&f, root, "existing", 1);
  snprintf(path, sizeof path, "%s/capture0.0000.jsonl", f.directory);
  file = fopen(path, "w");
  assert(file);
  assert(fputs("keep", file) >= 0);
  assert(!fclose(file));
  s = open_sink(&f, 1, 0, 100, 2);
  assert(s.state == EMUIF_SINK_FAILED && s.error == EMUIF_IO && s.bytes == 0);
  assert(!stat(path, &st) && st.st_size == 4);
  assert(!runtime_files_destroy(f.files, 1000));
  start(&f, root, "partial", 1);
  runtime_files_test_fail_after(f.files, 20);
  s = open_sink(&f, 1, 0, 100, 2);
  assert(s.state == EMUIF_SINK_FAILED && s.partial_output && s.bytes == 20);
  assert(!runtime_files_destroy(f.files, 1000));
  start(&f, root, "fsync", 1);
  assert(open_sink(&f, 1, 0, 100, 2).state == EMUIF_SINK_OPEN);
  runtime_files_test_fail_fsync(f.files, 1);
  s = operation(&f, 1, 0, 1);
  assert(s.state == EMUIF_SINK_FAILED && s.partial_output &&
         s.error == EMUIF_IO && s.completed_operation_id == 2);
  assert(!runtime_files_destroy(f.files, 1000));
  start(&f, root, "invalid", 1);
  r = request(1, 0, 1, 1, 1);
  m = metadata(1, 1);
  INIT(s);
  strcpy(r.destination_token, "missing");
  assert(f.p.open(f.p.context, &r, &m, &s) == EMUIF_DENIED);
  strcpy(r.destination_token, "t0");
  r.max_segment_bytes = 65535;
  assert(f.p.open(f.p.context, &r, &m, &s) == EMUIF_INVALID);
  r.max_segment_bytes = 65536;
  m.trace_config_sha256 = hash;
  assert(f.p.open(f.p.context, &r, &m, &s) == EMUIF_HASH);
  assert(f.p.status(f.p.context, 1, &s) == EMUIF_NOT_FOUND);
  assert(runtime_files_destroy(f.files, 1000));
}
static void paths_and_history(const char *root) {
  struct fixture f;
  struct runtime_files *files = NULL;
  char bad[700], link[700], target[700], path[700];
  const char *specs[1];
  unsigned i;
  struct emuif_sink_status s;
  snprintf(target, sizeof target, "%s/path-target", root);
  assert(!mkdir(target, 0700));
  snprintf(link, sizeof link, "%s/path-link", root);
  assert(!symlink(target, link));
  snprintf(bad, sizeof bad, "token=%s/path-link/file", root);
  specs[0] = bad;
  assert(!runtime_files_create(specs, 1, &files));
  snprintf(bad, sizeof bad, "token=%s/path-target/../file", root);
  assert(!runtime_files_create(specs, 1, &files));
  assert(!chmod(target, 0755));
  snprintf(bad, sizeof bad, "token=%s/path-target/file", root);
  assert(!runtime_files_create(specs, 1, &files));
  assert(!chmod(target, 0700));
  start(&f, root, "fifo", 1);
  snprintf(path, sizeof path, "%s/capture0.0000.jsonl", f.directory);
  assert(!mkfifo(path, 0600));
  s = open_sink(&f, 1, 0, 100, 2);
  assert(s.state == EMUIF_SINK_FAILED && s.error == EMUIF_IO);
  assert(!runtime_files_destroy(f.files, 1000));
  start(&f, root, "history", 1);
  snprintf(path, sizeof path, "%s/capture0.0000.jsonl", f.directory);
  for (i = 1; i <= 64; i++) {
    assert(open_sink(&f, i, 0, 100, 1).state == EMUIF_SINK_OPEN);
    assert(operation(&f, i, 1, 1).state == EMUIF_SINK_CLOSED);
    assert(!unlink(path));
  }
  {
    struct emuif_sink_request r = request(65, 0, 1, 100, 1);
    struct emu_trace_file_metadata m = metadata(65, 1);
    INIT(s);
    assert(f.p.open(f.p.context, &r, &m, &s) == EMUIF_EXHAUSTED);
  }
  assert(runtime_files_destroy(f.files, 1000));
  start(&f, root, "active", 8);
  runtime_files_test_hold(f.files, 1);
  for (i = 1; i <= 8; i++) {
    struct emuif_sink_request r = request(i, i - 1, 1, 100, 1);
    struct emu_trace_file_metadata m = metadata(i, 1);
    INIT(s);
    assert(f.p.open(f.p.context, &r, &m, &s) == EMUIF_OK);
  }
  {
    struct emuif_sink_request r = request(9, 0, 1, 100, 1);
    struct emu_trace_file_metadata m = metadata(9, 1);
    INIT(s);
    assert(f.p.open(f.p.context, &r, &m, &s) == EMUIF_CAPACITY);
  }
  runtime_files_test_hold(f.files, 0);
  for (i = 1; i <= 8; i++)
    assert(wait_operation(&f, i, 1).state == EMUIF_SINK_OPEN);
  assert(f.p.close_all(f.p.context, 1) == EMUIF_BUSY);
  assert(runtime_files_destroy(f.files, 1000));
}
static void barriers_and_exhaustion(const char *root) {
  struct fixture f;
  struct emuif_sink_status s;
  struct emuif_trace_record r;
  start(&f, root, "barrier", 1);
  assert(open_sink(&f, 1, 0, 100, 1).state == EMUIF_SINK_OPEN);
  runtime_files_test_hold(f.files, 1);
  emit_events(&f, 1, 1, 10);
  INIT(s);
  assert(f.p.operation(f.p.context, 1, 0, 1, &s) == EMUIF_OK);
  emit_events(&f, 1, 11, 3);
  runtime_files_test_hold_after_flush(f.files);
  runtime_files_test_hold(f.files, 0);
  s = wait_operation(&f, 1, 2);
  assert(s.records == 10 && s.queued == 3 && !s.lost);
  runtime_files_test_hold(f.files, 0);
  assert(operation(&f, 1, 1, 1).records == 13);
  assert(runtime_files_destroy(f.files, 1000));
  start(&f, root, "exhaustion", 1);
  assert(open_sink(&f, 1, 0, 100, 1).state == EMUIF_SINK_OPEN);
  runtime_files_test_sequence(f.files, 1, UINT64_MAX);
  r = event(1, 1, 1);
  assert(f.p.enqueue(f.p.context, &r) == EMUIF_OK);
  assert(operation(&f, 1, 0, 1).records == 1);
  r = event(1, 2, 1);
  assert(f.p.enqueue(f.p.context, &r) == EMUIF_EXHAUSTED);
  assert(!runtime_files_destroy(f.files, 1000));
  start(&f, root, "summary-exhaustion", 1);
  assert(open_sink(&f, 1, 0, 1000, 4).state == EMUIF_SINK_OPEN);
  runtime_files_test_hold(f.files, 1);
  emit_events(&f, 1, 1, 300);
  runtime_files_test_sequence(f.files, 1, 0);
  INIT(s);
  assert(f.p.operation(f.p.context, 1, 0, 1, &s) == EMUIF_EXHAUSTED);
  runtime_files_test_hold(f.files, 0);
  {
    unsigned i;
    for (i = 0; i < 5000; i++) {
      s = status(&f, 1);
      if (s.state == EMUIF_SINK_FAILED)
        break;
      wait_ms();
    }
  }
  assert(s.state == EMUIF_SINK_FAILED && s.error == EMUIF_EXHAUSTED &&
         s.partial_output);
  assert(!runtime_files_destroy(f.files, 1000));
}
static void blocked_shutdown(const char *root) {
  struct fixture f;
  struct emuif_sink_request r;
  struct emu_trace_file_metadata m;
  struct emuif_sink_status s;
  struct timespec before, after;
  int64_t elapsed;
  start(&f, root, "blocked", 1);
  runtime_files_test_hold(f.files, 1);
  r = request(1, 0, 1, 100, 1);
  m = metadata(1, 1);
  INIT(s);
  assert(f.p.open(f.p.context, &r, &m, &s) == EMUIF_OK);
  assert(!clock_gettime(CLOCK_MONOTONIC, &before));
  assert(!runtime_files_destroy(f.files, 20));
  assert(!clock_gettime(CLOCK_MONOTONIC, &after));
  elapsed = (after.tv_sec - before.tv_sec) * 1000 +
            (after.tv_nsec - before.tv_nsec) / 1000000;
  assert(elapsed >= 19 && elapsed < 1000);
  /* The host exits after reporting partial output; independent blocked writer
   * memory must remain alive until process teardown. */
  _exit(0);
}
int main(int argc, char **argv) {
  char root[] = "/tmp/emusa-file-test-XXXXXX";
  const char *directory;
  if (argc == 3 && !strcmp(argv[1], "--blocked-shutdown"))
    blocked_shutdown(argv[2]);
  if (argc == 2)
    directory = argv[1];
  else {
    assert(mkdtemp(root));
    directory = root;
  }
  basic(directory);
  limits(directory);
  failures(directory);
  paths_and_history(directory);
  barriers_and_exhaustion(directory);
  printf("file sink backend: PASS; evidence=%s\n", directory);
  return 0;
}
