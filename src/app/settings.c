/*
 * gtetrinet-ng — settings file.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "settings.h"
#include "fault.h"
#include "i18n.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int repeat_index(int profile)
{
    return profile >= 0 && profile < REPEAT_COUNT ? profile : REPEAT_NORMAL;
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

void gtn_settings_load(gtn_settings *s)
{
    memset(s, 0, sizeof *s);
    default_nick(s->nick, sizeof s->nick);
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
    }
    fclose(f);
}

int gtn_settings_save(const gtn_settings *s)
{
    char path[1200], tmp[1210];
    snprintf(path, sizeof path, "%ssettings.ini", gtn_pref_dir());
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) { gtn_fault("saving preferences failed", "could not open temporary file"); return -1; }
    int written = fprintf(f, "host=%s\nnick=%s\nlast_version=%s\nfast=%d\nrepeat=%d\nlang=%s\n",
            s->host, s->nick, s->last_version, s->fast, repeat_index(s->repeat_profile), gtn_lang_code(s->lang));
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
