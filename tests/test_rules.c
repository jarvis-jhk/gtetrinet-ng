/* SPDX-License-Identifier: GPL-2.0-or-later
 * Host settings for games hosted here: specials on/off, lines per special, starting garbage
 * and the piece mix. Saved settings, the options screens, the rules a game starts with and
 * a real TCP game in which the players get exactly those rules. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#include "await_game.h"
#undef main

/* Only the platform preference directory is replaced; load/save use actual files. */
static const char *pref_path;
static char *test_pref_path(const char *org, const char *app) { return SDL_strdup(pref_path); }
#define SDL_GetPrefPath test_pref_path
#include "../src/app/settings.c"
#undef SDL_GetPrefPath

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

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

static void taps(gtn_app *a, Uint8 button, int n) { while (n-- > 0) tap(a, 42, button); }

static void write_file(const char *path, const char *t)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs(t, f);
    fclose(f);
}

static int special_cells(const tn_field f)
{
    int n = 0;
    for (int y = 0; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++) n += f[y][x] >= 6;
    return n;
}

static int used_rows(const tn_field f)
{
    int n = 0;
    for (int y = 0; y < TN_FIELD_H; y++) {
        int used = 0;
        for (int x = 0; x < TN_FIELD_W; x++) used |= f[y][x] != 0;
        n += used;
    }
    return n;
}

/* Fills the bottom rows completely and clears them; returns the lines cleared. */
static int clear_rows(tn_game *g, int rows)
{
    memset(g->field, 0, sizeof g->field);
    for (int y = TN_FIELD_H - rows; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++) g->field[y][x] = 1;
    int lc;
    return tn_game_clear_lines(g, &lc);
}

static int blocks_drawn(const tn_rules *r, int kind, int n)
{
    tn_game g;
    tn_game_start(&g, r, 7);
    int count = 0;
    for (int i = 0; i < n; i++) count += tn_game_random_block(&g) == kind;
    return count;
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

    /* Piece mixes: each sums to 100, the classic one is the default, codes round-trip. */
    tn_rules r, back;
    tn_rules_default(&r);
    CHECK(tn_rules_piece_mix(&r) == TN_PIECES_CLASSIC);
    for (int m = 0; m < TN_PIECE_MIXES; m++) {
        CHECK(tn_rules_pieces(&r, m) == 0 && tn_rules_piece_mix(&r) == m);
        int sum = 0;
        for (int i = 0; i < 7; i++) sum += r.blockfreq[i];
        CHECK(sum == 100);
        CHECK(tn_piece_mix_parse(tn_piece_mix_code(m)) == m);
    }
    CHECK(tn_rules_pieces(&r, TN_PIECE_MIXES) == -1 && tn_rules_piece_mix(&r) == TN_PIECE_MIXES - 1);
    CHECK(tn_piece_mix_parse("NOSZ") == -1 && tn_piece_mix_parse("") == -1);
    r.blockfreq[0]++, r.blockfreq[1]--;
    CHECK(tn_rules_piece_mix(&r) == -1);

    /* The newgame line carries every host setting to the clients, classic fields only. */
    tn_rules_default(&r);
    r.specials_added = 0;
    r.lines_per_special = 3;
    r.stack = 6;
    tn_rules_pieces(&r, TN_PIECES_NO_SZ);
    char line[512];
    tn_rules_format(&r, line, sizeof line);
    CHECK(tn_rules_parse(&back, line) == 0);
    CHECK(back.specials_added == 0 && back.lines_per_special == 3 && back.stack == 6 && !back.tn2);
    CHECK(tn_rules_piece_mix(&back) == TN_PIECES_NO_SZ);

    /* The game follows them: no S or Z ever, more I pieces with long bars, garbage at the start. */
    CHECK(blocks_drawn(&back, 4, 4000) == 0 && blocks_drawn(&back, 5, 4000) == 0);
    tn_rules bars;
    tn_rules_default(&bars);
    int classic_i = blocks_drawn(&bars, 0, 4000);
    tn_rules_pieces(&bars, TN_PIECES_LONG_BARS);
    int many_i = blocks_drawn(&bars, 0, 4000);
    fprintf(stderr, "I pieces in 4000: classic %d, long bars %d\n", classic_i, many_i);
    CHECK(classic_i > 450 && classic_i < 750 && many_i > 950 && many_i < 1300);
    tn_game g;
    tn_game_start(&g, &back, 3);
    CHECK(used_rows(g.field) == 6);
    /* pure: clearing lines never brings a special */
    for (int i = 0; i < 6; i++) clear_rows(&g, 4);
    CHECK(special_cells(g.field) == 0 && g.nspecials == 0);
    /* one special per three lines: two lines bring none, the third one */
    tn_rules three;
    tn_rules_default(&three);
    three.lines_per_special = 3;
    tn_game_start(&g, &three, 3);
    CHECK(clear_rows(&g, 2) == 2 && special_cells(g.field) == 0);
    memset(g.field, 0, sizeof g.field);
    g.field[TN_FIELD_H - 1][0] = 1;   /* somewhere for the special to land */
    for (int x = 0; x < TN_FIELD_W; x++) g.field[TN_FIELD_H - 2][x] = 2;
    int lc;
    CHECK(tn_game_clear_lines(&g, &lc) == 1 && special_cells(g.field) == 1);

    /* Saved settings: defaults, a full set, malformed values ignored. */
    gtn_settings loaded;
    gtn_settings_load(&loaded);
    CHECK(!gtn_host_custom(&loaded) && loaded.special_lines == 1 && loaded.pieces == TN_PIECES_CLASSIC);
    write_file(path, "specials=off\nspecial_lines=4\ngarbage=10\npieces=bars\n");
    gtn_settings_load(&loaded);
    CHECK(loaded.specials_off && loaded.special_lines == 4 && loaded.garbage == 10 && loaded.pieces == TN_PIECES_LONG_BARS);
    CHECK(gtn_host_custom(&loaded));
    write_file(path, "specials=maybe\nspecial_lines=5\ngarbage=3\npieces=tetrominoes\n");
    gtn_settings_load(&loaded);
    CHECK(!gtn_host_custom(&loaded));
    write_file(path, "special_lines=0\ngarbage=12\ngarbage=-2\n");
    gtn_settings_load(&loaded);
    CHECK(!gtn_host_custom(&loaded));
    remove(path);

    /* gtn_host_rules: the rule set and the host settings together. */
    gtn_settings_load(&a.settings);
    a.settings.rules_tn2 = 1;
    a.settings.specials_off = 1;
    a.settings.garbage = 4;
    gtn_host_rules(&a.settings, &r);
    CHECK(r.tn2 && r.hold && r.specials_added == 0 && r.stack == 4 && r.lines_per_special == 1);
    a.settings.special_lines = 99;   /* never sent out of range */
    a.settings.pieces = 17;
    gtn_host_rules(&a.settings, &r);
    CHECK(r.lines_per_special == 1 && tn_rules_piece_mix(&r) == TN_PIECES_CLASSIC);
    gtn_settings_load(&a.settings);

    snprintf(a.settings.nick, sizeof a.settings.nick, "Host");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");

    /* Options -> rules for games here, from a controller that has not joined. */
    a.screen = SCR_TITLE;
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);
    CHECK(a.screen == SCR_INPUT && gtn_local_count(&a) == 0);
    taps(&a, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 5);
    CHECK(a.options_row == 5);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);   /* Left/Right still switch the rule set */
    CHECK(a.settings.rules_tn2 && a.screen == SCR_INPUT);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    CHECK(!a.settings.rules_tn2);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.screen == SCR_RULES && a.rules_row == 0 && gtn_local_count(&a) == 0);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.settings.rules_tn2);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_Y);            /* Y: this row's default */
    CHECK(!a.settings.rules_tn2);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK(a.settings.specials_off);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    taps(&a, SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1);    /* 1 wraps to 4 */
    CHECK(a.settings.special_lines == GTN_SPECIAL_LINES_MAX);
    taps(&a, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 3);
    CHECK(a.settings.special_lines == 3);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_LEFT);    /* none wraps to the most */
    CHECK(a.settings.garbage == GTN_GARBAGE_MAX);
    taps(&a, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 4);
    CHECK(a.settings.garbage == 6);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    CHECK(a.settings.pieces == TN_PIECES_NO_SZ);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN);    /* wraps to the first row */
    CHECK(a.rules_row == 0);
    gtn_settings_load(&loaded);
    CHECK(loaded.specials_off && loaded.special_lines == 3 && loaded.garbage == 6 &&
          loaded.pieces == TN_PIECES_NO_SZ && !loaded.rules_tn2);

    /* Render the screens that show them, in both languages, at small and Deck sizes. */
    static const enum gtn_screen screens[] = {SCR_RULES, SCR_INPUT};
    const int widths[] = {800, 1280}, heights[] = {600, 800};
    for (int lang = 0; lang < 2; lang++) {
        gtn_lang_set(lang ? GTN_LANG_DE : GTN_LANG_EN);
        for (int i = 0; i < 2; i++)
            for (int k = 0; k < 2; k++) {
                a.w = widths[i]; a.h = heights[i];
                SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, a.w, a.h, 32, SDL_PIXELFORMAT_ARGB8888);
                a.ren = SDL_CreateSoftwareRenderer(surface);
                CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
                if (a.ren) {
                    enum gtn_screen keep = a.screen;
                    a.screen = screens[k];
                    for (int row = 0; row < RULES_ROWS; row++) {
                        a.rules_row = row;
                        a.status[0] = 0;
                        gtn_text_abbreviated = 0;
                        gtn_draw(&a); SDL_RenderPresent(a.ren);
                        if (gtn_text_abbreviated) fprintf(stderr, "lang %d %dx%d screen %d row %d: cut\n", lang, a.w, a.h, k, row);
                        CHECK(gtn_text_abbreviated == 0);
                    }
                    a.rules_row = 0;
                    snprintf(path, sizeof path, "%srules-%s-%s-%d.bmp", pref_path, lang ? "de" : "en", k ? "options" : "rules", a.w);
                    CHECK(SDL_SaveBMP(surface, path) == 0);
                    a.screen = keep;
                    gtn_render_quit(); SDL_DestroyRenderer(a.ren);
                }
                SDL_FreeSurface(surface);
            }
    }
    gtn_lang_set(GTN_LANG_EN);
    a.ren = NULL;
    tap(&a, 42, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_INPUT && a.options_row == 5);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_TITLE);

    /* A real local game: two controllers and a computer player get exactly these rules. */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    tap(&a, 43, SDL_CONTROLLER_BUTTON_A);
    CHECK(gtn_local_count(&a) == 2);
    host_local(&a);
    for (int t = 0; t < 3000 && a.screen != SCR_LOBBY; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_LOBBY);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_X);
    CHECK(gtn_local_count(&a) == 3);
    for (int t = 0; t < 500; t++) { update_session(&a, 2); SDL_Delay(1); }

    /* the lobby says what the game will be played with */
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
                snprintf(path, sizeof path, "%srules-%s-lobby-%d.bmp", pref_path, lang ? "de" : "en", a.w);
                CHECK(SDL_SaveBMP(surface, path) == 0);
                gtn_render_quit(); SDL_DestroyRenderer(a.ren);
            }
            SDL_FreeSurface(surface);
        }
    }
    gtn_lang_set(GTN_LANG_EN);
    a.ren = NULL;

    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    for (int t = 0; t < 3000 && (a.screen != SCR_GAME || !all_playing(&a)); t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_GAME);
    for (int t = 0; t < 200; t++) { update_session(&a, 2); SDL_Delay(1); }
    for (int i = 0; i < 3; i++) {
        const tn_client *c = &a.pl[i].client;
        CHECK(c->ingame && c->rules.specials_added == 0 && c->rules.lines_per_special == 3 && c->rules.stack == 6);
        CHECK(tn_rules_piece_mix(&c->rules) == TN_PIECES_NO_SZ && !c->rules.tn2 && !c->rules.hold);
        CHECK(used_rows(c->game.field) >= 6);   /* the garbage, maybe a piece already on it */
    }
    /* the computer plays on: lines cleared, never a special, never an S or a Z */
    tn_client *bot = &a.pl[2].client;
    int s_or_z = 0, cells = 0;
    for (int t = 0; t < 12000 && bot->playing && bot->ev.clears < 3; t++) {
        update_session(&a, 16);
        s_or_z |= bot->game.block == 4 || bot->game.block == 5 || bot->game.next == 4 || bot->game.next == 5;
        cells += special_cells(bot->game.field) + bot->game.nspecials;
        SDL_Delay(1);
    }
    fprintf(stderr, "bot: %u locks, %u clears\n", bot->ev.locks, bot->ev.clears);
    CHECK(bot->ev.locks >= 5 && bot->ev.clears >= 1);
    CHECK(!s_or_z && !cells && bot->ev.specials == 0);

    tap(&a, 42, SDL_CONTROLLER_BUTTON_B);
    stop_session(&a);
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_Quit();
    printf("%d rules checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
