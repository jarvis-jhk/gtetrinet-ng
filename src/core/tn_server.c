/*
 * gtetrinet-ng — a small TetriNET server.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Message flow follows tetrinetx so classic clients feel at home: one channel, the lowest
 * occupied slot is the moderator, the server relays fields, specials and levels and decides
 * who won.
 */
#include "tn_server.h"
#include "version.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGIN_TIMEOUT_MS 15000

static void slog(tn_server *s, const char *fmt, ...)
{
    if (!s->log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[server] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void sendto_slot(tn_server *s, int i, const char *fmt, ...)
{
    char buf[TN_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (s->p[i].used) tn_conn_send(&s->p[i].conn, buf);
}

/* except = slot to leave out, 0 for everybody */
static void broadcast(tn_server *s, int except, const char *fmt, ...)
{
    char buf[TN_MSG_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (s->p[i].used && i != except) tn_conn_send(&s->p[i].conn, buf);
}

int tn_server_count(const tn_server *s)
{
    int n = 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++) n += s->p[i].used;
    return n;
}

int tn_server_start(tn_server *s, int port, int loopback_only, int max_slots, char *err, size_t errsz)
{
    memset(s, 0, sizeof *s);
    for (int i = 0; i <= TN_MAX_SLOTS; i++) s->p[i].conn.sock = TN_BAD_SOCKET;
    for (int i = 0; i < TN_SERVER_PENDING; i++) s->pending[i].conn.sock = TN_BAD_SOCKET;
    s->max_slots = max_slots < 1 ? TN_CLASSIC_SLOTS : max_slots > TN_MAX_SLOTS ? TN_MAX_SLOTS : max_slots;
    tn_rules_default(&s->rules);
    snprintf(s->motd, sizeof s->motd, "Welcome to %s %s", TN_APP_NAME, TN_VERSION);
    s->listener = tn_tcp_listen(port, loopback_only, err, errsz);
    if (s->listener == TN_BAD_SOCKET) return -1;
    s->port = tn_tcp_port(s->listener);
    return 0;
}

static void format_winlist(tn_server *s, char *out, size_t outsz)
{
    size_t o = 0;
    out[0] = 0;
    for (int i = 0; i < s->nwin && o + 1 < outsz; i++)
        o += (size_t)snprintf(out + o, outsz - o, "%s%s;%d", i ? " " : "", s->win[i].name, s->win[i].score);
}

static void add_win(tn_server *s, const char *name, int team, int points)
{
    char key[TN_NICK_MAX + 2];
    snprintf(key, sizeof key, "%c%s", team ? 't' : 'p', name);
    int i;
    for (i = 0; i < s->nwin; i++)
        if (strcmp(s->win[i].name, key) == 0) break;
    if (i == s->nwin) {
        if (s->nwin == (int)(sizeof s->win / sizeof s->win[0])) return;
        snprintf(s->win[s->nwin].name, sizeof s->win[0].name, "%s", key);
        s->win[s->nwin++].score = 0;
    }
    s->win[i].score += points;
    /* keep it sorted, best first */
    while (i > 0 && s->win[i].score > s->win[i - 1].score) {
        tn_winner t = s->win[i];
        s->win[i] = s->win[i - 1];
        s->win[i - 1] = t;
        i--;
    }
}

static int moderator(const tn_server *s)
{
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (s->p[i].used) return i;
    return 0;
}

static void end_game(tn_server *s)
{
    if (!s->ingame) return;
    s->ingame = s->paused = 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++) s->p[i].playing = 0;
    broadcast(s, 0, "endgame");
    slog(s, "game ended");
}

/* Ends the game when at most one team (or lone player) is still alive. */
static void check_winner(tn_server *s)
{
    if (!s->ingame) return;
    int alive = 0, last = 0;
    char seen[TN_MAX_SLOTS + 1][TN_TEAM_MAX];
    int nseen = 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++) {
        if (!s->p[i].used || !s->p[i].playing) continue;
        last = i;
        if (!s->p[i].team[0]) { alive++; continue; }
        int k;
        for (k = 0; k < nseen; k++)
            if (strcmp(seen[k], s->p[i].team) == 0) break;
        if (k == nseen) { snprintf(seen[nseen++], TN_TEAM_MAX, "%s", s->p[i].team); alive++; }
    }
    if (alive > 1) return;
    /* a solo game ends when its player is out; otherwise the last one standing wins */
    int total = tn_server_count(s);
    if (alive == 1 && total > 1) {
        broadcast(s, 0, "playerwon %d", last);
        if (s->p[last].team[0]) add_win(s, s->p[last].team, 1, 3);
        else add_win(s, s->p[last].nick, 0, 3);
        char wl[1024];
        format_winlist(s, wl, sizeof wl);
        broadcast(s, 0, "winlist %s", wl);
        slog(s, "%s won", s->p[last].nick);
        end_game(s);
    } else if (alive == 0 || total <= 1) {
        if (alive == 0) end_game(s);
    }
}

static void drop_player(tn_server *s, int i, const char *why)
{
    if (!s->p[i].used) return;
    slog(s, "slot %d (%s) left: %s", i, s->p[i].nick, why);
    tn_conn_close(&s->p[i].conn);
    int was_playing = s->p[i].playing;
    memset(&s->p[i], 0, sizeof s->p[i]);
    s->p[i].conn.sock = TN_BAD_SOCKET;
    broadcast(s, 0, "playerleave %d", i);
    if (s->ingame && tn_server_count(s) == 0) { s->ingame = s->paused = 0; return; }
    if (was_playing) check_winner(s);
}

static void field_reset(tn_server_player *p)
{
    memset(p->field, '0', TN_FIELD_W * TN_FIELD_H);
    p->field[TN_FIELD_W * TN_FIELD_H] = 0;
}

static void start_game(tn_server *s)
{
    char args[512];
    tn_rules_format(&s->rules, args, sizeof args);
    s->ingame = 1;
    s->paused = 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++) {
        if (!s->p[i].used) continue;
        s->p[i].playing = 1;
        s->p[i].level = s->rules.level;
        field_reset(&s->p[i]);
    }
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (s->p[i].used) sendto_slot(s, i, "%s %s", s->p[i].fast ? "*******" : "newgame", args);
    slog(s, "game started with %d player(s)", tn_server_count(s));
}

/* The query protocol of tetrinetx-derived servers, so server lists can show who is here.
 * Returns 1 if msg was a query; the connection stays open for the next one. */
static int query(tn_server *s, tn_server_pending *pd, const char *msg)
{
    char line[512];
    int n = tn_server_count(s);
    if (!strcmp(msg, "playerquery")) {
        snprintf(line, sizeof line, "Number of players logged in: %d", n);
        tn_conn_send_line(&pd->conn, line);
    } else if (!strcmp(msg, "version")) {
        snprintf(line, sizeof line, "%s %s", TN_APP_NAME, TN_VERSION);
        tn_conn_send_line(&pd->conn, line);
        tn_conn_send_line(&pd->conn, "+OK");
    } else if (!strcmp(msg, "listchan")) {
        snprintf(line, sizeof line, "\"%s\" \"%s\" %d %d 0 %d", TN_APP_NAME, s->max_slots > TN_CLASSIC_SLOTS ? "up to 8 players" : "TetriNET",
                 n, s->max_slots, s->ingame ? (s->paused ? 3 : 2) : 1);
        tn_conn_send_line(&pd->conn, line);
        tn_conn_send_line(&pd->conn, "+OK");
    } else if (!strcmp(msg, "listuser")) {
        for (int i = 1; i <= TN_MAX_SLOTS; i++) {
            if (!s->p[i].used) continue;
            snprintf(line, sizeof line, "\"%s\" \"%s\" \"1.13\" %d %d 1 \"%s\"", s->p[i].nick, s->p[i].team, i,
                     s->p[i].playing ? 1 : 0, TN_APP_NAME);
            tn_conn_send_line(&pd->conn, line);
        }
        tn_conn_send_line(&pd->conn, "+OK");
    } else {
        return 0;
    }
    return 1;
}

static void login(tn_server *s, tn_server_pending *pd, const char *msg)
{
    char plain[256], word[32], nick[TN_NICK_MAX], ver[32];
    if (tn_login_decode(msg, plain, sizeof plain) != 0) {
        tn_conn_send(&pd->conn, "noconnecting This is a TetriNET server, and that was not a TetriNET login.");
        return;
    }
    const char *rest = tn_word(plain, word, sizeof word);
    rest = tn_word(rest, nick, sizeof nick);
    tn_word(rest, ver, sizeof ver);
    int fast = strcmp(word, "tetrifaster") == 0;
    if (!nick[0]) { tn_conn_send(&pd->conn, "noconnecting No nickname given."); return; }
    if (strcmp(ver, "1.13") != 0) { tn_conn_send(&pd->conn, "noconnecting TetriNET 1.13 is needed."); return; }
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (s->p[i].used && strcmp(s->p[i].nick, nick) == 0) {
            tn_conn_send(&pd->conn, "noconnecting Nickname already exists on server!");
            return;
        }
    int slot = 0;
    for (int i = 1; i <= s->max_slots; i++)
        if (!s->p[i].used) { slot = i; break; }
    if (!slot) { tn_conn_send(&pd->conn, "noconnecting Server is Full!"); return; }

    tn_server_player *p = &s->p[slot];
    memset(p, 0, sizeof *p);
    p->conn = pd->conn;
    p->conn.outmax = TN_SERVER_OUT_MAX;
    p->budget = TN_SERVER_MSG_BURST * 1000;
    p->used = 1;
    p->fast = fast;
    snprintf(p->nick, sizeof p->nick, "%s", nick);
    field_reset(p);
    pd->used = 0;
    pd->conn.sock = TN_BAD_SOCKET;
    slog(s, "slot %d: %s%s", slot, nick, fast ? " (tetrifast)" : "");

    sendto_slot(s, slot, "%s %d", fast ? ")#)(!@(*3" : "playernum", slot);
    for (int i = 1; i <= TN_MAX_SLOTS; i++) {
        if (!s->p[i].used || i == slot) continue;
        sendto_slot(s, slot, "playerjoin %d %s", i, s->p[i].nick);
        sendto_slot(s, slot, "team %d %s", i, s->p[i].team);
    }
    char wl[1024];
    format_winlist(s, wl, sizeof wl);
    sendto_slot(s, slot, "winlist %s", wl);
    sendto_slot(s, slot, "pline 0 %s", s->motd);
    broadcast(s, slot, "playerjoin %d %s", slot, nick);
    if (s->ingame) {
        sendto_slot(s, slot, "ingame");
        for (int i = 1; i <= TN_MAX_SLOTS; i++)
            if (s->p[i].used && i != slot) sendto_slot(s, slot, "f %d %s", i, s->p[i].field);
        if (s->paused) sendto_slot(s, slot, "pause 1");
    }
}

/* the slot number a message names must be the sender's own */
static int level_arg(const char *s)
{
    long v = strtol(s, NULL, 10);
    return v < 0 ? 0 : v > 100 ? 100 : (int)v;
}

static int own_slot(const char **rest, int i)
{
    char w[16];
    *rest = tn_word(*rest, w, sizeof w);
    return atoi(w) == i;
}

static void handle(tn_server *s, int i, const char *msg)
{
    char cmd[32];
    const char *rest = tn_word(msg, cmd, sizeof cmd);
    tn_server_player *p = &s->p[i];

    if (strcmp(cmd, "team") == 0) {
        if (!own_slot(&rest, i)) return;
        snprintf(p->team, sizeof p->team, "%s", rest);
        broadcast(s, i, "team %d %s", i, p->team);
    } else if (strcmp(cmd, "pline") == 0) {
        if (!own_slot(&rest, i)) return;
        if (rest[0] == '/') {
            if (strncmp(rest, "/start", 6) == 0 && moderator(s) == i && !s->ingame) start_game(s);
            else if (strncmp(rest, "/stop", 5) == 0 && moderator(s) == i) end_game(s);
            else sendto_slot(s, i, "pline 0 Commands: /start /stop");
            return;
        }
        broadcast(s, i, "pline %d %s", i, rest);
    } else if (strcmp(cmd, "plineact") == 0) {
        if (!own_slot(&rest, i)) return;
        broadcast(s, i, "plineact %d %s", i, rest);
    } else if (strcmp(cmd, "startgame") == 0) {
        char w[8];
        rest = tn_word(rest, w, sizeof w);
        if (!own_slot(&rest, i) || moderator(s) != i) return;
        if (atoi(w) && !s->ingame) start_game(s);
        else if (!atoi(w)) end_game(s);
    } else if (strcmp(cmd, "pause") == 0) {
        char w[8];
        rest = tn_word(rest, w, sizeof w);
        if (!own_slot(&rest, i) || moderator(s) != i || !s->ingame) return;
        s->paused = atoi(w) != 0;
        broadcast(s, 0, "pause %d", s->paused);
    } else if (strcmp(cmd, "f") == 0) {
        if (!own_slot(&rest, i)) return;
        tn_field f;
        for (int k = 0; k < TN_FIELD_W * TN_FIELD_H; k++) (&f[0][0])[k] = (char)tn_cell_from_char(p->field[k]);
        tn_field_apply(f, rest);
        for (int k = 0; k < TN_FIELD_W * TN_FIELD_H; k++) p->field[k] = tn_cell_char((&f[0][0])[k]);
        broadcast(s, i, "f %d %s", i, rest);
    } else if (strcmp(cmd, "sb") == 0) {
        char to[8], id[8];
        rest = tn_word(rest, to, sizeof to);
        rest = tn_word(rest, id, sizeof id);
        if (atoi(rest) != i || tn_sb_from_id(id) < 0 || !s->ingame) return;
        broadcast(s, i, "sb %d %s %d", atoi(to), id, i);
    } else if (strcmp(cmd, "lvl") == 0) {
        if (!own_slot(&rest, i)) return;
        p->level = level_arg(rest);
        broadcast(s, i, "lvl %d %d", i, p->level);
    } else if (strcmp(cmd, "playerlost") == 0) {
        if (atoi(rest) != i || !p->playing) return;
        p->playing = 0;
        broadcast(s, 0, "playerlost %d", i);
        check_winner(s);
    } else if (strcmp(cmd, "gmsg") == 0) {
        if (s->ingame) broadcast(s, 0, "gmsg %s", rest);
    }
    /* clientinfo, version and anything unknown: ignored */
}

void tn_server_update(tn_server *s, int dt_ms)
{
    char msg[TN_MSG_MAX];
    if (s->listener == TN_BAD_SOCKET) return;
    for (;;) {
        tn_socket ns = tn_tcp_accept(s->listener);
        if (ns == TN_BAD_SOCKET) break;
        unsigned char addr[16];
        tn_tcp_peer_key(ns, addr);
        int k, free_k = -1, same = 0;
        for (k = 0; k < TN_SERVER_PENDING; k++) {
            if (!s->pending[k].used) { if (free_k < 0) free_k = k; }
            else same += memcmp(s->pending[k].addr, addr, sizeof addr) == 0;
        }
        if (free_k < 0 || same >= TN_SERVER_PER_ADDR) {
            slog(s, "connection refused: %s", free_k < 0 ? "too many waiting" : "too many from one address");
            tn_tcp_close(ns);
            continue;
        }
        tn_server_pending *pd = &s->pending[free_k];
        tn_conn_init(&pd->conn, ns);
        pd->conn.outmax = TN_SERVER_OUT_MAX;
        pd->used = 1;
        pd->age_ms = pd->queries = 0;
        memcpy(pd->addr, addr, sizeof addr);
    }
    for (int k = 0; k < TN_SERVER_PENDING; k++) {
        tn_server_pending *pd = &s->pending[k];
        if (!pd->used) continue;
        tn_conn_pump(&pd->conn);
        pd->age_ms += dt_ms;
        while (!pd->conn.dead && tn_conn_next(&pd->conn, msg, sizeof msg)) {
            if (query(s, pd, msg)) {
                tn_conn_pump(&pd->conn);
                if (++pd->queries >= TN_SERVER_QUERIES) pd->age_ms = LOGIN_TIMEOUT_MS + 1;
                if (pd->queries >= TN_SERVER_QUERIES) break;
                continue;
            }
            login(s, pd, msg);
            if (!pd->used) break;             /* became a player */
            tn_conn_pump(&pd->conn);          /* flush the refusal */
            pd->age_ms = LOGIN_TIMEOUT_MS - 500;
            break;
        }
        if (!pd->used) continue;
        if (pd->conn.dead || pd->age_ms > LOGIN_TIMEOUT_MS) {
            tn_conn_close(&pd->conn);
            pd->used = 0;
        }
    }
    for (int i = 1; i <= TN_MAX_SLOTS; i++) {
        if (!s->p[i].used) continue;
        tn_server_player *p = &s->p[i];
        tn_conn_pump(&p->conn);
        p->budget += (dt_ms <= 0 ? 0 : dt_ms > 10000 ? 10000 : dt_ms) * TN_SERVER_MSG_RATE;
        if (p->budget > TN_SERVER_MSG_BURST * 1000) p->budget = TN_SERVER_MSG_BURST * 1000;
        while (s->p[i].used && p->budget >= 1000 && tn_conn_next(&p->conn, msg, sizeof msg)) {
            p->budget -= 1000;
            handle(s, i, msg);
        }
        /* a whole buffer waiting behind the rate limit is no game client, it is a flood */
        if (p->used && !p->conn.dead && p->conn.inlen == sizeof p->conn.in) {
            p->conn.dead = 1;
            snprintf(p->conn.err, sizeof p->conn.err, "flooding");
        }
    }
    for (int i = 1; i <= TN_MAX_SLOTS; i++) {
        if (!s->p[i].used) continue;
        tn_conn_pump(&s->p[i].conn);
        if (s->p[i].conn.dead) drop_player(s, i, s->p[i].conn.err);
    }
}

void tn_server_stop(tn_server *s)
{
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (s->p[i].used) { tn_conn_pump(&s->p[i].conn); tn_conn_close(&s->p[i].conn); s->p[i].used = 0; }
    for (int k = 0; k < TN_SERVER_PENDING; k++)
        if (s->pending[k].used) { tn_conn_close(&s->pending[k].conn); s->pending[k].used = 0; }
    if (s->listener != TN_BAD_SOCKET) tn_tcp_close(s->listener);
    s->listener = TN_BAD_SOCKET;
}
