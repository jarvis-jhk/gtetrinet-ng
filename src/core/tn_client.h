/*
 * gtetrinet-ng — one TetriNET client session: a connection, the lobby it sees and the game it
 * plays. A splitscreen process runs one of these per local player.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TN_CLIENT_H
#define TN_CLIENT_H

#include "tn_game.h"
#include "tn_net.h"

enum tn_client_state { TN_OFFLINE, TN_CONNECTING, TN_LOGGING_IN, TN_CONNECTED, TN_FAILED };

enum tn_action {
    TN_ACT_LEFT, TN_ACT_RIGHT, TN_ACT_ROT_CW, TN_ACT_ROT_CCW,
    TN_ACT_SOFT_PRESS, TN_ACT_SOFT_RELEASE, TN_ACT_DROP, TN_ACT_DISCARD
};

#define TN_LOG_LINES 64
#define TN_LOG_LEN 160

typedef struct tn_log {
    char line[TN_LOG_LINES][TN_LOG_LEN];
    int head, count;   /* ring buffer; line (head - 1) is the newest */
    unsigned serial;   /* bumps on every add, so the UI can notice new lines */
} tn_log;

void tn_log_add(tn_log *l, const char *fmt, ...);
/* i = 0 is the newest line */
const char *tn_log_get(const tn_log *l, int i);

typedef struct tn_slot {
    char nick[TN_NICK_MAX];
    char team[TN_TEAM_MAX];
    int present, playing, level;
    tn_field field;
} tn_slot;

/* Things that happened to us, for sound and rumble: the UI compares them with the values it
 * saw last frame. Counters only grow; last_* describe the newest event of that kind. */
typedef struct tn_events {
    unsigned locks;                 /* pieces placed */
    unsigned clears;                /* placements that cleared lines */
    int last_clear;                 /* lines in the newest clear */
    unsigned specials;              /* specials we used (not classic add-to-all) */
    unsigned hits;                  /* specials from others that reached us */
    int last_hit;                   /* TN_SB_* of the newest hit */
    unsigned losses;                /* we lost a game */
    unsigned wins;                  /* we, or our team, won a game */
} tn_events;

typedef struct tn_client {
    enum tn_client_state state;
    char err[160];
    char host[128];
    int port;
    int fast;                       /* tetrifast mode */
    char nick[TN_NICK_MAX], team[TN_TEAM_MAX];
    tn_conn conn;

    int me;                         /* our slot, 0 until the server said */
    tn_slot slot[TN_MAX_SLOTS + 1]; /* index 1..TN_MAX_SLOTS */
    int ingame, paused, playing;    /* playing: we are alive in the current game */
    int last_winner;                /* slot that won the last game, 0 if unknown */
    tn_rules rules;
    tn_game game;
    tn_field sent;

    /* timers, in ms */
    int fall_acc, spawn_wait, downcount, soft_held, soft_steps;

    tn_log chat;                    /* partyline: chat and server messages */
    tn_log attacks;                 /* specials used, "Add Line on Anna by Jan" */
    char winlist[512];              /* raw "pJan;3 tFoo;1" */

    /* counters the UI can watch for effects */
    unsigned hits;                  /* specials that hit us */
    unsigned lines_total;
    tn_events ev;
    uint32_t seed;
} tn_client;

void tn_client_init(tn_client *c, const char *nick, const char *team, uint32_t seed);
/* Starts connecting. fast = tetrifast login. */
void tn_client_connect(tn_client *c, const char *host, int port, int fast);
void tn_client_disconnect(tn_client *c);

/* Network and game time. Call every frame. */
void tn_client_update(tn_client *c, int dt_ms);

void tn_client_action(tn_client *c, enum tn_action a);
/* Uses the first special on a slot (our own slot targets ourselves). */
void tn_client_special(tn_client *c, int target);

int tn_client_moderator(const tn_client *c); /* are we the lowest occupied slot? */
void tn_client_start_game(tn_client *c);
void tn_client_stop_game(tn_client *c);
void tn_client_pause(tn_client *c, int on);
void tn_client_say(tn_client *c, const char *text);
void tn_client_set_team(tn_client *c, const char *team);

/* Our field with the falling piece drawn in. */
void tn_client_view(const tn_client *c, tn_field out);
/* Gravity interval in ms, given the levels (averaged when the rules say so). */
int tn_client_gravity_level(const tn_client *c);

/* For tests: handles one message from the server as if it had arrived. */
void tn_client_handle(tn_client *c, const char *msg);

#endif
