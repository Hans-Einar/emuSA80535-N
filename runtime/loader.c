/* Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#define _GNU_SOURCE
#include "loader.h"
#include "emu_sha_internal.h"
#include "emusa80535/contracts/cpu.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int runtime_hash_valid(const char *s)
{
    size_t i;
    if (!s || strlen(s) != 64)
        return 0;
    for (i = 0; i < 64; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
            return 0;
    return 1;
}
int runtime_read(const char *path, size_t limit, unsigned char **out, size_t *size)
{
    int fd;
    struct stat st;
    unsigned char *p;
    size_t used = 0;
    ssize_t n;
    *out = NULL;
    *size = 0;
    if (!path || path[0] != '/')
        return 0;
    fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return 0;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 || (uint64_t)st.st_size > limit)
    {
        close(fd);
        return 0;
    }
    p = malloc(limit + 1);
    if (!p)
    {
        close(fd);
        return 0;
    }
    while (used <= limit)
    {
        n = read(fd, p + used, limit + 1 - used);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
        {
            free(p);
            close(fd);
            return 0;
        }
        if (!n)
            break;
        used += (size_t)n;
    }
    close(fd);
    if (used > limit)
    {
        free(p);
        return 0;
    }
    p[used] = 0;
    *out = p;
    *size = used;
    return 1;
}
static int label(const char *s, size_t n)
{
    size_t i = 0;
    if (!s[0])
        return 0;
    for (; i < n && s[i]; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') ||
              (s[i] >= '0' && s[i] <= '9') || strchr("._-", s[i])))
            return 0;
    if (i == n)
        return 0;
    for (; i < n; i++)
        if (s[i])
            return 0;
    return 1;
}
static int build_identity(const char *s)
{
    size_t n = strlen(s), i;
    if (n != 40 && n != 64)
        return 0;
    for (i = 0; i < n; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
            return 0;
    return 1;
}
void runtime_module_close(struct runtime_module *m)
{
    if (m->library)
        dlclose(m->library);
    if (m->fd >= 0)
        close(m->fd);
    memset(m, 0, sizeof(*m));
    m->fd = -1;
}
int runtime_module_open(struct runtime_module *m, const char *path, const char *hash,
                        const struct emuif_cpu_api *cpu, unsigned variant)
{
    unsigned char *bytes = NULL;
    size_t size = 0, done = 0, i;
    char actual[65], fdpath[64];
    ssize_t n;
    emuif_board_query_fn query = NULL;
    void *symbol;
    struct emuif_board_api *a = &m->api;
    memset(m, 0, sizeof(*m));
    m->fd = -1;
    if (!runtime_read(path, 64u * 1024u * 1024u, &bytes, &size))
        goto bad;
    sha256_hex(bytes, size, actual);
    if (strcmp(hash, actual))
        goto bad;
    /* Immutable private copy: the verified bytes are exactly those dlopen maps. */
    m->fd = memfd_create("emu-runtime-board", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (m->fd < 0)
        goto bad;
    while (done < size)
    {
        n = write(m->fd, bytes + done, size - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            goto bad;
        done += (size_t)n;
    }
    free(bytes);
    bytes = NULL;
    if (fcntl(m->fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0)
        goto bad;
    snprintf(fdpath, sizeof(fdpath), "/proc/self/fd/%d", m->fd);
    m->library = dlopen(fdpath, RTLD_NOW | RTLD_LOCAL);
    if (!m->library)
        goto bad;
    symbol = dlsym(m->library, EMUIF_BOARD_ENTRY_SYMBOL);
    if (!symbol)
        goto bad;
    memcpy(&query, &symbol, sizeof(query));
    a->struct_size = sizeof(*a);
    a->api_version = EMUIF_API_VERSION;
    if (query(a) != EMUIF_OK || a->struct_size < sizeof(*a) || a->api_version != 1 || a->flags ||
        a->abi_major != 1 || a->platform != 1)
        goto bad;
    if (!label(a->module_id, sizeof(a->module_id)) ||
        !label(a->module_version, sizeof(a->module_version)) ||
        !label(a->build_id, sizeof(a->build_id)) || !build_identity(a->build_id))
        goto bad;
    for (i = 0; i < sizeof(a->reserved); i++)
        if (a->reserved[i])
            goto bad;
    if (!a->create || !a->destroy || !a->reset || !a->boundary || !a->access || !a->port_output ||
        !a->signal_output)
        goto bad;
    if (!!(a->capabilities & EMUIF_BOARD_CAP_PEEK) != !!a->peek ||
        !!(a->capabilities & EMUIF_BOARD_CAP_INSPECT) != !!a->inspect)
        goto bad;
    if (a->capabilities & EMUIF_BOARD_CAP_STIMULUS)
    {
        if (!a->validate_stimulus || !a->apply_stimulus)
            goto bad;
    }
    else if (a->validate_stimulus || a->apply_stimulus)
        goto bad;
    if (a->required_cpu_capabilities & ~cpu->capabilities)
        goto bad;
    if (variant != EMUIF_VARIANT_SAB80535 &&
        (a->required_cpu_capabilities & EMUIF_CPU_CAP_UART_FRAME))
        goto bad;
    return 1;
bad:
    free(bytes);
    runtime_module_close(m);
    return 0;
}
