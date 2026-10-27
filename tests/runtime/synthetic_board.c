/* Independently compiled ABI1 fixture. Copyright 2026 Hans-Einar. MIT. */
#include "emusa80535/contracts/board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef FIXTURE_MODE
#define FIXTURE_MODE 0
#endif
struct board
{
    struct emuif_board_host host;
    uint64_t cycle, accesses, ports, applied, last_order, last_cycle;
    unsigned value;
    int fault;
};
static emuif_status create(const struct emuif_board_host *h, const struct emuif_board_create *c,
                           void **out)
{
    struct board *b;
    *out = NULL;
    if (FIXTURE_MODE == 10)
        return EMUIF_INVALID;
    b = calloc(1, sizeof(*b));
    if (!b)
        return EMUIF_CAPACITY;
    b->host = *h;
    b->fault = c->config_bytes == 14 && !memcmp(c->config_utf8, "{\"fault\":true}", 14);
    *out = b;
    return EMUIF_OK;
}
static void destroy(void *v)
{
    free(v);
}
static emuif_status reset(void *v, const struct emuif_board_reset *r)
{
    struct board *b = v;
    (void)r;
    if (FIXTURE_MODE == 11)
        return EMUIF_FAULT;
    b->cycle = b->accesses = b->ports = b->applied = b->last_order = b->last_cycle = 0;
    b->value = 0;
    return EMUIF_OK;
}
static emuif_status boundary(void *v, const struct emuif_board_boundary *x)
{
    struct board *b = v;
    b->cycle = x->cycle;
    return EMUIF_OK;
}
static emuif_status access(void *v, const struct emuif_bus_access *a, struct emuif_bus_result *r)
{
    struct board *b = v;
    b->accesses++;
    if (b->fault)
        return EMUIF_UNSUPPORTED;
    if (a->direction == EMUIF_ACCESS_WRITE)
        b->value = a->value;
    r->value = (uint8_t)b->value;
    return EMUIF_OK;
}
static emuif_status port(void *v, const struct emuif_port_output *p)
{
    struct board *b = v;
    (void)p;
    b->ports++;
    return EMUIF_OK;
}
static emuif_status signal_output(void *v, const struct emuif_signal_output *p)
{
    (void)v;
    (void)p;
    return EMUIF_OK;
}
static emuif_status validate(const void *v, const struct emuif_board_stimulus *s)
{
    (void)v;
    return strcmp(s->schema, "synthetic.inputs.v1") ? EMUIF_UNSUPPORTED : EMUIF_OK;
}
static emuif_status apply(void *v, const struct emuif_board_stimulus *s)
{
    struct board *b = v;
    struct emuif_input in = {0};
    b->applied++;
    b->last_order = s->order;
    b->last_cycle = s->cycle;
    in.struct_size = sizeof(in);
    in.api_version = 1;
    in.kind = EMUIF_INPUT_PORT;
    in.index = 1;
    in.mask = 255;
    in.value = 0x5a;
    in.cycle = s->cycle + 1;
    return b->host.schedule_input(b->host.context, &in);
}
static emuif_status inspect(const void *v, const struct emuif_board_inspect *q,
                            struct emuif_board_inspection *r)
{
    const struct board *b = v;
    char s[1024];
    int n;
    if (strcmp(q->schema, "synthetic.status.v1"))
        return EMUIF_UNSUPPORTED;
    n = snprintf(s, sizeof(s),
                 "{\"cycle\":\"%llu\",\"accesses\":\"%llu\",\"ports\":\"%llu\",\"applied\":\"%"
                 "llu\",\"lastOrder\":\"%llu\",\"lastCycle\":\"%llu\",\"value\":%u}",
                 (unsigned long long)b->cycle, (unsigned long long)b->accesses,
                 (unsigned long long)b->ports, (unsigned long long)b->applied,
                 (unsigned long long)b->last_order, (unsigned long long)b->last_cycle, b->value);
    r->required = (uint32_t)n;
    if (r->capacity < (unsigned)n)
        return EMUIF_CAPACITY;
    memcpy(r->response_utf8, s, (size_t)n);
    r->written = (uint32_t)n;
    return EMUIF_OK;
}
#if FIXTURE_MODE != 1
emuif_status emusa80535_board_query_v1(struct emuif_board_api *a)
{
    a->abi_major = FIXTURE_MODE == 2 ? 2 : 1;
    a->platform = FIXTURE_MODE == 3 ? 99 : 1;
    strcpy(a->module_id, "synthetic-board");
    strcpy(a->module_version, "1.0.0");
    strcpy(a->build_id, "0123456789abcdef0123456789abcdef01234567");
    a->capabilities = EMUIF_BOARD_CAP_STIMULUS | EMUIF_BOARD_CAP_INSPECT;
    a->create = create;
    a->destroy = destroy;
    a->reset = reset;
    a->boundary = boundary;
    a->access = access;
    a->port_output = port;
    a->signal_output = signal_output;
    a->validate_stimulus = validate;
    a->apply_stimulus = apply;
    a->inspect = inspect;
    if (FIXTURE_MODE == 4)
        a->access = NULL;
    if (FIXTURE_MODE == 5)
        a->capabilities |= EMUIF_BOARD_CAP_PEEK;
    if (FIXTURE_MODE == 6)
        a->required_cpu_capabilities = UINT64_C(4);
    if (FIXTURE_MODE == 7)
        a->reserved[0] = 1;
    if (FIXTURE_MODE == 8)
        memset(a->module_id, 'a', sizeof(a->module_id));
    if (FIXTURE_MODE == 9)
        a->api_version = 2;
    if (FIXTURE_MODE == 12)
        a->capabilities &= ~EMUIF_BOARD_CAP_STIMULUS;
    if (FIXTURE_MODE == 13)
        a->capabilities &= ~EMUIF_BOARD_CAP_INSPECT;
    if (FIXTURE_MODE == 14)
        a->required_cpu_capabilities = UINT64_C(2);
    return EMUIF_OK;
}
#endif
