/* SPDX-License-Identifier: GPL-2.0-or-later
 * The server list: recent and listed servers are asked in the background over real TCP —
 * our own server answers the query protocol, a silent one counts as reachable without
 * details, a closed port as not reachable — closing the list while asking is safe, picking a
 * server connects and remembers it, and the screen needs no cut text in either language. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#undef main

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

static tn_server server;
static tn_client anna;      /* somebody already on our server */

static void step(gtn_app *a, int ms)
{
    for (int t = 0; t < ms; t += 5) {
        tn_server_update(&server, 5);
        tn_client_update(&anna, 5);
        update_session(a, 5);
        gtn_browser_update(a);
        SDL_Delay(5);
    }
}

static void wait_answers(gtn_app *a)
{
    for (int t = 0; t < 12000 && gtn_browser_pending(a); t += 20) step(a, 20);
}

static int find(const gtn_app *a, const char *addr)
{
    for (int i = 0; i < a->browser.n; i++)
        if (!strcmp(a->browser.e[i].addr, addr)) return i;
    return -1;
}

static int draw_cuts(gtn_app *a, SDL_Surface *surface, const char *tag)
{
    int cut = 0;
    for (int sel = 0; sel <= a->browser.n; sel++) {
        a->browser.sel = sel;
        gtn_text_abbreviated = 0;
        gtn_draw(a);
        SDL_RenderPresent(a->ren);
        if (gtn_text_abbreviated) fprintf(stderr, "%s row %d %dx%d: %d cut texts\n", tag, sel, a->w, a->h, gtn_text_abbreviated);
        cut += gtn_text_abbreviated;
        char path[1200];
        snprintf(path, sizeof path, "%sservers-%s-%d-%d.bmp", pref_path, tag, sel, a->w);
        SDL_SaveBMP(surface, path);
    }
    return cut;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    pref_path = argv[1];
    char path[1200];
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_EVENTS) == 0);
    CHECK(tn_net_init() == 0);

    /* Parsing what servers say, and the address forms players type. */
    int n = -1;
    CHECK(tn_query_parse_players("Number of players logged in: 17", &n) == 0 && n == 17);
    CHECK(tn_query_parse_players("Number of players logged in: ", &n) != 0);
    CHECK(tn_query_parse_players("noconnecting go away", &n) != 0);
    tn_query_channel ch;
    CHECK(tn_query_parse_channel("\"lfjr\" \"2:1\" 3 6 98 2", &ch) == 0 && !strcmp(ch.name, "lfjr") && !strcmp(ch.desc, "2:1")
          && ch.players == 3 && ch.max == 6 && ch.status == 2);
    CHECK(tn_query_parse_channel("\"x\" \"\" 0 1", &ch) == 0 && ch.max == 1 && ch.status == 0);
    CHECK(tn_query_parse_channel("\"x\" 0 1", &ch) != 0 && tn_query_parse_channel("\"\" \"d\" 0 1", &ch) != 0);
    CHECK(tn_query_parse_channel("\"x\" \"d\" -1 6", &ch) != 0 && tn_query_parse_channel("+OK", &ch) != 0);
    char host[128];
    int port;
    CHECK(tn_query_split("tetrinet.example", host, sizeof host, &port) == 0 && !strcmp(host, "tetrinet.example") && port == TN_PORT);
    CHECK(tn_query_split(" host:31458 ", host, sizeof host, &port) == 0 && !strcmp(host, "host") && port == 31458);
    CHECK(tn_query_split("[::1]:31470", host, sizeof host, &port) == 0 && !strcmp(host, "::1") && port == 31470);
    CHECK(tn_query_split("::1", host, sizeof host, &port) == 0 && !strcmp(host, "::1") && port == TN_PORT);
    CHECK(tn_query_split("host:0", host, sizeof host, &port) == 0 && port == TN_PORT);
    CHECK(tn_query_split(":31457", host, sizeof host, &port) != 0 && tn_query_split("  ", host, sizeof host, &port) != 0);

    /* Recent servers: most recent first, once each, at most GTN_RECENT. */
    gtn_settings rs;
    memset(&rs, 0, sizeof rs);
    const char *seq[] = {"a", "b", "c", "A", "d", "e", "f", "g"};
    for (size_t i = 0; i < sizeof seq / sizeof *seq; i++) gtn_settings_remember(&rs, seq[i]);
    CHECK(!strcmp(rs.recent[0], "g") && !strcmp(rs.recent[3], "d") && !strcmp(rs.recent[4], "A") && !strcmp(rs.recent[5], "c"));
    gtn_settings_remember(&rs, "");
    CHECK(!strcmp(rs.recent[0], "g"));
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (f) { fputs("host=old.example\n", f); fclose(f); }
    gtn_settings_load(&rs);
    CHECK(!strcmp(rs.recent[0], "old.example") && !rs.recent[1][0]);
    f = fopen(path, "w");
    if (f) { fputs("host=x\nserver3=three\nserver1=one\nserver9=nine\n", f); fclose(f); }
    gtn_settings_load(&rs);
    CHECK(!strcmp(rs.recent[0], "one") && !strcmp(rs.recent[1], "three") && !rs.recent[2][0]);
    remove(path);

    /* Three servers: ours with Anna on it, one that accepts but never speaks, a closed port. */
    char err[160] = "";
    CHECK(tn_server_start(&server, 0, 1, 8, err, sizeof err) == 0);
    tn_socket silent = tn_tcp_listen(0, 1, err, sizeof err);
    tn_socket closed = tn_tcp_listen(0, 1, err, sizeof err);
    CHECK(silent != TN_BAD_SOCKET && closed != TN_BAD_SOCKET);
    int silent_port = tn_tcp_port(silent), closed_port = tn_tcp_port(closed);
    tn_tcp_close(closed);
    char ours[64], quiet[64], shut[64], list[256];
    snprintf(ours, sizeof ours, "127.0.0.1:%d", server.port);
    snprintf(quiet, sizeof quiet, "127.0.0.1:%d", silent_port);
    snprintf(shut, sizeof shut, "127.0.0.1:%d", closed_port);
    snprintf(list, sizeof list, "%s, %s,%s", quiet, shut, ours);
#ifdef _WIN32
    _putenv_s("GTN_SERVER_LIST", list);
#else
    setenv("GTN_SERVER_LIST", list, 1);
#endif
    tn_client_init(&anna, "Anna", "", 7);
    tn_client_connect(&anna, "127.0.0.1", server.port, 0);

    static gtn_app a;
    gtn_settings_load(&a.settings);
    gtn_settings_remember(&a.settings, ours);
    snprintf(a.settings.host, sizeof a.settings.host, "typed.example");
    a.screen = SCR_TITLE;
    for (int t = 0; t < 2000 && anna.state != TN_CONNECTED; t += 5) step(&a, 5);
    CHECK(anna.state == TN_CONNECTED);

    key(&a, SDLK_RETURN);                       /* the keyboard joins */
    CHECK(gtn_local_count(&a) == 1);
    key(&a, SDLK_DOWN);
    CHECK(a.menu_sel == MENU_ONLINE);
    key(&a, SDLK_RETURN);
    CHECK(a.screen == SCR_SERVERS && a.browser.n == 3 && a.browser.sel == 0);
    CHECK(!strcmp(a.browser.e[0].addr, ours) && !a.browser.e[0].known);   /* recent first, listed once */
    CHECK(find(&a, quiet) == 1 && a.browser.e[1].known && find(&a, shut) == 2);
    CHECK(gtn_browser_pending(&a) == 3);
    Uint64 t0 = SDL_GetTicks64();
    wait_answers(&a);
    CHECK(gtn_browser_pending(&a) == 0);
    const tn_query_result *r = &a.browser.e[0].res;
    CHECK(r->reachable && r->answered && r->players == 1 && r->nchan == 1 && r->chan[0].max == 8 && r->chan[0].status == 1);
    CHECK(!strncmp(r->version, TN_APP_NAME, strlen(TN_APP_NAME)) && !strcmp(r->chan[0].name, TN_APP_NAME));
    r = &a.browser.e[1].res;
    CHECK(r->reachable && !r->answered && r->players == 0);
    r = &a.browser.e[2].res;
    CHECK(!r->reachable && r->err[0]);
    CHECK(SDL_GetTicks64() - t0 < 10000);
    CHECK(server.p[1].used && tn_server_count(&server) == 1);   /* asking never took a seat */

    /* Every row, both languages, the Deck's screen and a small window: nothing is cut. */
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 800, 32, SDL_PIXELFORMAT_RGBA32);
    a.ren = SDL_CreateSoftwareRenderer(surface);
    CHECK(surface && a.ren && gtn_render_init(a.ren) == 0);
    if (!a.ren) return 1;
    a.browser.e[1].state = GTN_PROBE_ASKING;     /* and the "asking" look, without a thread */
    const int sizes[][2] = {{1280, 800}, {800, 600}};
    for (int k = 0; k < 2; k++) {
        a.w = sizes[k][0]; a.h = sizes[k][1];
        gtn_lang_set(GTN_LANG_EN);
        CHECK(draw_cuts(&a, surface, "en") == 0);
        gtn_lang_set(GTN_LANG_DE);
        CHECK(draw_cuts(&a, surface, "de") == 0);
    }
    gtn_lang_set(GTN_LANG_EN);
    a.browser.e[1].state = GTN_PROBE_DONE;
    a.browser.sel = 0;

    /* Asking again, and leaving while the answers are still out, then coming back. */
    key(&a, SDLK_s);
    CHECK(gtn_browser_pending(&a) == 3 && a.browser.e[0].res.players == 0);
    key(&a, SDLK_ESCAPE);
    CHECK(a.screen == SCR_TITLE && a.browser.n == 0);
    key(&a, SDLK_RETURN);
    CHECK(a.screen == SCR_SERVERS && a.browser.n == 3);
    key(&a, SDLK_ESCAPE);
    key(&a, SDLK_RETURN);
    wait_answers(&a);
    CHECK(a.browser.e[0].res.answered && a.browser.e[0].res.players == 1);

    /* Y switches the mode; up from the top reaches "type an address", which keeps the last one. */
    key(&a, SDLK_n);
    CHECK(a.settings.fast == 1);
    key(&a, SDLK_n);
    CHECK(a.settings.fast == 0);
    key(&a, SDLK_UP);
    CHECK(a.browser.sel == 3);
    key(&a, SDLK_RETURN);
    CHECK(a.screen == SCR_ONLINE && !strcmp(a.entry, "typed.example"));
    key(&a, SDLK_ESCAPE);
    CHECK(a.screen == SCR_SERVERS && a.browser.sel == 3);
    key(&a, SDLK_DOWN);
    CHECK(a.browser.sel == 0);

    /* Picking our server connects there, next to Anna, and remembers it. */
    key(&a, SDLK_DOWN);
    key(&a, SDLK_DOWN);
    CHECK(a.browser.sel == 2);
    key(&a, SDLK_UP);
    key(&a, SDLK_UP);
    key(&a, SDLK_RETURN);
    CHECK(a.screen == SCR_CONNECTING && !a.hosting && a.port == server.port && !strcmp(a.host, "127.0.0.1"));
    CHECK(a.browser.n == 0);
    for (int t = 0; t < 3000 && a.screen != SCR_LOBBY; t += 5) step(&a, 5);
    step(&a, 200);
    CHECK(a.screen == SCR_LOBBY && a.pl[0].client.state == TN_CONNECTED && tn_server_count(&server) == 2);
    gtn_settings loaded;
    gtn_settings_load(&loaded);
    CHECK(!strcmp(loaded.recent[0], ours) && !loaded.recent[1][0] && !strcmp(loaded.host, ours));

    /* A server nobody got onto is not remembered; a listed one played on moves to the top. */
    stop_session(&a);
    snprintf(a.entry, sizeof a.entry, "%s", shut);
    connect_online(&a);
    /* Windows retries a refused connect for about two seconds */
    for (int t = 0; t < 6000 && a.screen != SCR_TITLE; t += 20) step(&a, 20);
    CHECK(a.screen == SCR_TITLE);
    gtn_settings_load(&loaded);
    CHECK(!strcmp(loaded.recent[0], ours) && !loaded.recent[1][0] && !strcmp(loaded.host, shut));
    if (gtn_local_count(&a) == 0) key(&a, SDLK_RETURN);
    tn_server_stop(&server);
    CHECK(tn_server_start(&server, 0, 1, 8, err, sizeof err) == 0);
    snprintf(a.entry, sizeof a.entry, "127.0.0.1:%d", server.port);
    char moved[160];
    snprintf(moved, sizeof moved, "%s", a.entry);
    connect_online(&a);
    for (int t = 0; t < 3000 && a.screen != SCR_LOBBY; t += 5) step(&a, 5);
    CHECK(a.screen == SCR_LOBBY);
    gtn_settings_load(&loaded);
    CHECK(!strcmp(loaded.recent[0], moved) && !strcmp(loaded.recent[1], ours));
    stop_session(&a);
    a.menu_sel = MENU_ONLINE;
    key(&a, SDLK_RETURN);
    CHECK(a.screen == SCR_SERVERS && a.browser.n == 4 && !strcmp(a.browser.e[0].addr, moved) && !strcmp(a.browser.e[1].addr, ours));
    wait_answers(&a);
    for (int i = 0; i < a.browser.n; i++)
        if (!a.browser.e[0].res.answered || a.browser.e[1].res.reachable)
            fprintf(stderr, "%s: reachable %d answered %d players %d err %s\n", a.browser.e[i].addr, a.browser.e[i].res.reachable,
                    a.browser.e[i].res.answered, a.browser.e[i].res.players, a.browser.e[i].res.err);
    CHECK(a.browser.e[0].res.answered && !a.browser.e[1].res.reachable);
    gtn_browser_close(&a);

    tn_client_disconnect(&anna);
    tn_server_stop(&server);
    tn_tcp_close(silent);
    gtn_render_quit();
    SDL_DestroyRenderer(a.ren);
    SDL_FreeSurface(surface);
    remove(path);
    printf("browser: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
