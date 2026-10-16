/*
 * gtetrinet-ng — a small TetriNET server: one channel, classic 1.13 and tetrifast clients.
 * Embedded in the game so a Steam Deck can host offline, and built as gtetrinet-ng-server.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TN_SERVER_H
#define TN_SERVER_H

#include "tn_net.h"
#include "tn_protocol.h"

/* Limits for a server on the open internet. Connections that have not logged in yet: */
#define TN_SERVER_PENDING 32        /* at once, in all */
#define TN_SERVER_PER_ADDR 8        /* at once, from one address (8 local players fit) */
#define TN_SERVER_QUERIES 32        /* query commands one connection may ask */
/* A player's messages are read at most this fast (burst, then per second). Beyond that the
 * server stops reading, so TCP slows the sender down; only a sender with a whole input buffer
 * (16 KiB) waiting is dropped. A fast game sends a few messages per piece, far below this. */
#define TN_SERVER_MSG_BURST 200
#define TN_SERVER_MSG_RATE 100
/* Unsent output before a player who does not read counts as gone. */
#define TN_SERVER_OUT_MAX (1u << 20)

typedef struct tn_server_player {
    tn_conn conn;
    int used, fast, playing;
    char nick[TN_NICK_MAX], team[TN_TEAM_MAX];
    int level;
    int budget;                  /* messages it may still send now, in 1/1000 */
    char field[TN_FIELD_W * TN_FIELD_H + 1];
} tn_server_player;

typedef struct tn_server_pending {
    tn_conn conn;
    int used, age_ms, queries;
    unsigned char addr[16];      /* tn_tcp_peer_key */
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
