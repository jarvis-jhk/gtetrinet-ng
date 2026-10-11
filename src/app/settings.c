/*
 * gtetrinet-ng — settings file.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "settings.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    }
    fclose(f);
}

void gtn_settings_save(const gtn_settings *s)
{
    char path[1200], tmp[1210];
    snprintf(path, sizeof path, "%ssettings.ini", gtn_pref_dir());
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "host=%s\nnick=%s\nlast_version=%s\nfast=%d\n", s->host, s->nick, s->last_version, s->fast);
    if (fclose(f) == 0) {
        remove(path);
        rename(tmp, path);
    }
}
