/*
 * gtetrinet-ng — the TetriNET query protocol, for a server list.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tetrinetx-derived servers answer a few plain commands on the game port instead of a login:
 * "playerquery" -> "Number of players logged in: N", "version" -> one line then "+OK",
 * "listchan" -> one line per channel ("name" "description" players max priority status) then
 * "+OK". Servers without it (tetrinet-server) simply wait for a login, so silence means
 * "reachable, no details" rather than "down".
 */
#ifndef TN_QUERY_H
#define TN_QUERY_H

#include <stddef.h>

#define TN_QUERY_CHANNELS 24

typedef struct tn_query_channel {
    char name[32];
    char desc[64];
    int players, max;
    int status;                  /* 1 no game, 2 game running, 3 paused (0 unknown) */
} tn_query_channel;

typedef struct tn_query_result {
    int reachable;               /* a TCP connection was accepted */
    int ms;                      /* how long the connect took */
    int answered;                /* the server speaks the query protocol */
    int players;                 /* logged in, all channels */
    char version[64];
    int nchan;                   /* channels listed (at most TN_QUERY_CHANNELS kept) */
    tn_query_channel chan[TN_QUERY_CHANNELS];
    char err[128];               /* why it is not reachable */
} tn_query_result;

/* Splits "host", "host:port" or "[v6]:port". Returns 0, or -1 for an empty host. */
int tn_query_split(const char *addr, char *host, size_t hostsz, int *port);
/* Blocking: connects (timeout_ms for the connect and for each answer) and asks. Run it on a
 * thread; the result is filled in either way. Returns r->reachable. */
int tn_query(const char *host, int port, int timeout_ms, tn_query_result *r);

/* parsers, exposed for tests: 0 on success */
int tn_query_parse_players(const char *line, int *players);
int tn_query_parse_channel(const char *line, tn_query_channel *ch);

#endif
