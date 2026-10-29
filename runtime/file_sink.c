/* Bounded asynchronous host file delivery. Copyright 2026 Hans-Einar. MIT. */
#define _GNU_SOURCE
#include "file_sink.h"
#include "emu_json_internal.h"
#include "emu_outcome_internal.h"
#include "emu_sha_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SINKS 64u
#define ACTIVE 8u
#define QUEUE 256u
#define FOOTER_RESERVE 4096u
#define INIT(x)                                                                \
  do {                                                                         \
    memset(&(x), 0, sizeof(x));                                                \
    (x).struct_size = sizeof(x);                                               \
    (x).api_version = 1;                                                       \
  } while (0)
enum operation { OP_NONE, OP_OPEN, OP_FLUSH, OP_CLOSE };
enum task_kind {
  TASK_NONE,
  TASK_OPEN,
  TASK_RECORD,
  TASK_BARRIER,
  TASK_FAILURE
};
struct mapping {
  char token[65], stem[NAME_MAX + 1];
  int directory;
};
struct loss {
  uint64_t first, last, count, sequence, generation;
};
struct queued {
  struct emuif_trace_record record;
  uint64_t ticket;
};
struct sink {
  unsigned used, mapping, head, count, operation, io_busy;
  uint64_t next_sequence, next_ticket, barrier_ticket, generation;
  struct loss pending_loss, barrier_loss;
  struct emuif_sink_request request;
  struct emuif_sink_status status;
  struct queued *queue;
  char runtime_commit[41], manifest[65], session_id[33];
  char image_hash[65], module_hash[65], config_hash[65], trace_hash[65];
  char *configuration;
  uint64_t opening_generation;
  /* These fields are touched only by the writer during an open lifetime. */
  int fd;
  uint64_t segment_bytes, segment_records, segment_last;
  emuif_status failure_requested;
};
struct runtime_files {
  pthread_mutex_t mutex;
  pthread_cond_t wake;
  pthread_t thread;
  unsigned stopping, mapping_count, next_sink;
  struct mapping mappings[ACTIVE];
  struct sink sinks[SINKS];
#ifdef RUNTIME_FILE_TESTING
  int hold, hold_after_flush, hold_capacity, fail_fsync;
  uint64_t fail_after, written;
#endif
};
struct task {
  enum task_kind kind;
  unsigned index, operation;
  struct queued item;
  struct loss loss;
  emuif_status error;
};

static void saturate(uint64_t *v, uint64_t add) {
  *v = UINT64_MAX - *v < add ? UINT64_MAX : *v + add;
}
static int hex(const char *s, size_t n) {
  size_t i;
  if (!s || strlen(s) != n)
    return 0;
  for (i = 0; i < n; i++)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
      return 0;
  return 1;
}
static int token_valid(const char *s) {
  size_t n = 0;
  if (!s)
    return 0;
  while (*s && n < 65) {
    unsigned char c = (unsigned char)*s++;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
      return 0;
    ++n;
  }
  return n && n <= 64 && !*s;
}
static int mapping_prepare(struct mapping *m, const char *spec) {
  char path[PATH_MAX], *part, *next;
  const char *equal = spec ? strchr(spec, '=') : NULL;
  struct stat st;
  int fd = -1, child;
  size_t n;
  m->directory = -1;
  if (!equal || (size_t)(equal - spec) > 64 || equal == spec)
    return 0;
  memcpy(m->token, spec, (size_t)(equal - spec));
  m->token[equal - spec] = 0;
  if (!token_valid(m->token))
    return 0;
  ++equal;
  n = strlen(equal);
  if (!n || n >= sizeof path || equal[0] != '/' ||
      !utf8_valid((const unsigned char *)equal, n))
    return 0;
  memcpy(path, equal + 1, n);
  fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0)
    return 0;
  part = path;
  for (;;) {
    next = strchr(part, '/');
    if (next)
      *next = 0;
    if (!*part || !strcmp(part, ".") || !strcmp(part, ".."))
      goto fail;
    if (!next)
      break;
    child = openat(fd, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (child < 0)
      goto fail;
    close(fd);
    fd = child;
    part = next + 1;
  }
  if (strlen(part) > NAME_MAX - 11 || fstat(fd, &st) ||
      st.st_uid != geteuid() || !S_ISDIR(st.st_mode) ||
      (st.st_mode & 07777) != 0700)
    goto fail;
  strcpy(m->stem, part);
  m->directory = fd;
  return 1;
fail:
  close(fd);
  return 0;
}
static struct sink *find_sink(struct runtime_files *f, uint32_t id) {
  unsigned i;
  for (i = 0; i < SINKS; i++)
    if (f->sinks[i].used && f->sinks[i].status.destination_id == id)
      return &f->sinks[i];
  return NULL;
}
static int output_valid(const struct emuif_sink_status *out) {
  return out && out->struct_size >= sizeof *out && out->api_version == 1 &&
         !out->flags;
}
static void copy_status(struct sink *s, struct emuif_sink_status *out) {
  uint32_t size = out->struct_size;
  *out = s->status;
  out->struct_size = size;
  out->queued = s->count;
}
static const char *status_name(emuif_status s) {
  static const char *names[] = {"OK",    "INVALID",  "VERSION",     "STATE",
                                "RANGE", "CAPACITY", "UNSUPPORTED", "BUSY",
                                "STALE", "DENIED",   "FAULT",       "EXHAUSTED",
                                "IO",    "HASH",     "NOT_FOUND"};
  return s <= EMUIF_NOT_FOUND ? names[s] : "IO";
}
static void header_json(const struct sink *s, uint32_t segment,
                        struct output *o) {
  output_raw(o, "{\"type\":\"header\",\"format\":\"emusa80535-trace-jsonl-1\","
                "\"contractVersion\":\"0.3.0\",\"protocol\":{\"major\":3,"
                "\"minor\":0},\"runtimeCommit\":");
  output_string(o, s->runtime_commit);
  output_raw(o, ",\"contractManifestSha256\":");
  output_string(o, s->manifest);
  output_raw(o, ",\"sessionId\":");
  output_string(o, s->session_id);
  output_raw(o, ",\"imageSha256\":");
  output_string(o, s->image_hash);
  output_raw(o, ",\"moduleSha256\":");
  output_string(o, s->module_hash);
  output_raw(o, ",\"configSha256\":");
  output_string(o, s->config_hash);
  output_raw(o, ",\"traceConfigSha256\":");
  output_string(o, s->trace_hash);
  output_format(o,
                ",\"generation\":\"%" PRIu64
                "\",\"segment\":%u,\"destinationId\":%u,\"configuration\":",
                s->opening_generation, segment, s->status.destination_id);
  output_raw(o, s->configuration);
  output_raw(o, "}\n");
}
static int write_bytes(struct runtime_files *f, struct sink *s,
                       const char *bytes, size_t size) {
  while (size) {
    ssize_t n;
    size_t amount = size;
#ifdef RUNTIME_FILE_TESTING
    pthread_mutex_lock(&f->mutex);
    if (f->written >= f->fail_after)
      amount = 0;
    else if (amount > f->fail_after - f->written)
      amount = (size_t)(f->fail_after - f->written);
    pthread_mutex_unlock(&f->mutex);
    if (!amount) {
      errno = EIO;
      return 0;
    }
#endif
    n = write(s->fd, bytes, amount);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return 0;
    pthread_mutex_lock(&f->mutex);
    s->status.bytes += (uint64_t)n;
#ifdef RUNTIME_FILE_TESTING
    f->written += (uint64_t)n;
#endif
    pthread_mutex_unlock(&f->mutex);
    s->segment_bytes += (uint64_t)n;
    bytes += n;
    size -= (size_t)n;
  }
  return 1;
}
static int sync_file(struct runtime_files *f, struct sink *s) {
#ifdef RUNTIME_FILE_TESTING
  int fail;
  pthread_mutex_lock(&f->mutex);
  fail = f->fail_fsync;
  pthread_mutex_unlock(&f->mutex);
  if (fail) {
    errno = EIO;
    return 0;
  }
#else
  (void)f;
#endif
  return !fsync(s->fd);
}
static int footer(struct runtime_files *f, struct sink *s,
                  emuif_status status) {
  struct output o = {0};
  uint64_t lost;
  unsigned segment;
  pthread_mutex_lock(&f->mutex);
  lost = s->status.lost;
  segment = s->status.segment;
  pthread_mutex_unlock(&f->mutex);
  output_format(&o,
                "{\"type\":\"footer\",\"destinationId\":%u,\"segment\":%u,"
                "\"lastRecordSequence\":\"%" PRIu64 "\",\"records\":\"%" PRIu64
                "\",\"lost\":\"%" PRIu64
                "\",\"complete\":true,\"status\":\"%s\"}\n",
                s->status.destination_id, segment, s->segment_last,
                s->segment_records, lost, status_name(status));
  return !o.failed && o.length <= FOOTER_RESERVE &&
         write_bytes(f, s, o.data, o.length) && sync_file(f, s);
}
static int open_segment(struct runtime_files *f, struct sink *s,
                        unsigned segment) {
  const struct mapping *m = &f->mappings[s->mapping];
  struct stat st;
  char name[NAME_MAX + 1];
  struct output o = {0};
  header_json(s, segment, &o);
  if (o.failed || o.length > 65536 ||
      o.length + 3072 + 28 + FOOTER_RESERVE > s->request.max_segment_bytes)
    return 0;
  if (fstat(m->directory, &st) || st.st_uid != geteuid() ||
      (st.st_mode & 07777) != 0700)
    return 0;
  if (segment > 15)
    return 0;
  snprintf(name, sizeof name, "%.*s.%04u.jsonl", NAME_MAX - 11, m->stem,
           segment);
  s->fd = openat(m->directory, name,
                 O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (s->fd < 0)
    return 0;
  if (fstat(s->fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
      (st.st_mode & 0777) != 0600)
    return 0;
  s->segment_bytes = s->segment_records = s->segment_last = 0;
  pthread_mutex_lock(&f->mutex);
  s->status.segment = segment;
  pthread_mutex_unlock(&f->mutex);
  return write_bytes(f, s, o.data, o.length);
}
static void discard_queue(struct sink *s) {
  saturate(&s->status.lost, s->count);
  s->head = s->count = 0;
  s->status.queued = 0;
  memset(&s->pending_loss, 0, sizeof s->pending_loss);
  memset(&s->barrier_loss, 0, sizeof s->barrier_loss);
}
static void fail_sink(struct runtime_files *f, struct sink *s,
                      emuif_status error, unsigned lost_record) {
  if (s->fd >= 0) {
    close(s->fd);
    s->fd = -1;
  }
  pthread_mutex_lock(&f->mutex);
  saturate(&s->status.lost, lost_record);
  discard_queue(s);
  free(s->queue);
  s->queue = NULL;
  free(s->configuration);
  s->configuration = NULL;
  s->status.error = error;
  s->status.state = EMUIF_SINK_FAILED;
  s->status.partial_output = 1;
  s->status.completed_operation_id = s->status.operation_id;
  s->operation = OP_NONE;
  s->failure_requested = 0;
  s->io_busy = 0;
  pthread_cond_broadcast(&f->wake);
  pthread_mutex_unlock(&f->mutex);
}
static int capacity_close(struct runtime_files *f, struct sink *s) {
  /* Queued views cannot fit after the deterministic limit. Count them before
   * the final footer snapshot, without claiming they were written. */
  pthread_mutex_lock(&f->mutex);
  s->status.state = EMUIF_SINK_CLOSE_PENDING;
  s->status.error = EMUIF_CAPACITY;
  s->operation = OP_CLOSE;
  discard_queue(s);
#ifdef RUNTIME_FILE_TESTING
  while (f->hold_capacity)
    pthread_cond_wait(&f->wake, &f->mutex);
#endif
  pthread_mutex_unlock(&f->mutex);
  if (!footer(f, s, EMUIF_CAPACITY))
    return 0;
  if (close(s->fd)) {
    s->fd = -1;
    return 0;
  }
  s->fd = -1;
  pthread_mutex_lock(&f->mutex);
  free(s->queue);
  s->queue = NULL;
  free(s->configuration);
  s->configuration = NULL;
  s->status.state = EMUIF_SINK_CLOSED;
  s->status.error = EMUIF_CAPACITY;
  s->status.completed_operation_id = s->status.operation_id;
  s->operation = OP_NONE;
  s->io_busy = 0;
  pthread_cond_broadcast(&f->wake);
  pthread_mutex_unlock(&f->mutex);
  return 1;
}
static int write_record(struct runtime_files *f, struct sink *s,
                        const struct emuif_trace_record *record) {
  struct output o = {0};
  unsigned segment;
  uint64_t records;
  size_t record_start;
  output_raw(&o, "{\"type\":\"record\",\"record\":");
  record_start = o.length;
  emu_trace_record_json(&o, record);
  if (o.failed || o.length - record_start > 3072)
    return -1;
  output_raw(&o, "}\n");
  /* The bound applies to the encoded record, excluding its file envelope. */
  if (o.failed)
    return -1;
  pthread_mutex_lock(&f->mutex);
  segment = s->status.segment;
  records = s->status.records;
  pthread_mutex_unlock(&f->mutex);
  if (s->segment_bytes + o.length + FOOTER_RESERVE >
      s->request.max_segment_bytes) {
    if (segment + 1 >= s->request.max_segments) {
      pthread_mutex_lock(&f->mutex);
      saturate(&s->status.lost, 1);
      pthread_mutex_unlock(&f->mutex);
      return capacity_close(f, s) ? 0 : -2;
    }
    if (!footer(f, s, EMUIF_OK))
      return -1;
    if (close(s->fd)) {
      s->fd = -1;
      return -1;
    }
    s->fd = -1;
    if (!open_segment(f, s, segment + 1))
      return -1;
  }
  if (s->segment_bytes + o.length + FOOTER_RESERVE >
      s->request.max_segment_bytes)
    return -1;
  if (!write_bytes(f, s, o.data, o.length))
    return -1;
  ++s->segment_records;
  s->segment_last = record->record_sequence;
  pthread_mutex_lock(&f->mutex);
  s->status.records = records + 1;
  pthread_mutex_unlock(&f->mutex);
  if (records + 1 >= s->request.max_total_records)
    return capacity_close(f, s) ? 0 : -2;
  return 1;
}
static void loss_record(const struct sink *s, const struct loss *loss,
                        struct emuif_trace_record *r) {
  INIT(*r);
  INIT(r->event);
  INIT(r->event.old_value);
  INIT(r->event.new_value);
  r->event.old_value.width = r->event.new_value.width = 8;
  r->kind = EMUIF_RECORD_LOSS;
  r->destination_id = s->status.destination_id;
  r->epoch = 1;
  r->record_sequence = loss->sequence;
  r->generation = loss->generation;
  r->first_source_sequence = loss->first;
  r->last_source_sequence = loss->last;
  r->count = loss->count;
}
static int select_task(struct runtime_files *f, struct task *task) {
  unsigned n;
  for (n = 0; n < SINKS; n++) {
    unsigned i = (f->next_sink + n) % SINKS;
    struct sink *s = &f->sinks[i];
    if (!s->used || s->io_busy)
      continue;
    memset(task, 0, sizeof *task);
    task->index = i;
    if (s->failure_requested) {
      task->kind = TASK_FAILURE;
      task->error = s->failure_requested;
    } else if (s->operation == OP_OPEN)
      task->kind = TASK_OPEN;
    else if (s->count && (s->operation == OP_NONE ||
                          s->queue[s->head].ticket <= s->barrier_ticket)) {
      task->kind = TASK_RECORD;
      task->item = s->queue[s->head];
      s->head = (s->head + 1) % QUEUE;
      --s->count;
      s->status.queued = s->count;
    } else if (s->operation == OP_FLUSH || s->operation == OP_CLOSE) {
      task->kind = TASK_BARRIER;
      task->operation = s->operation;
      task->loss = s->barrier_loss;
      memset(&s->barrier_loss, 0, sizeof s->barrier_loss);
    }
    if (task->kind != TASK_NONE) {
      s->io_busy = 1;
      f->next_sink = (i + 1) % SINKS;
      return 1;
    }
  }
  return 0;
}
static void *writer(void *context) {
  struct runtime_files *f = context;
  for (;;) {
    struct task task;
    struct sink *s;
    int result = 1;
    pthread_mutex_lock(&f->mutex);
    for (;;) {
#ifdef RUNTIME_FILE_TESTING
      if (f->hold && !f->stopping) {
        pthread_cond_wait(&f->wake, &f->mutex);
        continue;
      }
#endif
      if (f->stopping) {
        pthread_mutex_unlock(&f->mutex);
        return NULL;
      }
      if (select_task(f, &task))
        break;
      pthread_cond_wait(&f->wake, &f->mutex);
    }
    s = &f->sinks[task.index];
    pthread_mutex_unlock(&f->mutex);
    if (task.kind == TASK_FAILURE) {
      fail_sink(f, s, task.error, 0);
      continue;
    }
    if (task.kind == TASK_OPEN) {
      if (!open_segment(f, s, 0)) {
        fail_sink(f, s, EMUIF_IO, 0);
        continue;
      }
      pthread_mutex_lock(&f->mutex);
      s->status.state = EMUIF_SINK_OPEN;
      s->status.completed_operation_id = s->status.operation_id;
      s->operation = OP_NONE;
      s->io_busy = 0;
      pthread_cond_broadcast(&f->wake);
      pthread_mutex_unlock(&f->mutex);
      continue;
    }
    if (task.kind == TASK_RECORD)
      result = write_record(f, s, &task.item.record);
    else {
      if (task.loss.count) {
        struct emuif_trace_record record;
        loss_record(s, &task.loss, &record);
        result = write_record(f, s, &record);
      }
      if (result > 0) {
        if (task.operation == OP_CLOSE) {
          if (!footer(f, s, EMUIF_OK))
            result = -2;
          else if (close(s->fd)) {
            s->fd = -1;
            result = -2;
          } else
            s->fd = -1;
        } else if (!sync_file(f, s))
          result = -2;
      }
    }
    if (result < 0) {
      fail_sink(f, s, EMUIF_IO, result == -1 ? 1 : 0);
      continue;
    }
    if (!result)
      continue; /* limit has already closed/published status */
    pthread_mutex_lock(&f->mutex);
    if (task.kind == TASK_BARRIER) {
      if (task.operation == OP_CLOSE) {
        free(s->queue);
        s->queue = NULL;
        free(s->configuration);
        s->configuration = NULL;
      }
      s->status.state =
          task.operation == OP_CLOSE ? EMUIF_SINK_CLOSED : EMUIF_SINK_OPEN;
      s->status.completed_operation_id = s->status.operation_id;
      s->operation = OP_NONE;
#ifdef RUNTIME_FILE_TESTING
      if (task.operation == OP_FLUSH && f->hold_after_flush) {
        f->hold = 1;
        f->hold_after_flush = 0;
      }
#endif
    }
    s->io_busy = 0;
    pthread_cond_broadcast(&f->wake);
    pthread_mutex_unlock(&f->mutex);
  }
}

static void remember_loss(struct sink *s, const struct emuif_trace_record *r) {
  uint64_t first = r->kind == EMUIF_RECORD_EVENT ? r->event.sequence
                                                 : r->first_source_sequence;
  uint64_t last = r->kind == EMUIF_RECORD_EVENT ? r->event.sequence
                                                : r->last_source_sequence;
  if (!s->pending_loss.count || (first && first < s->pending_loss.first))
    s->pending_loss.first = first;
  if (last > s->pending_loss.last)
    s->pending_loss.last = last;
  saturate(&s->pending_loss.count, 1);
  s->pending_loss.generation = r->generation;
  saturate(&s->status.lost, 1);
}
static int sequence_room(const struct sink *s, unsigned n) {
  return s->next_sequence && UINT64_MAX - s->next_sequence >= n - 1 &&
         s->next_ticket && UINT64_MAX - s->next_ticket >= n - 1;
}
static void put_record(struct sink *s, const struct emuif_trace_record *r) {
  struct queued *q = &s->queue[(s->head + s->count) % QUEUE];
  q->record = *r;
  q->record.destination_id = s->status.destination_id;
  q->record.epoch = 1;
  q->record.record_sequence = s->next_sequence++;
  q->ticket = s->next_ticket++;
  ++s->count;
  s->status.queued = s->count;
}
static emuif_status provider_open(void *context,
                                  const struct emuif_sink_request *request,
                                  const struct emu_trace_file_metadata *m,
                                  struct emuif_sink_status *out) {
  struct runtime_files *f = context;
  struct sink prepared, *s = NULL;
  struct output header = {0};
  struct json_document document;
  char hash[65];
  unsigned i, mapping = ACTIVE, active = 0;
  emuif_status result = EMUIF_OK;
  memset(&prepared, 0, sizeof prepared);
  prepared.fd = -1;
  if (!f || !request || !m || !output_valid(out) ||
      request->struct_size < sizeof *request || request->api_version != 1 ||
      request->flags || !request->destination_id || !request->generation ||
      request->generation != m->generation ||
      request->max_segment_bytes < 65536 ||
      request->max_segment_bytes > 1073741824 || request->max_segments < 1 ||
      request->max_segments > 16 || !request->max_total_records ||
      !memchr(request->destination_token, 0, 65))
    return EMUIF_INVALID;
  for (i = 0; i < sizeof request->reserved; i++)
    if (request->reserved[i])
      return EMUIF_INVALID;
  if (!token_valid(request->destination_token))
    return EMUIF_DENIED;
  for (i = 0; i < f->mapping_count; i++)
    if (!strcmp(f->mappings[i].token, request->destination_token)) {
      mapping = i;
      break;
    }
  if (mapping == ACTIVE)
    return EMUIF_DENIED;
  if (!hex(m->runtime_commit, 40) || !hex(m->contract_manifest_sha256, 64) ||
      !hex(m->session_id, 32) || !hex(m->image_sha256, 64) ||
      !hex(m->module_sha256, 64) || !hex(m->config_sha256, 64) ||
      !hex(m->trace_config_sha256, 64) || !m->configuration_json ||
      !m->configuration_bytes || m->configuration_bytes > 16384 ||
      strlen(m->configuration_json) != m->configuration_bytes ||
      !json_document_parse(&document, m->configuration_json,
                           m->configuration_bytes, true) ||
      document.tokens[0].type != JSON_OBJECT)
    return EMUIF_INVALID;
  sha256_hex((const uint8_t *)m->configuration_json, m->configuration_bytes,
             hash);
  if (strcmp(hash, m->trace_config_sha256))
    return EMUIF_HASH;
  prepared.configuration = malloc((size_t)m->configuration_bytes + 1);
  prepared.queue = calloc(QUEUE, sizeof *prepared.queue);
  if (!prepared.configuration || !prepared.queue) {
    result = EMUIF_CAPACITY;
    goto done;
  }
  memcpy(prepared.configuration, m->configuration_json,
         (size_t)m->configuration_bytes + 1);
  strcpy(prepared.runtime_commit, m->runtime_commit);
  strcpy(prepared.manifest, m->contract_manifest_sha256);
  strcpy(prepared.session_id, m->session_id);
  strcpy(prepared.image_hash, m->image_sha256);
  strcpy(prepared.module_hash, m->module_sha256);
  strcpy(prepared.config_hash, m->config_sha256);
  strcpy(prepared.trace_hash, m->trace_config_sha256);
  prepared.opening_generation = m->generation;
  prepared.generation = m->generation;
  prepared.request = *request;
  prepared.mapping = mapping;
  INIT(prepared.status);
  prepared.status.destination_id = request->destination_id;
  header_json(&prepared, 0, &header);
  if (header.failed || header.length > 65536 ||
      header.length + 3072 + 28 + FOOTER_RESERVE > request->max_segment_bytes) {
    result = EMUIF_RANGE;
    goto done;
  }
  pthread_mutex_lock(&f->mutex);
  for (i = 0; i < SINKS; i++)
    if (f->sinks[i].used &&
        f->sinks[i].status.state <= EMUIF_SINK_CLOSE_PENDING)
      ++active;
  s = find_sink(f, request->destination_id);
  if (s && (s->io_busy || s->status.state <= EMUIF_SINK_CLOSE_PENDING)) {
    result = EMUIF_BUSY;
    goto unlock;
  }
  if (active >= ACTIVE) {
    result = EMUIF_CAPACITY;
    goto unlock;
  }
  if (s && s->status.operation_id == UINT64_MAX) {
    result = EMUIF_EXHAUSTED;
    goto unlock;
  }
  if (!s)
    for (i = 0; i < SINKS; i++)
      if (!f->sinks[i].used) {
        s = &f->sinks[i];
        break;
      }
  if (!s) {
    result = EMUIF_EXHAUSTED;
    goto unlock;
  }
  prepared.next_sequence = s->used ? s->next_sequence : 1;
  prepared.next_ticket = s->used ? s->next_ticket : 1;
  if (!prepared.next_sequence || !prepared.next_ticket) {
    result = EMUIF_EXHAUSTED;
    goto unlock;
  }
  prepared.status.operation_id = s->used ? s->status.operation_id + 1 : 1;
  prepared.status.completed_operation_id = prepared.status.operation_id - 1;
  prepared.status.state = EMUIF_SINK_FLUSH_PENDING;
  prepared.operation = OP_OPEN;
  prepared.used = 1;
  *s = prepared;
  prepared.queue = NULL;
  prepared.configuration = NULL;
  /* Snapshot pending state before waking the writer. */
  copy_status(s, out);
  pthread_cond_signal(&f->wake);
unlock:
  pthread_mutex_unlock(&f->mutex);
done:
  free(prepared.queue);
  free(prepared.configuration);
  return result;
}
static emuif_status provider_enqueue(void *context,
                                     const struct emuif_trace_record *record) {
  struct runtime_files *f = context;
  struct sink *s;
  unsigned needed;
  emuif_status result = EMUIF_OK;
  if (!f || !record)
    return EMUIF_INVALID;
  pthread_mutex_lock(&f->mutex);
  s = find_sink(f, record->destination_id);
  if (!s)
    goto done;
  s->generation = record->generation;
  if (!s->next_sequence || !s->next_ticket)
    result = EMUIF_EXHAUSTED;
  if (s->status.state == EMUIF_SINK_FAILED ||
      ((s->status.state == EMUIF_SINK_CLOSED ||
        s->status.state == EMUIF_SINK_CLOSE_PENDING) &&
       s->status.error == EMUIF_CAPACITY)) {
    saturate(&s->status.lost, 1);
    goto done;
  }
  if (s->status.state == EMUIF_SINK_CLOSED ||
      s->status.state == EMUIF_SINK_CLOSE_PENDING)
    goto done;
  if (s->operation == OP_OPEN) {
    remember_loss(s, record);
    goto done;
  }
  needed = s->pending_loss.count ? 2 : 1;
  if (s->failure_requested || QUEUE - s->count < needed) {
    remember_loss(s, record);
    goto done;
  }
  if (!sequence_room(s, needed)) {
    remember_loss(s, record);
    s->failure_requested = EMUIF_EXHAUSTED;
    result = EMUIF_EXHAUSTED;
    pthread_cond_signal(&f->wake);
    goto done;
  }
  if (s->pending_loss.count) {
    struct emuif_trace_record loss;
    s->pending_loss.generation = record->generation;
    loss_record(s, &s->pending_loss, &loss);
    put_record(s, &loss);
    memset(&s->pending_loss, 0, sizeof s->pending_loss);
  }
  put_record(s, record);
  pthread_cond_signal(&f->wake);
done:
  pthread_mutex_unlock(&f->mutex);
  return result;
}
static emuif_status operation_locked(struct runtime_files *f, struct sink *s,
                                     unsigned close, uint64_t generation,
                                     struct emuif_sink_status *out) {
  if (s->status.state == EMUIF_SINK_CLOSED ||
      s->status.state == EMUIF_SINK_FAILED) {
    copy_status(s, out);
    return close ? EMUIF_OK : EMUIF_STATE;
  }
  if (s->operation != OP_NONE || s->failure_requested)
    return EMUIF_BUSY;
  if (s->status.operation_id == UINT64_MAX)
    return EMUIF_EXHAUSTED;
  if (s->pending_loss.count && !s->next_sequence) {
    s->failure_requested = EMUIF_EXHAUSTED;
    pthread_cond_signal(&f->wake);
    return EMUIF_EXHAUSTED;
  }
  s->generation = generation;
  s->operation = close ? OP_CLOSE : OP_FLUSH;
  s->barrier_ticket = s->next_ticket ? s->next_ticket - 1 : UINT64_MAX;
  s->barrier_loss = s->pending_loss;
  memset(&s->pending_loss, 0, sizeof s->pending_loss);
  if (s->barrier_loss.count) {
    s->barrier_loss.sequence = s->next_sequence++;
    s->barrier_loss.generation = generation;
  }
  ++s->status.operation_id;
  s->status.state = close ? EMUIF_SINK_CLOSE_PENDING : EMUIF_SINK_FLUSH_PENDING;
  copy_status(s, out);
  pthread_cond_signal(&f->wake);
  return EMUIF_OK;
}
static emuif_status provider_operation(void *context, uint32_t destination,
                                       unsigned close, uint64_t generation,
                                       struct emuif_sink_status *out) {
  struct runtime_files *f = context;
  struct sink *s;
  emuif_status result;
  if (!f || close > 1 || !generation || !output_valid(out))
    return EMUIF_INVALID;
  pthread_mutex_lock(&f->mutex);
  s = find_sink(f, destination);
  result = s ? operation_locked(f, s, close, generation, out) : EMUIF_NOT_FOUND;
  pthread_mutex_unlock(&f->mutex);
  return result;
}
static emuif_status provider_status(void *context, uint32_t destination,
                                    struct emuif_sink_status *out) {
  struct runtime_files *f = context;
  struct sink *s;
  if (!f || !output_valid(out))
    return EMUIF_INVALID;
  pthread_mutex_lock(&f->mutex);
  s = find_sink(f, destination);
  if (s)
    copy_status(s, out);
  pthread_mutex_unlock(&f->mutex);
  return s ? EMUIF_OK : EMUIF_NOT_FOUND;
}
static uint32_t provider_active(void *context) {
  struct runtime_files *f = context;
  unsigned i;
  uint32_t active = 0;
  if (!f)
    return 0;
  pthread_mutex_lock(&f->mutex);
  for (i = 0; i < SINKS; i++)
    if (f->sinks[i].used &&
        f->sinks[i].status.state <= EMUIF_SINK_CLOSE_PENDING)
      active = 1;
  pthread_mutex_unlock(&f->mutex);
  return active;
}
static emuif_status provider_close_all(void *context, uint64_t generation) {
  struct runtime_files *f = context;
  unsigned i;
  emuif_status result = EMUIF_OK;
  if (!f || !generation)
    return EMUIF_INVALID;
  pthread_mutex_lock(&f->mutex);
  for (i = 0; i < SINKS; i++) {
    struct sink *s = &f->sinks[i];
    struct emuif_sink_status out;
    emuif_status status;
    if (!s->used || s->status.state >= EMUIF_SINK_CLOSED)
      continue;
    INIT(out);
    status = operation_locked(f, s, 1, generation, &out);
    if (status == EMUIF_EXHAUSTED) {
      s->failure_requested = EMUIF_EXHAUSTED;
      pthread_cond_signal(&f->wake);
    }
    result = EMUIF_BUSY;
  }
  pthread_mutex_unlock(&f->mutex);
  return result;
}
void runtime_files_provider(struct runtime_files *f,
                            struct emu_trace_file_provider *out) {
  memset(out, 0, sizeof *out);
  out->context = f;
  out->open = provider_open;
  out->enqueue = provider_enqueue;
  out->operation = provider_operation;
  out->status = provider_status;
  out->active = provider_active;
  out->close_all = provider_close_all;
}
int runtime_files_create(const char *const *specs, size_t count,
                         struct runtime_files **out) {
  struct runtime_files *f;
  size_t i, j;
  if (!out)
    return 0;
  *out = NULL;
  if (!specs || !count || count > ACTIVE)
    return 0;
  f = calloc(1, sizeof *f);
  if (!f)
    return 0;
  for (i = 0; i < ACTIVE; i++)
    f->mappings[i].directory = -1;
  for (i = 0; i < count; i++) {
    if (!mapping_prepare(&f->mappings[i], specs[i]))
      goto fail;
    for (j = 0; j < i; j++)
      if (!strcmp(f->mappings[i].token, f->mappings[j].token))
        goto fail;
  }
  f->mapping_count = (unsigned)count;
#ifdef RUNTIME_FILE_TESTING
  f->fail_after = UINT64_MAX;
#endif
  if (pthread_mutex_init(&f->mutex, NULL))
    goto fail;
  if (pthread_cond_init(&f->wake, NULL)) {
    pthread_mutex_destroy(&f->mutex);
    goto fail;
  }
  if (pthread_create(&f->thread, NULL, writer, f)) {
    pthread_cond_destroy(&f->wake);
    pthread_mutex_destroy(&f->mutex);
    goto fail;
  }
  *out = f;
  return 1;
fail:
  for (i = 0; i < ACTIVE; i++)
    if (f->mappings[i].directory >= 0)
      close(f->mappings[i].directory);
  free(f);
  return 0;
}
static uint64_t milliseconds(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}
int runtime_files_destroy(struct runtime_files *f, unsigned timeout_ms) {
  uint64_t end;
  unsigned i;
  int complete = 0, clean = 1;
  if (!f)
    return 1;
  end = milliseconds() + timeout_ms;
  do {
    pthread_mutex_lock(&f->mutex);
    complete = 1;
    for (i = 0; i < SINKS; i++) {
      struct sink *s = &f->sinks[i];
      struct emuif_sink_status out;
      if (!s->used)
        continue;
      if (s->status.state == EMUIF_SINK_FAILED || s->status.partial_output)
        clean = 0;
      if (s->status.state <= EMUIF_SINK_CLOSE_PENDING || s->io_busy) {
        emuif_status status;
        complete = 0;
        INIT(out);
        status = operation_locked(f, s, 1, s->generation, &out);
        if (status == EMUIF_EXHAUSTED) {
          s->failure_requested = status;
          pthread_cond_signal(&f->wake);
        }
      }
    }
    if (complete) {
      f->stopping = 1;
      pthread_cond_signal(&f->wake);
    }
    pthread_mutex_unlock(&f->mutex);
    if (complete)
      break;
    {
      struct timespec delay = {0, 1000000};
      nanosleep(&delay, NULL);
    }
  } while (milliseconds() < end);
  if (!complete) {
    /* No cancellation/free race with a blocked writer. Only process teardown
     * may reclaim this independent host context after a bounded failure. */
    pthread_detach(f->thread);
    return 0;
  }
  pthread_join(f->thread, NULL);
  for (i = 0; i < f->mapping_count; i++)
    close(f->mappings[i].directory);
  for (i = 0; i < SINKS; i++) {
    free(f->sinks[i].queue);
    free(f->sinks[i].configuration);
  }
  pthread_cond_destroy(&f->wake);
  pthread_mutex_destroy(&f->mutex);
  free(f);
  return clean;
}
#ifdef RUNTIME_FILE_TESTING
void runtime_files_test_hold(struct runtime_files *f, int value) {
  pthread_mutex_lock(&f->mutex);
  f->hold = !!value;
  pthread_cond_broadcast(&f->wake);
  pthread_mutex_unlock(&f->mutex);
}
void runtime_files_test_hold_after_flush(struct runtime_files *f) {
  pthread_mutex_lock(&f->mutex);
  f->hold_after_flush = 1;
  pthread_mutex_unlock(&f->mutex);
}
void runtime_files_test_hold_capacity(struct runtime_files *f, int value) {
  pthread_mutex_lock(&f->mutex);
  f->hold_capacity = !!value;
  pthread_cond_broadcast(&f->wake);
  pthread_mutex_unlock(&f->mutex);
}
void runtime_files_test_sequence(struct runtime_files *f, uint32_t dest,
                                 uint64_t next) {
  struct sink *s;
  pthread_mutex_lock(&f->mutex);
  s = find_sink(f, dest);
  if (s)
    s->next_sequence = next;
  pthread_mutex_unlock(&f->mutex);
}
void runtime_files_test_fail_after(struct runtime_files *f, uint64_t bytes) {
  pthread_mutex_lock(&f->mutex);
  f->fail_after = bytes;
  pthread_mutex_unlock(&f->mutex);
}
void runtime_files_test_fail_fsync(struct runtime_files *f, int value) {
  pthread_mutex_lock(&f->mutex);
  f->fail_fsync = !!value;
  pthread_mutex_unlock(&f->mutex);
}
#endif
