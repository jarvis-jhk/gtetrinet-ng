/*
 * gtetrinet-ng — the game application: shared state of the screens, the local players and
 * their devices.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GTN_APP_H
#define GTN_APP_H

#include <SDL.h>
#include "tn_client.h"
#include "tn_server.h"
#include "tn_bot.h"
#include "settings.h"

#define GTN_MAX_LOCAL 8

/* DEV_BOT: a computer player added in the lobby; no device, it plays by itself. */
enum gtn_dev { DEV_NONE, DEV_KEYBOARD, DEV_PAD, DEV_BOT };

typedef struct gtn_local {
    int active;
    enum gtn_dev dev;
    SDL_JoystickID pad;
    char nick[TN_NICK_MAX];
    tn_client client;
    int target;              /* slot the next special goes to */
    int hold;                /* -1/0/1: left/right held, for auto-repeat */
    int repeat_ms;
    int stick_x, stick_y;    /* left stick as a d-pad: -1/0/1 */
    int flash_ms;            /* red border after being hit */
    unsigned seen_hits;
    tn_events seen;          /* client events already turned into sound and rumble */
    unsigned rumbles;        /* rumble requests for this player's controller, for tests */
    int nick_tries;
    int failed;              /* could not connect; the reason is on the status line */
    tn_bot bot;              /* DEV_BOT only */
} gtn_local;

enum gtn_screen { SCR_TITLE, SCR_ONLINE, SCR_CONNECTING, SCR_LOBBY, SCR_GAME, SCR_NEWS, SCR_INPUT, SCR_CONTROLS, SCR_NAME };

typedef struct gtn_app {
    SDL_Window *win;
    SDL_Renderer *ren;
    int w, h;
    enum gtn_screen screen;
    int quit;
    gtn_settings settings;

    gtn_local pl[GTN_MAX_LOCAL];
    int menu_sel;
    int options_row;         /* 0 movement repeat, 1 language, 2 controller buttons, 3 sound, 4 rumble */
    int controls_row;        /* action chosen on the controller-buttons screen */
    int controls_capture;    /* waiting for the button to give that action */

    tn_server server;
    int hosting;
    char host[128];
    int port;
    int fast;

    /* online screen: text entry with an on-screen keyboard for controllers */
    char entry[128];
    int osk_row, osk_col;
    int name_who;            /* name screen: the local player whose name is being typed */
    int osk_shift;           /* name screen: letters on the on-screen keyboard are capitals */

    char news[16384];        /* changelog text shown on the news screen */
    int news_scroll;
    enum gtn_screen news_back;

    char status[200];        /* one line of feedback at the bottom, e.g. connection errors */
    int status_ms;
    int banner_ms;           /* "X won" banner in the lobby after a game */
    int frame_ms;
    int steamdeck;
} gtn_app;

/* input.c */
void gtn_input_event(gtn_app *a, const SDL_Event *e);
void gtn_input_update(gtn_app *a, int dt_ms);
int gtn_local_count(const gtn_app *a);

/* main.c: what a button does on the current screen. who = local player index, or -1 for a
 * device nobody has joined with yet (dev/pad say which). */
void gtn_button(gtn_app *a, int who, enum gtn_dev dev, SDL_JoystickID pad, enum gtn_btn b, int down);
void gtn_text_input(gtn_app *a, const char *text);
void gtn_set_status(gtn_app *a, const char *fmt, ...);
const char *gtn_bot_level_name(int level);   /* translated: Easy, Normal, Hard */

/* ui.c */
void gtn_draw(gtn_app *a);
void gtn_logo_at(gtn_app *a, int y, int scale);
extern const char *const gtn_osk_rows[];
int gtn_osk_nrows(void);
extern const char *const gtn_name_rows[];
int gtn_name_nrows(void);

#endif
