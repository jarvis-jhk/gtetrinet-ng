/* SPDX-License-Identifier: GPL-2.0-or-later
 * Real application input/TCP play with an isolated preference path. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#undef main

/* Only the platform preference directory is replaced; load/save use actual files. */
static const char *pref_path;
static char *test_pref_path(const char *org, const char *app) { return SDL_strdup(pref_path); }
#define SDL_GetPrefPath test_pref_path
#include "../src/app/settings.c"
#undef SDL_GetPrefPath

#ifdef _WIN32
#include <direct.h>
static int make_dir(const char *path) { return _mkdir(path); }
static int remove_dir(const char *path) { return _rmdir(path); }
#else
#include <sys/stat.h>
#include <unistd.h>
static int make_dir(const char *path) { return mkdir(path, 0700); }
static int remove_dir(const char *path) { return rmdir(path); }
#endif

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

static void key(gtn_app *a, SDL_Keycode code, int down)
{
    SDL_Event e = {0};
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.keysym.sym = code;
    gtn_input_event(a, &e);
}

static void pad(gtn_app *a, Uint8 button, int down)
{
    SDL_Event e = {0};
    e.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    e.cbutton.which = 42;
    e.cbutton.button = button;
    gtn_input_event(a, &e);
}

static void await_screen(gtn_app *a, enum gtn_screen screen)
{
    for (int t = 0; t < 2000 && a->screen != screen; t++) { update_session(a, 2); SDL_Delay(1); }
    CHECK(a->screen == screen);
}

static void piece(gtn_local *p)
{
    memset(p->client.game.field, 0, sizeof p->client.game.field);
    p->client.game.block = 2;
    p->client.game.orient = 0;
    p->client.game.bx = 4;
    p->client.game.by = 4;
    p->client.spawn_wait = 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    pref_path = argv[1];
    static gtn_app a;
    char path[1200];
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_EVENTS) == 0);
    CHECK(tn_net_init() == 0);
    gtn_settings_load(&a.settings);
    CHECK(a.settings.repeat_profile == REPEAT_NORMAL);
    snprintf(a.settings.nick, sizeof a.settings.nick, "RepeatPlayer");
    snprintf(a.settings.host, sizeof a.settings.host, "example.invalid");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");
    a.settings.fast = 1;

    /* Options are usable without creating a player, and releases do not cycle twice. */
    a.screen = SCR_TITLE;
    pad(&a, SDL_CONTROLLER_BUTTON_X, 1);
    CHECK(a.screen == SCR_INPUT && gtn_local_count(&a) == 0);
    pad(&a, SDL_CONTROLLER_BUTTON_A, 1);
    pad(&a, SDL_CONTROLLER_BUTTON_A, 0);
    CHECK(a.settings.repeat_profile == REPEAT_RELAXED && gtn_local_count(&a) == 0);
    gtn_settings loaded;
    gtn_settings_load(&loaded);
    CHECK(loaded.repeat_profile == REPEAT_RELAXED);
    CHECK(!strcmp(loaded.nick, "RepeatPlayer") && !strcmp(loaded.host, "example.invalid"));
    CHECK(!strcmp(loaded.last_version, "fixture") && loaded.fast == 1);
    key(&a, SDLK_RIGHT, 1);
    CHECK(a.settings.repeat_profile == REPEAT_FAST);
    key(&a, SDLK_RIGHT, 1);
    CHECK(a.settings.repeat_profile == REPEAT_NORMAL);
    key(&a, SDLK_LEFT, 1);
    CHECK(a.settings.repeat_profile == REPEAT_FAST);
    pad(&a, SDL_CONTROLLER_BUTTON_Y, 1);
    gtn_settings_load(&loaded);
    CHECK(loaded.repeat_profile == REPEAT_NORMAL);
    key(&a, SDLK_ESCAPE, 1);
    CHECK(a.screen == SCR_TITLE);

    /* A local client whose view still lacks slot 1 must not be chosen to start the game. */
    static gtn_app lag;
    lag.pl[0].active = lag.pl[1].active = 1;
    lag.pl[0].client.state = lag.pl[1].client.state = TN_CONNECTED;
    lag.pl[0].client.me = 2; lag.pl[0].client.slot[2].present = 1;
    lag.pl[1].client.me = 1; lag.pl[1].client.slot[1].present = 1;
    CHECK(local_moderator(&lag) == 1);
    lag.pl[1].active = 0;
    CHECK(local_moderator(&lag) == 0);
    lag.pl[1].active = 1; lag.pl[1].client.state = TN_LOGGING_IN;
    CHECK(local_moderator(&lag) == -1);

    /* Two real clients: controller settings affect keyboard too, without cross-seat movement. */
    key(&a, SDLK_RETURN, 1);
    pad(&a, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK(gtn_local_count(&a) == 2);
    host_local(&a);
    await_screen(&a, SCR_LOBBY);
    /* As a player would, start once the lobby knows who moderates; an early press
     * only shows the moderator hint (native Windows CI joins more slowly). */
    for (int t = 0; t < 2000 && local_moderator(&a) != 0; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(local_moderator(&a) == 0);
    key(&a, SDLK_RETURN, 1);
    await_screen(&a, SCR_GAME);
    const int delays[] = {170, 250, 120}, intervals[] = {50, 100, 30};
    for (int profile = 0; profile < 3; profile++) {
        a.settings.repeat_profile = profile;
        gtn_settings_save(&a.settings);
        gtn_settings_load(&a.settings);
        CHECK(a.settings.repeat_profile == profile);
        for (int device = 0; device < 2; device++) {
            piece(&a.pl[0]); piece(&a.pl[1]);
            if (device) pad(&a, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);
            else key(&a, SDLK_LEFT, 1);
            CHECK(a.pl[device].client.game.bx == 3);
            CHECK(a.pl[1 - device].client.game.bx == 4);
            gtn_input_update(&a, delays[profile] - 1);
            CHECK(a.pl[device].client.game.bx == 3);
            gtn_input_update(&a, 1);
            CHECK(a.pl[device].client.game.bx == 2);
            gtn_input_update(&a, intervals[profile] - 1);
            CHECK(a.pl[device].client.game.bx == 2);
            gtn_input_update(&a, 1);
            CHECK(a.pl[device].client.game.bx == 1);
            if (device) pad(&a, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0);
            else key(&a, SDLK_LEFT, 0);
            gtn_input_update(&a, 1000);
            CHECK(a.pl[device].client.game.bx == 1 && !a.pl[device].hold);
            CHECK(a.pl[1 - device].client.game.bx == 4);
        }
    }
    /* Reversal starts a fresh delay, and the old direction's release cannot cancel it. */
    piece(&a.pl[0]);
    key(&a, SDLK_LEFT, 1);
    key(&a, SDLK_RIGHT, 1);
    key(&a, SDLK_LEFT, 0);
    gtn_input_update(&a, 119);
    CHECK(a.pl[0].client.game.bx == 4 && a.pl[0].hold == 1);
    gtn_input_update(&a, 61);
    CHECK(a.pl[0].client.game.bx == 7);
    key(&a, SDLK_RIGHT, 0);
    stop_session(&a);
    key(&a, SDLK_s, 1);
    CHECK(a.screen == SCR_INPUT && gtn_local_count(&a) == 2);
    pad(&a, SDL_CONTROLLER_BUTTON_START, 1);
    CHECK(a.screen == SCR_TITLE && gtn_local_count(&a) == 2);

    /* Old files and malformed/unbounded values must retain finite, safe default timing. */
    const char *values[] = {"", "repeat=-1\n", "repeat=3\n", "repeat=999999999999999999999\n", "repeat=2junk\n"};
    for (size_t i = 0; i < sizeof values / sizeof *values; i++) {
        FILE *f = fopen(path, "w");
        CHECK(f != NULL);
        if (!f) continue;
        fputs(values[i], f); fclose(f);
        gtn_settings_load(&loaded);
        CHECK(loaded.repeat_profile == REPEAT_NORMAL);
    }
    CHECK(gtn_repeat_delay(-1) == 170 && gtn_repeat_interval(999) == 50);

    /* A real write failure retains the old file and visibly reports unsaved input options. */
    char blocked[1210];
    snprintf(blocked, sizeof blocked, "%s.tmp", path);
    CHECK(make_dir(blocked) == 0);
    a.screen = SCR_INPUT;
    a.settings.repeat_profile = REPEAT_NORMAL;
    pad(&a, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK(a.settings.repeat_profile == REPEAT_RELAXED);
    CHECK(strstr(a.status, "could not be saved") != NULL);
    gtn_settings_load(&loaded);
    CHECK(loaded.repeat_profile == REPEAT_NORMAL);
    CHECK(remove_dir(blocked) == 0);
    CHECK(gtn_settings_save(&a.settings) == 0);
    gtn_settings_load(&loaded);
    CHECK(loaded.repeat_profile == REPEAT_RELAXED);
    a.status[0] = 0;

    /* Render the actual options page in both supported Deck-size layouts. */
    const int widths[] = {800, 1280}, heights[] = {600, 800};
    for (int i = 0; i < 2; i++) {
        a.w = widths[i]; a.h = heights[i]; a.screen = SCR_INPUT;
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, a.w, a.h, 32, SDL_PIXELFORMAT_ARGB8888);
        a.ren = SDL_CreateSoftwareRenderer(surface);
        CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
        if (a.ren) {
            a.settings.repeat_profile = REPEAT_RELAXED;
            gtn_draw(&a); SDL_RenderPresent(a.ren);
            snprintf(path, sizeof path, "%soptions-%d.bmp", pref_path, a.w);
            CHECK(SDL_SaveBMP(surface, path) == 0);
            gtn_render_quit(); SDL_DestroyRenderer(a.ren);
        }
        SDL_FreeSurface(surface);
    }
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_Quit();
    printf("%d repeat checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
