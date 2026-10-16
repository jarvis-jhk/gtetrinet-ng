/* SPDX-License-Identifier: GPL-2.0-or-later
 * Computer opponents: added and removed in the lobby of a local game, their strength saved,
 * and a real TCP game in which they move, clear lines, use specials and outlast an idle human. */
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

static void write_file(const char *path, const char *t)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs(t, f);
    fclose(f);
}

static void run(gtn_app *a, int ms, int dt)
{
    for (int t = 0; t < ms; t += dt) { update_session(a, dt); SDL_Delay(1); }
}

static int bots(const gtn_app *a)
{
    int n = 0;
    for (int i = 0; i < GTN_MAX_LOCAL; i++) n += a->pl[i].active && a->pl[i].dev == DEV_BOT;
    return n;
}

static int all_see(const gtn_app *a, const char *nick)
{
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        const tn_client *c = &a->pl[i].client;
        if (!a->pl[i].active) continue;
        int seen = 0;
        for (int s = 1; s <= TN_MAX_SLOTS; s++) seen |= c->slot[s].present && !strcmp(c->slot[s].nick, nick);
        if (!seen) return 0;
    }
    return 1;
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

    /* Strengths: slower and sloppier as they get easier. */
    tn_bot e, n, h;
    tn_bot_init_level(&e, TN_BOT_EASY, 1);
    tn_bot_init_level(&n, TN_BOT_NORMAL, 1);
    tn_bot_init_level(&h, TN_BOT_HARD, 1);
    CHECK(e.think_ms > n.think_ms && n.think_ms > h.think_ms);
    CHECK(e.mistake_pct > n.mistake_pct && n.mistake_pct > h.mistake_pct && h.mistake_pct == 0);
    tn_bot_init_level(&e, 99, 1);
    CHECK(e.think_ms == n.think_ms);
    tn_bot_init_level(&e, TN_BOT_EASY, 1);

    /* The saved strength: default Normal, unknown values ignored. */
    gtn_settings loaded;
    gtn_settings_load(&loaded);
    CHECK(loaded.bot_level == TN_BOT_NORMAL);
    write_file(path, "bots=hard\n");
    gtn_settings_load(&loaded);
    CHECK(loaded.bot_level == TN_BOT_HARD);
    write_file(path, "bots=godlike\n");
    gtn_settings_load(&loaded);
    CHECK(loaded.bot_level == TN_BOT_NORMAL);
    remove(path);

    gtn_settings_load(&a.settings);
    snprintf(a.settings.nick, sizeof a.settings.nick, "Jan");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");
    a.screen = SCR_TITLE;

    /* One person on a controller; on someone else's server X adds nobody. */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(gtn_local_count(&a) == 1);
    a.screen = SCR_LOBBY;
    a.status[0] = 0;
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);
    CHECK(bots(&a) == 0 && a.status[0]);
    a.screen = SCR_TITLE;

    /* A local game: X three times, LB once, from the controller; S from the keyboard. */
    host_local(&a);
    for (int t = 0; t < 3000 && a.screen != SCR_LOBBY; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_LOBBY);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);
    CHECK(bots(&a) == 3 && gtn_local_count(&a) == 4);
    CHECK(!strcmp(a.pl[1].nick, "CPU1") && !strcmp(a.pl[2].nick, "CPU2") && !strcmp(a.pl[3].nick, "CPU3"));
    tap(&a, 42, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    CHECK(bots(&a) == 2 && !a.pl[3].active);
    key(&a, SDLK_s);   /* the keyboard has not joined: it adds nobody */
    CHECK(bots(&a) == 2);
    /* the freed name is used again */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);
    CHECK(bots(&a) == 3 && !strcmp(a.pl[3].nick, "CPU3"));
    tap(&a, 42, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    /* RB: Normal -> Hard, saved, and the bots already seated play at the new strength */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    CHECK(a.settings.bot_level == TN_BOT_HARD && a.pl[1].bot.think_ms == h.think_ms);
    gtn_settings_load(&loaded);
    CHECK(loaded.bot_level == TN_BOT_HARD);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);   /* wraps to Easy */
    CHECK(a.settings.bot_level == TN_BOT_EASY && a.pl[2].bot.mistake_pct > 0);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    CHECK(a.settings.bot_level == TN_BOT_HARD);

    run(&a, 1500, 2);
    CHECK(all_see(&a, "Jan") && all_see(&a, "CPU1") && all_see(&a, "CPU2"));

    /* Render the lobby with its bot line at small and Deck sizes, in both languages. */
    const int widths[] = {800, 1280}, heights[] = {600, 800};
    for (int lang = 0; lang < 2; lang++) {
        gtn_lang_set(lang ? GTN_LANG_DE : GTN_LANG_EN);
        for (int i = 0; i < 2; i++) {
            a.w = widths[i]; a.h = heights[i];
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, a.w, a.h, 32, SDL_PIXELFORMAT_ARGB8888);
            a.ren = SDL_CreateSoftwareRenderer(surface);
            CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
            if (a.ren) {
                a.status[0] = 0;
                gtn_text_abbreviated = 0;
                gtn_draw(&a); SDL_RenderPresent(a.ren);
                CHECK(gtn_text_abbreviated == 0);
                snprintf(path, sizeof path, "%sbots-%s-lobby-%d.bmp", pref_path, lang ? "de" : "en", a.w);
                CHECK(SDL_SaveBMP(surface, path) == 0);
                gtn_render_quit(); SDL_DestroyRenderer(a.ren);
            }
            SDL_FreeSurface(surface);
        }
    }
    gtn_lang_set(GTN_LANG_EN);
    a.ren = NULL;

    /* The human starts and then does nothing; the computer players play on their own. */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    for (int t = 0; t < 3000 && a.screen != SCR_GAME; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_GAME);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);   /* in a game X is a special, not another bot */
    CHECK(bots(&a) == 2);
    for (int t = 0; t < 12000 && a.pl[0].client.playing; t++) { update_session(&a, 16); SDL_Delay(1); }
    CHECK(!a.pl[0].client.playing && a.pl[0].client.ev.losses == 1);
    unsigned locks = 0, clears = 0, specials = 0, moved = 0;
    for (int i = 1; i <= 2; i++) {
        const tn_events *ev = &a.pl[i].client.ev;
        locks += ev->locks; clears += ev->clears; specials += ev->specials;
        moved += a.pl[i].client.playing;
    }
    fprintf(stderr, "bots: %u locks, %u clears, %u specials, %u still playing\n", locks, clears, specials, moved);
    CHECK(locks >= 20 && clears >= 2 && specials >= 1 && moved >= 1);

    /* The moderator ends the game: back in the lobby the bots keep their seats. */
    int m = local_moderator(&a);
    CHECK(m == 0);
    if (m >= 0) tn_client_stop_game(&a.pl[m].client);
    for (int t = 0; t < 3000 && a.screen != SCR_LOBBY; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_LOBBY && bots(&a) == 2);

    /* Leaving the session sends them home; the human stays joined on the title screen. */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_TITLE && bots(&a) == 0 && gtn_local_count(&a) == 1);

    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_Quit();
    printf("%d bot checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
