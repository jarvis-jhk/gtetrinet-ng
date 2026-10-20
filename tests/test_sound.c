/* SPDX-License-Identifier: GPL-2.0-or-later
 * Sound effects and rumble: the synthesized effects, the saved switches, the options rows and
 * a real local TCP game whose events turn into sounds and rumble on the right controller. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#include "await_game.h"
#undef main

static const char *pref_path;
static char *test_pref_path(const char *org, const char *app) { return SDL_strdup(pref_path); }
#define SDL_GetPrefPath test_pref_path
#include "../src/app/settings.c"
#undef SDL_GetPrefPath

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

static void key(gtn_app *a, SDL_Keycode code, int down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.keysym.sym = code;
    gtn_input_event(a, &e);
}

static void pad(gtn_app *a, int which, Uint8 button, int down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    e.cbutton.which = which;
    e.cbutton.button = button;
    gtn_input_event(a, &e);
}

static void press(gtn_app *a, int which, Uint8 button) { pad(a, which, button, 1); pad(a, which, button, 0); }

static void settle(gtn_app *a, int ms)
{
    for (int t = 0; t < ms; t += 2) { update_session(a, 2); SDL_Delay(1); }
}

/* Waits until the effect has been played n more times than before, or gives up. */
static int await_sfx(gtn_app *a, enum gtn_sfx s, unsigned before)
{
    for (int t = 0; t < 3000 && gtn_sfx_played[s] == before; t++) { update_session(a, 2); SDL_Delay(1); }
    return gtn_sfx_played[s] - before;
}

static void piece(gtn_local *p)
{
    p->client.game.block = 0;                 /* the I piece, lying flat */
    p->client.game.orient = 0;
    p->client.game.bx = 4;
    p->client.game.by = 0;
    p->client.spawn_wait = 0;                 /* the piece is in play; the test drops it at once */
    p->client.fall_acc = 0;
}

static void give_special(gtn_local *p, int sb)
{
    p->client.game.specials[0] = (unsigned char)tn_sb_cell(sb);
    p->client.game.nspecials = 1;
}

static void check_effects(void)
{
    static int16_t buf[GTN_SFX_RATE * 2];
    int lens[SFX_COUNT];
    for (int s = 0; s < SFX_COUNT; s++) {
        int len = gtn_sfx_render((enum gtn_sfx)s, buf, (int)(sizeof buf / sizeof *buf));
        lens[s] = len;
        int peak = 0;
        for (int i = 0; i < len; i++) if (abs(buf[i]) > peak) peak = abs(buf[i]);
        CHECK(len > GTN_SFX_RATE / 50 && len < GTN_SFX_RATE);     /* 20 ms .. 1 s */
        CHECK(peak > 3000);                                       /* audible */
        CHECK(len > 0 && abs(buf[0]) < 2000 && abs(buf[len - 1]) < 2000); /* no click */
        CHECK(gtn_sfx_render((enum gtn_sfx)s, NULL, 1 << 20) == len);
        int16_t again[64];
        CHECK(gtn_sfx_render((enum gtn_sfx)s, again, 64) == 64 && !memcmp(again, buf, sizeof again));
    }
    /* more lines, a longer and higher-reaching jingle; the lock is the shortest sound */
    CHECK(lens[SFX_CLEAR1] < lens[SFX_CLEAR2] && lens[SFX_CLEAR2] < lens[SFX_CLEAR3] && lens[SFX_CLEAR3] < lens[SFX_CLEAR4]);
    for (int s = 0; s < SFX_COUNT; s++) if (s != SFX_LOCK && s != SFX_MENU) CHECK(lens[SFX_LOCK] < lens[s]);
    CHECK(gtn_sfx_render(SFX_COUNT, buf, 10) == 0);
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

    check_effects();

    /* Defaults, saved values and malformed files. */
    gtn_settings_load(&a.settings);
    CHECK(a.settings.volume == VOLUME_NORMAL && !a.settings.rumble_off);
    const char *bad[] = {"sound=\nrumble=\n", "sound=11\nrumble=yes\n", "sound=LOUD\nrumble=0\n", "sound=loudest\n"};
    gtn_settings loaded;
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        FILE *f = fopen(path, "w");
        CHECK(f != NULL);
        if (!f) continue;
        fputs(bad[i], f); fclose(f);
        gtn_settings_load(&loaded);
        CHECK(loaded.volume == VOLUME_NORMAL && !loaded.rumble_off);
    }
    remove(path);
    snprintf(a.settings.nick, sizeof a.settings.nick, "Sound");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");

    /* Options: Sound and Rumble rows, before anyone joined, from a controller. */
    a.screen = SCR_TITLE;
    press(&a, 42, SDL_CONTROLLER_BUTTON_X);
    CHECK(a.screen == SCR_INPUT && gtn_local_count(&a) == 0);
    for (int i = 0; i < 3; i++) press(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(a.options_row == 3);
    unsigned preview = gtn_sfx_played[SFX_CLEAR2];
    press(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK(a.settings.volume == VOLUME_LOUD && gtn_sfx_played[SFX_CLEAR2] == preview + 1);
    gtn_sound_frame();
    press(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.settings.volume == VOLUME_OFF);
    gtn_settings_load(&loaded);
    CHECK(loaded.volume == VOLUME_OFF);
    key(&a, SDLK_LEFT, 1); key(&a, SDLK_LEFT, 0);
    CHECK(a.settings.volume == VOLUME_LOUD);
    press(&a, 42, SDL_CONTROLLER_BUTTON_Y);
    CHECK(a.settings.volume == VOLUME_NORMAL);
    press(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(a.options_row == 4);
    press(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.settings.rumble_off == 1);
    gtn_settings_load(&loaded);
    CHECK(loaded.rumble_off == 1 && loaded.volume == VOLUME_NORMAL && !strcmp(loaded.nick, "Sound"));
    press(&a, 42, SDL_CONTROLLER_BUTTON_Y);
    CHECK(a.settings.rumble_off == 0);
    gtn_settings_load(&loaded);
    CHECK(loaded.rumble_off == 0);
    /* the rules row below: Right switches to TetriNET 2 for games hosted here, saved
     * (A opens the host settings, test_rules) */
    press(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(a.options_row == 5);
    press(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    gtn_settings_load(&loaded);
    CHECK(a.settings.rules_tn2 == 1 && loaded.rules_tn2 == 1);
    press(&a, 42, SDL_CONTROLLER_BUTTON_Y);
    gtn_settings_load(&loaded);
    CHECK(a.settings.rules_tn2 == 0 && loaded.rules_tn2 == 0);
    press(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN); /* block style, then around */
    press(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(a.options_row == 0);
    press(&a, 42, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_TITLE && gtn_local_count(&a) == 0);

    /* Options page renders in both Deck-size layouts with nothing cut at 1280x800. */
    const int widths[] = {800, 1280}, heights[] = {600, 800};
    for (int lang = 0; lang < 2; lang++)
        for (int i = 0; i < 2; i++) {
            gtn_lang_apply(lang ? LANG_DE : LANG_EN);
            a.w = widths[i]; a.h = heights[i]; a.screen = SCR_INPUT; a.options_row = 4;
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, a.w, a.h, 32, SDL_PIXELFORMAT_ARGB8888);
            a.ren = SDL_CreateSoftwareRenderer(surface);
            CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
            if (a.ren) {
                gtn_text_abbreviated = 0;
                gtn_draw(&a); SDL_RenderPresent(a.ren);
                if (a.w == 1280) CHECK(gtn_text_abbreviated == 0);
                snprintf(path, sizeof path, "%soptions-%s-%d.bmp", pref_path, lang ? "de" : "en", a.w);
                CHECK(SDL_SaveBMP(surface, path) == 0);
                gtn_render_quit(); SDL_DestroyRenderer(a.ren);
                a.ren = NULL;
            }
            SDL_FreeSurface(surface);
        }
    gtn_lang_apply(LANG_EN);
    a.screen = SCR_TITLE;

    /* A real two-controller game on the embedded server. */
    press(&a, 42, SDL_CONTROLLER_BUTTON_A);
    press(&a, 43, SDL_CONTROLLER_BUTTON_A);
    CHECK(gtn_local_count(&a) == 2 && a.pl[0].pad == 42 && a.pl[1].pad == 43);
    host_local(&a);
    for (int t = 0; t < 3000 && local_moderator(&a) != 0; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_LOBBY && local_moderator(&a) == 0);
    tn_client_start_game(&a.pl[0].client);
    for (int t = 0; t < 3000 && (a.screen != SCR_GAME || !all_playing(&a)); t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_GAME);
    settle(&a, 100);
    gtn_local *p0 = &a.pl[0], *p1 = &a.pl[1];
    int slot1 = p1->client.me, slot0 = p0->client.me;
    CHECK(slot0 > 0 && slot1 > 0 && slot0 != slot1);

    /* A piece that lands: the lock sound, no rumble. Two in one frame sound once. */
    unsigned r0 = p0->rumbles, r1 = p1->rumbles, n;
    for (int i = 0; i < 2; i++) { memset(a.pl[i].client.game.field, 0, sizeof(tn_field)); piece(&a.pl[i]); }
    n = gtn_sfx_played[SFX_LOCK];
    tn_client_action(&p0->client, TN_ACT_DROP);
    tn_client_action(&p1->client, TN_ACT_DROP);
    update_session(&a, 1);
    CHECK(gtn_sfx_played[SFX_LOCK] == n + 1);
    CHECK(p0->rumbles == r0 && p1->rumbles == r1);

    /* Four lines at once: the big jingle and a rumble for whoever cleared them. Classic rules
     * send the other player four lines, which hurts them. */
    for (int y = TN_FIELD_H - 4; y < TN_FIELD_H; y++) memset(p0->client.game.field[y], 1, TN_FIELD_W);
    for (int y = TN_FIELD_H - 4; y < TN_FIELD_H; y++) p0->client.game.field[y][4] = 0;
    piece(p0);
    p0->client.game.orient = 1;               /* standing: fills column 4 of all four rows */
    p0->client.game.bx = 2;
    unsigned c4 = gtn_sfx_played[SFX_CLEAR4], hurt = gtn_sfx_played[SFX_HIT_HURT];
    tn_client_action(&p0->client, TN_ACT_DROP);
    update_session(&a, 1);
    CHECK(gtn_sfx_played[SFX_CLEAR4] == c4 + 1);
    CHECK(p0->client.ev.last_clear == 4);
    CHECK(p0->rumbles == r0 + 1);
    if (p0->client.rules.classic) {
        CHECK(await_sfx(&a, SFX_HIT_HURT, hurt) == 1);
        CHECK(p1->rumbles == r1 + 1);
    }
    settle(&a, 50);
    r0 = p0->rumbles; r1 = p1->rumbles;

    /* A harmful special: the sender hears it go, the target's controller rumbles. */
    unsigned sp = gtn_sfx_played[SFX_SPECIAL];
    hurt = gtn_sfx_played[SFX_HIT_HURT];
    give_special(p0, TN_SB_ADDLINE);
    tn_client_special(&p0->client, slot1);
    update_session(&a, 1);
    CHECK(gtn_sfx_played[SFX_SPECIAL] == sp + 1);
    CHECK(await_sfx(&a, SFX_HIT_HURT, hurt) == 1);
    CHECK(p1->rumbles == r1 + 1 && p0->rumbles == r0);
    CHECK(p1->client.ev.last_hit == TN_SB_ADDLINE);

    /* A helpful one back: a different sound, a light rumble for the receiver. */
    unsigned help = gtn_sfx_played[SFX_HIT_HELP];
    give_special(p1, TN_SB_NUKE);
    tn_client_special(&p1->client, slot0);
    CHECK(await_sfx(&a, SFX_HIT_HELP, help) == 1);
    CHECK(p0->rumbles == r0 + 1 && p1->rumbles == r1 + 1);

    /* Mixed effects, and with rumble switched off: still heard, not felt. */
    a.settings.rumble_off = 1;
    unsigned mixed = gtn_sfx_played[SFX_HIT_MIXED];
    give_special(p0, TN_SB_QUAKE);
    tn_client_special(&p0->client, slot1);
    CHECK(await_sfx(&a, SFX_HIT_MIXED, mixed) == 1);
    CHECK(p1->rumbles == r1 + 1);
    a.settings.rumble_off = 0;

    /* Player 2 tops out: the loss sound and a long rumble; player 1 wins. */
    r0 = p0->rumbles; r1 = p1->rumbles;
    unsigned lose = gtn_sfx_played[SFX_LOSE], win = gtn_sfx_played[SFX_WIN];
    for (int y = 0; y < TN_FIELD_H; y++) { memset(p1->client.game.field[y], 1, TN_FIELD_W); p1->client.game.field[y][0] = 0; }
    piece(p1);
    tn_client_action(&p1->client, TN_ACT_DROP);
    update_session(&a, 1);
    CHECK(gtn_sfx_played[SFX_LOSE] == lose + 1);
    CHECK(p1->rumbles == r1 + 1 && !p1->client.playing);
    CHECK(await_sfx(&a, SFX_WIN, win) == 1);
    CHECK(p0->rumbles == r0 + 1 && p0->client.ev.wins == 1 && p1->client.ev.wins == 0);

    /* A new session starts its counters again: no burst of old events. */
    stop_session(&a);
    unsigned total = 0, after = 0;
    for (int s = 0; s < SFX_COUNT; s++) total += gtn_sfx_played[s];
    host_local(&a);
    settle(&a, 200);
    for (int s = 0; s < SFX_COUNT; s++) after += gtn_sfx_played[s];
    CHECK(after == total);
    stop_session(&a);

    /* Without an audio device everything above still worked; with the dummy driver the mixer
     * really runs and plays the effects into it. */
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    gtn_sound_init();
    CHECK(gtn_sound_active());
    gtn_sound_volume(VOLUME_LOUD);
    gtn_sound_frame();
    for (int s = 0; s < SFX_COUNT; s++) gtn_sound_play((enum gtn_sfx)s);
    SDL_Delay(100);
    gtn_sound_quit();
    CHECK(!gtn_sound_active());

    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_Quit();
    printf("%d sound checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
