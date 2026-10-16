/*
 * gtetrinet-ng — TetriNET for up to eight players on one screen, and online.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 1999-2003 Ka-shu Wong and the GTetrinet contributors
 * Copyright (C) 2026 gtetrinet-ng contributors
 */
#include "app.h"
#include "fault.h"
#include "i18n.h"
#include "render.h"
#include "sound.h"
#include "version.h"
#include "tn_bot.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { MENU_LOCAL, MENU_ONLINE, MENU_NEWS, MENU_QUIT, MENU_COUNT };

void gtn_set_status(gtn_app *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->status, sizeof a->status, fmt, ap);
    va_end(ap);
    a->status_ms = 6000;
    SDL_Log("%s", a->status);
}

/* ---------------------------------------------------------------- changelog */

static int read_file(const char *path, char *buf, size_t n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t k = fread(buf, 1, n - 1, f);
    buf[k] = 0;
    fclose(f);
    return 0;
}

static int load_changelog(char *buf, size_t n)
{
    char *base = SDL_GetBasePath();
    const char *rel[] = {"CHANGELOG.md", "../CHANGELOG.md", "../share/gtetrinet-ng/CHANGELOG.md", "../../CHANGELOG.md"};
    int ok = -1;
    for (size_t i = 0; i < sizeof rel / sizeof *rel && ok != 0; i++) {
        char path[1200];
        snprintf(path, sizeof path, "%s%s", base ? base : "", rel[i]);
        ok = read_file(path, buf, n);
    }
    SDL_free(base);
    return ok;
}

/* Keeps the sections newer than `since` (all of them if since is empty or unknown). */
static void changelog_since(char *text, const char *since)
{
    if (!since[0]) return;
    char marker[48];
    snprintf(marker, sizeof marker, "\n## %s", since);
    char *p = strstr(text, marker);
    if (p) *p = 0;
}

/* The changelog is hard-wrapped for a text editor; joining a paragraph's lines lets the news
 * screen wrap it at its own width instead of breaking every source line twice. */
static void reflow(char *text)
{
    for (char *p = strchr(text, '\n'); p; p = strchr(p + 1, '\n')) {
        const char *next = p + 1;
        int prev_blank = p == text || p[-1] == '\n';
        if (prev_blank || *next == '\n' || !*next || *next == '#' || *next == '-' || *next == '*' || *next == ' ')
            continue;
        /* a heading line keeps its own line */
        const char *start = p;
        while (start > text && start[-1] != '\n') start--;
        if (*start == '#') continue;
        *p = ' ';
    }
}

static void open_news(gtn_app *a, int only_new)
{
    if (load_changelog(a->news, sizeof a->news) != 0) {
        snprintf(a->news, sizeof a->news, "%s", gtn_tr("The changelog could not be found next to the game."));
    } else {
        if (only_new) changelog_since(a->news, a->settings.last_version);
        reflow(a->news);
        /* The entries themselves are written once, in English; say so instead of mixing. */
        const char *note = gtn_tr("(The changelog is only available in English.)");
        size_t k = strlen(note);
        if (gtn_lang_get() != GTN_LANG_EN && strlen(a->news) + k + 2 < sizeof a->news) {
            memmove(a->news + k + 2, a->news, strlen(a->news) + 1);
            memcpy(a->news, note, k);
            a->news[k] = a->news[k + 1] = '\n';
        }
    }
    a->news_scroll = 0;
    a->news_back = a->screen;
    a->screen = SCR_NEWS;
}

/* ---------------------------------------------------------------- local players and sessions */

static int back_held[GTN_MAX_LOCAL];

static void local_nick(gtn_app *a, int idx, char *out, size_t n)
{
    if (idx > 0 && idx < GTN_SEATS && a->settings.seat_nick[idx][0]) {
        snprintf(out, n, "%s", a->settings.seat_nick[idx]);
        return;
    }
    int k = 0;
    for (int i = 0; i < idx; i++) k += a->pl[i].active;
    if (k == 0) snprintf(out, n, "%s", a->settings.nick);
    else snprintf(out, n, "%.24s%d", a->settings.nick, k + 1);
}

static int connected_any(gtn_app *a)
{
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].client.state != TN_OFFLINE && a->pl[i].client.state != TN_FAILED) return 1;
    return 0;
}

static void connect_player(gtn_app *a, int i)
{
    gtn_local *p = &a->pl[i];
    tn_client_init(&p->client, p->nick, "", (uint32_t)time(NULL) * 2654435761u + (uint32_t)i * 7919u);
    tn_client_connect(&p->client, a->host, a->port, a->fast);
    p->target = 0;
    p->failed = 0;
}

static void join(gtn_app *a, enum gtn_dev dev, SDL_JoystickID pad)
{
    /* a controller that lost its player gets that player back first */
    if (dev == DEV_PAD)
        for (int i = 0; i < GTN_MAX_LOCAL; i++)
            if (a->pl[i].active && a->pl[i].dev == DEV_PAD && a->pl[i].pad == -1) {
                a->pl[i].pad = pad;
                gtn_set_status(a, gtn_tr("%s has a controller again"), a->pl[i].nick);
                return;
            }
    if (a->screen == SCR_GAME) return; /* new players join between games */
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        if (a->pl[i].active) continue;
        gtn_local *p = &a->pl[i];
        memset(p, 0, sizeof *p);
        back_held[i] = 0;
        local_nick(a, i, p->nick, sizeof p->nick);
        p->active = 1;
        p->dev = dev;
        p->pad = pad;
        tn_client_init(&p->client, p->nick, "", 1);
        if (a->screen == SCR_LOBBY || a->screen == SCR_CONNECTING) connect_player(a, i);
        return;
    }
    gtn_set_status(a, "%s", gtn_tr("Eight players is the most that fit"));
}

static void leave_local(gtn_app *a, int i)
{
    tn_client_disconnect(&a->pl[i].client);
    a->pl[i].active = 0;
}

/* ---------------------------------------------------------------- computer players */

const char *gtn_bot_level_name(int level)
{
    static const char *const names[TN_BOT_LEVELS] = {N_("Easy"), N_("Normal"), N_("Hard")};
    return gtn_tr(names[level >= 0 && level < TN_BOT_LEVELS ? level : TN_BOT_NORMAL]);
}

/* Computer players take a free seat in a game on this machine only: on someone else's server
 * they would be uninvited guests. They leave again when the session ends. */
static void add_bot(gtn_app *a)
{
    if (!a->hosting) { gtn_set_status(a, "%s", gtn_tr("Computer players can only join a game on this machine")); return; }
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        if (a->pl[i].active) continue;
        gtn_local *p = &a->pl[i];
        memset(p, 0, sizeof *p);
        back_held[i] = 0;
        for (int n = 1; n <= GTN_MAX_LOCAL && !p->nick[0]; n++) {
            char nick[TN_NICK_MAX];
            snprintf(nick, sizeof nick, "CPU%d", n);
            int taken = 0;
            for (int k = 0; k < GTN_MAX_LOCAL; k++) taken |= a->pl[k].active && !strcmp(a->pl[k].nick, nick);
            if (!taken) snprintf(p->nick, sizeof p->nick, "%s", nick);
        }
        p->active = 1;
        p->dev = DEV_BOT;
        p->pad = -1;
        tn_bot_init_level(&p->bot, a->settings.bot_level, (unsigned)time(NULL) + 977u * (unsigned)i);
        connect_player(a, i);
        return;
    }
    gtn_set_status(a, "%s", gtn_tr("Eight players is the most that fit"));
}

static void remove_bot(gtn_app *a)
{
    for (int i = GTN_MAX_LOCAL - 1; i >= 0; i--)
        if (a->pl[i].active && a->pl[i].dev == DEV_BOT) { leave_local(a, i); return; }
    gtn_set_status(a, "%s", gtn_tr("No computer players to remove"));
}

static void cycle_bot_level(gtn_app *a)
{
    a->settings.bot_level = (a->settings.bot_level + 1) % TN_BOT_LEVELS;
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].dev == DEV_BOT)
            tn_bot_init_level(&a->pl[i].bot, a->settings.bot_level, (unsigned)time(NULL) + 977u * (unsigned)i);
    if (gtn_settings_save(&a->settings) != 0)
        gtn_set_status(a, "%s", gtn_tr("The strength works for this run, but could not be saved"));
}

static void stop_session(gtn_app *a)
{
    int left = 0;
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        tn_client *c = &a->pl[i].client;
        /* tetrinet-server dies of SIGPIPE if it relays one player's leave to another who hung
         * up in the same instant, so local players leave someone else's server one by one */
        if (a->pl[i].active && c->state == TN_CONNECTED && !a->hosting && left++) SDL_Delay(60);
        if (a->pl[i].active) tn_client_disconnect(c);
        if (a->pl[i].dev == DEV_BOT) a->pl[i].active = 0;
    }
    if (a->hosting) tn_server_stop(&a->server);
    a->hosting = 0;
    a->screen = SCR_TITLE;
}

static void start_session(gtn_app *a)
{
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        if (!a->pl[i].active) continue;
        a->pl[i].nick_tries = 0;
        connect_player(a, i);
    }
    a->screen = SCR_CONNECTING;
}

static void host_local(gtn_app *a)
{
    char err[160] = "";
    /* the usual port lets friends on the same network join; any free one will do otherwise */
    if (tn_server_start(&a->server, TN_PORT, 0, TN_MAX_SLOTS, err, sizeof err) != 0 &&
        tn_server_start(&a->server, 0, 0, TN_MAX_SLOTS, err, sizeof err) != 0) {
        gtn_fault("starting the local server failed", err);
        gtn_set_status(a, gtn_tr("Could not start a game here: %s"), err);
        return;
    }
    a->hosting = 1;
    snprintf(a->host, sizeof a->host, "127.0.0.1");
    a->port = a->server.port;
    a->fast = 0;
    start_session(a);
}

static void connect_online(gtn_app *a)
{
    char host[128];
    int port;
    if (tn_query_split(a->entry, host, sizeof host, &port) != 0) {
        gtn_set_status(a, "%s", gtn_tr("Enter a server address first"));
        return;
    }
    snprintf(a->host, sizeof a->host, "%s", host);
    a->port = port;
    a->fast = a->settings.fast;
    snprintf(a->settings.host, sizeof a->settings.host, "%s", a->entry);
    gtn_settings_save(&a->settings);
    gtn_browser_close(a);
    start_session(a);
}

/* the local player whose client is the moderator, -1 if none */
static int local_moderator(gtn_app *a)
{
    /* A client's own view can lag: one in slot 2 that has not yet seen slot 1 join
     * believes it moderates, and the server silently ignores its start. Combine
     * what every local client knows before choosing. */
    int lowest = 0;
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        const tn_client *c = &a->pl[i].client;
        /* Until every local seat has its slot number, the lowest one is unknown. */
        if (a->pl[i].active && (c->state == TN_CONNECTING || c->state == TN_LOGGING_IN)) return -1;
        if (!a->pl[i].active || c->state != TN_CONNECTED) continue;
        for (int s = 1; s <= TN_MAX_SLOTS && (!lowest || s < lowest); s++)
            if (c->slot[s].present) { lowest = s; break; }
    }
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (lowest && a->pl[i].active && a->pl[i].client.state == TN_CONNECTED && a->pl[i].client.me == lowest
            && tn_client_moderator(&a->pl[i].client)) return i;
    return -1;
}

static void cycle_target(gtn_local *p, int dir)
{
    tn_client *c = &p->client;
    int t = p->target >= 1 ? p->target : c->me;
    for (int k = 0; k < TN_MAX_SLOTS; k++) {
        t = ((t - 1 + dir + TN_MAX_SLOTS) % TN_MAX_SLOTS) + 1;
        if (c->slot[t].playing) { p->target = t; return; }
    }
}

/* ---------------------------------------------------------------- buttons per screen */

static void online_key(gtn_app *a, const char *key)
{
    if (!strcmp(key, "DEL")) gtn_text_input(a, "\b");
    else if (!strcmp(key, "GO")) gtn_text_input(a, "\n");
    else gtn_text_input(a, key);
}

/* ---------------------------------------------------------------- names */

static void open_name(gtn_app *a, int who)
{
    a->name_who = who;
    gtn_nick_clean(a->entry, sizeof a->entry, a->pl[who].nick);
    a->osk_row = a->osk_col = 0;
    a->osk_shift = 0;
    a->screen = SCR_NAME;
    SDL_StartTextInput();
}

static void save_name(gtn_app *a)
{
    char name[GTN_NAME_MAX];
    int who = a->name_who;
    if (!gtn_nick_clean(name, sizeof name, a->entry)) {
        gtn_set_status(a, "%s", gtn_tr("A name needs at least one letter or digit"));
        return;
    }
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (i != who && a->pl[i].active && !strcmp(a->pl[i].nick, name)) {
            gtn_set_status(a, gtn_tr("%s is already playing here"), name);
            return;
        }
    if (who == 0) snprintf(a->settings.nick, sizeof a->settings.nick, "%s", name);
    else snprintf(a->settings.seat_nick[who], sizeof a->settings.seat_nick[who], "%s", name);
    /* Seats still on their default name follow P1's new one. */
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        gtn_local *p = &a->pl[i];
        if (!p->active) continue;
        if (i == who) snprintf(p->nick, sizeof p->nick, "%s", name);
        else if (!a->settings.seat_nick[i][0]) local_nick(a, i, p->nick, sizeof p->nick);
        tn_client_init(&p->client, p->nick, "", 1);
    }
    if (gtn_settings_save(&a->settings) != 0)
        gtn_set_status(a, "%s", gtn_tr("The name works for this run, but could not be saved"));
    SDL_StopTextInput();
    a->screen = SCR_TITLE;
}

void gtn_text_input(gtn_app *a, const char *t)
{
    if (a->screen == SCR_NAME) {
        size_t n = strlen(a->entry);
        if (!strcmp(t, "\b")) { if (n) a->entry[n - 1] = 0; return; }
        if (!strcmp(t, "\n")) { save_name(a); return; }
        for (; *t && n + 1 < GTN_NAME_MAX; t++)
            if ((unsigned char)*t > ' ' && (unsigned char)*t < 127) a->entry[n++] = *t;
        a->entry[n] = 0;
        return;
    }
    if (a->screen != SCR_ONLINE) return;
    size_t n = strlen(a->entry);
    if (!strcmp(t, "\b")) { if (n) a->entry[n - 1] = 0; return; }
    if (!strcmp(t, "\n")) { connect_online(a); return; }
    for (; *t && n + 1 < sizeof a->entry; t++)
        if ((unsigned char)*t > ' ' && (unsigned char)*t < 127) a->entry[n++] = *t;
    a->entry[n] = 0;
}

static void title_button(gtn_app *a, int who, enum gtn_btn b)
{
    switch (b) {
    case BTN_UP: a->menu_sel = (a->menu_sel + MENU_COUNT - 1) % MENU_COUNT; break;
    case BTN_DOWN: a->menu_sel = (a->menu_sel + 1) % MENU_COUNT; break;
    case BTN_B: leave_local(a, who); break;
    case BTN_Y: open_name(a, who); break;
    case BTN_A:
    case BTN_START:
        switch (a->menu_sel) {
        case MENU_LOCAL: host_local(a); break;
        case MENU_ONLINE:
            gtn_browser_open(a);
            a->screen = SCR_SERVERS;
            break;
        case MENU_NEWS: open_news(a, 0); break;
        case MENU_QUIT: a->quit = 1; break;
        }
        break;
    default: break;
    }
}

static int osk_cols(const char *row)
{
    char key[8];
    int n = 0;
    for (const char *q = row; *q; ) { q = tn_word(q, key, sizeof key); n++; }
    return n;
}

/* Moves over an on-screen keyboard given as space-separated rows; on A copies the chosen
 * key to key and returns 1. */
static int osk_button(gtn_app *a, const char *const *rows, int nrows, enum gtn_btn b, char *key, size_t n)
{
    int pressed = 0;
    switch (b) {
    case BTN_UP: a->osk_row = (a->osk_row + nrows - 1) % nrows; break;
    case BTN_DOWN: a->osk_row = (a->osk_row + 1) % nrows; break;
    case BTN_LEFT: a->osk_col--; break;
    case BTN_RIGHT: a->osk_col++; break;
    case BTN_A: pressed = 1; break;
    default: break;
    }
    /* keep the column inside the row after moving */
    int ncols = osk_cols(rows[a->osk_row]);
    if (a->osk_col < 0) a->osk_col = ncols - 1;
    if (a->osk_col >= ncols) a->osk_col = 0;
    if (!pressed) return 0;
    const char *q = rows[a->osk_row];
    for (int col = 0; col <= a->osk_col; col++) q = tn_word(q, key, n);
    return 1;
}

static void open_address(gtn_app *a)
{
    snprintf(a->entry, sizeof a->entry, "%s", a->settings.host);
    a->osk_row = a->osk_col = 0;
    a->screen = SCR_ONLINE;
    SDL_StartTextInput();
}

static void servers_button(gtn_app *a, enum gtn_btn b)
{
    gtn_browser *br = &a->browser;
    int rows = br->n + 1;
    switch (b) {
    case BTN_UP: br->sel = (br->sel + rows - 1) % rows; break;
    case BTN_DOWN: br->sel = (br->sel + 1) % rows; break;
    case BTN_A:
    case BTN_START:
        if (br->sel >= br->n) { open_address(a); break; }
        snprintf(a->entry, sizeof a->entry, "%s", br->e[br->sel].addr);
        connect_online(a);
        break;
    case BTN_X: gtn_browser_refresh(a); break;
    case BTN_Y: a->settings.fast = !a->settings.fast; break;
    case BTN_B: gtn_browser_close(a); a->screen = SCR_TITLE; break;
    default: break;
    }
}

static void online_button(gtn_app *a, enum gtn_btn b)
{
    char key[8];
    if (osk_button(a, gtn_osk_rows, gtn_osk_nrows(), b, key, sizeof key)) { online_key(a, key); return; }
    switch (b) {
    case BTN_X: gtn_text_input(a, "\b"); break;
    case BTN_Y: a->settings.fast = !a->settings.fast; break;
    case BTN_START: gtn_text_input(a, "\n"); break;
    case BTN_B:
        SDL_StopTextInput();
        if (!a->browser.n) gtn_browser_open(a);
        a->browser.sel = a->browser.n;
        a->screen = SCR_SERVERS;
        break;
    default: break;
    }
}

static void name_button(gtn_app *a, enum gtn_btn b)
{
    char key[8];
    if (osk_button(a, gtn_name_rows, gtn_name_nrows(), b, key, sizeof key)) {
        if (!strcmp(key, "DEL")) gtn_text_input(a, "\b");
        else if (!strcmp(key, "OK")) gtn_text_input(a, "\n");
        else if (!strcmp(key, "Aa")) a->osk_shift = !a->osk_shift;
        else {
            if (a->osk_shift && key[0] >= 'a' && key[0] <= 'z') key[0] = (char)(key[0] - 'a' + 'A');
            gtn_text_input(a, key);
        }
        return;
    }
    switch (b) {
    case BTN_X: gtn_text_input(a, "\b"); break;
    case BTN_Y: a->osk_shift = !a->osk_shift; break;
    case BTN_START: gtn_text_input(a, "\n"); break;
    case BTN_B: SDL_StopTextInput(); a->screen = SCR_TITLE; break;
    default: break;
    }
}

static void lobby_button(gtn_app *a, int who, enum gtn_btn b)
{
    gtn_local *p = &a->pl[who];
    switch (b) {
    case BTN_START:
    case BTN_A: {
        int m = local_moderator(a);
        if (m < 0) { gtn_set_status(a, "%s", gtn_tr("Only the moderator (the first player on the server) can start")); break; }
        tn_client_start_game(&a->pl[m].client);
        break;
    }
    case BTN_B:
        if (gtn_local_count(a) > 1 && b == BTN_B && p->client.state == TN_CONNECTED && who != local_moderator(a)) {
            leave_local(a, who);
        } else {
            stop_session(a);
        }
        break;
    case BTN_Y: {
        /* teams: cycle no team / Red / Blue / Green / Yellow */
        static const char *teams[] = {"", "Red", "Blue", "Green", "Yellow"};
        int k = 0;
        for (int i = 0; i < 5; i++) if (!strcmp(p->client.team, teams[i])) k = i;
        tn_client_set_team(&p->client, teams[(k + 1) % 5]);
        break;
    }
    case BTN_BACK: open_news(a, 0); break;
    case BTN_X: add_bot(a); break;
    case BTN_LB: remove_bot(a); break;
    case BTN_RB: cycle_bot_level(a); break;
    default: break;
    }
}

static void game_button(gtn_app *a, int who, enum gtn_btn b, int down)
{
    gtn_local *p = &a->pl[who];
    tn_client *c = &p->client;
    /* A controller's own layout; the keyboard keeps classic TetriNET keys. */
    if (p->dev == DEV_PAD) b = (enum gtn_btn)gtn_pad_translate(a->settings.pad_map, b);
    if (b == BTN_DOWN) { tn_client_action(c, down ? TN_ACT_SOFT_PRESS : TN_ACT_SOFT_RELEASE); return; }
    if (!down) return;
    switch (b) {
    case BTN_LEFT: tn_client_action(c, TN_ACT_LEFT); break;
    case BTN_RIGHT: tn_client_action(c, TN_ACT_RIGHT); break;
    case BTN_UP: tn_client_action(c, TN_ACT_DROP); break;
    case BTN_A: tn_client_action(c, TN_ACT_ROT_CW); break;
    case BTN_B: tn_client_action(c, TN_ACT_ROT_CCW); break;
    case BTN_X:
        if (p->target < 1 || !c->slot[p->target].playing) cycle_target(p, 1);
        tn_client_special(c, p->target >= 1 ? p->target : c->me);
        break;
    case BTN_Y: tn_client_special(c, c->me); break;
    case BTN_LB: cycle_target(p, -1); break;
    case BTN_RB: cycle_target(p, 1); break;
    case BTN_BACK: tn_client_action(c, TN_ACT_DISCARD); break;
    case BTN_START: {
        int m = local_moderator(a);
        if (m >= 0) tn_client_pause(&a->pl[m].client, !a->pl[m].client.paused);
        break;
    }
    default: break;
    }
}

#define OPTION_ROWS 5

static void save_controls(gtn_app *a)
{
    if (gtn_settings_save(&a->settings) != 0)
        gtn_set_status(a, "%s", gtn_tr("The controller buttons work for this run, but could not be saved"));
}

/* Choose an action, press A, then press the button it should have. The keyboard cannot
 * stand in for a controller here, so any key cancels the wait. */
static void controls_button(gtn_app *a, enum gtn_dev dev, enum gtn_btn b)
{
    if (a->controls_capture) {
        a->controls_capture = 0;
        if (dev != DEV_PAD || !gtn_pad_bindable(b)) return;
        gtn_pad_bind(a->settings.pad_map, a->controls_row, b);
        save_controls(a);
        return;
    }
    switch (b) {
    case BTN_UP: a->controls_row = (a->controls_row + PAD_ACTIONS - 1) % PAD_ACTIONS; break;
    case BTN_DOWN: a->controls_row = (a->controls_row + 1) % PAD_ACTIONS; break;
    case BTN_A: a->controls_capture = 1; break;
    case BTN_Y:
        gtn_pad_defaults(a->settings.pad_map);
        save_controls(a);
        break;
    case BTN_B: case BTN_START: a->screen = SCR_INPUT; break;
    default: break;
    }
}

/* A short rumble on one controller; nothing for the keyboard or a controller that is gone. */
static void rumble_pad(gtn_app *a, SDL_JoystickID pad, float low, float high, int ms)
{
    if (a->settings.rumble_off || pad < 0) return;
    SDL_GameController *gc = SDL_GameControllerFromInstanceID(pad);
    if (gc) SDL_GameControllerRumble(gc, (Uint16)(low * 65535), (Uint16)(high * 65535), (Uint32)ms);
}

static void rumble(gtn_app *a, int i, float low, float high, int ms)
{
    gtn_local *p = &a->pl[i];
    if (a->settings.rumble_off || p->dev != DEV_PAD) return;
    p->rumbles++;
    rumble_pad(a, p->pad, low, high, ms);
}

/* Turns what happened to a local player since the last frame into sound and rumble. The screen
 * is shared, so sounds are for everyone (and the same sound plays once per frame); the rumble
 * is the personal channel that tells a player it was them. */
static void feedback(gtn_app *a, int i)
{
    gtn_local *p = &a->pl[i];
    const tn_events *e = &p->client.ev, *o = &p->seen;
    if (e->locks < o->locks || e->clears < o->clears || e->specials < o->specials || e->hits < o->hits ||
        e->losses < o->losses || e->wins < o->wins) {
        p->seen = *e;                /* a new client session: its counters started again */
        return;
    }
    if (e->losses != o->losses) {
        gtn_sound_play(SFX_LOSE);
        rumble(a, i, 0.9f, 0.9f, 450);
    }
    if (e->wins != o->wins) {
        gtn_sound_play(SFX_WIN);
        rumble(a, i, 0.3f, 0.6f, 300);
    }
    if (e->hits != o->hits) {
        switch (gtn_special_effect(e->last_hit)) {
        case GTN_EFFECT_HURT: gtn_sound_play(SFX_HIT_HURT); rumble(a, i, 0.75f, 0.35f, 220); break;
        case GTN_EFFECT_HELP: gtn_sound_play(SFX_HIT_HELP); rumble(a, i, 0.0f, 0.35f, 90); break;
        default: gtn_sound_play(SFX_HIT_MIXED); rumble(a, i, 0.45f, 0.45f, 160); break;
        }
    }
    if (e->specials != o->specials) gtn_sound_play(SFX_SPECIAL);
    if (e->clears != o->clears) {
        int n = e->last_clear < 1 ? 1 : e->last_clear > 4 ? 4 : e->last_clear;
        gtn_sound_play((enum gtn_sfx)(SFX_CLEAR1 + n - 1));
        if (n == 4) rumble(a, i, 0.2f, 0.5f, 150);
    } else if (e->locks != o->locks) gtn_sound_play(SFX_LOCK);
    p->seen = *e;
}

static void input_options_button(gtn_app *a, enum gtn_btn b, enum gtn_dev dev, SDL_JoystickID pad)
{
    int dir = 0;
    switch (b) {
    case BTN_UP: a->options_row = (a->options_row + OPTION_ROWS - 1) % OPTION_ROWS; return;
    case BTN_DOWN: a->options_row = (a->options_row + 1) % OPTION_ROWS; return;
    case BTN_LEFT: dir = -1; break;
    case BTN_RIGHT: case BTN_A: dir = 1; break;
    case BTN_Y: break;
    case BTN_B: case BTN_START: a->screen = SCR_TITLE; return;
    default: return;
    }
    if (a->options_row == 3) {
        a->settings.volume = dir ? (a->settings.volume + VOLUME_COUNT + dir) % VOLUME_COUNT : VOLUME_NORMAL;
        gtn_sound_volume(a->settings.volume);
        gtn_sound_play(SFX_CLEAR2);  /* so the new level can be heard */
        if (gtn_settings_save(&a->settings) != 0)
            gtn_set_status(a, "%s", gtn_tr("Sound works for this run, but could not be saved"));
        return;
    }
    if (a->options_row == 4) {
        a->settings.rumble_off = dir ? !a->settings.rumble_off : 0;
        if (dev == DEV_PAD) rumble_pad(a, pad, 0.5f, 0.5f, 200);  /* feel it switch on */
        if (gtn_settings_save(&a->settings) != 0)
            gtn_set_status(a, "%s", gtn_tr("Rumble works for this run, but could not be saved"));
        return;
    }
    if (a->options_row == 2) {
        if (b == BTN_Y) {
            gtn_pad_defaults(a->settings.pad_map);
            save_controls(a);
        } else {
            a->screen = SCR_CONTROLS;
            a->controls_row = 0;
            a->controls_capture = 0;
        }
        return;
    }
    if (a->options_row == 1) {
        a->settings.lang = dir ? (a->settings.lang + LANG_CHOICES + dir) % LANG_CHOICES : LANG_AUTO;
        gtn_lang_apply(a->settings.lang);
        if (gtn_settings_save(&a->settings) != 0)
            gtn_set_status(a, "%s", gtn_tr("The language works for this run, but could not be saved"));
        return;
    }
    int profile = a->settings.repeat_profile;
    a->settings.repeat_profile = dir ? (profile + REPEAT_COUNT + dir) % REPEAT_COUNT : REPEAT_NORMAL;
    if (gtn_settings_save(&a->settings) != 0)
        gtn_set_status(a, "%s", gtn_tr("Movement repeat works for this run, but could not be saved"));
}

void gtn_button(gtn_app *a, int who, enum gtn_dev dev, SDL_JoystickID pad, enum gtn_btn b, int down)
{
    /* Options work before joining too; their A button must not claim a player seat. */
    if (a->screen == SCR_INPUT) {
        if (down) input_options_button(a, b, dev, pad);
        return;
    }
    if (a->screen == SCR_CONTROLS) {
        if (down) controls_button(a, dev, b);
        return;
    }
    /* Only the player being named types; the keyboard can always help, and anyone may
     * close the screen when that player's controller is gone. */
    if (a->screen == SCR_NAME) {
        const gtn_local *p = &a->pl[a->name_who];
        int lost = !p->active || (p->dev == DEV_PAD && p->pad < 0);
        if (down && (who == a->name_who || dev == DEV_KEYBOARD || (lost && b == BTN_B))) name_button(a, b);
        return;
    }
    if (a->screen == SCR_TITLE && down && b == BTN_X) {
        a->screen = SCR_INPUT;
        a->options_row = 0;
        return;
    }
    if (who < 0) {
        if (down && (b == BTN_A || b == BTN_START) && a->screen != SCR_NEWS && a->screen != SCR_ONLINE) join(a, dev, pad);
        else if (down && a->screen == SCR_ONLINE) online_button(a, b);
        else if (down && a->screen == SCR_SERVERS) servers_button(a, b);
        else if (down && a->screen == SCR_NEWS && (b == BTN_A || b == BTN_B)) a->screen = a->news_back;
        else if (down && a->screen == SCR_TITLE && (b == BTN_UP || b == BTN_DOWN)) title_button(a, -1, b);
        return;
    }
    if (b == BTN_BACK) back_held[who] = down;
    if (a->screen == SCR_GAME) {
        /* Back + Start: the moderator ends the game for everybody */
        if (down && b == BTN_START && back_held[who]) {
            int m = local_moderator(a);
            if (m >= 0) tn_client_stop_game(&a->pl[m].client);
            return;
        }
        game_button(a, who, b, down);
        return;
    }
    if (!down) return;
    switch (a->screen) {
    case SCR_TITLE: title_button(a, who, b); break;
    case SCR_SERVERS: servers_button(a, b); break;
    case SCR_ONLINE: online_button(a, b); break;
    case SCR_CONNECTING: if (b == BTN_B) stop_session(a); break;
    case SCR_LOBBY: lobby_button(a, who, b); break;
    case SCR_NEWS:
        if (b == BTN_UP) a->news_scroll = a->news_scroll > 0 ? a->news_scroll - 1 : 0;
        else if (b == BTN_DOWN) a->news_scroll++;
        else if (b == BTN_LB) a->news_scroll = a->news_scroll > 10 ? a->news_scroll - 10 : 0;
        else if (b == BTN_RB) a->news_scroll += 10;
        else if (b == BTN_A || b == BTN_B || b == BTN_START) a->screen = a->news_back;
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------- per frame */

static void update_session(gtn_app *a, int dt)
{
    gtn_sound_frame();
    if (a->hosting) tn_server_update(&a->server, dt);
    if (a->screen != SCR_CONNECTING && a->screen != SCR_LOBBY && a->screen != SCR_GAME) return;
    int any_ingame = 0, connected = 0, pending = 0, active = 0;
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        gtn_local *p = &a->pl[i];
        if (!p->active) continue;
        active++;
        tn_client_update(&p->client, dt);
        tn_client *c = &p->client;
        if (p->dev == DEV_BOT && c->ingame) tn_bot_update(&p->bot, c, dt);
        if (c->state == TN_FAILED && strstr(c->err, "Nickname") && p->nick_tries < 5) {
            /* somebody online already has this name: try the next number */
            p->nick_tries++;
            snprintf(p->nick, sizeof p->nick, "%.18s_%d", p->dev == DEV_BOT ? "CPU" : a->settings.nick, i + 1 + p->nick_tries * 8);
            connect_player(a, i);
            pending++;
            continue;
        }
        if (c->state == TN_FAILED) {
            if (!p->failed) {
                char why[sizeof c->err + 64];
                gtn_tr_log(gtn_tr(c->err), why, sizeof why);
                gtn_set_status(a, gtn_tr("%s could not connect: %s"), p->nick, why);
                p->failed = 1;
            }
            /* While nobody is in yet the seat stays: if every seat fails (a typo, a server
             * without TetriFast) the whole couch is back on the title screen still joined. */
            if (a->screen != SCR_CONNECTING) p->active = 0;
            continue;
        }
        if (c->state == TN_CONNECTED) connected++;
        else if (c->state == TN_CONNECTING || c->state == TN_LOGGING_IN) pending++;
        if (c->ingame) any_ingame = 1;
        if (c->hits != p->seen_hits) { p->seen_hits = c->hits; p->flash_ms = 350; }
        if (p->flash_ms > 0) p->flash_ms -= dt;
        feedback(a, i);
    }
    if (active == 0 || (connected == 0 && pending == 0)) {
        if (a->screen != SCR_TITLE) {
            if (!a->status_ms) gtn_set_status(a, "%s", gtn_tr("Disconnected"));
            stop_session(a);
        }
        return;
    }
    if (a->screen == SCR_CONNECTING && pending == 0) {
        a->screen = SCR_LOBBY;
        /* only a server somebody got onto goes into the list, not a typo */
        if (!a->hosting && connected) {
            gtn_settings_remember(&a->settings, a->settings.host);
            gtn_settings_save(&a->settings);
        }
        for (int i = 0; i < GTN_MAX_LOCAL; i++)
            if (a->pl[i].active && a->pl[i].failed) a->pl[i].active = 0;
    }
    if (a->screen == SCR_LOBBY && any_ingame) {
        a->screen = SCR_GAME;
        for (int i = 0; i < GTN_MAX_LOCAL; i++) a->pl[i].target = 0;
    }
    if (a->screen == SCR_GAME && !any_ingame) {
        a->screen = SCR_LOBBY;
        a->banner_ms = 5000;
    }
    (void)connected_any;
}

static void open_initial_controllers(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);
}

static int screenshot(gtn_app *a, const char *path)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, a->w, a->h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return -1;
    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
    SDL_RenderReadPixels(a->ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    int r = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return r;
}

/* --art DIR: Steam library artwork made from a running demo game: the game in the background,
 * the logo on top. Written as BMP; the release workflow converts them to PNG. */
static void render_art(gtn_app *a, const char *dir)
{
    static const struct { const char *name; int w, h, logo; } art[] = {
        {"p", 600, 900, 1},        /* portrait capsule */
        {"", 920, 430, 1},         /* wide capsule */
        {"_hero", 1920, 620, 0},   /* hero, Steam puts the logo on top */
        {"_icon", 256, 256, 2},
        {"_logo", 1280, 320, 3},   /* logo only, transparent */
    };
    for (size_t i = 0; i < sizeof art / sizeof *art; i++) {
        /* drawn into an offscreen texture of exactly the right size */
        SDL_Texture *t = SDL_CreateTexture(a->ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, art[i].w, art[i].h);
        if (!t || SDL_SetRenderTarget(a->ren, t) != 0) { fprintf(stderr, "art: %s\n", SDL_GetError()); return; }
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        a->w = art[i].w;
        a->h = art[i].h;
        if (art[i].logo == 3) {
            SDL_SetRenderDrawColor(a->ren, 0, 0, 0, 0);
            SDL_RenderClear(a->ren);
        } else {
            gtn_draw(a);
            gtn_fill(0, 0, a->w, a->h, (gtn_color){0x0b, 0x0d, 0x14, (Uint8)(art[i].logo ? 150 : 60)});
        }
        if (art[i].logo == 1 || art[i].logo == 3) {
            const char *t = "GTetrinet NG";
            int sc = a->w / (int)(strlen(t) * 8 + 4);
            gtn_logo_at(a, (a->h - 8 * sc) / 2, sc);
        } else if (art[i].logo == 2) {
            gtn_fill(0, 0, a->w, a->h, (gtn_color){0x0b, 0x0d, 0x14, 255});
            static const int shape[3][3] = {{1, 1, 1}, {0, 1, 0}, {0, 0, 0}};
            int cs = a->w / 5;
            for (int y = 0; y < 3; y++)
                for (int x = 0; x < 3; x++)
                    if (shape[y][x]) gtn_cell(cs + x * cs, cs + cs / 2 + y * cs, cs, 4);
            gtn_cell(2 * cs, cs * 5 / 2 + cs, cs, 6);
        }
        char path[1024];
        snprintf(path, sizeof path, "%s/steam%s.bmp", dir, art[i].name);
        if (screenshot(a, path) != 0) fprintf(stderr, "art %s failed: %s\n", path, SDL_GetError());
        SDL_SetRenderTarget(a->ren, NULL);
        SDL_DestroyTexture(t);
    }
}

/* --demo: a scripted session for screenshots and smoke tests: n local players on a local
 * server, the game started, everybody dropping pieces at random. */
static int demo_nostart;
static void demo_tick(gtn_app *a, int t_ms, int nplayers)
{
    static int started;
    if (demo_nostart) started = 1;
    if (t_ms < 50) return;
    if (a->screen == SCR_TITLE && gtn_local_count(a) < nplayers) {
        join(a, DEV_PAD, 1000 + gtn_local_count(a));
        return;
    }
    if (a->screen == SCR_TITLE) { host_local(a); return; }
    if (a->screen == SCR_LOBBY && !started) {
        int m = local_moderator(a);
        if (m >= 0) { tn_client_start_game(&a->pl[m].client); started = 1; }
        return;
    }
    if (a->screen != SCR_GAME) return;
    static tn_bot bots[GTN_MAX_LOCAL];
    static int bots_ready;
    if (!bots_ready) {
        for (int i = 0; i < GTN_MAX_LOCAL; i++) tn_bot_init(&bots[i], 60 + 25 * i);
        bots_ready = 1;
    }
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        gtn_local *p = &a->pl[i];
        if (!p->active) continue;
        tn_bot_update(&bots[i], &p->client, 16);
        p->target = 0;
    }
}

static void usage(void)
{
    printf("%s %s\n"
           "usage: gtetrinet-ng [--connect host[:port]] [--nick name] [--fast] [--lang auto|en|de]\n"
           "                    [--window|--fullscreen]\n"
           "                    [--demo N --frames F --screenshot out.bmp] [--version]\n",
           TN_APP_NAME, TN_VERSION);
}

int main(int argc, char **argv)
{
    static gtn_app app;
    gtn_app *a = &app;
    const char *connect_to = NULL, *shot = NULL, *artdir = NULL;
    int demo = 0, frames = 0, fullscreen = -1, cli_lang = 0;
    gtn_fault_install();
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--connect") && i + 1 < argc) connect_to = argv[++i];
        else if (!strcmp(argv[i], "--nick") && i + 1 < argc) snprintf(a->settings.nick, sizeof a->settings.nick, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--fast")) a->fast = 1;
        else if (!strcmp(argv[i], "--lang") && i + 1 < argc) cli_lang = gtn_lang_parse(argv[++i]) + 1;
        else if (!strcmp(argv[i], "--demo") && i + 1 < argc) demo = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--demo-lobby")) demo_nostart = 1;
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--art") && i + 1 < argc) artdir = argv[++i];
        else if (!strcmp(argv[i], "--window")) fullscreen = 0;
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
        else if (!strcmp(argv[i], "--version")) { printf("%s %s\n", TN_APP_NAME, TN_VERSION); return 0; }
        else { usage(); return strcmp(argv[i], "--help") ? 2 : 0; }
    }
    char cli_nick[64];
    snprintf(cli_nick, sizeof cli_nick, "%s", a->settings.nick);
    int cli_fast = a->fast;

    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0"); /* A is the bottom button everywhere */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        gtn_fault("SDL_Init failed", SDL_GetError());
        return 1;
    }
    if (tn_net_init() != 0) gtn_fault("network init failed", "");
    gtn_settings_load(&a->settings);
    if (cli_nick[0]) snprintf(a->settings.nick, sizeof a->settings.nick, "%s", cli_nick);
    a->settings.fast |= cli_fast;
    /* --lang is for this run (screenshots, tests); the saved choice stays as it was */
    gtn_lang_apply(cli_lang ? cli_lang - 1 : a->settings.lang);
    gtn_sound_volume(a->settings.volume);
    gtn_sound_init();

    /* SteamOS sets SteamDeck=1; Steam sets SteamGameId when it launched us (Game Mode) */
    a->steamdeck = getenv("SteamDeck") && !strcmp(getenv("SteamDeck"), "1");
    if (fullscreen < 0) fullscreen = a->steamdeck || getenv("SteamGameId") != NULL;
    Uint32 wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    a->win = SDL_CreateWindow("gtetrinet-ng", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800, wflags);
    if (!a->win) { gtn_fault("no window", SDL_GetError()); return 1; }
    a->ren = SDL_CreateRenderer(a->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!a->ren) a->ren = SDL_CreateRenderer(a->win, -1, SDL_RENDERER_SOFTWARE);
    if (!a->ren) { gtn_fault("no renderer", SDL_GetError()); return 1; }
    if (gtn_render_init(a->ren) != 0) { gtn_fault("font init", SDL_GetError()); return 1; }
    open_initial_controllers();
    SDL_StopTextInput();

    a->screen = SCR_TITLE;
    if (!demo && strcmp(a->settings.last_version, TN_VERSION) != 0 && strstr(TN_VERSION, "dev") == NULL) {
        if (a->settings.last_version[0]) open_news(a, 1);
        snprintf(a->settings.last_version, sizeof a->settings.last_version, "%s", TN_VERSION);
        gtn_settings_save(&a->settings);
    }
    if (connect_to) {
        join(a, DEV_KEYBOARD, 0);
        snprintf(a->entry, sizeof a->entry, "%s", connect_to);
        a->screen = SCR_ONLINE;
        connect_online(a);
    }

    Uint64 last = SDL_GetTicks64(), t0 = last;
    int frame = 0;
    srand((unsigned)time(NULL));
    while (!a->quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) a->quit = 1;
            else gtn_input_event(a, &e);
        }
        Uint64 now = SDL_GetTicks64();
        int dt = (int)(now - last);
        if (demo) dt = 16;               /* deterministic pace for screenshots */
        if (dt > 250) dt = 250;          /* after a stall, do not fast-forward the game */
        last = now;
        a->frame_ms = dt;
        if (a->status_ms > 0 && (a->status_ms -= dt) <= 0) { a->status_ms = 0; a->status[0] = 0; }
        if (a->banner_ms > 0) a->banner_ms -= dt;
        if (demo) demo_tick(a, frame * 16, demo);
        gtn_input_update(a, dt);
        update_session(a, dt);
        gtn_browser_update(a);
        SDL_GetRendererOutputSize(a->ren, &a->w, &a->h);
        gtn_draw(a);
        frame++;
        if (frames && frame >= frames) {
            if (shot && screenshot(a, shot) != 0) fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());
            if (artdir) render_art(a, artdir);
            a->quit = 1;
        }
        SDL_RenderPresent(a->ren);
        if (demo) SDL_Delay(1);
        (void)t0;
    }
    stop_session(a);
    gtn_browser_close(a);
    gtn_sound_quit();
    gtn_render_quit();
    SDL_DestroyRenderer(a->ren);
    SDL_DestroyWindow(a->win);
    SDL_Quit();
    return 0;
}
