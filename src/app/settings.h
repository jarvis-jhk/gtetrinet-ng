/*
 * gtetrinet-ng — the few things worth remembering between runs, as key=value lines in the
 * user's preference folder.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GTN_SETTINGS_H
#define GTN_SETTINGS_H

#include <stddef.h>

/* Every device speaks in these, keyboard included. */
enum gtn_btn {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_A, BTN_B, BTN_X, BTN_Y, BTN_LB, BTN_RB, BTN_START, BTN_BACK,
    BTN_RT,                  /* right trigger or right stick click; C/Shift on the keyboard */
    BTN_COUNT
};

/* What a controller button does in a game. Left/Right/Down and Start stay where they are:
 * moving and pausing must work the same on every pad, and menus never change. */
enum gtn_pad_action {
    PAD_ROT_CW, PAD_ROT_CCW, PAD_DROP, PAD_SPECIAL, PAD_SPECIAL_SELF,
    PAD_TARGET_PREV, PAD_TARGET_NEXT, PAD_DISCARD, PAD_HOLD, PAD_ACTIONS
};

/* Local seats P1..P8 and how long a name typed in the game may be (classic servers show
 * about this much, and it fits a player card). */
#define GTN_SEATS 8
#define GTN_NAME_MAX 16

/* servers remembered for the server list, most recent first */
#define GTN_RECENT 6

enum gtn_repeat_profile { REPEAT_NORMAL, REPEAT_RELAXED, REPEAT_FAST, REPEAT_COUNT };

typedef struct gtn_settings {
    char host[128];          /* last online server */
    char recent[GTN_RECENT][128]; /* servers played on, most recent first, "" = none */
    char nick[64];           /* P1's name, and the base other seats append a number to */
    char seat_nick[GTN_SEATS][GTN_NAME_MAX]; /* names chosen for P2..P8; [0] unused, "" = default */
    char last_version[32];   /* version whose changelog was last shown */
    int fast;                /* tetrifast login for online servers */
    int repeat_profile;      /* shared by keyboard and controllers */
    int lang;                /* enum gtn_lang_choice: automatic, English, Deutsch */
    int pad_map[PAD_ACTIONS]; /* enum gtn_btn per action, shared by all controllers */
    int volume;              /* enum gtn_volume, sound effects */
    int rumble_off;          /* controllers do not rumble on hits, losses and wins */
    int bot_level;           /* enum tn_bot_level for computer players added in the lobby */
    int blocks;              /* enum gtn_block_style: modern or the original GTetrinet blocks */
    int rules_tn2;           /* games hosted here use the TetriNET 2 rules (hold, 14 specials) */
} gtn_settings;

/* Folder for settings and logs, ending in a separator. Never NULL. */
const char *gtn_pref_dir(void);
void gtn_settings_load(gtn_settings *s);
/* Returns zero when persisted, -1 after forwarding a real write failure to the fault relay. */
int gtn_settings_save(const gtn_settings *s);
const char *gtn_repeat_name(int profile);
/* Puts addr first in the recent servers (once, case-insensitively), dropping the oldest. */
void gtn_settings_remember(gtn_settings *s, const char *addr);
/* A TetriNET name is one word: copies the printable ASCII of in, without spaces, at most
 * GTN_NAME_MAX - 1 characters. Returns its length; zero means no usable name. */
int gtn_nick_clean(char *out, size_t n, const char *in);
int gtn_repeat_delay(int profile);
int gtn_repeat_interval(int profile);


/* The controller layout is always a one-to-one assignment of the nine bindable buttons,
 * so no action can be lost and no button can do two things. */
void gtn_pad_defaults(int map[PAD_ACTIONS]);
int gtn_pad_valid(const int map[PAD_ACTIONS]);
int gtn_pad_is_default(const int map[PAD_ACTIONS]);
int gtn_pad_bindable(int b);
/* Gives act the button b; the action that had b takes act's old button. */
void gtn_pad_bind(int map[PAD_ACTIONS], int act, int b);
/* The button whose default game action the pressed button b performs now; b itself when it
 * is not bindable or the layout is invalid. */
int gtn_pad_translate(const int map[PAD_ACTIONS], int b);
const char *gtn_pad_action_name(int act);
const char *gtn_btn_name(int b);

#endif
