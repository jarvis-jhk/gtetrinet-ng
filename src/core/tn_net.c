/*
 * gtetrinet-ng — portable non-blocking TCP and 0xFF-framed message buffers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
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

tn_socket tn_tcp_connect(const char *host, int port, char *err, size_t errsz)
{
    struct addrinfo hints, *res = NULL, *ai;
    char service[16];
    snprintf(service, sizeof service, "%d", port);
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    if (getaddrinfo(host, service, &hints, &res) != 0 || !res) {
        hints.ai_family = AF_UNSPEC;
        if (getaddrinfo(host, service, &hints, &res) != 0 || !res) {
            snprintf(err, errsz, "cannot find server '%s'", host);
            return TN_BAD_SOCKET;
        }
    }
    tn_socket s = TN_BAD_SOCKET;
    for (ai = res; ai; ai = ai->ai_next) {
        s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == TN_BAD_SOCKET) continue;
        set_nonblocking(s);
        if (connect(s, ai->ai_addr, (int)ai->ai_addrlen) == 0 || WOULDBLOCK(lasterr())) break;
        snprintf(err, errsz, "%s", errstr(lasterr()));
        CLOSESOCK(s);
        s = TN_BAD_SOCKET;
    }
    freeaddrinfo(res);
    if (s == TN_BAD_SOCKET && err[0] == 0) snprintf(err, errsz, "cannot connect to '%s'", host);
    return s;
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

tn_socket tn_tcp_listen(int port, int loopback_only, char *err, size_t errsz)
{
    tn_socket s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == TN_BAD_SOCKET) { snprintf(err, errsz, "%s", errstr(lasterr())); return s; }
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(loopback_only ? INADDR_LOOPBACK : INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0 || listen(s, 16) != 0) {
        snprintf(err, errsz, "port %d: %s", port, errstr(lasterr()));
        CLOSESOCK(s);
        return TN_BAD_SOCKET;
    }
    set_nonblocking(s);
    return s;
}

int tn_tcp_port(tn_socket s)
{
    struct sockaddr_in a;
    socklen_t len = sizeof a;
    if (getsockname(s, (struct sockaddr *)&a, &len) != 0) return 0;
    return ntohs(a.sin_port);
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

void tn_tcp_close(tn_socket s)
{
    if (s != TN_BAD_SOCKET) CLOSESOCK(s);
}

void tn_conn_init(tn_conn *c, tn_socket s)
{
    memset(c, 0, sizeof *c);
    c->sock = s;
}

void tn_conn_send(tn_conn *c, const char *msg)
{
    if (c->dead) return;
    size_t n = strlen(msg);
    if (c->outlen + n + 1 > c->outcap) {
        size_t cap = c->outcap ? c->outcap : 4096;
        while (cap < c->outlen + n + 1) cap *= 2;
        if (cap > (1u << 22)) { c->dead = 1; snprintf(c->err, sizeof c->err, "send buffer overflow"); return; }
        char *p = realloc(c->out, cap);
        if (!p) { c->dead = 1; return; }
        c->out = p;
        c->outcap = cap;
    }
    memcpy(c->out + c->outlen, msg, n);
    c->out[c->outlen + n] = (char)0xFF;
    c->outlen += n + 1;
}

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
