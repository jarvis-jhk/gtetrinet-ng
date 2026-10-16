/*
 * gtetrinet-ng — settings file.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "settings.h"
#include "fault.h"
#include "i18n.h"
#include "sound.h"
#include "tn_bot.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int repeat_index(int profile)
{
    return profile >= 0 && profile < REPEAT_COUNT ? profile : REPEAT_NORMAL;
}

int gtn_nick_clean(char *out, size_t n, const char *in)
{
    size_t k = 0;
    for (; in && *in && k + 1 < n && k + 1 < GTN_NAME_MAX; in++)
        if ((unsigned char)*in > ' ' && (unsigned char)*in < 127) out[k++] = *in;
    if (n) out[k] = 0;
    return (int)k;
}

const char *gtn_repeat_name(int profile)
{
    static const char *const names[] = {N_("Normal"), N_("Relaxed"), N_("Fast")};
    return names[repeat_index(profile)];
}

int gtn_repeat_delay(int profile)
{
    static const int delays[] = {170, 250, 120};
    return delays[repeat_index(profile)];
}

int gtn_repeat_interval(int profile)
{
    static const int intervals[] = {50, 100, 30};
    return intervals[repeat_index(profile)];
}

static const int pad_default[PAD_ACTIONS] = {BTN_A, BTN_B, BTN_UP, BTN_X, BTN_Y, BTN_LB, BTN_RB, BTN_BACK, BTN_RT};

void gtn_pad_defaults(int map[PAD_ACTIONS])
{
    memcpy(map, pad_default, sizeof pad_default);
}

int gtn_pad_bindable(int b)
{
    for (int i = 0; i < PAD_ACTIONS; i++)
        if (pad_default[i] == b) return 1;
    return 0;
}

int gtn_pad_valid(const int map[PAD_ACTIONS])
{
    for (int i = 0; i < PAD_ACTIONS; i++) {
        if (!gtn_pad_bindable(map[i])) return 0;
        for (int j = 0; j < i; j++)
            if (map[j] == map[i]) return 0;
    }
    return 1;
}

int gtn_pad_is_default(const int map[PAD_ACTIONS])
{
    return !memcmp(map, pad_default, sizeof pad_default);
}

void gtn_pad_bind(int map[PAD_ACTIONS], int act, int b)
{
    if (act < 0 || act >= PAD_ACTIONS || !gtn_pad_bindable(b)) return;
    if (!gtn_pad_valid(map)) gtn_pad_defaults(map);
    for (int i = 0; i < PAD_ACTIONS; i++)
        if (map[i] == b) map[i] = map[act];
    map[act] = b;
}

int gtn_pad_translate(const int map[PAD_ACTIONS], int b)
{
    if (!gtn_pad_valid(map)) return b;
    for (int i = 0; i < PAD_ACTIONS; i++)
        if (map[i] == b) return pad_default[i];
    return b;
}

const char *gtn_pad_action_name(int act)
{
    static const char *const names[] = {
        N_("Rotate clockwise"), N_("Rotate counterclockwise"), N_("Hard drop"), N_("Special on target"),
        N_("Special on yourself"), N_("Previous target"), N_("Next target"), N_("Discard special"),
        N_("Hold piece"),
    };
    return act >= 0 && act < PAD_ACTIONS ? names[act] : "?";
}

/* Controller names as printed on an Xbox-layout pad, which is what SDL reports for every pad. */
static const char *const btn_names[BTN_COUNT] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "LB", "RB", "START", "BACK", "RT"};

const char *gtn_btn_name(int b)
{
    return b >= 0 && b < BTN_COUNT ? btn_names[b] : "?";
}

/* "pad=A,B,UP,..." in action order; anything that is not a complete layout keeps the default. */
static void parse_pad(int map[PAD_ACTIONS], const char *v)
{
    int m[PAD_ACTIONS], n = 0;
    char word[16];
    while (*v && n < PAD_ACTIONS) {
        size_t k = strcspn(v, ",");
        if (k >= sizeof word) return;
        memcpy(word, v, k);
        word[k] = 0;
        int b = -1;
        for (int i = 0; i < BTN_COUNT; i++)
            if (!strcmp(word, btn_names[i])) b = i;
        if (b < 0) return;
        m[n++] = b;
        v += k;
        if (*v != ',') break;
        if (!*++v) return; /* a trailing comma is not a complete layout */
    }
    /* layouts saved before the hold piece existed: the right trigger was never bindable */
    if (n == PAD_ACTIONS - 1 && !*v) m[n++] = BTN_RT;
    if (n == PAD_ACTIONS && !*v && gtn_pad_valid(m)) memcpy(map, m, sizeof m);
}

const char *gtn_pref_dir(void)
{
    static char dir[1024];
    if (!dir[0]) {
        char *p = SDL_GetPrefPath("gtetrinet-ng", "gtetrinet-ng");
        snprintf(dir, sizeof dir, "%s", p ? p : "./");
        SDL_free(p);
    }
    return dir;
}

static void default_nick(char *out, size_t n)
{
    const char *u = getenv("USER");
    if (!u || !*u) u = getenv("USERNAME");
    if (!u || !*u || strcmp(u, "root") == 0) u = "Player";
    size_t k = 0;
    for (; *u && k + 1 < n && k < 12; u++)
        if ((*u >= 'a' && *u <= 'z') || (*u >= 'A' && *u <= 'Z') || (*u >= '0' && *u <= '9') || *u == '_' || *u == '-')
            out[k++] = *u;
    out[k] = 0;
    if (!k) snprintf(out, n, "Player");
    /* "deck" is every Steam Deck's user name: capitalise it so it reads like a name */
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
}

static const char *volume_codes[VOLUME_COUNT] = {"off", "quiet", "normal", "loud"};
static const char *bot_codes[TN_BOT_LEVELS] = {"easy", "normal", "hard"};

static int parse_volume(const char *v)
{
    for (int i = 0; i < VOLUME_COUNT; i++)
        if (!strcmp(v, volume_codes[i])) return i;
    return VOLUME_NORMAL;
}

void gtn_settings_load(gtn_settings *s)
{
    memset(s, 0, sizeof *s);
    default_nick(s->nick, sizeof s->nick);
    gtn_pad_defaults(s->pad_map);
    s->volume = VOLUME_NORMAL;
    s->bot_level = TN_BOT_NORMAL;
    char path[1200];
    snprintf(path, sizeof path, "%ssettings.ini", gtn_pref_dir());
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "host")) snprintf(s->host, sizeof s->host, "%s", v);
        else if (!strcmp(k, "nick") && *v) snprintf(s->nick, sizeof s->nick, "%.31s", v);
        else if (!strcmp(k, "last_version")) snprintf(s->last_version, sizeof s->last_version, "%s", v);
        else if (!strcmp(k, "fast")) s->fast = atoi(v);
        else if (!strcmp(k, "repeat"))
            s->repeat_profile = !strcmp(v, "1") ? REPEAT_RELAXED : !strcmp(v, "2") ? REPEAT_FAST : REPEAT_NORMAL;
        else if (!strcmp(k, "lang")) s->lang = gtn_lang_parse(v);
        else if (!strcmp(k, "pad")) parse_pad(s->pad_map, v);
        else if (!strcmp(k, "sound")) s->volume = parse_volume(v);
        else if (!strcmp(k, "rumble")) s->rumble_off = !strcmp(v, "off");
        else if (!strcmp(k, "rules")) s->rules_tn2 = !strcmp(v, "tn2");
        else if (!strcmp(k, "bots"))
            for (int i = 0; i < TN_BOT_LEVELS; i++) { if (!strcmp(v, bot_codes[i])) s->bot_level = i; }
        else if (!strncmp(k, "server", 6) && k[6] >= '1' && k[6] < '1' + GTN_RECENT && !k[7])
            snprintf(s->recent[k[6] - '1'], sizeof s->recent[0], "%s", v);
        else if (!strncmp(k, "nick", 4) && k[4] >= '2' && k[4] <= '0' + GTN_SEATS && !k[5])
            gtn_nick_clean(s->seat_nick[k[4] - '1'], sizeof s->seat_nick[0], v);
    }
    fclose(f);
    /* close the gaps a hand-edited file may have; settings from before the list had host= */
    int n = 0;
    for (int i = 0; i < GTN_RECENT; i++)
        if (s->recent[i][0]) {
            if (n != i) { memcpy(s->recent[n], s->recent[i], sizeof s->recent[0]); s->recent[i][0] = 0; }
            n++;
        }
    if (!n && s->host[0]) gtn_settings_remember(s, s->host);
}

static int same_text(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

void gtn_settings_remember(gtn_settings *s, const char *addr)
{
    if (!addr[0]) return;
    char keep[128];
    snprintf(keep, sizeof keep, "%s", addr);
    int at = GTN_RECENT - 1;
    for (int i = 0; i < GTN_RECENT; i++)
        if (same_text(s->recent[i], keep)) { at = i; break; }
    memmove(s->recent[1], s->recent[0], (size_t)at * sizeof s->recent[0]);
    snprintf(s->recent[0], sizeof s->recent[0], "%s", keep);
}

int gtn_settings_save(const gtn_settings *s)
{
    char path[1200], tmp[1210];
    snprintf(path, sizeof path, "%ssettings.ini", gtn_pref_dir());
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) { gtn_fault("saving preferences failed", "could not open temporary file"); return -1; }
    int pad[PAD_ACTIONS];
    memcpy(pad, s->pad_map, sizeof pad);
    if (!gtn_pad_valid(pad)) gtn_pad_defaults(pad);
    char padline[128] = "";
    for (int i = 0; i < PAD_ACTIONS; i++)
        snprintf(padline + strlen(padline), sizeof padline - strlen(padline), "%s%s", i ? "," : "", btn_names[pad[i]]);
    int vol = s->volume >= 0 && s->volume < VOLUME_COUNT ? s->volume : VOLUME_NORMAL;
    int bots = s->bot_level >= 0 && s->bot_level < TN_BOT_LEVELS ? s->bot_level : TN_BOT_NORMAL;
    int written = fprintf(f, "host=%s\nnick=%s\nlast_version=%s\nfast=%d\nrepeat=%d\nlang=%s\npad=%s\nsound=%s\nrumble=%s\nbots=%s\nrules=%s\n",
            s->host, s->nick, s->last_version, s->fast, repeat_index(s->repeat_profile), gtn_lang_code(s->lang), padline,
            volume_codes[vol], s->rumble_off ? "off" : "on", bot_codes[bots],
            s->rules_tn2 ? "tn2" : "classic");
    for (int i = 1; i < GTN_SEATS && written >= 0; i++)
        if (s->seat_nick[i][0]) written = fprintf(f, "nick%d=%s\n", i + 1, s->seat_nick[i]);
    for (int i = 0; i < GTN_RECENT && written >= 0; i++)
        if (s->recent[i][0]) written = fprintf(f, "server%d=%s\n", i + 1, s->recent[i]);
    int closed = fclose(f);
    if (written < 0 || closed != 0) {
        remove(tmp);
        gtn_fault("saving preferences failed", "could not write temporary file");
        return -1;
    }
#ifdef _WIN32
    /* The Windows C runtime does not replace an existing destination. */
    remove(path);
#endif
    if (rename(tmp, path) != 0) {
        remove(tmp);
        gtn_fault("saving preferences failed", "could not replace settings file");
        return -1;
    }
    return 0;
}
