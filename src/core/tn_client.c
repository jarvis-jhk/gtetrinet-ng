/*
 * gtetrinet-ng — one TetriNET client session.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 1999-2003 Ka-shu Wong and the GTetrinet contributors (original behaviour)
 * Copyright (C) 2026 gtetrinet-ng contributors
 */
#include "tn_client.h"
#include "version.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEXTBLOCK_MS(c) ((c)->fast ? 0 : 1000)
#define SOFTDROP_MS 100

void tn_log_add(tn_log *l, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(l->line[l->head], TN_LOG_LEN, fmt, ap);
    va_end(ap);
    /* strip TetriNET's colour/attribute control bytes */
    char *s = l->line[l->head], *d = s;
    for (; *s; s++)
        if ((unsigned char)*s >= 32) *d++ = *s;
    *d = 0;
    l->head = (l->head + 1) % TN_LOG_LINES;
    if (l->count < TN_LOG_LINES) l->count++;
    l->serial++;
}

const char *tn_log_get(const tn_log *l, int i)
{
    if (i < 0 || i >= l->count) return "";
    return l->line[(l->head - 1 - i + 2 * TN_LOG_LINES) % TN_LOG_LINES];
}

static void send_msg(tn_client *c, const char *fmt, ...)
{
    char buf[TN_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (c->state == TN_CONNECTED || c->state == TN_LOGGING_IN) tn_conn_send(&c->conn, buf);
}

void tn_client_init(tn_client *c, const char *nick, const char *team, uint32_t seed)
{
    memset(c, 0, sizeof *c);
    snprintf(c->nick, sizeof c->nick, "%s", nick);
    for (char *p = c->nick; *p; p++)
        if (isspace((unsigned char)*p)) *p = '_';
    snprintf(c->team, sizeof c->team, "%s", team ? team : "");
    c->conn.sock = TN_BAD_SOCKET;
    c->conn.dead = 1;
    c->seed = seed;
    tn_rules_default(&c->rules);
}

void tn_client_connect(tn_client *c, const char *host, int port, int fast)
{
    tn_client_disconnect(c);
    snprintf(c->host, sizeof c->host, "%s", host);
    c->port = port ? port : TN_PORT;
    c->fast = fast;
    c->err[0] = 0;
    tn_socket s = tn_tcp_connect(host, c->port, c->err, sizeof c->err);
    if (s == TN_BAD_SOCKET) { c->state = TN_FAILED; return; }
    tn_conn_init(&c->conn, s);
    c->state = TN_CONNECTING;
}

static void reset_lobby(tn_client *c)
{
    c->me = 0;
    memset(c->slot, 0, sizeof c->slot);
    c->ingame = c->paused = c->playing = 0;
}

void tn_client_disconnect(tn_client *c)
{
    if (c->conn.sock != TN_BAD_SOCKET) tn_conn_close(&c->conn);
    if (c->state != TN_FAILED) c->state = TN_OFFLINE;
    reset_lobby(c);
}

static void send_field(tn_client *c, int full)
{
    char data[TN_FIELD_W * TN_FIELD_H * 3 + 16];
    tn_field_encode(c->game.field, c->sent, full, data, sizeof data);
    if (!data[0]) return;
    send_msg(c, "f %d %s", c->me, data);
    memcpy(c->sent, c->game.field, sizeof(tn_field));
    memcpy(c->slot[c->me].field, c->game.field, sizeof(tn_field));
}

static void player_lost(tn_client *c)
{
    if (!c->playing) return;
    c->playing = 0;
    c->slot[c->me].playing = 0;
    c->ev.losses++;
    tn_game_fill_lost(&c->game);
    send_field(c, 1);
    send_msg(c, "playerlost %d", c->me);
}

static const char *slot_name(const tn_client *c, int s)
{
    if (s < 1 || s > TN_MAX_SLOTS || !c->slot[s].nick[0]) return "?";
    return c->slot[s].nick;
}

/* Effects of a special on us, with the classic client's targeting rules. */
static void do_special(tn_client *c, int from, int to, int sb)
{
    if (to)
        tn_log_add(&c->attacks, "%s on %s by %s", tn_sb_name(sb), slot_name(c, to), from ? slot_name(c, from) : "server");
    else
        tn_log_add(&c->attacks, "%s to All by %s", tn_sb_name(sb), from ? slot_name(c, from) : "server");
    if (!c->playing) return;
    if (from == c->me && sb == TN_SB_SWITCH) {
        /* we switch too: we get the target's field */
        from = to;
        to = c->me;
    }
    if (!(to == 0 && from != c->me) && to != c->me) return;
    if (to == 0 && c->team[0] && from >= 1 && from <= TN_MAX_SLOTS && strcmp(c->slot[from].team, c->team) == 0)
        return;
    const char (*other)[TN_FIELD_W] = (from >= 1 && from <= TN_MAX_SLOTS) ? (const char (*)[TN_FIELD_W])c->slot[from].field : NULL;
    if (from != c->me) {
        c->hits++;
        c->ev.hits++;
        c->ev.last_hit = sb;
    }
    if (tn_game_apply_special(&c->game, sb, other) < 0) {
        player_lost(c);
        return;
    }
    send_field(c, 0);
}

static void slot_clear(tn_slot *s)
{
    memset(s, 0, sizeof *s);
}

static int parse_slot(const char **rest)
{
    char w[16];
    *rest = tn_word(*rest, w, sizeof w);
    if (!w[0]) return -1;
    int n = atoi(w);
    return (n >= 0 && n <= TN_MAX_SLOTS) ? n : -1;
}

static void start_game(tn_client *c, const char *args)
{
    tn_rules_parse(&c->rules, args);
    c->ingame = 1;
    c->paused = 0;
    c->last_winner = 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++) {
        memset(c->slot[i].field, 0, sizeof(tn_field));
        c->slot[i].playing = c->slot[i].present;
        c->slot[i].level = c->rules.level;
    }
    c->seed = c->seed * 1103515245u + 12345u + (uint32_t)c->me;
    tn_game_start(&c->game, &c->rules, c->seed);
    memset(c->sent, 0, sizeof c->sent);
    c->playing = c->me > 0;
    c->fall_acc = c->downcount = c->soft_held = c->soft_steps = 0;
    c->spawn_wait = 0;
    if (c->playing) {
        send_field(c, 1);
        if (tn_game_spawn(&c->game) < 0) player_lost(c);
    }
    tn_log_add(&c->chat, "*** The game has started");
}

static void end_game(tn_client *c)
{
    c->ingame = c->playing = c->paused = 0;
    c->game.block = TN_NO_BLOCK;
}

void tn_client_handle(tn_client *c, const char *msg)
{
    char cmd[32];
    const char *rest = tn_word(msg, cmd, sizeof cmd);
    int n;

    if (strcmp(cmd, "noconnecting") == 0) {
        snprintf(c->err, sizeof c->err, "%s", *rest ? rest : "the server refused the connection");
        tn_conn_close(&c->conn);
        c->state = TN_FAILED;
        return;
    }
    if (strcmp(cmd, "playernum") == 0 || strcmp(cmd, ")#)(!@(*3") == 0) {
        n = atoi(rest);
        if (n < 1 || n > TN_MAX_SLOTS) return;
        c->me = n;
        snprintf(c->slot[n].nick, sizeof c->slot[n].nick, "%s", c->nick);
        snprintf(c->slot[n].team, sizeof c->slot[n].team, "%s", c->team);
        c->slot[n].present = 1;
        if (c->state != TN_CONNECTED) {
            c->state = TN_CONNECTED;
            tn_log_add(&c->chat, "*** Connected to %s", c->host);
        }
        send_msg(c, "team %d %s", c->me, c->team);
        return;
    }
    if (strcmp(cmd, "playerjoin") == 0) {
        if ((n = parse_slot(&rest)) < 1) return;
        slot_clear(&c->slot[n]);
        snprintf(c->slot[n].nick, sizeof c->slot[n].nick, "%s", rest);
        c->slot[n].present = 1;
        tn_log_add(&c->chat, "*** %s has joined", rest);
        return;
    }
    if (strcmp(cmd, "playerleave") == 0) {
        if ((n = parse_slot(&rest)) < 1) return;
        if (c->slot[n].present) tn_log_add(&c->chat, "*** %s has left", c->slot[n].nick);
        slot_clear(&c->slot[n]);
        return;
    }
    if (strcmp(cmd, "kick") == 0) {
        if ((n = parse_slot(&rest)) < 1) return;
        tn_log_add(&c->chat, "*** %s has been kicked", slot_name(c, n));
        return;
    }
    if (strcmp(cmd, "team") == 0) {
        if ((n = parse_slot(&rest)) < 1) return;
        snprintf(c->slot[n].team, sizeof c->slot[n].team, "%s", rest);
        return;
    }
    if (strcmp(cmd, "pline") == 0) {
        if ((n = parse_slot(&rest)) < 0) return;
        if (n == 0) {
            if (strncmp(rest, "\4\4\4\4", 4) != 0) tn_log_add(&c->chat, "%s", rest);
        } else {
            tn_log_add(&c->chat, "<%s> %s", slot_name(c, n), rest);
        }
        return;
    }
    if (strcmp(cmd, "plineact") == 0) {
        if ((n = parse_slot(&rest)) < 0) return;
        tn_log_add(&c->chat, "* %s %s", slot_name(c, n), rest);
        return;
    }
    if (strcmp(cmd, "playerlost") == 0) {
        n = atoi(rest);
        if (n >= 1 && n <= TN_MAX_SLOTS) c->slot[n].playing = 0;
        return;
    }
    if (strcmp(cmd, "playerwon") == 0) {
        n = atoi(rest);
        if (n < 1 || n > TN_MAX_SLOTS) return;
        c->last_winner = n;
        if (c->me && (n == c->me || (c->team[0] && strcmp(c->slot[n].team, c->team) == 0))) c->ev.wins++;
        if (c->slot[n].team[0]) tn_log_add(&c->chat, "*** Team %s has won the game", c->slot[n].team);
        else tn_log_add(&c->chat, "*** %s has won the game", slot_name(c, n));
        return;
    }
    if (strcmp(cmd, "newgame") == 0 || strcmp(cmd, "*******") == 0 || strcmp(cmd, "btrixnewgame") == 0) {
        start_game(c, rest);
        return;
    }
    if (strcmp(cmd, "ingame") == 0) {
        /* joined while a game runs: we watch until the next one */
        c->ingame = 1;
        c->playing = 0;
        c->paused = 0;
        if (c->me) {
            tn_game_fill_lost(&c->game);
            memset(c->sent, 0, sizeof c->sent);
            send_field(c, 1);
        }
        tn_log_add(&c->chat, "*** A game is in progress");
        return;
    }
    if (strcmp(cmd, "pause") == 0) {
        c->paused = atoi(rest) != 0;
        tn_log_add(&c->chat, c->paused ? "*** The game is paused" : "*** The game continues");
        return;
    }
    if (strcmp(cmd, "endgame") == 0) {
        end_game(c);
        tn_log_add(&c->chat, "*** The game has ended");
        return;
    }
    if (strcmp(cmd, "f") == 0) {
        if ((n = parse_slot(&rest)) < 1) return;
        if (n == c->me && c->playing) return; /* we own our field */
        tn_field_apply(c->slot[n].field, rest);
        return;
    }
    if (strcmp(cmd, "sb") == 0) {
        int to, from, sb;
        char id[8];
        if ((to = parse_slot(&rest)) < 0) return;
        rest = tn_word(rest, id, sizeof id);
        from = atoi(rest);
        if (from < 0 || from > TN_MAX_SLOTS) return;
        if ((sb = tn_sb_from_id(id)) < 0) return;
        if (c->ingame) do_special(c, from, to, sb);
        return;
    }
    if (strcmp(cmd, "lvl") == 0) {
        if ((n = parse_slot(&rest)) < 0) return;
        if (n == 0 && atoi(rest) == 0) {
            /* the server asks who we are */
            send_msg(c, "clientinfo %s %s", TN_APP_NAME, TN_VERSION);
            return;
        }
        if (n >= 1) c->slot[n].level = atoi(rest);
        return;
    }
    if (strcmp(cmd, "gmsg") == 0) {
        tn_log_add(&c->chat, "%s", rest);
        return;
    }
    if (strcmp(cmd, "winlist") == 0) {
        snprintf(c->winlist, sizeof c->winlist, "%s", rest);
        return;
    }
}

int tn_client_gravity_level(const tn_client *c)
{
    if (!c->rules.average_levels) return c->game.level;
    int t = 0, k = 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (c->slot[i].playing) {
            t += i == c->me ? c->game.level : c->slot[i].level;
            k++;
        }
    return k ? t / k : c->game.level;
}

static void after_solidify(tn_client *c)
{
    int lvl_changed = 0;
    int lines = tn_game_clear_lines(&c->game, &lvl_changed);
    c->lines_total += (unsigned)lines;
    c->ev.locks++;
    if (lines > 0) {
        c->ev.clears++;
        c->ev.last_clear = lines;
    }
    if (lvl_changed) {
        c->slot[c->me].level = c->game.level;
        send_msg(c, "lvl %d %d", c->me, c->game.level);
    }
    if (lines && c->rules.classic) {
        int sb = lines == 2 ? TN_SB_ADDALL1 : lines == 3 ? TN_SB_ADDALL2 : lines >= 4 ? TN_SB_ADDALL4 : -1;
        if (sb >= 0) {
            send_msg(c, "sb 0 %s %d", tn_sb_id(sb), c->me);
            do_special(c, c->me, 0, sb);
        }
    }
    send_field(c, 0);
    c->spawn_wait = NEXTBLOCK_MS(c) > 0 ? NEXTBLOCK_MS(c) : 1;
    c->downcount = 0;
}

static void solidify(tn_client *c)
{
    if (tn_game_solidify(&c->game) < 0) {
        player_lost(c);
        return;
    }
    after_solidify(c);
}

/* one gravity step, as GTetrinet's timeout: a piece that cannot fall locks on the second try */
static void gravity_step(tn_client *c)
{
    if (tn_game_down(&c->game)) {
        if (c->downcount) {
            solidify(c);
        } else {
            c->downcount++;
        }
    } else {
        c->downcount = 0;
    }
}

void tn_client_update(tn_client *c, int dt_ms)
{
    char msg[TN_MSG_MAX];
    if (c->state == TN_CONNECTING) {
        int r = tn_tcp_connected(c->conn.sock, c->err, sizeof c->err);
        if (r < 0) { tn_conn_close(&c->conn); c->state = TN_FAILED; return; }
        if (r == 1) {
            unsigned char ip[4];
            char login[128], enc[512];
            tn_tcp_peer_ipv4(c->conn.sock, ip);
            snprintf(login, sizeof login, "%s %s 1.13", c->fast ? "tetrifaster" : "tetrisstart", c->nick);
            tn_login_encode(login, ip, 0, enc, sizeof enc);
            c->state = TN_LOGGING_IN;
            tn_conn_send(&c->conn, enc);
        }
    }
    if (c->state == TN_LOGGING_IN || c->state == TN_CONNECTED) {
        tn_conn_pump(&c->conn);
        while (c->state != TN_FAILED && tn_conn_next(&c->conn, msg, sizeof msg)) tn_client_handle(c, msg);
        if (c->state != TN_FAILED && c->conn.dead) {
            int was = c->state;
            if (was == TN_LOGGING_IN && c->fast)
                /* tetrinetx and tetrinet-server only decode the classic login and hang up */
                snprintf(c->err, sizeof c->err, "the server does not accept TetriFast - pick classic with Y");
            else
                snprintf(c->err, sizeof c->err, "%s", c->conn.err[0] ? c->conn.err : "disconnected");
            tn_conn_close(&c->conn);
            reset_lobby(c);
            c->state = was == TN_CONNECTED ? TN_OFFLINE : TN_FAILED;
            if (was == TN_CONNECTED) tn_log_add(&c->chat, "*** Disconnected: %s", c->err);
            return;
        }
    }

    if (!c->playing || c->paused || dt_ms <= 0) goto flush;
    if (c->spawn_wait > 0) {
        c->spawn_wait -= dt_ms;
        if (c->spawn_wait > 0) goto flush;
        c->spawn_wait = 0;
        c->fall_acc = 0;
        if (tn_game_spawn(&c->game) < 0) { player_lost(c); goto flush; }
    }
    {
        int interval = c->soft_held ? SOFTDROP_MS : tn_gravity_ms(tn_client_gravity_level(c));
        c->fall_acc += dt_ms;
        while (c->playing && c->spawn_wait == 0 && c->fall_acc >= interval) {
            c->fall_acc -= interval;
            if (c->soft_held) c->soft_held++;
            gravity_step(c);
        }
        if (c->spawn_wait) c->fall_acc = 0;
    }
flush:
    if (c->state == TN_CONNECTED || c->state == TN_LOGGING_IN) tn_conn_pump(&c->conn);
}

void tn_client_action(tn_client *c, enum tn_action a)
{
    if (!c->playing || c->paused) return;
    int piece = c->game.block != TN_NO_BLOCK && c->spawn_wait == 0;
    switch (a) {
    case TN_ACT_LEFT: if (piece) tn_game_move(&c->game, -1); break;
    case TN_ACT_RIGHT: if (piece) tn_game_move(&c->game, 1); break;
    case TN_ACT_ROT_CW: if (piece) tn_game_rotate(&c->game, 1); break;
    case TN_ACT_ROT_CCW: if (piece) tn_game_rotate(&c->game, -1); break;
    case TN_ACT_SOFT_PRESS:
        if (!c->soft_held && piece) {
            gravity_step(c);
            c->soft_held = 1;
            c->fall_acc = 0;
        }
        break;
    case TN_ACT_SOFT_RELEASE:
        /* a quick tap nudges it one more row, as in the original */
        if (c->soft_held == 1 && piece) gravity_step(c);
        c->soft_held = 0;
        c->fall_acc = 0;
        break;
    case TN_ACT_DROP:
        if (piece) {
            tn_game_drop(&c->game);
            solidify(c);
        }
        break;
    case TN_ACT_DISCARD:
        tn_game_pop_special(&c->game);
        break;
    }
}

void tn_client_special(tn_client *c, int target)
{
    if (!c->playing || c->paused || c->game.nspecials == 0) return;
    if (target < 1 || target > TN_MAX_SLOTS || !c->slot[target].playing) return;
    int sb = tn_game_pop_special(&c->game);
    if (sb < 0) return;
    send_msg(c, "sb %d %s %d", target, tn_sb_id(sb), c->me);
    c->ev.specials++;
    do_special(c, c->me, target, sb);
}

int tn_client_moderator(const tn_client *c)
{
    if (!c->me) return 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (c->slot[i].present) return i == c->me;
    return 0;
}

void tn_client_start_game(tn_client *c) { send_msg(c, "startgame 1 %d", c->me); }
void tn_client_stop_game(tn_client *c) { send_msg(c, "startgame 0 %d", c->me); }
void tn_client_pause(tn_client *c, int on) { send_msg(c, "pause %d %d", on ? 1 : 0, c->me); }

void tn_client_say(tn_client *c, const char *text)
{
    if (!c->me || !text[0]) return;
    send_msg(c, "pline %d %s", c->me, text);
    if (text[0] != '/') tn_log_add(&c->chat, "<%s> %s", c->nick, text);
}

void tn_client_set_team(tn_client *c, const char *team)
{
    snprintf(c->team, sizeof c->team, "%s", team);
    if (c->me) {
        snprintf(c->slot[c->me].team, sizeof c->slot[c->me].team, "%s", team);
        send_msg(c, "team %d %s", c->me, team);
    }
}

void tn_client_view(const tn_client *c, tn_field out)
{
    if (c->playing) {
        tn_game_view(&c->game, out);
        if (c->spawn_wait > 0) memcpy(out, c->game.field, sizeof(tn_field));
    } else if (c->me) {
        memcpy(out, c->slot[c->me].field, sizeof(tn_field));
    } else {
        memset(out, 0, sizeof(tn_field));
    }
}
