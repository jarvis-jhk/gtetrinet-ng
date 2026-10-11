/*
 * gtetrinet-ng — a small TetriNET server: one channel, classic 1.13 and tetrifast clients.
 * Embedded in the game so a Steam Deck can host offline, and built as gtetrinet-ng-server.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TN_SERVER_H
#define TN_SERVER_H

#include "tn_net.h"
#include "tn_protocol.h"

#define TN_SERVER_PENDING 16

typedef struct tn_server_player {
    tn_conn conn;
    int used, fast, playing;
    char nick[TN_NICK_MAX], team[TN_TEAM_MAX];
    int level;
    char field[TN_FIELD_W * TN_FIELD_H + 1];
} tn_server_player;

typedef struct tn_server_pending {
    tn_conn conn;
    int used, age_ms;
} tn_server_pending;

typedef struct tn_winner { char name[TN_NICK_MAX + 2]; int score; } tn_winner;

typedef struct tn_server {
    tn_socket listener;
    int port;
    int max_slots;               /* 6 for classic clients, up to 8 */
    tn_rules rules;
    char motd[256];
    tn_server_player p[TN_MAX_SLOTS + 1];
    tn_server_pending pending[TN_SERVER_PENDING];
    int ingame, paused;
    tn_winner win[32];
    int nwin;
    int log;                     /* print a line per connection/game to stderr */
} tn_server;

/* port 0 picks a free port; loopback_only keeps it off the network. */
int tn_server_start(tn_server *s, int port, int loopback_only, int max_slots, char *err, size_t errsz);
void tn_server_update(tn_server *s, int dt_ms);
void tn_server_stop(tn_server *s);
int tn_server_count(const tn_server *s);

#endif
