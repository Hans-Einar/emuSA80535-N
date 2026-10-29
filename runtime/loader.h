/* Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#ifndef EMU_RUNTIME_LOADER_H
#define EMU_RUNTIME_LOADER_H
#include "emusa80535/contracts/board.h"
#include "emusa80535/contracts/cpu.h"
#include <stddef.h>
struct runtime_module
{
    void *library;
    int fd;
    struct emuif_board_api api;
};
int runtime_read(const char *, size_t, unsigned char **, size_t *);
int runtime_hash_valid(const char *);
int runtime_module_open(struct runtime_module *, const char *, const char *,
                        const struct emuif_cpu_api *, unsigned);
void runtime_module_close(struct runtime_module *);
#endif
