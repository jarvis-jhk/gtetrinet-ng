/*
 * gtetrinet-ng — drawing the screens.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "app.h"
#include "render.h"
#include "i18n.h"
#include "version.h"

#include <stdio.h>
#include <string.h>

const char *const gtn_osk_rows[] = {
    "1 2 3 4 5 6 7 8 9 0",
    "q w e r t y u i o p",
    "a s d f g h j k l",
    "z x c v b n m . - :",
    "DEL GO",
};
int gtn_osk_nrows(void) { return (int)(sizeof gtn_osk_rows / sizeof *gtn_osk_rows); }

/* Names are one word, so there is no space key; Aa switches to capitals. */
const char *const gtn_name_rows[] = {
    "1 2 3 4 5 6 7 8 9 0",
    "q w e r t y u i o p",
    "a s d f g h j k l _",
    "z x c v b n m . - !",
    "Aa DEL OK",
};
int gtn_name_nrows(void) { return (int)(sizeof gtn_name_rows / sizeof *gtn_name_rows); }

static gtn_color BG = {0x0b, 0x0d, 0x14, 255};
static gtn_color PANEL = {0x16, 0x19, 0x24, 255};
static gtn_color TEXT = {0xe6, 0xe8, 0xf0, 255};
static gtn_color MUTED = {0x8a, 0x90, 0xa6, 255};
static gtn_color ACCENT = {0xff, 0xc8, 0x3d, 255};
static gtn_color BAD = {0xff, 0x4a, 0x4a, 255};

static int ts(const gtn_app *a) { return a->h >= 700 && a->w >= 1000 ? 2 : 1; }

/* the client whose view of the lobby we show: the first connected local player */
static const tn_client *view_client(const gtn_app *a)
{
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].client.state == TN_CONNECTED) return &a->pl[i].client;
    return NULL;
}

/* which local player (index) sits in a slot, -1 if a remote one */
static int local_of_slot(const gtn_app *a, int slot)
{
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].client.state == TN_CONNECTED && a->pl[i].client.me == slot) return i;
    return -1;
}

static void hints(gtn_app *a, const char *text)
{
    int s = ts(a);
    gtn_fill(0, a->h - 14 * s, a->w, 14 * s, PANEL);
    gtn_text_fit(8 * s, a->h - 11 * s, s, a->w - 16 * s, MUTED, text);
}

static void status_line(gtn_app *a)
{
    if (!a->status[0]) return;
    int s = ts(a);
    int w = gtn_text_width(s, a->status) + 24 * s;
    if (w > a->w - 20) w = a->w - 20;
    int x = (a->w - w) / 2, y = a->h - 44 * s;
    gtn_fill(x, y, w, 20 * s, gtn_rgb(0x40, 0x16, 0x16));
    gtn_text_fit(x + 12 * s, y + 6 * s, s, w - 24 * s, TEXT, a->status);
}

void gtn_logo_at(gtn_app *a, int y, int s)
{
    /* "gtetrinet-ng" with each letter in a block colour */
    static const gtn_color cols[] = {{0x2f, 0x6b, 0xff, 255}, {0xff, 0xd2, 0x2e, 255}, {0x2e, 0xc8, 0x4a, 255},
                                     {0xb0, 0x4a, 0xff, 255}, {0xff, 0x3b, 0x3b, 255}};
    const char *t = "GTetrinet NG";
    int x = (a->w - gtn_text_width(s, t)) / 2, sh = s > 2 ? s / 2 : 1;
    for (int i = 0; t[i]; i++) {
        char c[2] = {t[i], 0};
        gtn_text(x + sh, y + sh, s, gtn_rgb(0, 0, 0), c);
        x += gtn_text(x, y, s, cols[i % 5], c);
    }
}

static void logo(gtn_app *a, int y) { gtn_logo_at(a, y, a->w >= 1000 ? 7 : 4); }

static void player_card(gtn_app *a, int i, int x, int y, int w, int h)
{
    int s = ts(a);
    const gtn_local *p = &a->pl[i];
    gtn_color pc = gtn_player_color(i);
    gtn_fill(x, y, w, h, PANEL);
    if (!p->active) {
        gtn_frame(x, y, w, h, 1, gtn_rgb(0x2a, 0x2e, 0x3c));
        gtn_text_center_fit(x + w / 2, y + h / 2 - 12 * s, s, w - 8, MUTED, gtn_tr("Press A"));
        gtn_text_center_fit(x + w / 2, y + h / 2 + 2 * s, s, w - 8, MUTED, gtn_tr("to join"));
        return;
    }
    gtn_frame(x, y, w, h, 2 * s, pc);
    char n[32];
    snprintf(n, sizeof n, "P%d", i + 1);
    gtn_text(x + 6 * s, y + 6 * s, 2 * s, pc, n);
    gtn_text_fit(x + 6 * s, y + h / 2, s, w - 12 * s, TEXT, p->nick);
    gtn_text_fit(x + 6 * s, y + h - 14 * s, s, w - 12 * s, MUTED,
                 gtn_tr(p->dev == DEV_KEYBOARD ? "Keyboard" : p->pad < 0 ? "No pad!" : "Gamepad"));
}

static void draw_title(gtn_app *a)
{
    int s = ts(a);
    logo(a, a->h / 10);
    char v[96];
    snprintf(v, sizeof v, gtn_tr("TetriNET for up to 8 players - version %s"), TN_VERSION);
    gtn_text_center_fit(a->w / 2, a->h / 10 + (a->w >= 1000 ? 70 : 40), s, a->w - 32, MUTED, v);

    int cw = (a->w - 40) / 8, chh = a->h / 6;
    if (cw > 150) cw = 150;
    int x0 = (a->w - cw * 8) / 2, y0 = a->h / 3;
    for (int i = 0; i < GTN_MAX_LOCAL; i++) player_card(a, i, x0 + i * cw + 3, y0, cw - 6, chh);

    static const char *const items[] = {N_("Play here (everyone on this screen)"), N_("Play online"), N_("What's new"), N_("Quit")};
    int y = y0 + chh + 30 * s;
    for (int i = 0; i < 4; i++) {
        int sel = a->menu_sel == i;
        const char *item = gtn_tr(items[i]);
        int ms = gtn_text_scale(2 * s, a->w - 48 * s, item);
        int w = gtn_text_width(ms, item);
        if (sel) gtn_fill((a->w - w) / 2 - 12 * s, y - 6 * s, w + 24 * s, 28 * s, gtn_rgb(0x2a, 0x30, 0x48));
        gtn_text_center_fit(a->w / 2, y, ms, a->w - 48 * s, sel ? ACCENT : TEXT, item);
        y += 34 * s;
    }
    if (gtn_local_count(a) == 0)
        gtn_text_center_fit(a->w / 2, y + 6 * s, s, a->w - 32, ACCENT, gtn_tr("Everyone who wants to play: press A on your controller (or Enter)"));
    else
        gtn_text_center_fit(a->w / 2, y + 6 * s, s, a->w - 32, MUTED, gtn_tr("Your name: press Y on your controller (or N on the keyboard)"));
    hints(a, gtn_tr("A/Enter: join / choose   B: leave   Up/Down: menu   X/S: options"));
}

/* The language row names each choice in its own language, and Automatic in the current one. */
static void language_value(char *out, size_t n, int choice)
{
    if (choice == LANG_EN) snprintf(out, n, "%s", gtn_lang_endonym(GTN_LANG_EN));
    else if (choice == LANG_DE) snprintf(out, n, "%s", gtn_lang_endonym(GTN_LANG_DE));
    else snprintf(out, n, gtn_tr("Automatic (%s)"), gtn_lang_endonym(gtn_lang_detect()));
}

static void option_row(gtn_app *a, int row, int y, const char *label, const char *value)
{
    int s = ts(a), sel = a->options_row == row;
    char buf[128];
    if (sel) gtn_fill(a->w / 6, y - 8 * s, a->w * 2 / 3, 64 * s, gtn_rgb(0x2a, 0x30, 0x48));
    gtn_text_center_fit(a->w / 2, y, 2 * s, a->w - 32, sel ? TEXT : MUTED, label);
    snprintf(buf, sizeof buf, "< %s >", value);
    gtn_text_center_fit(a->w / 2, y + 26 * s, 2 * s, a->w - 32, sel ? ACCENT : TEXT, buf);
}

static void draw_input_options(gtn_app *a)
{
    int s = ts(a), y = a->h / 8;
    char buf[128];
    gtn_text_center_fit(a->w / 2, y, 3 * s, a->w - 32, TEXT, gtn_tr("Options"));
    y += 50 * s;
    option_row(a, 0, y, gtn_tr("Movement repeat"), gtn_tr(gtn_repeat_name(a->settings.repeat_profile)));
    y += 64 * s;
    gtn_text_center_fit(a->w / 2, y, s, a->w - 32, MUTED, gtn_tr("Holding Left/Right moves repeatedly after a short delay."));
    y += 16 * s;
    snprintf(buf, sizeof buf, gtn_tr("First repeat: %d ms   Next repeats: every %d ms"),
             gtn_repeat_delay(a->settings.repeat_profile), gtn_repeat_interval(a->settings.repeat_profile));
    gtn_text_center_fit(a->w / 2, y, s, a->w - 32, TEXT, buf);
    y += 40 * s;
    language_value(buf, sizeof buf, a->settings.lang);
    option_row(a, 1, y, gtn_tr("Language"), buf);
    y += 72 * s;
    option_row(a, 2, y, gtn_tr("Controller buttons"),
               gtn_tr(gtn_pad_is_default(a->settings.pad_map) || !gtn_pad_valid(a->settings.pad_map) ? N_("Standard") : N_("Custom")));
    y += 72 * s;
    /* the status line (a failed save) takes this place at Deck height */
    if (!a->status[0])
        gtn_text_center_fit(a->w / 2, y, s, a->w - 32, MUTED, gtn_tr("Saved for all local players, keyboard and controllers."));
    status_line(a);
    hints(a, gtn_tr("Up/Down: choose   Left/Right/A: change   Y: default   B/Esc/Start: back"));
}

static void draw_controls(gtn_app *a)
{
    int s = ts(a), y = a->h / 10;
    gtn_text_center_fit(a->w / 2, y, 3 * s, a->w - 32, TEXT, gtn_tr("Controller buttons"));
    y += 34 * s;
    gtn_text_center_fit(a->w / 2, y, s, a->w - 32, MUTED, gtn_tr("In a game, for every controller. Left/Right/Down move, Start pauses."));
    y += 22 * s;
    int x0 = a->w / 8, x1 = a->w - a->w / 8, rowh = 26 * s;
    /* one text size for all rows, the largest at which the longest label still fits */
    int scale = 2 * s, namew = gtn_text_width(scale, gtn_btn_name(BTN_START)) + 24 * s;
    for (int i = 0; i < PAD_ACTIONS; i++) {
        int k = gtn_text_scale(2 * s, x1 - x0 - 16 * s - namew, gtn_tr(gtn_pad_action_name(i)));
        if (k < scale) scale = k;
    }
    int map[PAD_ACTIONS];
    memcpy(map, a->settings.pad_map, sizeof map);
    if (!gtn_pad_valid(map)) gtn_pad_defaults(map);
    for (int i = 0; i < PAD_ACTIONS; i++, y += rowh) {
        int sel = a->controls_row == i;
        if (sel) gtn_fill(x0, y - 4 * s, x1 - x0, rowh - 2 * s, gtn_rgb(0x2a, 0x30, 0x48));
        gtn_text_fit(x0 + 8 * s, y, scale, x1 - x0 - 16 * s - namew, sel ? TEXT : MUTED, gtn_tr(gtn_pad_action_name(i)));
        const char *name = sel && a->controls_capture ? "?" : gtn_btn_name(map[i]);
        gtn_text(x1 - 8 * s - gtn_text_width(scale, name), y, scale, sel ? ACCENT : TEXT, name);
    }
    y += 8 * s;
    if (a->controls_capture) {
        char buf[160];
        snprintf(buf, sizeof buf, gtn_tr("Press the controller button for: %s"), gtn_tr(gtn_pad_action_name(a->controls_row)));
        gtn_text_center_fit(a->w / 2, y, s, a->w - 32, ACCENT, buf);
    } else {
        gtn_text_center_fit(a->w / 2, y, s, a->w - 32, MUTED, gtn_tr("A button that is taken swaps with the old one."));
    }
    status_line(a);
    hints(a, a->controls_capture ? gtn_tr("Press a button on the controller   Start or any key: cancel")
                                 : gtn_tr("Up/Down: choose   A: new button   Y: all standard   B/Start: back"));
}

/* Draws the keys from y down and returns the y below the last row. */
static int draw_osk(gtn_app *a, const char *const *rows, int nrows, int ky, int shift)
{
    int s = ts(a), kh = 30 * s;
    for (int r = 0; r < nrows; r++) {
        char key[8];
        int n = 0;
        for (const char *q = rows[r]; *q; ) { q = tn_word(q, key, sizeof key); n++; }
        int kw = r == nrows - 1 ? 80 * s : 34 * s;
        int x = (a->w - n * (kw + 4 * s)) / 2, col = 0;
        for (const char *q = rows[r]; *q; col++) {
            q = tn_word(q, key, sizeof key);
            int sel = r == a->osk_row && col == a->osk_col;
            if (shift && !key[1] && key[0] >= 'a' && key[0] <= 'z') key[0] = (char)(key[0] - 'a' + 'A');
            gtn_fill(x, ky, kw, kh, sel ? gtn_rgb(0x3a, 0x44, 0x6a) : PANEL);
            if (sel) gtn_frame(x, ky, kw, kh, s, ACCENT);
            gtn_text_center(x + kw / 2, ky + kh / 2 - 8 * s, 2 * s, sel ? ACCENT : TEXT, gtn_tr(key));
            x += kw + 4 * s;
        }
        ky += kh + 4 * s;
    }
    return ky;
}

static void draw_name(gtn_app *a)
{
    int s = ts(a), who = a->name_who;
    char buf[160];
    snprintf(buf, sizeof buf, gtn_tr("Name for P%d"), who + 1);
    gtn_text_center_fit(a->w / 2, a->h / 10, 3 * s, a->w - 32, gtn_player_color(who), buf);
    gtn_text_center_fit(a->w / 2, a->h / 10 + 36 * s, s, a->w - 32, MUTED,
                        gtn_tr("Everyone sees it, online too. One word, at most 15 characters."));
    int bw = a->w / 2, bx = (a->w - bw) / 2, by = a->h / 10 + 56 * s;
    gtn_fill(bx, by, bw, 28 * s, PANEL);
    gtn_frame(bx, by, bw, 28 * s, s, gtn_player_color(who));
    snprintf(buf, sizeof buf, "%s_", a->entry);
    gtn_text_fit(bx + 8 * s, by + 6 * s, 2 * s, bw - 16 * s, TEXT, buf);
    int ky = draw_osk(a, gtn_name_rows, gtn_name_nrows(), by + 50 * s, a->osk_shift);
    gtn_text_center_fit(a->w / 2, ky + 12 * s, s, a->w - 32, MUTED, gtn_tr("Saved for this seat; the next game uses it."));
    status_line(a);
    hints(a, gtn_tr("D-pad/A: type   X: delete   Y: Aa   Start: save   B: back"));
}

static void draw_online(gtn_app *a)
{
    int s = ts(a);
    gtn_text_center_fit(a->w / 2, a->h / 10, 3 * s, a->w - 32, TEXT, gtn_tr("Play online"));
    gtn_text_center_fit(a->w / 2, a->h / 10 + 36 * s, s, a->w - 32, MUTED, gtn_tr("Server address (TetriNET 1.13 or gtetrinet-ng-server)"));
    int bw = a->w * 2 / 3, bx = (a->w - bw) / 2, by = a->h / 10 + 56 * s;
    gtn_fill(bx, by, bw, 28 * s, PANEL);
    gtn_frame(bx, by, bw, 28 * s, s, ACCENT);
    char buf[160];
    snprintf(buf, sizeof buf, "%s_", a->entry);
    gtn_text_fit(bx + 8 * s, by + 6 * s, 2 * s, bw - 16 * s, TEXT, buf);

    int ky = draw_osk(a, gtn_osk_rows, gtn_osk_nrows(), by + 50 * s, 0);
    snprintf(buf, sizeof buf, gtn_tr("Mode: %s (Y to change)"), a->settings.fast ? "TetriFast" : gtn_tr("classic TetriNET"));
    gtn_text_center_fit(a->w / 2, ky + 12 * s, s, a->w - 32, MUTED, buf);
    gtn_text_center_fit(a->w / 2, ky + 28 * s, s, a->w - 32, MUTED, gtn_tr("Everybody who joined plays on that server, each as their own player."));
    status_line(a);
    hints(a, gtn_tr("D-pad/A: type   X: delete   Start/GO/Enter: connect   B/Esc: back"));
}

static void draw_connecting(gtn_app *a)
{
    int s = ts(a);
    char buf[200];
    snprintf(buf, sizeof buf, gtn_tr("Connecting to %s ..."), a->hosting ? gtn_tr("the game on this machine") : a->host);
    gtn_text_center_fit(a->w / 2, a->h / 2 - 10 * s, 2 * s, a->w - 32, TEXT, buf);
    status_line(a);
    hints(a, gtn_tr("B: cancel"));
}

static void draw_lobby(gtn_app *a)
{
    int s = ts(a);
    const tn_client *c = view_client(a);
    if (!c) { draw_connecting(a); return; }
    char buf[256];
    if (a->hosting)
        snprintf(buf, sizeof buf, gtn_tr("Game on this machine (port %d) - others on your network can join it"), a->server.port);
    else
        snprintf(buf, sizeof buf, gtn_tr("Online: %s"), a->host);
    gtn_text(16 * s, 12 * s, 2 * s, TEXT, gtn_tr(a->hosting ? "Local game" : "Online game"));
    gtn_text_fit(16 * s, 34 * s, s, a->w - 32 * s, MUTED, buf);

    int lx = 16 * s, ly = 56 * s, lw = a->w / 2 - 24 * s, row = 26 * s;
    gtn_fill(lx, ly, lw, row * TN_MAX_SLOTS + 8 * s, PANEL);
    int mod = 0;
    for (int i = 1; i <= TN_MAX_SLOTS; i++) if (c->slot[i].present) { mod = i; break; }
    for (int i = 1; i <= TN_MAX_SLOTS; i++) {
        int y = ly + 4 * s + (i - 1) * row;
        const tn_slot *sl = &c->slot[i];
        int loc = local_of_slot(a, i);
        gtn_color col = loc >= 0 ? gtn_player_color(loc) : TEXT;
        snprintf(buf, sizeof buf, "%d", i);
        gtn_text(lx + 8 * s, y + 5 * s, 2 * s, MUTED, buf);
        if (!sl->present) { gtn_text(lx + 40 * s, y + 9 * s, s, MUTED, gtn_tr("- free -")); continue; }
        if (loc >= 0) gtn_fill(lx + 32 * s, y + 4 * s, 4 * s, row - 8 * s, col);
        snprintf(buf, sizeof buf, "%s%s%s%s", sl->nick, sl->team[0] ? "  [" : "", sl->team, sl->team[0] ? "]" : "");
        gtn_text_fit(lx + 42 * s, y + 5 * s, 2 * s, lw - 150 * s, col, buf);
        if (i == mod) gtn_text(lx + lw - 100 * s, y + 9 * s, s, ACCENT, gtn_tr("moderator"));
    }

    /* chat and server messages */
    int cx = a->w / 2 + 8 * s, cy = 56 * s, cw = a->w / 2 - 24 * s, chh = a->h - cy - 70 * s;
    gtn_fill(cx, cy, cw, chh, PANEL);
    int lines = (chh - 8 * s) / (12 * s);
    for (int i = 0; i < lines && i < c->chat.count; i++) {
        char line[TN_LOG_LEN + 64];
        gtn_tr_log(tn_log_get(&c->chat, i), line, sizeof line);
        gtn_text_fit(cx + 6 * s, cy + chh - (i + 1) * 12 * s, s, cw - 12 * s, TEXT, line);
    }

    int by = ly + row * TN_MAX_SLOTS + 24 * s;
    if (c->last_winner && a->banner_ms > 0) {
        const tn_slot *w = &c->slot[c->last_winner];
        snprintf(buf, sizeof buf, gtn_tr("%s wins!"), w->team[0] ? w->team : w->nick);
        gtn_text_fit(lx, by, 3 * s, lw, ACCENT, buf);
        by += 34 * s;
    }
    if (c->winlist[0]) {
        gtn_text(lx, by, s, MUTED, gtn_tr("Winners so far:"));
        char wl[512];
        snprintf(wl, sizeof wl, "%s", c->winlist);
        int y = by + 14 * s, k = 0;
        /* stop above the moderator prompt; with eight players and a banner there is less room */
        for (char *tok = strtok(wl, " "); tok && k < 6 && y + 8 * s < a->h - 56 * s; tok = strtok(NULL, " "), k++) {
            char *semi = strchr(tok, ';');
            if (semi) *semi = 0;
            snprintf(buf, sizeof buf, "%s%s  %s", tok[0] == 't' ? gtn_tr("Team ") : "", tok + 1, semi ? semi + 1 : "");
            gtn_text(lx + 8 * s, y, s, TEXT, buf);
            y += 12 * s;
        }
    }
    int m = -1;
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].client.state == TN_CONNECTED && tn_client_moderator(&a->pl[i].client)) m = i;
    if (m >= 0) snprintf(buf, sizeof buf, gtn_tr("Press Start or A to begin the game (%s is the moderator)."), a->pl[m].nick);
    else snprintf(buf, sizeof buf, gtn_tr("Waiting for the moderator (%s) to start the game."), mod ? c->slot[mod].nick : "?");
    gtn_text_center_fit(a->w / 2, a->h - 52 * s, s, a->w - 32, ACCENT, buf);
    status_line(a);
    hints(a, gtn_tr("Start/A: start   Y: team   B: leave/end   Back: news   New pad A: join"));
}

static void draw_news(gtn_app *a)
{
    int s = ts(a);
    gtn_text_center_fit(a->w / 2, 14 * s, 3 * s, a->w - 32, TEXT, gtn_tr("What's new"));
    int maxc = (a->w - 48 * s) / (8 * s), lh = 12 * s;
    int y0 = 50 * s, y = y0, line = 0;
    const char *p = a->news;
    while (*p && y < a->h - 30 * s) {
        const char *e = strchr(p, '\n');
        int n = e ? (int)(e - p) : (int)strlen(p);
        int head = n >= 3 && !strncmp(p, "## ", 3);
        const char *q = p;
        int left = n;
        do {
            int limit = head ? maxc / 2 : maxc;
            /* k bytes holding at most `limit` whole characters; prefer breaking at a space */
            const char *r = q;
            for (int ch = 0; ch < limit && r < q + left; ch++) gtn_utf8_next(&r);
            if (r > q + left) r = q + left;
            int k = (int)(r - q);
            if (k < left) { int b = k; while (b > k / 2 && q[b] != ' ') b--; if (q[b] == ' ') k = b; }
            if (line >= a->news_scroll && y < a->h - 30 * s) {
                char buf[512];
                snprintf(buf, sizeof buf, "%.*s", k, q);
                gtn_text(24 * s, y, head ? 2 * s : s, head ? ACCENT : TEXT, head && q == p ? buf + 3 : buf);
                y += head ? 2 * lh : lh;
            }
            line++;
            q += k;
            left -= k;
            while (left > 0 && *q == ' ') { q++; left--; }
        } while (left > 0);
        if (n == 0) { if (line > a->news_scroll) y += lh / 2; line++; }
        p = e ? e + 1 : p + n;
    }
    if (a->news_scroll > line) a->news_scroll = line;
    hints(a, gtn_tr("Up/Down: scroll   LB/RB: page   A/B: close"));
}

/* ---------------------------------------------------------------- game */

static void panel(gtn_app *a, int li, int x, int y, int w, int h, int cs)
{
    const gtn_local *p = &a->pl[li];
    const tn_client *c = &p->client;
    int s = cs >= 14 ? 2 : 1;
    gtn_color pc = gtn_player_color(li);
    gtn_fill(x, y, w, h, PANEL);
    gtn_frame(x, y, w, h, p->flash_ms > 0 ? 4 : 2, p->flash_ms > 0 ? BAD : pc);

    int fx = x + cs / 2, fy = y + cs + cs / 2;
    char buf[96];
    snprintf(buf, sizeof buf, "%d %s", c->me, p->nick);
    gtn_text_fit(fx, y + cs / 3, s, TN_FIELD_W * cs, pc, buf);

    tn_field f;
    tn_client_view(c, f);
    gtn_field(fx, fy, cs, f, !c->playing);
    if (c->playing && c->game.block != TN_NO_BLOCK && c->spawn_wait == 0) {
        int gy = tn_game_drop_y(&c->game);
        if (gy > c->game.by) gtn_piece(fx + c->game.bx * cs, fy + gy * cs, cs, c->game.block, c->game.orient, 60);
    }
    gtn_frame(fx - 1, fy - 1, TN_FIELD_W * cs + 2, TN_FIELD_H * cs + 2, 1, gtn_rgb(0x40, 0x46, 0x5c));
    int fw = TN_FIELD_W * cs, fh = TN_FIELD_H * cs;
    if (!c->playing && c->ingame)
        gtn_text_center(fx + fw / 2, fy + fh / 2 - 8 * s, 2 * s, c->last_winner == c->me ? ACCENT : TEXT,
                        gtn_tr(c->last_winner == c->me ? "WINNER" : "OUT"));

    /* side: next piece, level, lines, target */
    int sx = fx + fw + cs / 2, sw = x + w - sx - cs / 3;
    s = sw >= 8 * 2 * 8 ? 2 : 1; /* "TARGET"/"DANACH" must fit */
    gtn_text(sx, fy, s, MUTED, gtn_tr("NEXT"));
    if (c->playing) gtn_piece(sx, fy + 10 * s, sw / 4 < cs ? sw / 4 : cs, c->game.next, c->game.next_orient, 255);
    int ty = fy + 10 * s + 4 * cs + cs / 2;
    snprintf(buf, sizeof buf, gtn_tr("LVL %d"), c->playing ? c->game.level : c->slot[c->me].level);
    gtn_text_fit(sx, ty, s, sw, TEXT, buf);
    snprintf(buf, sizeof buf, gtn_tr("LNS %d"), c->game.lines);
    gtn_text_fit(sx, ty + 12 * s, s, sw, TEXT, buf);
    gtn_text(sx, ty + 32 * s, s, MUTED, gtn_tr("TARGET"));
    int t = p->target >= 1 && c->slot[p->target].playing ? p->target : 0;
    if (t) {
        int lt = local_of_slot(a, t);
        snprintf(buf, sizeof buf, "%d %s", t, c->slot[t].nick);
        gtn_text_fit(sx, ty + 44 * s, s, sw, lt >= 0 ? gtn_player_color(lt) : TEXT, t == c->me ? gtn_tr("yourself") : buf);
    } else {
        gtn_text_fit(sx, ty + 44 * s, s, sw, MUTED, "LB/RB");
    }
    if (t && c->playing && c->game.nspecials > 0) {
        int sb = tn_sb_from_cell(c->game.specials[0]);
        gtn_text_fit(sx, ty + 56 * s, s, sw, gtn_special_color(sb), gtn_tr(gtn_special_target_hint(sb, t == c->me)));
    }
    /* last attacks */
    for (int i = 0; i < 3 && i < c->attacks.count; i++) {
        int ly = fy + fh - (i + 1) * 11 * s;
        if (ly < ty + 68 * s) break;
        char line[TN_LOG_LEN + 64];
        gtn_tr_log(tn_log_get(&c->attacks, i), line, sizeof line);
        gtn_text_fit(sx, ly, 1, sw, i == 0 ? TEXT : MUTED, line);
    }

    /* specials queue under the field */
    int qy = fy + fh + cs / 3, qs = cs * 3 / 4 > 6 ? cs * 3 / 4 : 6;
    int maxq = (w - cs) / qs;
    for (int i = 0; i < c->game.nspecials && i < maxq && c->playing; i++) {
        gtn_cell(fx + i * qs, qy, qs, c->game.specials[i]);
        if (i == 0) gtn_frame(fx - 1, qy - 1, qs + 2, qs + 2, 1, ACCENT);
    }
    if (c->playing && c->game.nspecials > 0) {
        int sb = tn_sb_from_cell(c->game.specials[0]);
        /* The queue must never consume the effect label's width. */
        snprintf(buf, sizeof buf, "%s %s", gtn_tr(gtn_special_cue(sb)), gtn_tr_special(sb));
        gtn_text_fit(fx, qy + qs + 2, 1, x + w - fx - 4, gtn_special_color(sb), buf);
    }
}

static void mini(gtn_app *a, const tn_client *c, int slot, int x, int y, int cs)
{
    const tn_slot *sl = &c->slot[slot];
    char buf[64];
    snprintf(buf, sizeof buf, "%d %s", slot, sl->nick);
    gtn_text_fit(x, y, 1, TN_FIELD_W * cs, TEXT, buf);
    gtn_field(x, y + 10, cs, (const char (*)[TN_FIELD_W])sl->field, !sl->playing);
    gtn_frame(x - 1, y + 9, TN_FIELD_W * cs + 2, TN_FIELD_H * cs + 2, 1, gtn_rgb(0x40, 0x46, 0x5c));
    /* which local players aim at this one */
    int k = 0;
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].target == slot) {
            gtn_fill(x + k * 8, y + 12 + TN_FIELD_H * cs, 6, 6, gtn_player_color(i));
            k++;
        }
}

static void draw_game(gtn_app *a)
{
    const tn_client *c = view_client(a);
    if (!c) return;
    int idx[GTN_MAX_LOCAL], n = 0;
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].client.state == TN_CONNECTED) idx[n++] = i;
    int remote[TN_MAX_SLOTS], r = 0;
    for (int sl = 1; sl <= TN_MAX_SLOTS; sl++)
        if (c->slot[sl].present && local_of_slot(a, sl) < 0) remote[r++] = sl;

    int W = a->w, H = a->h;
    int stripw = 0, mcs = 0, mcols = 1;
    if (r > 0) {
        mcols = r > 3 ? 2 : 1;
        int mrows = (r + mcols - 1) / mcols;
        mcs = (H - 10) / (mrows * (TN_FIELD_H + 2));
        int maxw = W / 4 / mcols / (TN_FIELD_W + 1);
        if (mcs > maxw) mcs = maxw;
        if (mcs < 3) mcs = 3;
        stripw = mcols * (TN_FIELD_W + 1) * mcs + 10;
        W -= stripw;
    }
    /* the grid that gives the biggest cells */
    int best = 0, bc = 1;
    for (int cols = 1; cols <= n; cols++) {
        int rows = (n + cols - 1) / cols;
        int pw = W / cols, ph = H / rows;
        int cs = pw * 2 / 37;            /* 18.5 cells wide */
        if ((ph - 10) * 2 / 51 < cs) cs = (ph - 10) * 2 / 51; /* 25.5 cells high */
        if (cs > best) { best = cs; bc = cols; }
    }
    int cols = bc, rows = (n + cols - 1) / cols, cs = best;
    if (cs < 4) cs = 4;
    int pw = W / cols, ph = H / rows;
    int panw = cs * 37 / 2, panh = cs * 51 / 2 + 10;
    if (panw > pw) panw = pw;
    if (panh > ph) panh = ph;
    for (int k = 0; k < n; k++) {
        int cx = k % cols, cy = k / cols;
        /* centre a last, incomplete row */
        int inrow = cy == rows - 1 ? n - cols * (rows - 1) : cols;
        int offx = (W - inrow * pw) / 2;
        panel(a, idx[k], offx + cx * pw + (pw - panw) / 2, cy * ph + (ph - panh) / 2, panw, panh, cs);
    }
    for (int k = 0; k < r; k++) {
        int mrows = (r + mcols - 1) / mcols;
        int cx = k / mrows, cy = k % mrows;
        mini(a, c, remote[k], W + 6 + cx * (TN_FIELD_W + 1) * mcs, 6 + cy * (TN_FIELD_H + 2) * mcs, mcs);
    }
    if (c->paused) {
        gtn_fill(0, 0, a->w, a->h, (gtn_color){0, 0, 0, 140});
        gtn_text_center_fit(a->w / 2, a->h / 2 - 20, 4, a->w - 32, ACCENT, gtn_tr("PAUSED"));
        gtn_text_center_fit(a->w / 2, a->h / 2 + 24, 2, a->w - 32, TEXT, gtn_tr("Start: continue   Back+Start: end the game"));
    }
    status_line(a);
}

void gtn_draw(gtn_app *a)
{
    SDL_SetRenderDrawColor(a->ren, BG.r, BG.g, BG.b, 255);
    SDL_RenderClear(a->ren);
    switch (a->screen) {
    case SCR_TITLE: draw_title(a); status_line(a); break;
    case SCR_ONLINE: draw_online(a); break;
    case SCR_CONNECTING: draw_connecting(a); break;
    case SCR_LOBBY: draw_lobby(a); break;
    case SCR_GAME: draw_game(a); break;
    case SCR_NEWS: draw_news(a); break;
    case SCR_INPUT: draw_input_options(a); break;
    case SCR_CONTROLS: draw_controls(a); break;
    case SCR_NAME: draw_name(a); break;
    }
}
