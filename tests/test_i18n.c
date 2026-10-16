/* SPDX-License-Identifier: GPL-2.0-or-later
 * German UI: translation table, UTF-8/Latin-1 glyphs, translated core log lines, the saved
 * language choice and unabbreviated layouts at the Deck's 1280x800. Software rendering is a
 * layout fixture, not a check on a physical Deck screen. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#undef main

static const char *pref_path;
static char *test_pref_path(const char *org, const char *app) { return SDL_strdup(pref_path); }
#define SDL_GetPrefPath test_pref_path
#include "../src/app/settings.c"
#undef SDL_GetPrefPath

#include "font8x8_basic.h"
#include "font8x8_ext_latin.h"

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

static void pad(gtn_app *a, Uint8 button)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_CONTROLLERBUTTONDOWN;
    e.cbutton.which = 42;
    e.cbutton.button = button;
    gtn_input_event(a, &e);
    e.type = SDL_CONTROLLERBUTTONUP;
    gtn_input_event(a, &e);
}

static int lit(SDL_Surface *s, int x, int y)
{
    Uint8 r, g, b, al;
    SDL_GetRGBA(((Uint32 *)((char *)s->pixels + y * s->pitch))[x], s->format, &r, &g, &b, &al);
    return r == 255 && g == 255 && b == 255;
}

static void check_log(const char *in, const char *want)
{
    char out[256];
    gtn_tr_log(in, out, sizeof out);
    if (strcmp(out, want)) fprintf(stderr, "'%s' -> '%s', want '%s'\n", in, out, want);
    CHECK(!strcmp(out, want));
}

static void fake_session(gtn_app *a, int players, int ingame)
{
    memset(a->pl, 0, sizeof a->pl);
    a->hosting = 1;
    a->server.port = TN_PORT;
    for (int i = 0; i < players; i++) {
        gtn_local *p = &a->pl[i];
        p->active = 1;
        p->dev = i ? DEV_PAD : DEV_KEYBOARD;
        snprintf(p->nick, sizeof p->nick, "Jürgen%d", i + 1);
        tn_client *c = &p->client;
        c->state = TN_CONNECTED;
        c->me = i + 1;
        c->ingame = c->playing = ingame != 0;
        c->game.block = TN_NO_BLOCK;
        c->game.next = 2;
        c->game.level = 12;
        c->game.lines = 123;
        c->game.nspecials = ingame ? 30 : 0;
        for (int k = 0; k < 30; k++) c->game.specials[k] = (unsigned char)(6 + (i + k) % 9);
        p->target = i == 0 ? 1 : i;
        for (int k = 1; k <= players; k++) {
            c->slot[k].present = c->slot[k].playing = 1;
            snprintf(c->slot[k].nick, sizeof c->slot[k].nick, "Jürgen%d", k);
        }
        snprintf(c->slot[1].team, sizeof c->slot[1].team, "Rot");
        snprintf(c->winlist, sizeof c->winlist, "pJürgen1;3 tRot;2");
        c->last_winner = 1;
        tn_log_add(&c->chat, "*** Jürgen2 has joined");
        tn_log_add(&c->chat, "*** The game has started");
        tn_log_add(&c->chat, "<Jürgen1> Grüße");
        if (ingame == 3) {
            /* a game under way: a stack of every color with the classic specials in it, a piece falling */
            for (int r = 12; r < TN_FIELD_H; r++)
                for (int x = 0; x < TN_FIELD_W; x++)
                    if ((x * 7 + r * 3 + i) % 11 != 0 && r + (x % 4) > 13) c->game.field[r][x] = (char)(1 + (x + r + i) % 5);
            for (int k = 0; k < 9; k++) c->game.field[TN_FIELD_H - 1 - k % 4][(k * 5 + i) % TN_FIELD_W] = (char)(6 + k);
            c->game.block = 2;
            c->game.bx = 4;
            c->game.by = 3;
        }
        if (ingame == 2) {
            /* TetriNET 2: a held piece, every timer running, the five new specials queued */
            c->rules.tn2 = c->rules.hold = 1;
            c->game.hold = 3;
            c->immune[c->me] = TN_IMMUNITY_MS;
            c->confused_ms = TN_CONFUSION_MS;
            c->dark_ms = TN_DARKNESS_MS;
            for (int k = 0; k < 30; k++) c->game.specials[k] = (unsigned char)(15 + (i + k) % 5);
        }
    }
    tn_rules_default(&a->server.rules);
    if (ingame == 2) tn_rules_tn2(&a->server.rules);
    a->settings.rules_tn2 = ingame == 2;
    a->banner_ms = 1000;
}

/* Every screen in both languages; returns how often text had to be cut. */
static int draw_all(gtn_app *a, SDL_Surface *surface, const char *tag)
{
    static const struct { enum gtn_screen screen; int players, ingame, row; const char *name; } views[] = {
        {SCR_TITLE, 0, 0, 0, "title"}, {SCR_TITLE, 3, 0, 0, "title3"}, {SCR_INPUT, 0, 0, 1, "options"},
        {SCR_ONLINE, 1, 0, 0, "online"}, {SCR_CONNECTING, 1, 0, 0, "connecting"}, {SCR_LOBBY, 8, 0, 0, "lobby"},
        {SCR_NEWS, 0, 0, 0, "news"}, {SCR_GAME, 1, 1, 0, "game1"}, {SCR_GAME, 8, 1, 0, "game8"},
        {SCR_INPUT, 1, 2, 5, "options-tn2"}, {SCR_LOBBY, 8, 2, 0, "lobby-tn2"}, {SCR_CONTROLS, 0, 0, 0, "controls"},
        {SCR_GAME, 1, 2, 0, "game1-tn2"}, {SCR_GAME, 2, 2, 0, "game2-tn2"}, {SCR_GAME, 8, 2, 0, "game8-tn2"},
        {SCR_INPUT, 0, 0, 6, "options-blocks"}, {SCR_GAME, 1, 3, 0, "game1-orig"}, {SCR_GAME, 8, 3, 0, "game8-orig"},
    };
    int cut = 0;
    for (size_t i = 0; i < sizeof views / sizeof *views; i++) {
        fake_session(a, views[i].players, views[i].ingame);
        if (!views[i].players) memset(a->pl, 0, sizeof a->pl);
        a->screen = views[i].screen;
        a->options_row = views[i].row;
        a->menu_sel = 0;
        gtn_block_style = strstr(views[i].name, "-orig") ? GTN_BLOCKS_ORIGINAL : GTN_BLOCKS_MODERN;
        snprintf(a->entry, sizeof a->entry, "tetrinet.example:31457");
        if (views[i].screen == SCR_NEWS) {
            a->screen = SCR_TITLE;
            open_news(a, 0);
        }
        if (views[i].screen == SCR_GAME && views[i].ingame == 1) a->pl[0].client.paused = 1;
        gtn_set_status(a, gtn_tr("%s could not connect: %s"), "Jürgen1", gtn_tr("connection closed"));
        gtn_text_abbreviated = 0;
        gtn_draw(a);
        SDL_RenderPresent(a->ren);
        if (gtn_text_abbreviated) fprintf(stderr, "%s %s %dx%d: %d cut texts\n", tag, views[i].name, a->w, a->h, gtn_text_abbreviated);
        cut += gtn_text_abbreviated;
        char path[1200];
        snprintf(path, sizeof path, "%s%s-%s-%d.bmp", pref_path, tag, views[i].name, a->w);
        SDL_SaveBMP(surface, path);
        a->status[0] = 0;
    }
    return cut;
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

    /* The table: unique keys, a German text for every one, and umlauts actually in use. */
    int umlauts = 0;
    for (int i = 0; i < gtn_i18n_count(); i++) {
        CHECK(gtn_i18n_value(i, GTN_LANG_DE) && gtn_i18n_value(i, GTN_LANG_DE)[0]);
        for (int j = 0; j < i; j++) CHECK(strcmp(gtn_i18n_key(i), gtn_i18n_key(j)) != 0);
        /* Placeholders stay in the same order, or a translation would read the wrong argument. */
        char ke[16] = "", kd[16] = "";
        size_t ne = 0, nd = 0;
        for (const char *q = gtn_i18n_key(i); *q && ne + 1 < sizeof ke; q++) if (q[0] == '%' && q[1]) ke[ne++] = *++q;
        for (const char *q = gtn_i18n_value(i, GTN_LANG_DE); *q && nd + 1 < sizeof kd; q++) if (q[0] == '%' && q[1]) kd[nd++] = *++q;
        if (strcmp(ke, kd)) fprintf(stderr, "placeholders differ: %s\n", gtn_i18n_key(i));
        CHECK(!strcmp(ke, kd));
        umlauts += strstr(gtn_i18n_value(i, GTN_LANG_DE), "ä") || strstr(gtn_i18n_value(i, GTN_LANG_DE), "ü");
    }
    CHECK(gtn_i18n_count() > 60 && umlauts > 5);
    CHECK(gtn_lang_get() == GTN_LANG_EN && !strcmp(gtn_tr("Quit"), "Quit"));
    gtn_lang_set(GTN_LANG_DE);
    CHECK(!strcmp(gtn_tr("Quit"), "Beenden") && !strcmp(gtn_tr("not in the table"), "not in the table"));
    CHECK(!strcmp(gtn_tr_special(TN_SB_NUKE), "Feld leeren") && !strcmp(gtn_tr_special(-1), "?"));

    /* UTF-8 decodes to characters; stray Latin-1 bytes from classic clients still show. */
    CHECK(gtn_text_len("Grüße") == 5 && gtn_text_width(2, "ÄÖÜ") == 48);
    const char *q = "J\xfcrgen";
    CHECK(gtn_utf8_next(&q) == 'J' && gtn_utf8_next(&q) == 0xfc && *q == 'r');
    q = "\xc3\xa4";
    CHECK(gtn_utf8_next(&q) == 0xe4 && !*q);
    q = "\xc0\xaf"; /* overlong '/' must not become '/' */
    CHECK(gtn_utf8_next(&q) == 0xc0 && gtn_utf8_next(&q) == 0xaf && !*q);
    q = "\xe2\x82\xac"; /* euro sign: real text without a glyph */
    CHECK(gtn_glyph(gtn_utf8_next(&q)) == '?' && !*q);
    q = "\xed\xa0\x80"; /* surrogate */
    CHECK(gtn_utf8_next(&q) == 0xed);
    q = "\xc3";
    CHECK(gtn_utf8_next(&q) == 0xc3 && !*q);
    CHECK(gtn_glyph(0x85) == '?' && gtn_glyph(0xdf) == 128 + 0x3f);

    /* Core log lines in German; anything unknown, and everything in English, passes through. */
    check_log("*** Jürgen has joined", "*** Jürgen ist beigetreten");
    check_log("*** Team Rot has won the game", "*** Team Rot hat das Spiel gewonnen");
    check_log("*** Ann has won the game", "*** Ann hat das Spiel gewonnen");
    check_log("*** The game is paused", "*** Das Spiel ist pausiert");
    check_log("Add Line on Bob by Ann", "Zeile dazu auf Bob von Ann");
    check_log("Block Bomb on Bob by server", "Blockbombe auf Bob von Server");
    check_log("Nuke Field to All by Ann", "Feld leeren an alle von Ann");
    check_log("2 Lines Added to All by Ann", "2 Zeilen dazu an alle von Ann");
    check_log("cannot find server 'x.invalid'", "Server 'x.invalid' nicht gefunden");
    check_log("<Ann> Add Line on Bob by Ann", "<Ann> Add Line on Bob by Ann");
    check_log("*** has joined", "*** has joined");
    check_log("Server message", "Server message");
    gtn_lang_set(GTN_LANG_EN);
    check_log("Add Line on Bob by Ann", "Add Line on Bob by Ann");

    /* The language choice is saved, survives malformed values and is changed on the options page. */
    gtn_settings loaded;
    gtn_settings_load(&a.settings);
    CHECK(a.settings.lang == LANG_AUTO);
    a.screen = SCR_TITLE;
    pad(&a, SDL_CONTROLLER_BUTTON_X);
    CHECK(a.screen == SCR_INPUT && a.options_row == 0 && gtn_local_count(&a) == 0);
    pad(&a, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(a.options_row == 1 && a.settings.repeat_profile == REPEAT_NORMAL);
    pad(&a, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    CHECK(a.settings.lang == LANG_DE && gtn_lang_get() == GTN_LANG_DE);
    gtn_settings_load(&loaded);
    CHECK(loaded.lang == LANG_DE && loaded.repeat_profile == REPEAT_NORMAL);
    pad(&a, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.settings.lang == LANG_AUTO && gtn_lang_get() == gtn_lang_detect());
    pad(&a, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.settings.lang == LANG_EN && gtn_lang_get() == GTN_LANG_EN);
    pad(&a, SDL_CONTROLLER_BUTTON_Y);
    gtn_settings_load(&loaded);
    CHECK(loaded.lang == LANG_AUTO);
    pad(&a, SDL_CONTROLLER_BUTTON_DPAD_UP);
    pad(&a, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.settings.repeat_profile == REPEAT_RELAXED && a.settings.lang == LANG_AUTO);
    pad(&a, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_TITLE && gtn_local_count(&a) == 0);
    const char *values[] = {"lang=fr\n", "lang=\n", "lang=DE\n", "lang=dee\n"};
    for (size_t i = 0; i < sizeof values / sizeof *values; i++) {
        FILE *f = fopen(path, "w");
        CHECK(f != NULL);
        if (!f) continue;
        fputs(values[i], f);
        fclose(f);
        gtn_settings_load(&loaded);
        CHECK(loaded.lang == LANG_AUTO);
    }
    CHECK(gtn_lang_parse("de") == LANG_DE && gtn_lang_parse("en") == LANG_EN && !strcmp(gtn_lang_code(LANG_DE), "de"));

    /* Glyph pixels: every German special letter is the font's Latin-1 bitmap. */
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 800, 32, SDL_PIXELFORMAT_RGBA32);
    a.ren = SDL_CreateSoftwareRenderer(surface);
    CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
    if (!a.ren) return 1;
    SDL_SetRenderDrawColor(a.ren, 0, 0, 0, 255);
    SDL_RenderClear(a.ren);
    const char *german = "ÄÖÜäöüß";
    gtn_text(0, 0, 1, gtn_rgb(255, 255, 255), german);
    gtn_text(0, 10, 1, gtn_rgb(255, 255, 255), "Aa");
    SDL_RenderPresent(a.ren);
    int i = 0;
    for (const char *g = german; *g; i++) {
        int cp = (int)gtn_utf8_next(&g);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) CHECK(lit(surface, i * 8 + x, y) == ((font8x8_ext_latin[cp - 0xa0][y] >> x) & 1));
    }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) CHECK(lit(surface, x, 10 + y) == ((font8x8_basic['A'][y] >> x) & 1));
    CHECK(i == 7);

    /* Cutting keeps whole characters: four umlauts at 4 x 8 px leave "ää.." (no half byte). */
    SDL_RenderClear(a.ren);
    gtn_text_abbreviated = 0;
    gtn_text_fit(0, 0, 1, 32, gtn_rgb(255, 255, 255), "äääää");
    SDL_RenderPresent(a.ren);
    CHECK(gtn_text_abbreviated == 1);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) CHECK(lit(surface, 8 + x, y) == ((font8x8_ext_latin[0xe4 - 0xa0][y] >> x) & 1));
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) CHECK(lit(surface, 16 + x, y) == ((font8x8_basic['.'][y] >> x) & 1));

    /* Deck layout: no screen needs to cut its text at 1280x800, in either language. */
    a.w = 1280; a.h = 800;
    gtn_lang_set(GTN_LANG_EN);
    CHECK(draw_all(&a, surface, "en") == 0);
    gtn_lang_set(GTN_LANG_DE);
    CHECK(draw_all(&a, surface, "de") == 0);
    gtn_render_quit();
    SDL_DestroyRenderer(a.ren);
    SDL_FreeSurface(surface);
    remove(path);
    SDL_Quit();
    printf("%d i18n checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
