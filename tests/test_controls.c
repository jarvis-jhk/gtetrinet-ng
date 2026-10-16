/* SPDX-License-Identifier: GPL-2.0-or-later
 * Controller button layout: saved settings, the options screens and real TCP play. */
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

static void key(gtn_app *a, SDL_Keycode code, int down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.keysym.sym = code;
    gtn_input_event(a, &e);
}

static void pad(gtn_app *a, Uint8 button, int down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    e.cbutton.which = 42;
    e.cbutton.button = button;
    gtn_input_event(a, &e);
}

static void tap(gtn_app *a, Uint8 button) { pad(a, button, 1); pad(a, button, 0); }

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

static int orient_after(const gtn_local *p, int dir)
{
    tn_game g = p->client.game;
    tn_game_rotate(&g, dir);
    return g.orient;
}

static int field_used(const gtn_local *p)
{
    for (int y = 0; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++)
            if (p->client.game.field[y][x]) return 1;
    return 0;
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs(text, f);
    fclose(f);
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

    /* Defaults are the layout the README documents, and translate to themselves. */
    gtn_settings loaded;
    gtn_settings_load(&a.settings);
    CHECK(gtn_pad_valid(a.settings.pad_map) && gtn_pad_is_default(a.settings.pad_map));
    CHECK(a.settings.pad_map[PAD_ROT_CW] == BTN_A && a.settings.pad_map[PAD_DROP] == BTN_UP);
    for (int b = 0; b < BTN_COUNT; b++) CHECK(gtn_pad_translate(a.settings.pad_map, b) == b);
    CHECK(!gtn_pad_bindable(BTN_DOWN) && !gtn_pad_bindable(BTN_LEFT) && !gtn_pad_bindable(BTN_START));

    /* Binding swaps: every action keeps exactly one button. */
    int map[PAD_ACTIONS];
    gtn_pad_defaults(map);
    gtn_pad_bind(map, PAD_ROT_CW, BTN_B);
    CHECK(map[PAD_ROT_CW] == BTN_B && map[PAD_ROT_CCW] == BTN_A && gtn_pad_valid(map));
    CHECK(gtn_pad_translate(map, BTN_B) == BTN_A && gtn_pad_translate(map, BTN_A) == BTN_B);
    gtn_pad_bind(map, PAD_DROP, BTN_DOWN);           /* movement is not bindable */
    gtn_pad_bind(map, 99, BTN_X);
    CHECK(map[PAD_DROP] == BTN_UP && gtn_pad_valid(map));
    int broken[PAD_ACTIONS] = {0};                   /* a zeroed layout behaves like the default */
    CHECK(!gtn_pad_valid(broken) && gtn_pad_translate(broken, BTN_UP) == BTN_UP);
    gtn_pad_bind(broken, PAD_SPECIAL, BTN_Y);
    CHECK(gtn_pad_valid(broken) && broken[PAD_SPECIAL] == BTN_Y && broken[PAD_SPECIAL_SELF] == BTN_X);

    /* Old and malformed files keep the default; a complete layout loads. */
    const char *bad[] = {"", "pad=\n", "pad=A,A,UP,X,Y,LB,RB,BACK\n", "pad=A,B,UP,X,Y,LB,RB\n",
                         "pad=A,B,UP,X,Y,LB,RB,BACK,\n", "pad=A,B,UP,X,Y,LB,RB,BACK,X\n",
                         "pad=A,B,DOWN,X,Y,LB,RB,BACK\n", "pad=a,b,up,x,y,lb,rb,back\n",
                         "pad=A,B,UP,X,Y,LB,RB,BACKBACKBACKBACKBACK\n", "pad=,A,B,UP,X,Y,LB,RB\n"};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        write_file(path, bad[i]);
        gtn_settings_load(&loaded);
        CHECK(gtn_pad_is_default(loaded.pad_map));
    }
    write_file(path, "repeat=1\npad=B,A,Y,X,UP,RB,LB,BACK\n");
    gtn_settings_load(&loaded);
    CHECK(loaded.repeat_profile == REPEAT_RELAXED && loaded.pad_map[PAD_ROT_CW] == BTN_B);
    CHECK(loaded.pad_map[PAD_DROP] == BTN_Y && loaded.pad_map[PAD_SPECIAL_SELF] == BTN_UP);
    CHECK(loaded.pad_map[PAD_TARGET_PREV] == BTN_RB && gtn_pad_valid(loaded.pad_map));
    CHECK(loaded.pad_map[PAD_HOLD] == BTN_RT);        /* saved before hold existed */
    write_file(path, "pad=B,A,Y,X,UP,RB,LB,RT,BACK\n");
    gtn_settings_load(&loaded);
    CHECK(loaded.pad_map[PAD_HOLD] == BTN_BACK && loaded.pad_map[PAD_DISCARD] == BTN_RT && gtn_pad_valid(loaded.pad_map));
    CHECK(gtn_pad_translate(loaded.pad_map, BTN_BACK) == BTN_RT);
    remove(path);

    snprintf(a.settings.nick, sizeof a.settings.nick, "PadPlayer");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");

    /* Options -> controller buttons, without joining; releases never act twice. */
    a.screen = SCR_TITLE;
    tap(&a, SDL_CONTROLLER_BUTTON_X);
    CHECK(a.screen == SCR_INPUT);
    for (int i = 0; i < 5; i++) tap(&a, SDL_CONTROLLER_BUTTON_DPAD_UP); /* past blocks, rules, rumble and sound */
    CHECK(a.options_row == 2);
    tap(&a, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.screen == SCR_CONTROLS && a.controls_row == 0 && !a.controls_capture && gtn_local_count(&a) == 0);
    /* rotate clockwise -> B: the old B action (counterclockwise) takes A */
    tap(&a, SDL_CONTROLLER_BUTTON_A);
    CHECK(a.controls_capture);
    tap(&a, SDL_CONTROLLER_BUTTON_B);
    CHECK(!a.controls_capture && a.screen == SCR_CONTROLS);
    CHECK(a.settings.pad_map[PAD_ROT_CW] == BTN_B && a.settings.pad_map[PAD_ROT_CCW] == BTN_A);
    gtn_settings_load(&loaded);
    CHECK(!memcmp(loaded.pad_map, a.settings.pad_map, sizeof map));
    /* hard drop -> Y, so special-on-yourself moves to Up */
    tap(&a, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    tap(&a, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(a.controls_row == PAD_DROP);
    tap(&a, SDL_CONTROLLER_BUTTON_A);
    tap(&a, SDL_CONTROLLER_BUTTON_Y);
    CHECK(a.settings.pad_map[PAD_DROP] == BTN_Y && a.settings.pad_map[PAD_SPECIAL_SELF] == BTN_UP);
    /* Start, the keyboard and unbindable buttons cancel the wait without changing anything */
    memcpy(map, a.settings.pad_map, sizeof map);
    tap(&a, SDL_CONTROLLER_BUTTON_A);
    tap(&a, SDL_CONTROLLER_BUTTON_START);
    CHECK(!a.controls_capture && a.screen == SCR_CONTROLS);
    tap(&a, SDL_CONTROLLER_BUTTON_A);
    key(&a, SDLK_x, 1); key(&a, SDLK_x, 0);
    CHECK(!a.controls_capture && a.screen == SCR_CONTROLS);
    tap(&a, SDL_CONTROLLER_BUTTON_A);
    tap(&a, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    CHECK(!a.controls_capture && !memcmp(map, a.settings.pad_map, sizeof map));
    /* Y restores the standard layout, which can then be rebuilt; B goes back to the options */
    tap(&a, SDL_CONTROLLER_BUTTON_Y);
    CHECK(gtn_pad_is_default(a.settings.pad_map));
    gtn_settings_load(&loaded);
    CHECK(gtn_pad_is_default(loaded.pad_map));
    memcpy(a.settings.pad_map, map, sizeof map);
    CHECK(gtn_settings_save(&a.settings) == 0);
    tap(&a, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_INPUT && a.options_row == 2);
    tap(&a, SDL_CONTROLLER_BUTTON_B);
    CHECK(a.screen == SCR_TITLE && gtn_local_count(&a) == 0);

    /* Render both screens at Deck and small sizes, mid-capture too, in both languages. */
    const int widths[] = {800, 1280}, heights[] = {600, 800};
    for (int lang = 0; lang < 2; lang++) {
        gtn_lang_set(lang ? GTN_LANG_DE : GTN_LANG_EN);
        for (int i = 0; i < 2; i++) {
            a.w = widths[i]; a.h = heights[i];
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, a.w, a.h, 32, SDL_PIXELFORMAT_ARGB8888);
            a.ren = SDL_CreateSoftwareRenderer(surface);
            CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
            if (a.ren) {
                const enum gtn_screen screens[] = {SCR_INPUT, SCR_CONTROLS, SCR_CONTROLS};
                for (int v = 0; v < 3; v++) {
                    a.screen = screens[v]; a.options_row = 2; a.controls_row = 1; a.controls_capture = v == 2;
                    gtn_text_abbreviated = 0;
                    gtn_draw(&a); SDL_RenderPresent(a.ren);
                    if (a.w == 1280) CHECK(gtn_text_abbreviated == 0);
                    snprintf(path, sizeof path, "%scontrols-%s-%d-%d.bmp", pref_path, lang ? "de" : "en", v, a.w);
                    CHECK(SDL_SaveBMP(surface, path) == 0);
                }
                gtn_render_quit(); SDL_DestroyRenderer(a.ren);
            }
            SDL_FreeSurface(surface);
        }
    }
    gtn_lang_set(GTN_LANG_EN);
    a.controls_capture = 0;
    a.ren = NULL;
    a.screen = SCR_TITLE;

    /* Two real clients: the controller uses its layout, the keyboard keeps classic keys. */
    key(&a, SDLK_RETURN, 1);
    pad(&a, SDL_CONTROLLER_BUTTON_A, 1);
    CHECK(gtn_local_count(&a) == 2 && a.pl[1].dev == DEV_PAD);
    host_local(&a);
    await_screen(&a, SCR_LOBBY);
    for (int t = 0; t < 2000 && local_moderator(&a) != 0; t++) { update_session(&a, 2); SDL_Delay(1); }
    CHECK(local_moderator(&a) == 0);
    key(&a, SDLK_RETURN, 1);
    await_screen(&a, SCR_GAME);
    gtn_local *k = &a.pl[0], *p = &a.pl[1];

    piece(k); piece(p);
    int ccw = orient_after(p, -1), cw = orient_after(p, 1);
    CHECK(ccw != cw);
    tap(&a, SDL_CONTROLLER_BUTTON_A);                 /* A is counterclockwise now */
    CHECK(p->client.game.orient == ccw && k->client.game.orient == 0);
    piece(p);
    tap(&a, SDL_CONTROLLER_BUTTON_B);                 /* B is clockwise */
    CHECK(p->client.game.orient == cw);
    key(&a, SDLK_x, 1); key(&a, SDLK_x, 0);           /* keyboard X still clockwise */
    CHECK(k->client.game.orient == cw);

    /* Up (d-pad and stick) no longer drops: it uses a special on yourself, of which there are none. */
    piece(p);
    p->client.game.nspecials = 0;
    tap(&a, SDL_CONTROLLER_BUTTON_DPAD_UP);
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_CONTROLLERAXISMOTION; e.caxis.which = 42; e.caxis.axis = SDL_CONTROLLER_AXIS_LEFTY; e.caxis.value = -30000;
    gtn_input_event(&a, &e);
    e.caxis.value = 0;
    gtn_input_event(&a, &e);
    CHECK(p->client.game.by == 4 && p->client.game.block == 2 && !field_used(p));
    /* Y drops for the controller; Space still drops for the keyboard */
    tap(&a, SDL_CONTROLLER_BUTTON_Y);
    CHECK(field_used(p));
    CHECK(!field_used(k));
    key(&a, SDLK_SPACE, 1); key(&a, SDLK_SPACE, 0);
    CHECK(field_used(k));
    /* unchanged bindings still work: Back discards */
    p->client.game.nspecials = 2;
    p->client.game.specials[0] = p->client.game.specials[1] = 6;
    tap(&a, SDL_CONTROLLER_BUTTON_BACK);
    CHECK(p->client.game.nspecials == 1);

    stop_session(&a);
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_Quit();
    printf("%d controls checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
