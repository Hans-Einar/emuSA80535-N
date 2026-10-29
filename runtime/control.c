/* Copyright 2026 Hans-Einar. MIT; see LICENSE. */
#define _GNU_SOURCE
#include "control.h"
#include "emu_json_internal.h"
#include "emu_session_internal.h"
#include <emusa80535/contracts/control3.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define CLIENTS 8
#define FRAME 65536
#define TIMEOUT_MS 5000
#define INIT(x)                                                                                    \
    do                                                                                             \
    {                                                                                              \
        memset(&(x), 0, sizeof(x));                                                                \
        (x).struct_size = sizeof(x);                                                               \
        (x).api_version = 1;                                                                       \
    } while (0)
struct peer
{
    int fd;
    uint64_t id, input_since, output_since;
    size_t used, written, output_bytes;
    int hello, closing;
    unsigned char input[FRAME], output[FRAME];
};
static volatile sig_atomic_t shutdown_requested;
static void shutdown_signal(int signo)
{
    (void)signo;
    shutdown_requested = 1;
}
static uint64_t milliseconds(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t))
        return 0;
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
int runtime_endpoint_prepare(struct runtime_endpoint *ep, const char *path)
{
    char copy[108], *part, *next;
    struct stat st;
    int fd, child;
    size_t n = path ? strlen(path) : 0;
    ep->directory = -1;
    if (!n || n > 107 || path[0] != '/' || !utf8_valid((const unsigned char *)path, n))
        return 0;
    memcpy(copy, path + 1, n);
    fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    part = copy;
    for (;;)
    {
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
    if (fstat(fd, &st) || st.st_uid != geteuid() || (st.st_mode & 07777) != 0700)
        goto fail;
    if (!fstatat(fd, part, &st, AT_SYMLINK_NOFOLLOW) || errno != ENOENT)
        goto fail;
    strcpy(ep->name, part);
    ep->directory = fd;
    return 1;
fail:
    close(fd);
    return 0;
}
void runtime_endpoint_release(struct runtime_endpoint *ep)
{
    if (ep->directory >= 0)
        close(ep->directory);
    ep->directory = -1;
}
static int listener_open(struct runtime_endpoint *ep, struct stat *created)
{
    struct sockaddr_un addr;
    struct stat st;
    mode_t previous;
    int fd = -1, cwd, bound;
    /* Pin traversal and bind relative to that directory. No module callback runs
     * while cwd is temporarily changed. Never unlink a preexisting endpoint. */
    if (!fstatat(ep->directory, ep->name, &st, AT_SYMLINK_NOFOLLOW) || errno != ENOENT)
        return -1;
    if (fstat(ep->directory, &st) || st.st_uid != geteuid() || (st.st_mode & 07777) != 0700)
        return -1;
    cwd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (cwd < 0)
        return -1;
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0 || fchdir(ep->directory))
    {
        close(cwd);
        if (fd >= 0)
            close(fd);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, ep->name);
    previous = umask(0177);
    bound = bind(fd, (struct sockaddr *)&addr,
                 (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(ep->name) + 1));
    umask(previous);
    /* Keep ownership evidence even if restoring cwd or listen fails. */
    if (!bound && fstatat(ep->directory, ep->name, created, AT_SYMLINK_NOFOLLOW))
        bound = -1;
    if (fchdir(cwd))
        bound = -1;
    close(cwd);
    if (bound || listen(fd, CLIENTS))
    {
        close(fd);
        return -1;
    }
    return fd;
}
static void peer_close(struct emuif_session *session, const struct emuif_control3_api *api,
                       struct peer *p)
{
    if (p->fd >= 0)
    {
        api->close(session, p->id);
        close(p->fd);
    }
    memset(p, 0, sizeof(*p));
    p->fd = -1;
}
static void accept_peer(int listener, struct emuif_session *session,
                        const struct emuif_control3_api *api, struct peer *peers, uint64_t *next_id)
{
    struct ucred cred;
    socklen_t size = sizeof(cred);
    struct emuif_client client;
    unsigned i;
    int fd = accept4(listener, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0)
        return;
    for (i = 0; i < CLIENTS && peers[i].fd >= 0; i++)
    {
    }
    if (i == CLIENTS || !*next_id || getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &size) ||
        size != sizeof(cred) || cred.uid != geteuid())
    {
        close(fd);
        return;
    }
    INIT(client);
    client.connection_id = (*next_id)++;
    client.role = EMUIF_ROLE_CONTROLLER; /* same-UID host authorization; attach selects role */
    client.may_terminate = 1;
    if (api->open(session, &client) != EMUIF_OK)
    {
        close(fd);
        return;
    }
    peers[i].fd = fd;
    peers[i].id = client.connection_id;
    peers[i].input_since = milliseconds();
}
static void read_peer(struct emuif_session *session, const struct emuif_control3_api *api,
                      struct peer *p)
{
    unsigned char peek[FRAME], *lf;
    ssize_t n;
    size_t take;
    struct emuif_json_request request;
    struct emuif_json_response response;
    /* Peek to consume exactly one frame. Additional bytes remain kernel-side:
     * admission is serialized and only resumes once this response is written.
     * Thus there are <=8 admitted requests (below the global limit of32), and
     * exactly one request/output buffer per peer, regardless of pipelining. */
    n = recv(p->fd, peek, FRAME - p->used, MSG_PEEK);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return;
    if (n <= 0)
    {
        peer_close(session, api, p);
        return;
    }
    lf = memchr(peek, '\n', (size_t)n);
    take = lf ? (size_t)(lf - peek) + 1 : (size_t)n;
    n = recv(p->fd, p->input + p->used, take, 0);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return;
    if (n <= 0)
    {
        peer_close(session, api, p);
        return;
    }
    if (!p->input_since)
        p->input_since = milliseconds();
    p->used += (size_t)n;
    if (p->input[p->used - 1] != '\n')
    {
        if (p->used == FRAME)
            peer_close(session, api, p);
        return;
    }
    if (p->used == 1)
    {
        peer_close(session, api, p);
        return;
    }
    INIT(request);
    INIT(response);
    request.bytes = p->input;
    request.byte_count = (uint32_t)p->used - 1;
    response.bytes = p->output;
    response.capacity = FRAME;
    if (api->request(session, p->id, &request, &response) != EMUIF_OK || !response.written ||
        response.written >= FRAME)
    {
        peer_close(session, api, p);
        return;
    }
    p->output[response.written] = '\n';
    p->output_bytes = response.written + 1;
    p->written = 0;
    p->output_since = milliseconds();
    p->used = 0;
    p->hello = emu_session_client_ready(session, p->id);
    if (p->hello)
        p->input_since = 0;
    p->closing = !emu_session_client_open(session, p->id);
}
static void write_peer(struct emuif_session *session, const struct emuif_control3_api *api,
                       struct peer *p)
{
    ssize_t n = send(p->fd, p->output + p->written, p->output_bytes - p->written, MSG_NOSIGNAL);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return;
    if (n <= 0)
    {
        peer_close(session, api, p);
        return;
    }
    p->written += (size_t)n;
    if (p->written == p->output_bytes)
    {
        p->written = p->output_bytes = 0;
        p->output_since = 0;
        if (p->closing)
            peer_close(session, api, p);
    }
}
int runtime_control_run(struct emuif_session *session, struct runtime_endpoint *ep, uint32_t mode)
{
    struct emuif_control3_api api;
    struct peer *peers = NULL;
    struct stat created = {0}, current;
    struct sigaction action, old_int, old_term;
    struct pollfd polls[CLIENTS + 1];
    uint64_t next_id = 1, now;
    int listener = -1, result = 3, signals = 0, ready, terminating, pending;
    unsigned i;
    INIT(api);
    if (emusa80535_control_query_v3(&api) != EMUIF_OK)
        goto done;
    peers = calloc(CLIENTS, sizeof(*peers));
    if (!peers)
        goto done;
    for (i = 0; i < CLIENTS; i++)
        peers[i].fd = -1;
    memset(&action, 0, sizeof(action));
    action.sa_handler = shutdown_signal;
    sigemptyset(&action.sa_mask);
    shutdown_requested = 0;
    if (sigaction(SIGINT, &action, &old_int))
        goto done;
    if (sigaction(SIGTERM, &action, &old_term))
    {
        sigaction(SIGINT, &old_int, NULL);
        goto done;
    }
    signals = 1;
    listener = listener_open(ep, &created);
    if (listener < 0)
        goto done;
    result = 0;
    while (!shutdown_requested)
    {
        terminating = emu_session_state(session) == EMUIF_STATE_TERMINATED;
        pending = 0;
        polls[0].fd = terminating ? -1 : listener;
        polls[0].events = POLLIN;
        now = milliseconds();
        for (i = 0; i < CLIENTS; i++)
        {
            struct peer *p = &peers[i];
            if (p->fd >= 0 && ((p->input_since && now - p->input_since >= TIMEOUT_MS) ||
                               (p->output_since && now - p->output_since >= TIMEOUT_MS) ||
                               (terminating && !p->output_bytes)))
                peer_close(session, &api, p);
            polls[i + 1].fd = p->fd;
            polls[i + 1].events = p->output_bytes ? POLLOUT : POLLIN;
            pending += p->output_bytes != 0;
        }
        if (terminating && !pending)
            break;
        ready = poll(polls, CLIENTS + 1,
                     mode == EMUIF_EXECUTION_HOST_DRIVEN &&
                             emu_session_state(session) == EMUIF_STATE_RUNNING
                         ? 0
                         : 100);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            result = 3;
            break;
        }
        if (polls[0].revents & POLLIN)
            accept_peer(listener, session, &api, peers, &next_id);
        for (i = 0; i < CLIENTS; i++)
        {
            struct peer *p = &peers[i];
            short events = polls[i + 1].revents;
            if (p->fd < 0)
                continue;
            if (events & (POLLERR | POLLNVAL))
            {
                peer_close(session, &api, p);
                continue;
            }
            if ((events & POLLOUT) && p->output_bytes)
                write_peer(session, &api, p);
            else if ((events & (POLLIN | POLLHUP)) && !p->output_bytes)
                read_peer(session, &api, p);
        }
        if (mode == EMUIF_EXECUTION_HOST_DRIVEN &&
            emu_session_state(session) == EMUIF_STATE_RUNNING)
        {
            struct emuif_progress progress;
            INIT(progress);
            /* Native pump records faults; control remains available for recovery. */
            emusa80535_session_pump_v1(session, 64, &progress);
        }
    }
done:
    if (peers)
    {
        for (i = 0; i < CLIENTS; i++)
            peer_close(session, &api, &peers[i]);
        free(peers);
    }
    if (listener >= 0)
        close(listener);
    if (created.st_ino && !fstatat(ep->directory, ep->name, &current, AT_SYMLINK_NOFOLLOW) &&
        S_ISSOCK(current.st_mode) && current.st_ino == created.st_ino &&
        current.st_dev == created.st_dev)
        unlinkat(ep->directory, ep->name, 0);
    if (signals)
    {
        sigaction(SIGINT, &old_int, NULL);
        sigaction(SIGTERM, &old_term, NULL);
    }
    return result;
}
