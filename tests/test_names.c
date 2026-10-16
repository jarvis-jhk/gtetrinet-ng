/* SPDX-License-Identifier: GPL-2.0-or-later
 * Nicknames per seat: cleaning, saved settings, the name screen and a real TCP join. */
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

static void text(gtn_app *a, const char *t)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_TEXTINPUT;
    snprintf(e.text.text, sizeof e.text.text, "%s", t);
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

static void taps(gtn_app *a, int which, Uint8 button, int n) { while (n-- > 0) tap(a, which, button); }

static void write_file(const char *path, const char *t)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs(t, f);
    fclose(f);
}

static int has_slot(const tn_client *c, const char *nick)
{
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (c->slot[i].present && !strcmp(c->slot[i].nick, nick)) return 1;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    pref_path = argv[1];
    static gtn_app a;
    char path[1200], out[64];
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_EVENTS) == 0);
    CHECK(tn_net_init() == 0);

    /* One word of printable ASCII, at most 15 characters. */
    CHECK(gtn_nick_clean(out, sizeof out, "Anna Lena") == 8 && !strcmp(out, "AnnaLena"));
    CHECK(gtn_nick_clean(out, sizeof out, "abcdefghijklmnopqrstuvwxyz") == 15 && !strcmp(out, "abcdefghijklmno"));
    CHECK(gtn_nick_clean(out, sizeof out, "J\xc3\xbcrgen\t!") == 6 && !strcmp(out, "Jrgen!"));
    CHECK(gtn_nick_clean(out, sizeof out, " \t ") == 0 && !out[0]);
    CHECK(gtn_nick_clean(out, 4, "Maximilian") == 3 && !strcmp(out, "Max"));

    /* Seat names load cleaned; unknown or out-of-range keys are ignored. */
    gtn_settings loaded;
    write_file(path, "nick=Base\nnick2=Mo Mo\nnick1=Nope\nnick9=Nope\nnick10=Nope\nnick3=\nnick8=Eight\n");
    gtn_settings_load(&loaded);
    CHECK(!strcmp(loaded.nick, "Base") && !strcmp(loaded.seat_nick[1], "MoMo"));
    CHECK(!loaded.seat_nick[0][0] && !loaded.seat_nick[2][0] && !strcmp(loaded.seat_nick[7], "Eight"));
    remove(path);

    gtn_settings_load(&a.settings);
    snprintf(a.settings.nick, sizeof a.settings.nick, "Base");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");
    a.screen = SCR_TITLE;

    /* Keyboard, then two controllers join with default names. */
    key(&a, SDLK_RETURN);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    tap(&a, 43, SDL_CONTROLLER_BUTTON_A);
    CHECK(gtn_local_count(&a) == 3);
    CHECK(!strcmp(a.pl[0].nick, "Base") && !strcmp(a.pl[1].nick, "Base2") && !strcmp(a.pl[2].nick, "Base3"));

    /* P2 opens its own name; P3's buttons do nothing there. */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_Y);
    CHECK(a.screen == SCR_NAME && a.name_who == 1 && !strcmp(a.entry, "Base2"));
    tap(&a, 43, SDL_CONTROLLER_BUTTON_X);
    tap(&a, 43, SDL_CONTROLLER_BUTTON_B);
    tap(&a, 43, SDL_CONTROLLER_BUTTON_START);
    CHECK(a.screen == SCR_NAME && !strcmp(a.entry, "Base2"));
    /* deleting everything and saving is refused */
    taps(&a, 42, SDL_CONTROLLER_BUTTON_X, 6);
    CHECK(!a.entry[0]);
    a.status[0] = 0;
    tap(&a, 42, SDL_CONTROLLER_BUTTON_START);
    CHECK(a.screen == SCR_NAME && a.status[0]);
    /* "Mo" on the on-screen keyboard: Y for a capital M, then a small o */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_Y);
    CHECK(a.osk_shift);
    taps(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 3);
    taps(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 6);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(!strcmp(a.entry, "M"));
    tap(&a, 42, SDL_CONTROLLER_BUTTON_Y);
    taps(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_UP, 2);
    taps(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 2);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(!strcmp(a.entry, "Mo"));
    /* the keyboard can type and delete, but not spaces or past 15 characters */
    text(&a, "x y");
    CHECK(!strcmp(a.entry, "Moxy"));
    key(&a, SDLK_BACKSPACE); key(&a, SDLK_BACKSPACE);
    CHECK(!strcmp(a.entry, "Mo"));
    text(&a, "1234567890123456789");
    CHECK(strlen(a.entry) == 15);
    snprintf(a.entry, sizeof a.entry, "Base");          /* P1's name is taken */
    a.status[0] = 0;
    tap(&a, 42, SDL_CONTROLLER_BUTTON_START);
    CHECK(a.screen == SCR_NAME && a.status[0] && !strcmp(a.pl[1].nick, "Base2"));
    snprintf(a.entry, sizeof a.entry, "Mo");
    /* the on-screen OK key saves */
    taps(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_DOWN, 3);  /* row 1 -> the last row */
    CHECK(a.osk_row == gtn_name_nrows() - 1);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_DPAD_LEFT);       /* wraps to the last key, OK */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.screen == SCR_TITLE && !strcmp(a.pl[1].nick, "Mo") && !strcmp(a.pl[1].client.nick, "Mo"));
    CHECK(!strcmp(a.settings.seat_nick[1], "Mo"));
    gtn_settings_load(&loaded);
    CHECK(!strcmp(loaded.seat_nick[1], "Mo") && !strcmp(loaded.nick, "Base"));

    /* P1 renames from the keyboard with N; P3 keeps following the base name, P2 keeps Mo. */
    key(&a, SDLK_n);
    CHECK(a.screen == SCR_NAME && a.name_who == 0 && !strcmp(a.entry, "Base"));
    for (int i = 0; i < 4; i++) key(&a, SDLK_BACKSPACE);
    text(&a, "J"); text(&a, "a"); text(&a, "n");       /* letters that are also keys stay text */
    CHECK(!strcmp(a.entry, "Jan") && a.screen == SCR_NAME);
    key(&a, SDLK_RETURN);
    CHECK(a.screen == SCR_TITLE);
    CHECK(!strcmp(a.pl[0].nick, "Jan") && !strcmp(a.pl[1].nick, "Mo") && !strcmp(a.pl[2].nick, "Jan3"));
    gtn_settings_load(&loaded);
    CHECK(!strcmp(loaded.nick, "Jan") && !strcmp(loaded.seat_nick[1], "Mo") && !loaded.seat_nick[2][0]);

    /* Esc and B cancel without changing the name. */
    tap(&a, 43, SDL_CONTROLLER_BUTTON_Y);
    CHECK(a.screen == SCR_NAME && a.name_who == 2);
    text(&a, "zzz");
    key(&a, SDLK_ESCAPE);
    CHECK(a.screen == SCR_TITLE && !strcmp(a.pl[2].nick, "Jan3") && !a.settings.seat_nick[2][0]);
    /* a lost controller does not trap the screen: anyone's B closes it */
    tap(&a, 43, SDL_CONTROLLER_BUTTON_Y);
    a.pl[2].pad = -1;
    tap(&a, 42, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_TITLE && gtn_local_count(&a) == 3);
    a.pl[2].pad = 43;

    /* Leaving and joining again keeps the seat's name. */
    tap(&a, 42, SDL_CONTROLLER_BUTTON_B);
    CHECK(gtn_local_count(&a) == 2 && !a.pl[1].active);
    tap(&a, 42, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.pl[1].active && !strcmp(a.pl[1].nick, "Mo"));

    /* Render the name screen at Deck and small sizes, with capitals, in both languages. */
    const int widths[] = {800, 1280}, heights[] = {600, 800};
    for (int lang = 0; lang < 2; lang++) {
        gtn_lang_set(lang ? GTN_LANG_DE : GTN_LANG_EN);
        for (int i = 0; i < 2; i++) {
            a.w = widths[i]; a.h = heights[i];
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, a.w, a.h, 32, SDL_PIXELFORMAT_ARGB8888);
            a.ren = SDL_CreateSoftwareRenderer(surface);
            CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
            if (a.ren) {
                for (int v = 0; v < 2; v++) {
                    a.screen = v ? SCR_NAME : SCR_TITLE;
                    a.name_who = 1; a.osk_shift = 1; a.osk_row = 4; a.osk_col = 2;
                    snprintf(a.entry, sizeof a.entry, "MaximilianXYZ12");
                    a.status[0] = 0;
                    gtn_text_abbreviated = 0;
                    gtn_draw(&a); SDL_RenderPresent(a.ren);
                    if (a.w == 1280) CHECK(gtn_text_abbreviated == 0);
                    snprintf(path, sizeof path, "%snames-%s-%s-%d.bmp", pref_path, lang ? "de" : "en", v ? "name" : "title", a.w);
                    CHECK(SDL_SaveBMP(surface, path) == 0);
                }
                gtn_render_quit(); SDL_DestroyRenderer(a.ren);
            }
            SDL_FreeSurface(surface);
        }
    }
    gtn_lang_set(GTN_LANG_EN);
    a.ren = NULL;
    a.screen = SCR_TITLE;

    /* A real local game: the server and every client see the chosen names. */
    host_local(&a);
    for (int t = 0; t < 3000 && a.screen != SCR_LOBBY; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(a.screen == SCR_LOBBY);
    for (int t = 0; t < 1000; t++) { update_session(&a, 2); SDL_Delay(1); }
    for (int i = 0; i < 3; i++) {
        const tn_client *c = &a.pl[i].client;
        CHECK(c->state == TN_CONNECTED);
        CHECK(has_slot(c, "Jan") && has_slot(c, "Mo") && has_slot(c, "Jan3"));
    }
    CHECK(!strcmp(a.pl[0].client.slot[a.pl[0].client.me].nick, "Jan"));

    stop_session(&a);
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_Quit();
    printf("%d name checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
