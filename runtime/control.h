/* Copyright 2026 Hans-Einar. MIT; see LICENSE. Private Linux host transport. */
#ifndef EMU_RUNTIME_CONTROL_H
#define EMU_RUNTIME_CONTROL_H
#include <stdint.h>
struct emuif_session;
/* Validate/pin the directory before loading modules. No listener is opened. */
struct runtime_endpoint
{
    int directory;
    char name[108];
};
int runtime_endpoint_prepare(struct runtime_endpoint *, const char *);
void runtime_endpoint_release(struct runtime_endpoint *);
/* One serialized owner; returns 0 for termination/shutdown, 3 for host failure. */
int runtime_control_run(struct emuif_session *, struct runtime_endpoint *, uint32_t mode);
#endif
