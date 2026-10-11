/* SPDX-License-Identifier: GPL-2.0-or-later
 * Exercise the actual SDL event dispatcher and application button handlers without rendering.
 * These simulated events prove routing; physical device/Steam Input verification remains open. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#undef main

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

static void pad(gtn_app *a, int id, Uint8 button, int down)
{
    SDL_Event event = {0};
    event.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    event.cbutton.which = id;
    event.cbutton.button = button;
    gtn_input_event(a, &event);
}

int main(void)
{
    static gtn_app a;
    tn_net_init();
    SDL_SetMainReady();
    SDL_Init(SDL_INIT_EVENTS);
    a.screen = SCR_TITLE;
    snprintf(a.settings.nick, sizeof a.settings.nick, "Player");
    for (int i = 0; i < 8; i++) {
        pad(&a, 100 + i, SDL_CONTROLLER_BUTTON_A, 1);
        CHECK(gtn_local_count(&a) == i + 1);
        CHECK(a.pl[i].pad == 100 + i);
        for (int k = 0; k < i; k++) CHECK(strcmp(a.pl[i].nick, a.pl[k].nick));
    }
    pad(&a, 999, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK(gtn_local_count(&a) == 8);
    /* Actual TCP match, eight app-owned client sessions. */
    host_local(&a);
    for (int t = 0; t < 2000 && a.screen != SCR_LOBBY; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_LOBBY);
    pad(&a, 100, SDL_CONTROLLER_BUTTON_START, 1);
    for (int t = 0; t < 2000 && a.screen != SCR_GAME; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_GAME);
    for (int i = 0; i < 8; i++) {
        tn_game_spawn(&a.pl[i].client.game);
        CHECK(a.pl[i].client.playing);
    }
    int x0 = a.pl[0].client.game.bx, x1 = a.pl[1].client.game.bx;
    pad(&a, 101, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
    CHECK(a.pl[0].client.game.bx == x0);
    CHECK(a.pl[1].client.game.bx == x1 - 1);
    CHECK(a.pl[1].hold == -1 && a.pl[0].hold == 0);
    pad(&a, 101, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1);
    pad(&a, 101, SDL_CONTROLLER_BUTTON_BACK, 1);
    CHECK(a.pl[1].client.soft_held != 0);
    SDL_Event event = {0};
    event.type = SDL_CONTROLLERDEVICEREMOVED;
    event.cdevice.which = 101;
    gtn_input_event(&a, &event);
    CHECK(a.pl[1].active && a.pl[1].pad == -1);
    CHECK(!a.pl[1].hold && !a.pl[1].client.soft_held && !back_held[1]);
    int me = a.pl[1].client.me;
    pad(&a, 900, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK(a.pl[1].pad == 900 && a.pl[1].client.me == me);
    CHECK(gtn_local_count(&a) == 8);
    pad(&a, 101, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
    CHECK(!a.pl[1].hold);
    pad(&a, 900, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1);
    CHECK(a.pl[1].hold == 1 && !a.pl[0].hold);
    /* Releasing a seat and joining again must not inherit a held Back chord. */
    stop_session(&a);
    back_held[3] = 1;
    leave_local(&a, 3);
    pad(&a, 901, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK(a.pl[3].pad == 901 && !back_held[3]);
    stop_session(&a);
    SDL_Quit();
    printf("%d input checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
