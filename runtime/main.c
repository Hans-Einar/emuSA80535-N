/* Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#include "emu_json_internal.h"
#include "emu_session_internal.h"
#include "emu_sha_internal.h"
#include "emusa80535/contracts/debug.h"
#include "control.h"
#include "file_sink.h"
#include "loader.h"
#include <emusa80535/contracts/control3.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef EMU_RUNTIME_COMMIT
#define EMU_RUNTIME_COMMIT "unknown"
#endif
#define INIT(x)                                                                                    \
    do                                                                                             \
    {                                                                                              \
        memset(&(x), 0, sizeof(x));                                                                \
        (x).struct_size = sizeof(x);                                                               \
        (x).api_version = 1;                                                                       \
    } while (0)
struct command
{
    char *json;
    char op[32];
};
struct host
{
    struct emuif_session *session;
    unsigned id;
    char session_id[33], generation[21];
    char snapshot[8192];
};
static const char *status_name(unsigned s)
{
    static const char *names[] = {"OK",       "INVALID",     "VERSION", "STATE", "RANGE",
                                  "CAPACITY", "UNSUPPORTED", "BUSY",    "STALE", "DENIED",
                                  "FAULT",    "EXHAUSTED",   "IO",      "HASH",  "NOT_FOUND"};
    return s < 15 ? names[s] : "FAULT";
}
static int emit(struct output *o)
{
    if (o->failed || o->length >= 65536)
        return 0;
    return fwrite(o->data, 1, o->length, stdout) == o->length && fputc('\n', stdout) != EOF &&
           fflush(stdout) == 0;
}
static int uint_arg(const char *s, uint64_t max, uint32_t *v)
{
    uint64_t x = 0;
    const char *p = s;
    if (!s || !*s)
        return 0;
    for (; *p; p++)
    {
        if (*p < '0' || *p > '9' || x > (max - (unsigned)(*p - '0')) / 10)
            return 0;
        x = x * 10 + (unsigned)(*p - '0');
    }
    *v = (uint32_t)x;
    return 1;
}
static int keys(const struct json_document *d, int obj, const char *const *allowed, size_t count)
{
    int i;
    size_t j;
    if (obj < 0 || d->tokens[obj].type != JSON_OBJECT)
        return 0;
    for (i = obj + 1; i < json_next(d, obj);)
    {
        char key[64];
        if (!json_string(d, i, key, sizeof(key)))
            return 0;
        for (j = 0; j < count; j++)
            if (!strcmp(key, allowed[j]))
                break;
        if (j == count)
            return 0;
        i = json_next(d, i + 1);
    }
    return 1;
}
static int primitive(const struct json_document *d, int t, const char *value)
{
    return t >= 0 && t < d->count && d->tokens[t].type == JSON_PRIMITIVE &&
           (size_t)(d->tokens[t].end - d->tokens[t].start) == strlen(value) &&
           !memcmp(d->text + d->tokens[t].start, value, strlen(value));
}
static int number(const struct json_document *d, const char *key, uint64_t min, uint64_t max)
{
    int64_t v;
    return json_integer(d, json_member(d, 0, key), (int64_t)min, max, &v);
}
static int schema_id(const struct json_document *d)
{
    char s[64];
    size_t i;
    if (!json_string(d, json_member(d, 0, "schema"), s, sizeof(s)) || !s[0])
        return 0;
    for (i = 0; s[i]; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') ||
              (s[i] >= '0' && s[i] <= '9') || strchr("._-", s[i])))
            return 0;
    return 1;
}
static int validate_command(struct command *c)
{
    struct json_document d;
    int t, i;
    uint64_t u;
    struct output compact = {0};
    const char *simple[] = {"op"}, *run[] = {"op", "cycles"}, *step[] = {"op", "maxCycles"},
               *reset[] = {"op", "seed", "entry"}, *input[] = {"op", "schema", "cycle", "payload"},
               *inspect[] = {"op", "schema", "request"}, *bp[] = {"op", "addresses"};
    if (!json_document_parse(&d, c->json, strlen(c->json), true) ||
        !json_string(&d, json_member(&d, 0, "op"), c->op, sizeof(c->op)))
        return 0;
    if (!strcmp(c->op, "state") || !strcmp(c->op, "pause"))
        return keys(&d, 0, simple, 1);
    if (!strcmp(c->op, "run"))
        return keys(&d, 0, run, 2) && number(&d, "cycles", 1, 100000);
    if (!strcmp(c->op, "step"))
        return keys(&d, 0, step, 2) && number(&d, "maxCycles", 1, 100000);
    if (!strcmp(c->op, "reset"))
        return keys(&d, 0, reset, 3) && number(&d, "seed", 0, UINT32_MAX) &&
               number(&d, "entry", 0, 65535);
    if (!strcmp(c->op, "boardInput") || !strcmp(c->op, "boardInspect"))
    {
        int in = !strcmp(c->op, "boardInput");
        if (!keys(&d, 0, in ? input : inspect, in ? 4 : 3) || !schema_id(&d))
            return 0;
        t = json_member(&d, 0, in ? "payload" : "request");
        if (t < 0 || d.tokens[t].type != JSON_OBJECT)
            return 0;
        /* The native wire arguments envelope adds one level to script nesting.
         * Reject this static limit here, before any preceding command can run. */
        for (i = t; i < json_next(&d, t); i++)
        {
            int parent = d.tokens[i].parent, depth = 1;
            while (parent >= 0)
            {
                depth++;
                parent = d.tokens[parent].parent;
            }
            if (depth > 15)
                return 0;
        }
        json_compact(&d, t, &compact);
        if (compact.failed || compact.length > 4096)
            return 0;
        if (in)
        {
            t = json_member(&d, 0, "cycle");
            if (!primitive(&d, t, "null") && !json_u64(&d, t, &u))
                return 0;
        }
        return 1;
    }
    if (!strcmp(c->op, "replaceCodeBreakpoints"))
    {
        unsigned char seen[8192] = {0};
        int64_t v;
        t = json_member(&d, 0, "addresses");
        if (!keys(&d, 0, bp, 2) || t < 0 || d.tokens[t].type != JSON_ARRAY ||
            d.tokens[t].size > 1024)
            return 0;
        for (i = t + 1; i < json_next(&d, t); i = json_next(&d, i))
        {
            if (!json_integer(&d, i, 0, 65535, &v) || (seen[v >> 3] & (1u << (v & 7))))
                return 0;
            seen[v >> 3] |= (unsigned char)(1u << (v & 7));
        }
        return 1;
    }
    return 0;
}
static int load_script(const char *path, struct command *commands, size_t *count)
{
    unsigned char *bytes = NULL;
    size_t n = 0, start = 0, pos, line = 0;
    struct json_document d;
    const char *fields[] = {"format", "version"};
    int64_t v;
    char format[32];
    *count = 0;
    if (!runtime_read(path, 8u * 1024u * 1024u, &bytes, &n) || !n || bytes[n - 1] != '\n')
        goto bad;
    for (pos = 0; pos < n; pos++)
        if (bytes[pos] == '\n')
        {
            size_t len = pos - start;
            if (!len || len + 1 > 65536 || line > 256 || memchr(bytes + start, 0, len))
                goto bad;
            bytes[pos] = 0;
            if (!line)
            {
                if (!json_document_parse(&d, (char *)bytes + start, len, true) ||
                    !keys(&d, 0, fields, 2) ||
                    !json_string(&d, json_member(&d, 0, "format"), format, sizeof(format)) ||
                    strcmp(format, "emu-runtime-script") ||
                    !json_integer(&d, json_member(&d, 0, "version"), 1, 1, &v))
                    goto bad;
            }
            else
            {
                struct command *c = &commands[*count];
                c->json = malloc(len + 1);
                if (!c->json)
                    goto bad;
                memcpy(c->json, bytes + start, len + 1);
                (*count)++;
                if (!validate_command(c))
                    goto bad;
            }
            start = pos + 1;
            line++;
        }
    free(bytes);
    return 1;
bad:
    free(bytes);
    return 0;
}
static int raw_member(const char *text, const char *name, char *out, size_t capacity)
{
    struct json_document d;
    struct output o = {0};
    int t;
    if (!json_document_parse(&d, text, strlen(text), true) || (t = json_member(&d, 0, name)) < 0)
        return 0;
    json_compact(&d, t, &o);
    if (o.failed || o.length >= capacity)
        return 0;
    memcpy(out, o.data, o.length);
    out[o.length] = 0;
    return 1;
}
static int request(struct host *h, const char *command, const char *args, char *response)
{
    struct output o = {0};
    struct emuif_json_request req;
    struct emuif_json_response res;
    output_format(&o, "{\"type\":\"request\",\"id\":%u,\"command\":", ++h->id);
    output_string(&o, command);
    output_raw(&o, ",\"arguments\":");
    output_raw(&o, args);
    output_raw(&o, "}");
    if (o.failed)
        return 0;
    INIT(req);
    INIT(res);
    req.bytes = (const uint8_t *)o.data;
    req.byte_count = (uint32_t)o.length;
    res.bytes = (uint8_t *)response;
    res.capacity = 65536;
    if (emusa80535_debug_request_v1(h->session, 1, &req, &res) != EMUIF_OK || res.written >= 65536)
        return 0;
    response[res.written] = 0;
    return 1;
}
static int succeeded(const char *s)
{
    struct json_document d;
    return json_document_parse(&d, s, strlen(s), true) &&
           primitive(&d, json_member(&d, 0, "success"), "true");
}
static int capture_snapshot(struct host *h, const char *response)
{
    struct json_document d;
    char body[8192];
    if (!succeeded(response) || !raw_member(response, "body", body, sizeof(body)) ||
        !json_document_parse(&d, body, strlen(body), true) ||
        !json_string(&d, json_member(&d, 0, "sessionId"), h->session_id, sizeof(h->session_id)) ||
        !json_string(&d, json_member(&d, 0, "generation"), h->generation, sizeof(h->generation)))
        return 0;
    strcpy(h->snapshot, body);
    return 1;
}
static void args_base(struct host *h, struct output *o, int generation)
{
    output_raw(o, "{\"sessionId\":");
    output_string(o, h->session_id);
    if (generation)
    {
        output_raw(o, ",\"generation\":");
        output_string(o, h->generation);
    }
}
static int status(struct host *h)
{
    struct output o = {0};
    char response[65536];
    args_base(h, &o, 0);
    output_raw(&o, "}");
    o.data[o.length] = 0;
    return request(h, "status", o.data, response) && capture_snapshot(h, response);
}
static int is_running(struct host *h)
{
    struct json_document d;
    return json_document_parse(&d, h->snapshot, strlen(h->snapshot), true) &&
           token_raw_equals(&d, json_member(&d, 0, "state"), "running");
}
static int control(struct host *h, const char *command, char *response)
{
    struct output o = {0};
    args_base(h, &o, 1);
    output_raw(&o, "}");
    o.data[o.length] = 0;
    return request(h, command, o.data, response);
}
static void typed_error(char *response, unsigned s, const char *message)
{
    struct output o = {0};
    output_raw(&o, "{\"success\":false,\"error\":{\"code\":");
    output_string(&o, status_name(s));
    output_raw(&o, ",\"message\":");
    output_string(&o, message);
    output_raw(&o, ",\"retryable\":false}}");
    memcpy(response, o.data, o.length);
    response[o.length] = 0;
}
static int execute(struct host *h, struct command *c, size_t index)
{
    struct json_document d;
    struct output args = {0}, out = {0};
    char response[65536], body[65536];
    int i, ok;
    const char *command = c->op;
    struct emuif_progress progress;
    emuif_status pumped = EMUIF_OK;
    json_document_parse(&d, c->json, strlen(c->json), true);
    if (!strcmp(c->op, "state"))
    {
        if (!status(h))
            return -1;
        snprintf(response, sizeof(response), "{\"success\":true,\"body\":%s}", h->snapshot);
    }
    else if (!strcmp(c->op, "run"))
    {
        int64_t cycles;
        json_integer(&d, json_member(&d, 0, "cycles"), 1, 100000, &cycles);
        if (!control(h, "resume", response))
            return -1;
        INIT(progress);
        if (succeeded(response))
        {
            pumped = emusa80535_session_pump_v1(h->session, (uint32_t)cycles, &progress);
            if (!status(h))
                return -1;
            if (is_running(h))
            {
                if (!control(h, "pause", response))
                    return -1;
                if (!succeeded(response))
                    goto result;
            }
            if (pumped != EMUIF_OK)
                typed_error(response, pumped, "session pump stopped with failure");
            else
                snprintf(response, sizeof(response),
                         "{\"success\":true,\"body\":{\"cycles\":\"%" PRIu64
                         "\",\"instructions\":\"%" PRIu64 "\",\"outcome\":\"%s\"}}",
                         progress.cycles, progress.instructions,
                         progress.stop_reason == 0 || progress.stop_reason == 7 ? "budget"
                                                                                : "stop");
        }
    }
    else
    {
        args_base(h, &args, 1);
        for (i = 1; i < d.count;)
        {
            char key[64];
            json_string(&d, i, key, sizeof(key));
            if (strcmp(key, "op"))
            {
                output_raw(&args, ",");
                output_string(&args, !strcmp(key, "request") ? "payload" : key);
                output_raw(&args, ":");
                json_compact(&d, i + 1, &args);
            }
            i = json_next(&d, i + 1);
        }
        output_raw(&args, "}");
        if (args.failed)
            return -1;
        args.data[args.length] = 0;
        if (!request(h, command, args.data, response))
            return -1;
    }
result:
    ok = succeeded(response);
    if (!status(h))
        return -1;
    if (!raw_member(response, ok ? "body" : "error", body, sizeof(body)))
        return -1;
    output_format(&out, "{\"type\":\"result\",\"index\":%zu,\"op\":", index);
    output_string(&out, c->op);
    output_format(&out, ",\"success\":%s,\"%s\":", ok ? "true" : "false", ok ? "body" : "error");
    output_raw(&out, body);
    output_raw(&out, ",\"snapshot\":");
    output_raw(&out, h->snapshot);
    if (!strcmp(c->op, "run"))
    {
        output_format(&out,
                      ",\"progress\":{\"cycles\":\"%" PRIu64 "\",\"instructions\":\"%" PRIu64 "\"}",
                      progress.cycles, progress.instructions);
    }
    output_raw(&out, "}");
    return emit(&out) ? ok : -1;
}
static void help(void)
{
    puts("emu-runtime script1 / persistent control3 (Linux x86-64)\n"
         "Usage: emu-runtime --variant 8051|8052|sab80535 --clock-hz N --seed N --entry N\n"
         "  --image /absolute/image.bin --image-sha256 HEX64\n"
         "  --board /absolute/module.so --board-sha256 HEX64\n"
         "  --board-config /absolute/config.json\n"
         "  (--script /absolute/commands.ndjson |\n"
         "   --control-unix /private/directory/socket --execution-mode externally-driven|host-driven)\n"
         "  [--board-inspect-fault]\n"
         "The optional flag asserts the pinned module promises pure bounded post-fault inspection.\n"
         "Control3 supports wire3.0 / bundle0.3.0 or explicit wire3.1 / wire-only0.4.0;\n"
         "script1 uses wire2.1 / bundle0.2.0.\n"
         "Unix control requires an owned 0700 directory and same-UID clients. No listener opens\n"
         "without --control-unix. External mode advances only through step/advance.\n"
         "Optional --trace-file TOKEN=/absolute/private/stem (repeat up to8) enables file sinks.\n"
         "Files use new .0000.jsonl segments; control commands accept tokens, never paths.\n"
         "--help and --version exit without loading inputs. No physical I/O.");
}
int main(int argc, char **argv)
{
    const char *names[] = {"--variant",      "--clock-hz",       "--seed",        "--entry",
                           "--image",        "--image-sha256",   "--board",       "--board-sha256",
                           "--board-config", "--script",         "--control-unix", "--execution-mode"};
    const char *values[12] = {0};
    const char *trace_specs[8] = {0};
    size_t trace_count = 0;
    struct runtime_files *files = NULL;
    struct emu_trace_file_provider file_provider;
    struct runtime_endpoint endpoint = {.directory = -1};
    uint32_t execution_mode = EMUIF_EXECUTION_EXTERNALLY_DRIVEN;
    int opt, i, inspect_fault = 0, rc = 2;
    size_t j, count = 0, image_n = 0, config_n = 0;
    unsigned char *image = NULL, *config = NULL;
    uint32_t clock_hz, seed, entry, variant;
    char hash[65], config_hash[65], response[65536], hello[16384];
    struct command commands[256] = {{0}};
    struct host h = {0};
    struct runtime_module module = {.fd = -1};
    struct emuif_cpu_api cpu;
    struct emuif_session_config sc;
    struct emuif_client client;
    struct json_document doc;
    struct output identity = {0};
    if (argc == 2 && !strcmp(argv[1], "--help"))
    {
        help();
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--version"))
    {
        printf("emu-runtime script1 source=%s bundle=%s API1 boardABI1.0 wire2.1; "
               "control3 bundle=0.3.0 wire3.0; opt-in wire-contract=0.4.0 wire3.1\n",
               EMU_RUNTIME_COMMIT, EMUIF_BUNDLE_VERSION);
        return 0;
    }
    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--trace-file"))
        {
            if (trace_count == 8 || i + 1 == argc)
                goto cleanup;
            trace_specs[trace_count++] = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "--board-inspect-fault"))
        {
            if (inspect_fault)
                goto cleanup;
            inspect_fault = 1;
            continue;
        }
        for (opt = 0; opt < 12; opt++)
            if (!strcmp(argv[i], names[opt]))
                break;
        if (opt == 12 || values[opt] || i + 1 == argc)
            goto cleanup;
        values[opt] = argv[++i];
    }
    for (opt = 0; opt < 9; opt++)
        if (!values[opt])
            goto cleanup;
    if (values[9])
    {
        if (values[10] || values[11] || trace_count)
            goto cleanup;
    }
    else
    {
        if (!values[10] || !values[11])
            goto cleanup;
        if (!strcmp(values[11], "host-driven"))
            execution_mode = EMUIF_EXECUTION_HOST_DRIVEN;
        else if (strcmp(values[11], "externally-driven"))
            goto cleanup;
        if (!runtime_endpoint_prepare(&endpoint, values[10]))
            goto cleanup;
    }
    if (!strcmp(values[0], "8051"))
        variant = 0;
    else if (!strcmp(values[0], "8052"))
        variant = 1;
    else if (!strcmp(values[0], "sab80535"))
        variant = 2;
    else
        goto cleanup;
    if (!uint_arg(values[1], 100000000, &clock_hz) || !clock_hz ||
        !uint_arg(values[2], UINT32_MAX, &seed) || !uint_arg(values[3], 65535, &entry) ||
        !runtime_hash_valid(values[5]) || !runtime_hash_valid(values[7]))
        goto cleanup;
    /* Complete static validation precedes dlopen, module create and CPU progress. */
    if ((values[9] && !load_script(values[9], commands, &count)) ||
        !runtime_read(values[4], 65536, &image, &image_n) || image_n != 65536 ||
        !runtime_read(values[8], 16384, &config, &config_n) || !config_n)
        goto cleanup;
    sha256_hex(image, image_n, hash);
    if (strcmp(hash, values[5]))
        goto cleanup;
    sha256_hex(config, config_n, config_hash);
    if (!json_document_parse_multiline(&doc, (char *)config, config_n) ||
        doc.tokens[0].type != JSON_OBJECT)
        goto cleanup;
    if (trace_count && !runtime_files_create(trace_specs, trace_count, &files))
        goto cleanup;
    INIT(cpu);
    if (emusa80535_cpu_query_v1(&cpu) != EMUIF_OK ||
        !runtime_module_open(&module, values[6], values[7], &cpu, variant))
        goto cleanup;
    if (inspect_fault && !(module.api.capabilities & EMUIF_BOARD_CAP_INSPECT))
        goto cleanup;
    INIT(sc);
    sc.variant = variant;
    sc.oscillator_hz = clock_hz;
    sc.seed = seed;
    sc.entry = (uint16_t)entry;
    sc.code = image;
    sc.code_bytes = (uint32_t)image_n;
    sc.board_config_utf8 = config;
    sc.board_config_bytes = (uint32_t)config_n;
    memcpy(sc.module_sha256, values[7], 65);
    sc.cpu_api = &cpu;
    sc.board_api = &module.api;
    if (emusa80535_session_create_v1(&sc, &h.session) != EMUIF_OK)
        goto cleanup;
    if (emu_session_allow_fault_inspection(h.session, (uint32_t)inspect_fault) != EMUIF_OK)
        goto cleanup;
    if (files)
    {
        runtime_files_provider(files, &file_provider);
        if (emu_session_set_file_provider(h.session, &file_provider) != EMUIF_OK)
            goto cleanup;
    }
    if (values[10])
    {
        if (emu_session_set_execution_mode(h.session, execution_mode) != EMUIF_OK)
            goto cleanup;
        rc = runtime_control_run(h.session, &endpoint, execution_mode);
        if (rc)
            fprintf(stderr, "emu-runtime: Unix control host failure\n");
        goto cleanup;
    }
    INIT(client);
    client.connection_id = 1;
    client.role = EMUIF_ROLE_CONTROLLER;
    client.may_terminate = 1;
    if (emusa80535_debug_open_v1(h.session, &client) != EMUIF_OK)
        goto cleanup;
    if (!request(&h, "hello",
                 "{\"protocol\":{\"major\":2,\"minor\":1},\"contractVersion\":\"0.2.0\","
                 "\"requiredCapabilities\":[\"attachSession\",\"status\",\"control\","
                 "\"codeBreakpoints\",\"decodeCode\"]}",
                 response) ||
        !succeeded(response) || !raw_member(response, "body", hello, sizeof(hello)))
        goto cleanup;
    if (!request(&h, "attach", "{\"sessionId\":null,\"role\":\"controller\"}", response) ||
        !capture_snapshot(&h, response))
        goto cleanup;
    output_raw(
        &identity,
        "{\"type\":\"identity\",\"format\":\"emu-runtime-script\",\"version\":1,\"runtime\":");
    output_raw(&identity, hello);
    output_raw(&identity, ",\"module\":{\"id\":");
    output_string(&identity, module.api.module_id);
    output_raw(&identity, ",\"version\":");
    output_string(&identity, module.api.module_version);
    output_raw(&identity, ",\"buildId\":");
    output_string(&identity, module.api.build_id);
    output_raw(&identity, ",\"sha256\":");
    output_string(&identity, values[7]);
    output_format(&identity, ",\"inspectFaultPromise\":%s}", inspect_fault ? "true" : "false");
    output_raw(&identity, ",\"imageSha256\":");
    output_string(&identity, hash);
    output_raw(&identity, ",\"configSha256\":");
    output_string(&identity, config_hash);
    output_raw(&identity, ",\"variant\":");
    output_string(&identity, values[0]);
    output_format(&identity, ",\"clockHz\":%u,\"seed\":%u,\"entry\":%u,\"snapshot\":", clock_hz,
                  seed, entry);
    output_raw(&identity, h.snapshot);
    output_raw(&identity, "}");
    if (!emit(&identity))
    {
        rc = 3;
        goto cleanup;
    }
    rc = 0;
    for (j = 0; j < count; j++)
    {
        int result = execute(&h, &commands[j], j);
        if (result <= 0)
            rc = 3;
        if (result < 0)
        {
            fprintf(stderr, "emu-runtime: response/output failure\n");
            break;
        }
    }
cleanup:
    if (rc == 2)
        fprintf(stderr, "emu-runtime: startup validation failed (arguments, script, hashes, "
                        "configuration or module/API); no CPU execution\n");
    if (h.session && files)
        emu_session_close_file_sinks(h.session);
    if (h.session)
    {
        emusa80535_debug_close_v1(h.session, 1);
        emusa80535_session_destroy_v1(h.session);
    }
    if (files && !runtime_files_destroy(files, 2000))
    {
        fprintf(stderr, "emu-runtime: trace sink shutdown has partial output or failed footer\n");
        if (rc != 2) rc = 3;
    }
    runtime_module_close(&module);
    runtime_endpoint_release(&endpoint);
    for (j = 0; j < count; j++)
        free(commands[j].json);
    free(image);
    free(config);
    return rc;
}
