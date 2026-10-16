/*
 * gtetrinet-ng — asking a TetriNET server who is there, without logging in.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_query.h"
#include "tn_net.h"
#include "tn_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

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

static void nap(void)
{
#ifdef _WIN32
    Sleep(10);
#else
    struct timespec t = {0, 10 * 1000000};
    nanosleep(&t, NULL);
#endif
}

int tn_query_split(const char *addr, char *host, size_t hostsz, int *port)
{
    char buf[160];
    while (*addr == ' ') addr++;
    snprintf(buf, sizeof buf, "%s", addr);
    for (size_t n = strlen(buf); n && buf[n - 1] == ' '; n--) buf[n - 1] = 0;
    *port = TN_PORT;
    char *h = buf, *colon = strrchr(buf, ':');
    if (buf[0] == '[') {
        char *end = strchr(buf, ']');
        if (end) {
            *end = 0;
            h = buf + 1;
            if (end[1] == ':') *port = atoi(end + 2);
        }
    } else if (colon && colon == strchr(buf, ':')) {
        /* exactly one colon: host:port (a bare IPv6 address has several) */
        *colon = 0;
        *port = atoi(colon + 1);
    }
    if (*port <= 0 || *port > 65535) *port = TN_PORT;
    snprintf(host, hostsz, "%s", h);
    return host[0] ? 0 : -1;
}

int tn_query_parse_players(const char *line, int *players)
{
    static const char prefix[] = "Number of players logged in:";
    if (strncmp(line, prefix, sizeof prefix - 1) != 0) return -1;
    char *end;
    long n = strtol(line + sizeof prefix - 1, &end, 10);
    if (end == line + sizeof prefix - 1 || n < 0 || n > 100000) return -1;
    *players = (int)n;
    return 0;
}

/* The double quote, as a number: tests/test_i18n_strings.py reads C literals with a regex. */
#define DQ 34

/* one quoted word into out; returns the rest, or NULL */
static const char *quoted(const char *p, char *out, size_t n)
{
    while (*p == ' ') p++;
    if (*p != DQ) return NULL;
    p++;
    size_t k = 0;
    for (; *p && *p != DQ; p++)
        if (k + 1 < n && (unsigned char)*p >= ' ' && (unsigned char)*p < 127) out[k++] = *p;
    out[k] = 0;
    return *p == DQ ? p + 1 : NULL;
}

int tn_query_parse_channel(const char *line, tn_query_channel *ch)
{
    memset(ch, 0, sizeof *ch);
    const char *p = quoted(line, ch->name, sizeof ch->name);
    if (p) p = quoted(p, ch->desc, sizeof ch->desc);
    if (!p || !ch->name[0]) return -1;
    int prio = 0;
    int got = sscanf(p, "%d %d %d %d", &ch->players, &ch->max, &prio, &ch->status);
    if (got < 2 || ch->players < 0 || ch->max < 0) return -1;
    if (got < 4) ch->status = 0;
    return 0;
}

/* next line within the deadline: 1, or 0 on timeout/closed */
static int next_line(tn_conn *c, char *line, size_t n, long deadline)
{
    for (;;) {
        if (tn_conn_next(c, line, n)) return 1;
        if (c->dead || now_ms() > deadline) return 0;
        tn_conn_pump(c);
        if (tn_conn_next(c, line, n)) return 1;
        if (c->dead) return 0;
        nap();
    }
}

int tn_query(const char *host, int port, int timeout_ms, tn_query_result *r)
{
    memset(r, 0, sizeof *r);
    tn_net_init();
    long t0 = now_ms();
    tn_socket s = tn_tcp_connect(host, port, r->err, sizeof r->err);
    if (s == TN_BAD_SOCKET) return 0;
    for (;;) {
        int st = tn_tcp_connected(s, r->err, sizeof r->err);
        if (st > 0) break;
        if (st < 0) { tn_tcp_close(s); return 0; }
        if (now_ms() - t0 > timeout_ms) {
            snprintf(r->err, sizeof r->err, "no answer");
            tn_tcp_close(s);
            return 0;
        }
        nap();
    }
    r->reachable = 1;
    r->ms = (int)(now_ms() - t0);
    r->err[0] = 0;

    tn_conn c;
    tn_conn_init(&c, s);
    char line[512];
    tn_conn_send(&c, "playerquery");
    if (next_line(&c, line, sizeof line, now_ms() + timeout_ms) && tn_query_parse_players(line, &r->players) == 0) {
        r->answered = 1;
        tn_conn_send(&c, "version");
        if (next_line(&c, line, sizeof line, now_ms() + timeout_ms) && strcmp(line, "+OK") != 0) {
            snprintf(r->version, sizeof r->version, "%.63s", line);
            while (next_line(&c, line, sizeof line, now_ms() + timeout_ms) && strcmp(line, "+OK") != 0) {}
        }
        tn_conn_send(&c, "listchan");
        for (int k = 0; k < 256 && next_line(&c, line, sizeof line, now_ms() + timeout_ms); k++) {
            if (!strcmp(line, "+OK")) break;
            tn_query_channel ch;
            if (tn_query_parse_channel(line, &ch) != 0) continue;
            if (r->nchan < TN_QUERY_CHANNELS) r->chan[r->nchan] = ch;
            r->nchan++;
        }
    }
    tn_conn_close(&c);
    return 1;
}
