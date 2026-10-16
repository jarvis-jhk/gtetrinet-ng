/* SPDX-License-Identifier: GPL-2.0-or-later
 * The game itself on someone else's server: four local players join a running TetriNET server
 * through the online entry, one of them has to pick another name, a game started by an online
 * player reaches all of them, and ending the session leaves the server running.
 *
 *   test_online HOST PORT [fast-unsupported]
 *
 * tests/interop.sh runs it against tetrinetx and tetrinet-server. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#undef main

static const char *pref_path = "./";
static char *test_pref_path(const char *org, const char *app) { return SDL_strdup(pref_path); }
#define SDL_GetPrefPath test_pref_path
#include "../src/app/settings.c"
#undef SDL_GetPrefPath

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

static void key(gtn_app *a, SDL_Keycode code)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = code;
    gtn_input_event(a, &e);
    e.type = SDL_KEYUP;
    gtn_input_event(a, &e);
}

static void tap(gtn_app *a, int which, Uint8 button)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_CONTROLLERBUTTONDOWN;
    e.cbutton.which = which;
    e.cbutton.button = button;
    gtn_input_event(a, &e);
    e.type = SDL_CONTROLLERBUTTONUP;
    gtn_input_event(a, &e);
}

static tn_client online;   /* somebody already on the server */

static void step(gtn_app *a, int n)
{
    for (int t = 0; t < n; t++) {
        update_session(a, 5);
        tn_client_update(&online, 5);
        SDL_Delay(5);
    }
}

static int sees(const tn_client *c, const char *nick)
{
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (c->slot[i].present && !strcmp(c->slot[i].nick, nick)) return 1;
    return 0;
}

static void dump(gtn_app *a, const char *why)
{
    fprintf(stderr, "-- %s\n", why);
    const tn_client *all[5] = { &online, &a->pl[0].client, &a->pl[1].client, &a->pl[2].client, &a->pl[3].client };
    for (int k = 0; k < 5; k++) {
        const tn_client *c = all[k];
        fprintf(stderr, "%s: state %d me %d ingame %d playing %d slots:", c->nick, c->state, c->me, c->ingame, c->playing);
        for (int s = 1; s <= TN_MAX_SLOTS; s++) if (c->slot[s].present) fprintf(stderr, " %d=%s", s, c->slot[s].nick);
        fprintf(stderr, "\n");
        for (int i = c->chat.count - 1; i >= 0; i--) fprintf(stderr, "   chat: %s\n", tn_log_get(&c->chat, i));
    }
}

static void open_online(gtn_app *a, const char *host, int port, int fast)
{
    snprintf(a->entry, sizeof a->entry, "%s:%d", host, port);
    a->settings.fast = fast;
    connect_online(a);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s HOST PORT [fast-unsupported]\n", argv[0]); return 2; }
    const char *host = argv[1];
    int port = atoi(argv[2]);
    int fast_unsupported = argc > 3 && !strcmp(argv[3], "fast-unsupported");
    static gtn_app a;
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_EVENTS) == 0);
    CHECK(tn_net_init() == 0);
    gtn_settings_load(&a.settings);
    snprintf(a.settings.nick, sizeof a.settings.nick, "Jan");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");
    a.screen = SCR_TITLE;

    key(&a, SDLK_RETURN);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    tap(&a, 43, SDL_CONTROLLER_BUTTON_A);
    tap(&a, 44, SDL_CONTROLLER_BUTTON_A);
    CHECK(gtn_local_count(&a) == 4);

    if (fast_unsupported) {
        open_online(&a, host, port, 1);
        for (int t = 0; t < 4000 && a.screen != SCR_TITLE; t++) step(&a, 1);
        CHECK(a.screen == SCR_TITLE);
        CHECK(strstr(a.status, "TetriFast") != NULL);
        /* nobody got in, so all four seats are still joined for the next try */
        CHECK(gtn_local_count(&a) == 4);
        printf("status: %s\n", a.status);
        a.status[0] = 0;
    }

    tn_client_init(&online, "Jan", "", 5);
    tn_client_connect(&online, host, port, 0);
    for (int t = 0; t < 4000 && online.state != TN_CONNECTED && online.state != TN_FAILED; t++) step(&a, 1);
    CHECK(online.state == TN_CONNECTED);

    open_online(&a, host, port, 0);
    for (int t = 0; t < 4000 && a.screen != SCR_LOBBY; t++) step(&a, 1);
    CHECK(a.screen == SCR_LOBBY);
    step(&a, 200);
    /* "Jan" is taken online, so the first seat came in under another name */
    CHECK(strcmp(a.pl[0].nick, "Jan") != 0 && !strncmp(a.pl[0].nick, "Jan_", 4));
    for (int i = 0; i < 4; i++) {
        const tn_client *c = &a.pl[i].client;
        CHECK(c->state == TN_CONNECTED);
        CHECK(sees(c, "Jan") && sees(c, a.pl[0].nick) && sees(c, "Jan2") && sees(c, "Jan3") && sees(c, "Jan4"));
        CHECK(sees(&online, a.pl[i].nick));
    }
    if (failures) dump(&a, "lobby");
    /* the online player was first, so the lobby must not offer our seats the start */
    CHECK(local_moderator(&a) < 0);

    tn_client_start_game(&online);
    for (int t = 0; t < 1000 && a.screen != SCR_GAME; t++) step(&a, 1);
    CHECK(a.screen == SCR_GAME);
    step(&a, 200);
    for (int i = 0; i < 4; i++) CHECK(a.pl[i].client.playing);
    tn_client_stop_game(&online);
    for (int t = 0; t < 1000 && a.pl[0].client.ingame; t++) step(&a, 1);
    for (int i = 0; i < 4; i++) CHECK(!a.pl[i].client.ingame);

    /* ending the session: all four leave together, and the server survives it */
    stop_session(&a);
    CHECK(a.screen == SCR_TITLE);
    step(&a, 400);
    CHECK(online.state == TN_CONNECTED);
    for (int i = 0; i < 4; i++) CHECK(!sees(&online, a.pl[i].nick));
    tn_client later;
    tn_client_init(&later, "Later", "", 9);
    tn_client_connect(&later, host, port, 0);
    for (int t = 0; t < 4000 && later.state != TN_CONNECTED && later.state != TN_FAILED; t++) {
        step(&a, 1);
        tn_client_update(&later, 5);
    }
    CHECK(later.state == TN_CONNECTED);
    tn_client_disconnect(&later);
    step(&a, 50);
    tn_client_disconnect(&online);

    if (failures) dump(&a, "end");
    remove("./settings.ini");
    SDL_Quit();
    printf("test_online %s:%d: %d checks, %d failures\n", host, port, checks, failures);
    return failures ? 1 : 0;
}
