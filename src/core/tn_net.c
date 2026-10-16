/*
 * gtetrinet-ng — portable non-blocking TCP and 0xFF-framed message buffers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#include <windows.h>
#define CLOSESOCK closesocket
#define WOULDBLOCK(e) ((e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS)
static int lasterr(void) { return WSAGetLastError(); }
static const char *errstr(int e)
{
    static char buf[64];
    snprintf(buf, sizeof buf, "network error %d", e);
    return buf;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#define CLOSESOCK close
#define WOULDBLOCK(e) ((e) == EAGAIN || (e) == EWOULDBLOCK || (e) == EINPROGRESS)
static int lasterr(void) { return errno; }
static const char *errstr(int e) { return strerror(e); }
#endif

int tn_net_init(void)
{
#ifdef _WIN32
    WSADATA d;
    return WSAStartup(MAKEWORD(2, 2), &d) == 0 ? 0 : -1;
#else
    signal(SIGPIPE, SIG_IGN);
    return 0;
#endif
}

static void set_nonblocking(tn_socket s)
{
#ifdef _WIN32
    u_long one = 1;
    ioctlsocket(s, FIONBIO, &one);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
    int one1 = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one1, sizeof one1);
}

int tn_net_test_resolve_delay_ms;
int tn_net_test_prefer_ipv6;

static long now_ms(void)
{
#ifdef _WIN32
    return (long)GetTickCount();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
#endif
}

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000};
    nanosleep(&t, NULL);
#endif
}

/* One name lookup, shared by its thread and the connector until both have let go: a player
 * who backs out while a slow DNS server thinks must not wait for it, nor leave it writing
 * into freed memory. */
typedef struct resolve_job {
    atomic_int refs;
    atomic_int done;
    char host[256], service[16];
    int delay_ms;
    int rc;
    struct addrinfo *res;
} resolve_job;

static void job_release(resolve_job *j)
{
    if (atomic_fetch_sub(&j->refs, 1) != 1) return;
    if (j->res) freeaddrinfo(j->res);
    free(j);
}

static void job_run(resolve_job *j)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    if (j->delay_ms > 0) sleep_ms(j->delay_ms);
    j->rc = getaddrinfo(j->host, j->service, &hints, &j->res);
    atomic_store(&j->done, 1);
}

#ifdef _WIN32
static DWORD WINAPI job_thread(LPVOID arg)
#else
static void *job_thread(void *arg)
#endif
{
    job_run(arg);
    job_release(arg);
    return 0;
}

#define TN_MAX_ADDRS 16
/* how long one address may take before the next one is tried */
#define ATTEMPT_MS 3000

struct tn_connector {
    resolve_job *job;
    char host[96]; /* only for messages; the lookup has the whole name */
    struct sockaddr_storage addr[TN_MAX_ADDRS];
    int addrlen[TN_MAX_ADDRS];
    int naddr, next;
    tn_socket sock;
    long started;
    int failed;
    char err[128];
};

tn_connector *tn_connector_start(const char *host, int port)
{
    tn_connector *c = calloc(1, sizeof *c);
    resolve_job *j = calloc(1, sizeof *j);
    if (!c || !j) { free(c); free(j); return NULL; }
    c->sock = TN_BAD_SOCKET;
    snprintf(c->host, sizeof c->host, "%s", host);
    snprintf(j->host, sizeof j->host, "%s", host);
    snprintf(j->service, sizeof j->service, "%d", port);
    j->delay_ms = tn_net_test_resolve_delay_ms;
    atomic_init(&j->done, 0);
    atomic_init(&j->refs, 2);
    c->job = j;
#ifdef _WIN32
    HANDLE t = CreateThread(NULL, 0, job_thread, j, 0, NULL);
    int started = t != NULL;
    if (t) CloseHandle(t);
#else
    pthread_t t;
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    int started = pthread_create(&t, &at, job_thread, j) == 0;
    pthread_attr_destroy(&at);
#endif
    if (!started) {
        /* no thread to be had: look it up here, as before */
        job_run(j);
        atomic_fetch_sub(&j->refs, 1);
    }
    return c;
}

int tn_connector_resolving(const tn_connector *c)
{
    return c && c->job != NULL;
}

static int take_addresses(tn_connector *c)
{
    resolve_job *j = c->job;
    c->job = NULL;
    if (j->rc == 0) {
        /* two passes: the preferred family first, the other after it, each in the resolver's order */
        int first = tn_net_test_prefer_ipv6 ? AF_INET6 : AF_INET;
        for (int pass = 0; pass < 2; pass++)
            for (struct addrinfo *ai = j->res; ai && c->naddr < TN_MAX_ADDRS; ai = ai->ai_next) {
                if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6) continue;
                if ((ai->ai_family == first) != (pass == 0)) continue;
                if (ai->ai_addrlen > sizeof c->addr[0]) continue;
                memcpy(&c->addr[c->naddr], ai->ai_addr, ai->ai_addrlen);
                c->addrlen[c->naddr++] = (int)ai->ai_addrlen;
            }
    }
    job_release(j);
    if (c->naddr == 0) {
        snprintf(c->err, sizeof c->err, "cannot find server '%s'", c->host);
        return -1;
    }
    return 0;
}

/* starts the next address; -1 when none is left */
static int try_next(tn_connector *c)
{
    while (c->next < c->naddr) {
        int k = c->next++;
        tn_socket s = socket(c->addr[k].ss_family, SOCK_STREAM, IPPROTO_TCP);
        if (s == TN_BAD_SOCKET) { snprintf(c->err, sizeof c->err, "%s", errstr(lasterr())); continue; }
        set_nonblocking(s);
        if (connect(s, (struct sockaddr *)&c->addr[k], c->addrlen[k]) == 0 || WOULDBLOCK(lasterr())) {
            c->sock = s;
            c->started = now_ms();
            return 0;
        }
        snprintf(c->err, sizeof c->err, "%s", errstr(lasterr()));
        CLOSESOCK(s);
    }
    if (!c->err[0]) snprintf(c->err, sizeof c->err, "cannot connect to '%s'", c->host);
    return -1;
}

int tn_connector_poll(tn_connector *c, tn_socket *out, char *err, size_t errsz)
{
    if (!c) { snprintf(err, errsz, "out of memory"); return -1; }
    if (c->failed) { snprintf(err, errsz, "%s", c->err); return -1; }
    if (c->job) {
        if (!atomic_load(&c->job->done)) return 0;
        if (take_addresses(c) < 0 || try_next(c) < 0) goto fail;
    }
    for (;;) {
        int r = tn_tcp_connected(c->sock, c->err, sizeof c->err);
        if (r > 0) {
            *out = c->sock;
            c->sock = TN_BAD_SOCKET;
            c->failed = 1; /* the socket is handed over once */
            snprintf(c->err, sizeof c->err, "already connected");
            return 1;
        }
        if (r == 0 && (c->next >= c->naddr || now_ms() - c->started < ATTEMPT_MS)) return 0;
        if (r == 0) snprintf(c->err, sizeof c->err, "no answer");
        CLOSESOCK(c->sock);
        c->sock = TN_BAD_SOCKET;
        if (try_next(c) < 0) goto fail;
    }
fail:
    c->failed = 1;
    snprintf(err, errsz, "%s", c->err);
    return -1;
}

void tn_connector_free(tn_connector *c)
{
    if (!c) return;
    if (c->job) job_release(c->job);
    if (c->sock != TN_BAD_SOCKET) CLOSESOCK(c->sock);
    free(c);
}

int tn_tcp_connected(tn_socket s, char *err, size_t errsz)
{
    fd_set w, e;
    struct timeval tv = {0, 0};
    FD_ZERO(&w);
    FD_ZERO(&e);
    FD_SET(s, &w);
    FD_SET(s, &e);
    int r = select((int)s + 1, NULL, &w, &e, &tv);
    if (r < 0) { snprintf(err, errsz, "%s", errstr(lasterr())); return -1; }
    if (r == 0) return 0;
    int soerr = 0;
    socklen_t len = sizeof soerr;
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&soerr, &len);
    if (soerr) { snprintf(err, errsz, "%s", errstr(soerr)); return -1; }
    return 1;
}

static tn_socket listen_on(int family, int port, int loopback_only, char *err, size_t errsz)
{
    tn_socket s = socket(family, SOCK_STREAM, 0);
    if (s == TN_BAD_SOCKET) { snprintf(err, errsz, "%s", errstr(lasterr())); return s; }
    int one = 1, zero = 0;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
    struct sockaddr_storage ss;
    socklen_t len;
    memset(&ss, 0, sizeof ss);
    if (family == AF_INET6) {
        /* one socket for both: IPv4 clients arrive as ::ffff:a.b.c.d (Windows defaults to v6-only) */
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&zero, sizeof zero);
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&ss;
        a->sin6_family = AF_INET6;
        a->sin6_port = htons((unsigned short)port);
        len = sizeof *a;
    } else {
        struct sockaddr_in *a = (struct sockaddr_in *)&ss;
        a->sin_family = AF_INET;
        a->sin_port = htons((unsigned short)port);
        a->sin_addr.s_addr = htonl(loopback_only ? INADDR_LOOPBACK : INADDR_ANY);
        len = sizeof *a;
    }
    if (bind(s, (struct sockaddr *)&ss, len) != 0 || listen(s, 16) != 0) {
        snprintf(err, errsz, "port %d: %s", port, errstr(lasterr()));
        CLOSESOCK(s);
        return TN_BAD_SOCKET;
    }
    set_nonblocking(s);
    return s;
}

tn_socket tn_tcp_listen(int port, int loopback_only, char *err, size_t errsz)
{
    /* The embedded server stays on 127.0.0.1: its clients are local and connect there. */
    if (!loopback_only) {
        char e6[128];
        tn_socket s = listen_on(AF_INET6, port, 0, e6, sizeof e6);
        if (s != TN_BAD_SOCKET) return s;
        /* no IPv6 on this system (or the port is taken: IPv4 then says so as well) */
    }
    return listen_on(AF_INET, port, loopback_only, err, errsz);
}

int tn_tcp_port(tn_socket s)
{
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;
    if (getsockname(s, (struct sockaddr *)&ss, &len) != 0) return 0;
    if (ss.ss_family == AF_INET6) return ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
    return ntohs(((struct sockaddr_in *)&ss)->sin_port);
}

tn_socket tn_tcp_accept(tn_socket ls)
{
    tn_socket s = accept(ls, NULL, NULL);
    if (s != TN_BAD_SOCKET) set_nonblocking(s);
    return s;
}

void tn_tcp_peer_ipv4(tn_socket s, unsigned char ip[4])
{
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;
    memset(ip, 0, 4);
    if (getpeername(s, (struct sockaddr *)&ss, &len) != 0) return;
    if (ss.ss_family == AF_INET) {
        memcpy(ip, &((struct sockaddr_in *)&ss)->sin_addr, 4);
    } else if (ss.ss_family == AF_INET6) {
        const unsigned char *b = (const unsigned char *)&((struct sockaddr_in6 *)&ss)->sin6_addr;
        memcpy(ip, b + 12, 4);
    }
}

void tn_tcp_peer_key(tn_socket s, unsigned char key[16])
{
    struct sockaddr_storage ss;
    socklen_t len = sizeof ss;
    memset(key, 0, 16);
    if (getpeername(s, (struct sockaddr *)&ss, &len) != 0) return;
    if (ss.ss_family == AF_INET) {
        key[10] = key[11] = 0xFF;
        memcpy(key + 12, &((struct sockaddr_in *)&ss)->sin_addr, 4);
    } else if (ss.ss_family == AF_INET6) {
        const unsigned char *b = (const unsigned char *)&((struct sockaddr_in6 *)&ss)->sin6_addr;
        static const unsigned char mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
        /* one host usually owns a whole /64, so that is what counts as one address */
        memcpy(key, b, memcmp(b, mapped, 12) == 0 ? 16 : 8);
    }
}

void tn_tcp_close(tn_socket s)
{
    if (s != TN_BAD_SOCKET) CLOSESOCK(s);
}

void tn_conn_init(tn_conn *c, tn_socket s)
{
    memset(c, 0, sizeof *c);
    c->sock = s;
}

static void queue(tn_conn *c, const char *msg, char term)
{
    if (c->dead) return;
    size_t n = strlen(msg);
    if (c->outlen + n + 1 > c->outcap) {
        size_t cap = c->outcap ? c->outcap : 4096;
        while (cap < c->outlen + n + 1) cap *= 2;
        if (cap > (c->outmax ? c->outmax : (size_t)1 << 22)) { c->dead = 1; snprintf(c->err, sizeof c->err, "send buffer overflow"); return; }
        char *p = realloc(c->out, cap);
        if (!p) { c->dead = 1; return; }
        c->out = p;
        c->outcap = cap;
    }
    memcpy(c->out + c->outlen, msg, n);
    c->out[c->outlen + n] = term;
    c->outlen += n + 1;
}

void tn_conn_send(tn_conn *c, const char *msg) { queue(c, msg, (char)0xFF); }
void tn_conn_send_line(tn_conn *c, const char *msg) { queue(c, msg, '\n'); }

void tn_conn_pump(tn_conn *c)
{
    if (c->dead) return;
    while (c->outlen > 0) {
        int r = send(c->sock, c->out, (int)c->outlen, 0);
        if (r > 0) {
            memmove(c->out, c->out + r, c->outlen - (size_t)r);
            c->outlen -= (size_t)r;
        } else {
            int e = lasterr();
            if (r < 0 && WOULDBLOCK(e)) break;
            c->dead = 1;
            snprintf(c->err, sizeof c->err, "%s", r == 0 ? "connection closed" : errstr(e));
            return;
        }
    }
    while (c->inlen < sizeof c->in) {
        int r = recv(c->sock, c->in + c->inlen, (int)(sizeof c->in - c->inlen), 0);
        if (r > 0) { c->inlen += (size_t)r; continue; }
        if (r == 0) { c->dead = 1; snprintf(c->err, sizeof c->err, "connection closed"); return; }
        int e = lasterr();
        if (!WOULDBLOCK(e)) { c->dead = 1; snprintf(c->err, sizeof c->err, "%s", errstr(e)); }
        return;
    }
}

int tn_conn_next(tn_conn *c, char *msg, size_t msgsz)
{
    for (size_t i = 0; i < c->inlen; i++) {
        unsigned char ch = (unsigned char)c->in[i];
        if (ch == 0xFF || ch == '\n') {
            size_t n = i < msgsz - 1 ? i : msgsz - 1;
            memcpy(msg, c->in, n);
            msg[n] = 0;
            if (n && msg[n - 1] == '\r') msg[n - 1] = 0;
            memmove(c->in, c->in + i + 1, c->inlen - i - 1);
            c->inlen -= i + 1;
            return 1;
        }
    }
    if (c->inlen == sizeof c->in) {
        /* a message longer than the buffer: nothing legitimate is, drop the peer */
        c->dead = 1;
        snprintf(c->err, sizeof c->err, "message too long");
    }
    return 0;
}

void tn_conn_close(tn_conn *c)
{
    if (c->sock != TN_BAD_SOCKET) tn_tcp_close(c->sock);
    c->sock = TN_BAD_SOCKET;
    free(c->out);
    c->out = NULL;
    c->outlen = c->outcap = 0;
    c->dead = 1;
}
