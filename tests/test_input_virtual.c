/* SPDX-License-Identifier: GPL-2.0-or-later
 * Drive SDL's virtual hardware through its event queue, including device lifetime.
 * This verifies the controller pipeline, not physical Deck/Steam Input behavior. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#include "await_game.h"
#undef main

static int checks;
#define REQUIRE(c) do { checks++; if (!(c)) { \
    fprintf(stderr, "line %d: %s (SDL: %s)\n", __LINE__, #c, SDL_GetError()); \
    SDL_Quit(); exit(1); \
} } while (0)

typedef struct virtual_pad {
    SDL_Joystick *joystick;
    SDL_JoystickID id;
} virtual_pad;

static int added, removed, buttons, axes;

static void pump(gtn_app *a)
{
    SDL_JoystickUpdate();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        added += event.type == SDL_CONTROLLERDEVICEADDED;
        removed += event.type == SDL_CONTROLLERDEVICEREMOVED;
        buttons += event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP;
        axes += event.type == SDL_CONTROLLERAXISMOTION;
        gtn_input_event(a, &event);
    }
}

static virtual_pad attach(gtn_app *a)
{
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                        SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    REQUIRE(index >= 0);
    /* Explicit mapping keeps the virtual wiring independent of platform databases. */
    char guid[33], mapping[512];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof guid);
    snprintf(mapping, sizeof mapping,
             "%s,GTN virtual pad,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,"
             "leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,"
             "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,"
             "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,", guid);
    REQUIRE(SDL_GameControllerAddMapping(mapping) >= 0);
    REQUIRE(SDL_IsGameController(index));
    virtual_pad p = {SDL_JoystickOpen(index), SDL_JoystickGetDeviceInstanceID(index)};
    REQUIRE(p.joystick != NULL && p.id >= 0);
    int before = added;
    pump(a);
    REQUIRE(added == before + 1);
    REQUIRE(SDL_GameControllerFromInstanceID(p.id) != NULL);
    return p;
}

static void button(gtn_app *a, virtual_pad p, SDL_GameControllerButton b, int down)
{
    int before = buttons;
    REQUIRE(SDL_JoystickSetVirtualButton(p.joystick, b, (Uint8)down) == 0);
    pump(a);
    REQUIRE(buttons == before + 1);
}

static void tap(gtn_app *a, virtual_pad p, SDL_GameControllerButton b)
{
    button(a, p, b, 1);
    button(a, p, b, 0);
}

static void axis(gtn_app *a, virtual_pad p, SDL_GameControllerAxis which, Sint16 value)
{
    int before = axes;
    REQUIRE(SDL_JoystickSetVirtualAxis(p.joystick, which, value) == 0);
    pump(a);
    REQUIRE(axes > before);
}

static void detach(gtn_app *a, virtual_pad *p)
{
    /* Discovery indices shift when a different pad is removed; identity does not. */
    int index = -1;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_JoystickGetDeviceInstanceID(i) == p->id) index = i;
    REQUIRE(index >= 0);
    int before = removed;
    REQUIRE(SDL_JoystickDetachVirtual(index) == 0);
    pump(a);
    REQUIRE(removed == before + 1);
    REQUIRE(SDL_GameControllerFromInstanceID(p->id) == NULL);
    REQUIRE(!SDL_JoystickGetAttached(p->joystick));
    SDL_JoystickClose(p->joystick);
    p->joystick = NULL;
}

static void await_screen(gtn_app *a, enum gtn_screen screen)
{
    for (int t = 0; t < 2000 && (a->screen != screen || (screen == SCR_GAME && !all_playing(a))); t++) {
        update_session(a, 2);
        pump(a);
        SDL_Delay(1);
    }
    REQUIRE(a->screen == screen);
}

int main(void)
{
    static gtn_app a;
    virtual_pad pads[GTN_MAX_LOCAL];
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    REQUIRE(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0);
    REQUIRE(tn_net_init() == 0);
    a.screen = SCR_TITLE;
    snprintf(a.settings.nick, sizeof a.settings.nick, "Virtual");
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        pads[i] = attach(&a);
        tap(&a, pads[i], SDL_CONTROLLER_BUTTON_A);
        REQUIRE(gtn_local_count(&a) == i + 1);
        REQUIRE(a.pl[i].active && a.pl[i].pad == pads[i].id);
        for (int k = 0; k < i; k++) REQUIRE(pads[i].id != pads[k].id);
    }
    virtual_pad spare = attach(&a);
    tap(&a, spare, SDL_CONTROLLER_BUTTON_A);
    REQUIRE(gtn_local_count(&a) == GTN_MAX_LOCAL);
    for (int repeat = 0; repeat < 4; repeat++) {
        tap(&a, pads[3], SDL_CONTROLLER_BUTTON_B);
        REQUIRE(!a.pl[3].active && gtn_local_count(&a) == GTN_MAX_LOCAL - 1);
        tap(&a, pads[3], SDL_CONTROLLER_BUTTON_A);
        REQUIRE(a.pl[3].active && a.pl[3].pad == pads[3].id);
        REQUIRE(!a.pl[3].hold && !a.pl[3].stick_x && !a.pl[3].stick_y && !back_held[3]);
        for (int i = 0; i < GTN_MAX_LOCAL; i++) REQUIRE(a.pl[i].pad == pads[i].id);
    }

    host_local(&a);
    await_screen(&a, SCR_LOBBY);
    tap(&a, pads[0], SDL_CONTROLLER_BUTTON_START);
    await_screen(&a, SCR_GAME);
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        REQUIRE(a.pl[i].client.playing);
        REQUIRE(a.pl[i].client.me >= 1 && a.pl[i].client.me <= GTN_MAX_LOCAL);
        for (int k = 0; k < i; k++) REQUIRE(a.pl[i].client.me != a.pl[k].client.me);
        /* A fixed four-orientation piece makes movement/rotation independent of RNG. */
        tn_game *g = &a.pl[i].client.game;
        memset(g->field, 0, sizeof g->field);
        g->block = 2;
        g->orient = 0;
        g->bx = 4;
        g->by = 4;
        a.pl[i].client.spawn_wait = 0;
    }
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        int x[GTN_MAX_LOCAL];
        for (int k = 0; k < GTN_MAX_LOCAL; k++) x[k] = a.pl[k].client.game.bx;
        button(&a, pads[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
        for (int k = 0; k < GTN_MAX_LOCAL; k++) {
            REQUIRE(a.pl[k].client.game.bx == x[k] - (k == i));
            REQUIRE(a.pl[k].hold == (k == i ? -1 : 0));
        }
        button(&a, pads[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0);
        REQUIRE(!a.pl[i].hold);
        tap(&a, pads[i], SDL_CONTROLLER_BUTTON_A);
        for (int k = 0; k < GTN_MAX_LOCAL; k++) REQUIRE(a.pl[k].client.game.orient == (k == i));
        tap(&a, pads[i], SDL_CONTROLLER_BUTTON_B);
        REQUIRE(a.pl[i].client.game.orient == 0);
    }

    /* Concurrent holds and independent releases exercise all eight repeat states. */
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        a.pl[i].client.game.bx = 4;
        button(&a, pads[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
        button(&a, pads[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1);
    }
    gtn_input_update(&a, 170);
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        REQUIRE(a.pl[i].client.game.bx == 2);
        REQUIRE(a.pl[i].hold == -1 && a.pl[i].client.soft_held);
    }
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        button(&a, pads[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0);
        button(&a, pads[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0);
        for (int k = 0; k < GTN_MAX_LOCAL; k++) {
            REQUIRE(a.pl[k].hold == (k > i ? -1 : 0));
            REQUIRE(!!a.pl[k].client.soft_held == (k > i));
        }
    }

    int me = a.pl[3].client.me;
    char nick[TN_NICK_MAX];
    snprintf(nick, sizeof nick, "%s", a.pl[3].nick);
    for (int repeat = 0; repeat < 4; repeat++) {
        axis(&a, pads[3], SDL_CONTROLLER_AXIS_LEFTX, 24000);
        axis(&a, pads[3], SDL_CONTROLLER_AXIS_LEFTY, 24000);
        button(&a, pads[3], SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1);
        button(&a, pads[3], SDL_CONTROLLER_BUTTON_BACK, 1);
        REQUIRE(a.pl[3].hold == 1 && a.pl[3].stick_x == 1 && a.pl[3].stick_y == 1);
        REQUIRE(a.pl[3].client.soft_held && back_held[3]);
        SDL_JoystickID old = pads[3].id;
        detach(&a, &pads[3]);
        REQUIRE(a.pl[3].active && a.pl[3].pad == -1);
        REQUIRE(!a.pl[3].hold && !a.pl[3].stick_x && !a.pl[3].stick_y);
        REQUIRE(!a.pl[3].client.soft_held && !back_held[3]);
        int x = a.pl[3].client.game.bx;
        gtn_input_update(&a, 500);
        REQUIRE(a.pl[3].client.game.bx == x);
        REQUIRE(a.pl[3].client.me == me && !strcmp(a.pl[3].nick, nick));
        /* First replace with another existing pad, then recreate the detached device. */
        if (repeat == 0) {
            pads[3] = spare;
            spare.joystick = NULL;
        } else pads[3] = attach(&a);
        REQUIRE(pads[3].id != old);
        tap(&a, pads[3], SDL_CONTROLLER_BUTTON_A);
        REQUIRE(a.pl[3].pad == pads[3].id && gtn_local_count(&a) == GTN_MAX_LOCAL);
        REQUIRE(a.pl[3].client.me == me && !strcmp(a.pl[3].nick, nick));
        REQUIRE(!a.pl[3].hold && !a.pl[3].client.soft_held && !back_held[3]);
        /* Returning to neutral and moving again must produce fresh input. */
        axis(&a, pads[3], SDL_CONTROLLER_AXIS_LEFTX, -24000);
        REQUIRE(a.pl[3].hold == -1 && a.pl[3].client.game.bx == x - 1);
        axis(&a, pads[3], SDL_CONTROLLER_AXIS_LEFTX, 0);
        REQUIRE(!a.pl[3].hold && !a.pl[3].stick_x);
        for (int i = 0; i < GTN_MAX_LOCAL; i++) {
            REQUIRE(a.pl[i].pad == pads[i].id);
            REQUIRE(!a.pl[i].hold && !a.pl[i].client.soft_held);
        }
    }
    stop_session(&a);
    for (int i = GTN_MAX_LOCAL - 1; i >= 0; i--) detach(&a, &pads[i]);
    REQUIRE(SDL_NumJoysticks() == 0);
    SDL_Quit();
    printf("%d virtual controller checks, %d adds, %d removals, %d button events, %d axis events\n",
           checks, added, removed, buttons, axes);
    return 0;
}
