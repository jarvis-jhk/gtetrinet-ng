/*
 * gtetrinet-ng — the few things worth remembering between runs, as key=value lines in the
 * user's preference folder.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GTN_SETTINGS_H
#define GTN_SETTINGS_H

enum gtn_repeat_profile { REPEAT_NORMAL, REPEAT_RELAXED, REPEAT_FAST, REPEAT_COUNT };

typedef struct gtn_settings {
    char host[128];          /* last online server */
    char nick[64];           /* base nickname; local players 2.. get a number appended */
    char last_version[32];   /* version whose changelog was last shown */
    int fast;                /* tetrifast login for online servers */
    int repeat_profile;      /* shared by keyboard and controllers */
} gtn_settings;

/* Folder for settings and logs, ending in a separator. Never NULL. */
const char *gtn_pref_dir(void);
void gtn_settings_load(gtn_settings *s);
/* Returns zero when persisted, -1 after forwarding a real write failure to the fault relay. */
int gtn_settings_save(const gtn_settings *s);
const char *gtn_repeat_name(int profile);
int gtn_repeat_delay(int profile);
int gtn_repeat_interval(int profile);

#endif
