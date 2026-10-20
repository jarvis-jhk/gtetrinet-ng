/*
 * gtetrinet-ng — tests for the protocol, the rules and a real client/server game over
 * localhost. SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_client.h"
#include "tn_server.h"
#include "tn_bot.h"
#include "tn_query.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#include <windows.h>
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
static long clock_ms(void) { return (long)GetTickCount(); }
#else
#include <netdb.h>
#include <sys/socket.h>
#include <time.h>
static long clock_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
static void sleep_ms(int ms)
{
    struct timespec ts = {0, (long)ms * 1000000L};
    nanosleep(&ts, NULL);
}
#endif

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } } while (0)

static void test_login(void)
{
    const char *logins[] = {"tetrisstart Jan 1.13", "tetrifaster Player_8 1.13", "tetrisstart x 1.13"};
    unsigned char ips[][4] = {{127, 0, 0, 1}, {192, 168, 1, 23}, {0, 0, 0, 0}, {255, 255, 255, 255}, {10, 1, 2, 3}};
    for (size_t i = 0; i < sizeof logins / sizeof *logins; i++)
        for (size_t k = 0; k < sizeof ips / sizeof *ips; k++)
            for (int seed = 0; seed < 256; seed += 37) {
                char enc[256], dec[256];
                tn_login_encode(logins[i], ips[k], seed, enc, sizeof enc);
                CHECK(strlen(enc) == 2 * (strlen(logins[i]) + 1));
                CHECK(tn_login_decode(enc, dec, sizeof dec) == 0);
                CHECK(strcmp(dec, logins[i]) == 0);
            }
    /* GTetrinet's output for 127.0.0.1 (hash 54*127+17 = 6875), first byte 0 */
    char enc[256];
    unsigned char lo[4] = {127, 0, 0, 1};
    tn_login_encode("tetrisstart a 1.13", lo, 0, enc, sizeof enc);
    CHECK(strncmp(enc, "00", 2) == 0);
    unsigned v = ((0 + 't') % 255) ^ '6';
    char first[3];
    snprintf(first, sizeof first, "%02X", v);
    CHECK(strncmp(enc + 2, first, 2) == 0);
    char dec[256];
    CHECK(tn_login_decode("41424344454647484950515253", dec, sizeof dec) != 0);
    CHECK(tn_login_decode("zz", dec, sizeof dec) != 0);
}

static void random_field(tn_field f, tn_rng *r, int density)
{
    for (int y = 0; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++)
            f[y][x] = (char)(tn_rng_int(r, 100) < density ? tn_rng_int(r, 15) : 0);
}

static void test_field_codec(void)
{
    tn_rng r;
    tn_rng_seed(&r, 42);
    for (int round = 0; round < 200; round++) {
        tn_field a, b, c;
        random_field(a, &r, 50);
        memcpy(b, a, sizeof b);
        for (int k = 0; k < round % 40; k++) b[tn_rng_int(&r, TN_FIELD_H)][tn_rng_int(&r, TN_FIELD_W)] = (char)tn_rng_int(&r, 15);
        char data[2048];
        tn_field_encode(b, a, 0, data, sizeof data);
        memcpy(c, a, sizeof c);
        CHECK(tn_field_apply(c, data) == 0);
        CHECK(memcmp(b, c, sizeof c) == 0);
        tn_field_encode(b, a, 1, data, sizeof data);
        CHECK(strlen(data) == TN_FIELD_W * TN_FIELD_H);
        memset(c, 0, sizeof c);
        CHECK(tn_field_apply(c, data) == 0);
        CHECK(memcmp(b, c, sizeof c) == 0);
    }
    tn_field e = {{0}};
    char data[16];
    tn_field_encode(e, e, 0, data, sizeof data);
    CHECK(data[0] == 0);
}

static void test_rules(void)
{
    tn_rules r, p;
    tn_rules_default(&r);
    r.stack = 3;
    r.classic = 0;
    r.average_levels = 1;
    char s[512];
    tn_rules_format(&r, s, sizeof s);
    CHECK(tn_rules_parse(&p, s) == 0);
    CHECK(memcmp(&r, &p, sizeof r) == 0);
    CHECK(tn_rules_parse(&p, "garbage") == -1);
}

static void test_game(void)
{
    tn_rules r;
    tn_rules_default(&r);
    tn_game g;
    tn_game_start(&g, &r, 7);
    CHECK(g.alive && g.block == TN_NO_BLOCK);
    CHECK(tn_game_spawn(&g) == 0);
    CHECK(g.block >= 0 && g.block < 7);

    /* a full bottom row with two specials is cleared and both specials are collected */
    memset(&g.field, 0, sizeof g.field);
    g.block = TN_NO_BLOCK;
    for (int x = 0; x < TN_FIELD_W; x++) g.field[TN_FIELD_H - 1][x] = 1;
    g.field[TN_FIELD_H - 1][3] = 6;  /* a: add line */
    g.field[TN_FIELD_H - 1][7] = 8;  /* n: nuke */
    g.field[TN_FIELD_H - 2][0] = 2;
    int lvl = 0;
    CHECK(tn_game_clear_lines(&g, &lvl) == 1);
    CHECK(g.nspecials == 2);
    CHECK(g.field[TN_FIELD_H - 1][0] != 0); /* the row above came down (may now be a special) */
    CHECK(g.lines == 1);

    /* add-line pushes up; a full top row loses */
    tn_game h;
    tn_game_start(&h, &r, 9);
    CHECK(tn_game_add_lines(&h, 3, 2) == 0);
    int holes = 0;
    for (int x = 0; x < TN_FIELD_W; x++) holes += h.field[TN_FIELD_H - 1][x] == 0;
    CHECK(holes == 1);
    h.field[0][5] = 1;
    CHECK(tn_game_add_lines(&h, 1, 1) == -1);
    CHECK(!h.alive);

    /* nuke clears, gravity compacts, switch copies with six free rows */
    tn_game k;
    tn_game_start(&k, &r, 11);
    k.field[10][4] = 3;
    tn_game_apply_special(&k, TN_SB_GRAVITY, NULL);
    CHECK(k.field[TN_FIELD_H - 1][4] == 3 && k.field[10][4] == 0);
    tn_game_apply_special(&k, TN_SB_NUKE, NULL);
    int any = 0;
    for (int y = 0; y < TN_FIELD_H; y++) for (int x = 0; x < TN_FIELD_W; x++) any |= k.field[y][x];
    CHECK(!any);
    tn_field other;
    memset(other, 1, sizeof other);
    for (int y = 0; y < TN_FIELD_H; y++) other[y][0] = 0; /* no full lines */
    tn_game_apply_special(&k, TN_SB_SWITCH, (const char (*)[TN_FIELD_W])other);
    for (int y = 0; y < 6; y++) for (int x = 0; x < TN_FIELD_W; x++) CHECK(k.field[y][x] == 0);
    CHECK(k.field[6][1] == 1 && k.field[TN_FIELD_H - 1][1] == 1);

    /* hard drop lands on the floor */
    tn_game d;
    tn_game_start(&d, &r, 3);
    tn_game_spawn(&d);
    tn_game_drop(&d);
    CHECK(tn_game_solidify(&d) == 0);
    int bottom = 0;
    for (int x = 0; x < TN_FIELD_W; x++) bottom |= d.field[TN_FIELD_H - 1][x];
    CHECK(bottom);

    /* a field filled to the top kills the next spawn */
    tn_game f;
    tn_game_start(&f, &r, 5);
    memset(f.field, 1, sizeof f.field);
    CHECK(tn_game_spawn(&f) == -1);
    CHECK(!f.alive);
}

static void pump(tn_server *s, tn_client *c, int n, int ms)
{
    for (int round = 0; round < ms / 2; round++) {
        tn_server_update(s, 2);
        for (int i = 0; i < n; i++) tn_client_update(&c[i], 2);
        sleep_ms(1);
    }
}

static int all_connected(tn_client *c, int n)
{
    for (int i = 0; i < n; i++) if (c[i].state != TN_CONNECTED) return 0;
    return 1;
}

static void test_network_game(void)
{
    tn_server s;
    char err[160] = "";
    CHECK(tn_server_start(&s, 0, 1, 8, err, sizeof err) == 0);
    if (err[0]) { fprintf(stderr, "server: %s\n", err); return; }
    s.rules.stack = 0;

    enum { N = 3 };
    tn_client c[N];
    const char *nicks[N] = {"Anna", "Ben", "Cleo"};
    for (int i = 0; i < N; i++) {
        tn_client_init(&c[i], nicks[i], "", 100 + (uint32_t)i);
        tn_client_connect(&c[i], "127.0.0.1", s.port, i == 2);  /* Cleo uses tetrifast */
    }
    for (int t = 0; t < 200 && !all_connected(c, N); t++) pump(&s, c, N, 10);
    CHECK(all_connected(c, N));
    pump(&s, c, N, 50);
    for (int i = 0; i < N; i++) {
        CHECK(c[i].me >= 1 && c[i].me <= 8);
        for (int k = 0; k < N; k++) CHECK(strcmp(c[i].slot[c[k].me].nick, nicks[k]) == 0);
    }
    int mods = 0, mod = 0;
    for (int i = 0; i < N; i++) if (tn_client_moderator(&c[i])) { mods++; mod = i; }
    CHECK(mods == 1);

    /* a taken nickname is refused */
    tn_client dup;
    tn_client_init(&dup, "Anna", "", 1);
    tn_client_connect(&dup, "127.0.0.1", s.port, 0);
    for (int t = 0; t < 100 && dup.state != TN_FAILED; t++) { tn_server_update(&s, 2); tn_client_update(&dup, 2); sleep_ms(1); }
    CHECK(dup.state == TN_FAILED && strstr(dup.err, "Nickname"));

    tn_client_start_game(&c[mod]);
    pump(&s, c, N, 100);
    for (int i = 0; i < N; i++) CHECK(c[i].ingame && c[i].playing);

    /* the moderator gets an add-line special and uses it on the next player */
    int victim = (mod + 1) % N;
    c[mod].game.specials[0] = 6;
    c[mod].game.nspecials = 1;
    unsigned hits_before = c[victim].hits;
    tn_client_special(&c[mod], c[victim].me);
    pump(&s, c, N, 100);
    CHECK(c[victim].hits == hits_before + 1);
    int garbage = 0;
    for (int x = 0; x < TN_FIELD_W; x++) garbage |= c[victim].game.field[TN_FIELD_H - 1][x];
    CHECK(garbage);
    /* everybody else sees the victim's new field */
    int other = (mod + 2) % N;
    CHECK(memcmp(c[other].slot[c[victim].me].field, c[victim].game.field, sizeof(tn_field)) == 0);

    /* two players drop pieces until they are out; the moderator wins */
    for (int t = 0; t < 4000 && (c[victim].playing || c[other].playing); t++) {
        tn_client_action(&c[victim], TN_ACT_DROP);
        tn_client_action(&c[other], TN_ACT_DROP);
        pump(&s, c, N, 4);
        tn_client_update(&c[victim], 1000);
        tn_client_update(&c[other], 1000);
    }
    pump(&s, c, N, 200);
    for (int i = 0; i < N; i++) {
        CHECK(!c[i].ingame);
        CHECK(c[i].last_winner == c[mod].me);
        CHECK(strstr(c[i].winlist, nicks[mod]) != NULL);
    }

    /* chat reaches the others */
    tn_client_say(&c[victim], "good game");
    pump(&s, c, N, 50);
    CHECK(strstr(tn_log_get(&c[mod].chat, 0), "good game") != NULL);

    /* eight players fit, a ninth is refused */
    tn_client more[6];
    char nick[16];
    for (int i = 0; i < 6; i++) {
        snprintf(nick, sizeof nick, "P%d", i + 4);
        tn_client_init(&more[i], nick, "", 1);
        tn_client_connect(&more[i], "127.0.0.1", s.port, 0);
    }
    for (int t = 0; t < 200; t++) {
        tn_server_update(&s, 2);
        for (int i = 0; i < 6; i++) tn_client_update(&more[i], 2);
        sleep_ms(1);
    }
    int ok = 0, full = 0;
    for (int i = 0; i < 6; i++) {
        ok += more[i].state == TN_CONNECTED;
        full += more[i].state == TN_FAILED && strstr(more[i].err, "Full") != NULL;
    }
    CHECK(ok == 5 && full == 1);
    CHECK(tn_server_count(&s) == 8);

    /* leaving frees the slot and the others notice */
    int leaver = c[victim].me;
    tn_client_disconnect(&c[victim]);
    for (int t = 0; t < 50; t++) { tn_server_update(&s, 2); tn_client_update(&c[mod], 2); sleep_ms(1); }
    CHECK(!c[mod].slot[leaver].present);

    for (int i = 0; i < 6; i++) tn_client_disconnect(&more[i]);
    for (int i = 0; i < N; i++) tn_client_disconnect(&c[i]);
    tn_client_disconnect(&dup);
    tn_server_stop(&s);
}

static void test_solo_game(void)
{
    /* one player alone: the game ends when they are out, with no winner */
    tn_server s;
    char err[160] = "";
    CHECK(tn_server_start(&s, 0, 1, 6, err, sizeof err) == 0);
    tn_client c;
    tn_client_init(&c, "Solo", "", 3);
    tn_client_connect(&c, "127.0.0.1", s.port, 0);
    for (int t = 0; t < 200 && c.state != TN_CONNECTED; t++) pump(&s, &c, 1, 4);
    CHECK(tn_client_moderator(&c));
    tn_client_start_game(&c);
    pump(&s, &c, 1, 50);
    CHECK(c.playing);
    /* gravity alone, at high speed, ends it */
    for (int t = 0; t < 20000 && c.ingame; t++) {
        tn_client_update(&c, 1000);
        pump(&s, &c, 1, 2);
    }
    CHECK(!c.ingame && c.last_winner == 0);
    tn_client_disconnect(&c);
    tn_server_stop(&s);
}


static void test_bot_game(int tn2)
{
    /* four computer players over the real server: lines get cleared, specials fly, somebody wins */
    tn_server s;
    char err[160] = "";
    CHECK(tn_server_start(&s, 0, 1, 8, err, sizeof err) == 0);
    if (tn2) tn_rules_tn2(&s.rules);
    enum { N = 4 };
    tn_client c[N];
    tn_bot b[N];
    char nick[16];
    for (int i = 0; i < N; i++) {
        snprintf(nick, sizeof nick, "Bot%d", i + 1);
        tn_client_init(&c[i], nick, "", 1000 + (uint32_t)i * 77);
        tn_client_connect(&c[i], "127.0.0.1", s.port, 0);
        tn_bot_init(&b[i], 40 + 30 * i);
    }
    for (int t = 0; t < 200 && !all_connected(c, N); t++) pump(&s, c, N, 4);
    CHECK(all_connected(c, N));
    pump(&s, c, N, 20);
    for (int i = 0; i < N; i++) if (tn_client_moderator(&c[i])) tn_client_start_game(&c[i]);
    pump(&s, c, N, 40);
    CHECK(c[0].ingame);
    unsigned lines = 0, hits = 0;
    int steps = 0;
    /* simulated time: 20 ms per step, network pumped for real */
    for (; steps < 60000 && c[0].ingame; steps++) {
        for (int i = 0; i < N; i++) { tn_bot_update(&b[i], &c[i], 20); }
        tn_server_update(&s, 20);
        for (int i = 0; i < N; i++) tn_client_update(&c[i], 20);
        if (steps % 50 == 0) sleep_ms(1);
    }
    pump(&s, c, N, 100);
    for (int i = 0; i < N; i++) { lines += c[i].lines_total; hits += c[i].hits; }
    printf("%sbot game: %d steps (%d s of play), %u lines, %u hits, winner slot %d\n", tn2 ? "TN2 " : "", steps, steps / 50, lines, hits, c[0].last_winner);
    CHECK(lines > 20);
    CHECK(hits > 0);
    CHECK(!c[0].ingame && c[0].last_winner >= 1);
    for (int i = 0; i < N; i++) tn_client_disconnect(&c[i]);
    tn_server_stop(&s);
}

static int cells_of(const char (*sh)[4])
{
    int n = 0;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) n += sh[y][x] != 0;
    return n;
}

static void test_tn2_rules(void)
{
    /* classic newgame lines are unchanged; the TN2 suffix round-trips and classic parsers stop before it */
    tn_rules r, p;
    char s[512];
    tn_rules_default(&r);
    tn_rules_format(&r, s, sizeof s);
    CHECK(strstr(s, "tn2") == NULL);
    tn_rules_tn2(&r);
    tn_rules_format(&r, s, sizeof s);
    CHECK(strstr(s, " tn2 1 ") != NULL);
    CHECK(tn_rules_parse(&p, s) == 0);
    CHECK(memcmp(&r, &p, sizeof r) == 0);
    int sum = 0;
    for (int i = 0; i < TN_SPECIALS; i++) sum += p.specialfreq[i];
    CHECK(sum == 100 && p.specialfreq[TN_SB_CONFUSION - TN_SB_ADDLINE] > 0);
    /* the classic part alone: what a classic client reads, only classic specials */
    char classic[512];
    snprintf(classic, sizeof classic, "%.*s", (int)(strstr(s, " tn2") - s), s);
    CHECK(tn_rules_parse(&p, classic) == 0 && !p.tn2 && !p.hold);
    for (int i = TN_CLASSIC_SPECIALS; i < TN_SPECIALS; i++) CHECK(p.specialfreq[i] == 0);
    /* a classic special string never carries the TN2 letters */
    char sf[128];
    sscanf(classic, "%*d %*d %*d %*d %*d %*d %*d %*s %127s", sf);
    CHECK(strspn(sf, "123456789") == strlen(sf));

    /* fields with TN2 specials go whole, and arrive intact */
    tn_field a = {{0}}, b = {{0}}, c;
    b[21][3] = 15; b[20][4] = 19; b[19][0] = 7;
    char data[2048];
    tn_field_encode(b, a, 0, data, sizeof data);
    CHECK(strlen(data) == TN_FIELD_W * TN_FIELD_H);
    memset(c, 0, sizeof c);
    CHECK(tn_field_apply(c, data) == 0 && memcmp(b, c, sizeof c) == 0);
    for (int sb = TN_SB_IMMUNITY; sb <= TN_SB_CONFUSION; sb++) {
        CHECK(tn_sb_from_cell(tn_sb_cell(sb)) == sb);
        CHECK(tn_cell_from_char(tn_cell_char(tn_sb_cell(sb))) == tn_sb_cell(sb));
        CHECK(tn_sb_from_id(tn_sb_id(sb)) == sb);
    }
}

static void test_tn2_game(void)
{
    tn_rules r;
    tn_rules_default(&r);
    tn_game g;
    /* no hold under classic rules */
    tn_game_start(&g, &r, 5);
    CHECK(tn_game_spawn(&g) == 0);
    int b0 = g.block;
    CHECK(tn_game_hold(&g) == 1 && g.block == b0 && g.hold == TN_NO_BLOCK);

    tn_rules_tn2(&r);
    tn_game_start(&g, &r, 5);
    CHECK(tn_game_spawn(&g) == 0);
    b0 = g.block;
    int n0 = g.next;
    tn_game_move(&g, 1);
    CHECK(tn_game_hold(&g) == 0);
    CHECK(g.hold == b0 && g.block == n0 && g.bx == TN_FIELD_W / 2 - 2 && g.by == 0);
    CHECK(tn_game_hold(&g) == 1 && g.block == n0);        /* once per piece */
    tn_game_drop(&g);
    CHECK(tn_game_solidify(&g) == 0);
    CHECK(tn_game_spawn(&g) == 0);
    int b1 = g.block;
    CHECK(tn_game_hold(&g) == 0 && g.block == b0 && g.hold == b1);  /* swaps back */

    /* Mutate Pieces: the next three pieces are odd shapes of five or six cells, then normal again */
    tn_field none = {{0}};
    tn_game_apply_special(&g, TN_SB_MUTATE, none);
    CHECK(g.next == TN_MUTANT_NEXT);
    int k = cells_of(tn_game_shape(&g, g.next, 0));
    CHECK(k == 5 || k == 6);
    for (int i = 0; i < TN_MUTATE_PIECES; i++) {
        tn_game_drop(&g);
        tn_game_solidify(&g);
        memset(g.field, 0, sizeof g.field);
        CHECK(tn_game_spawn(&g) == 0);
        CHECK(g.block == TN_MUTANT_CUR);
        int cells = cells_of(tn_game_shape(&g, g.block, 0));
        CHECK(cells == 5 || cells == 6);
        for (int o = 1; o < 4; o++) CHECK(cells_of(tn_game_shape(&g, g.block, o)) == cells);
        int before = g.orient;
        tn_game_rotate(&g, 1);
        CHECK(g.orient == (before + 1) % 4);
        if (i == 0) {
            /* a held mutant keeps its shape */
            char shape[4][4];
            memcpy(shape, tn_game_shape(&g, g.block, 0), sizeof shape);
            int held = g.hold;
            CHECK(tn_game_hold(&g) == 0 && g.hold == TN_MUTANT_HOLD && g.block == held);
            CHECK(memcmp(tn_game_shape(&g, g.hold, 0), shape, sizeof shape) == 0);
        }
    }
    tn_game_drop(&g);
    tn_game_solidify(&g);
    memset(g.field, 0, sizeof g.field);
    CHECK(tn_game_spawn(&g) == 0 && g.block < 7);
    int landed = 0;
    tn_game_drop(&g);
    landed = tn_game_solidify(&g) == 0;
    CHECK(landed);

    /* Clear Column empties exactly one whole column */
    for (int y = 4; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++) g.field[y][x] = (char)(1 + (x + y) % 5);
    for (int y = 4; y < TN_FIELD_H; y++) g.field[y][(y * 7) % TN_FIELD_W] = 0; /* no full rows */
    tn_field before;
    memcpy(before, g.field, sizeof before);
    tn_game_apply_special(&g, TN_SB_CLEARCOLUMN, none);
    int empty_cols = 0;
    for (int x = 0; x < TN_FIELD_W; x++) {
        int filled = 0;
        for (int y = 0; y < TN_FIELD_H; y++) filled += g.field[y][x] != 0;
        empty_cols += filled == 0;
    }
    CHECK(empty_cols == 1);

    /* TN2 specials appear on fields only under TN2 rules */
    int seen[TN_CELLS] = {0};
    for (int round = 0; round < 400; round++) {
        tn_game_start(&g, &r, (uint32_t)round + 1);
        for (int x = 0; x < TN_FIELD_W - 1; x++) g.field[TN_FIELD_H - 1][x] = 1;
        g.field[TN_FIELD_H - 2][0] = 2;
        g.field[TN_FIELD_H - 1][TN_FIELD_W - 1] = 3;
        int lvl;
        tn_game_clear_lines(&g, &lvl);
        for (int y = 0; y < TN_FIELD_H; y++)
            for (int x = 0; x < TN_FIELD_W; x++) seen[(int)g.field[y][x]]++;
    }
    for (int cell = 15; cell < TN_CELLS; cell++) CHECK(seen[cell] > 0);
}

static void test_tn2_network(void)
{
    /* three clients on a TN2 server: immunity, darkness, confusion and switch across the wire */
    tn_server s;
    char err[160] = "";
    CHECK(tn_server_start(&s, 0, 1, 8, err, sizeof err) == 0);
    tn_rules_tn2(&s.rules);
    enum { N = 3 };
    tn_client c[N];
    const char *nicks[N] = {"Ida", "Vic", "Max"};
    for (int i = 0; i < N; i++) {
        tn_client_init(&c[i], nicks[i], "", 300 + (uint32_t)i);
        tn_client_connect(&c[i], "127.0.0.1", s.port, 0);
    }
    for (int t = 0; t < 200 && !all_connected(c, N); t++) pump(&s, c, N, 4);
    CHECK(all_connected(c, N));
    pump(&s, c, N, 20);
    for (int i = 0; i < N; i++) if (tn_client_moderator(&c[i])) tn_client_start_game(&c[i]);
    pump(&s, c, N, 40);
    for (int i = 0; i < N; i++) CHECK(c[i].ingame && c[i].rules.tn2 && c[i].rules.hold);

    /* Ida makes herself immune; everybody sees it, Vic's add line does nothing to her */
    c[0].game.specials[0] = (unsigned char)tn_sb_cell(TN_SB_IMMUNITY);
    c[0].game.nspecials = 1;
    tn_client_special(&c[0], c[0].me);
    pump(&s, c, N, 40);
    for (int i = 0; i < N; i++) CHECK(c[i].immune[c[0].me] > 0);
    tn_field before;
    memcpy(before, c[0].game.field, sizeof before);
    c[1].game.specials[0] = 6;
    c[1].game.nspecials = 1;
    tn_client_special(&c[1], c[0].me);
    pump(&s, c, N, 40);
    CHECK(c[0].ev.blocked == 1 && memcmp(before, c[0].game.field, sizeof before) == 0);
    /* a switch onto her is refused on both ends */
    for (int x = 0; x < TN_FIELD_W - 1; x++) c[1].game.field[TN_FIELD_H - 1][x] = 2;
    tn_field vic;
    memcpy(vic, c[1].game.field, sizeof vic);
    c[1].game.specials[0] = (unsigned char)tn_sb_cell(TN_SB_SWITCH);
    c[1].game.nspecials = 1;
    tn_client_special(&c[1], c[0].me);
    pump(&s, c, N, 40);
    CHECK(memcmp(vic, c[1].game.field, sizeof vic) == 0 && memcmp(before, c[0].game.field, sizeof before) == 0);
    /* it runs out */
    tn_client_update(&c[0], TN_IMMUNITY_MS);
    CHECK(c[0].immune[c[0].me] == 0);

    /* Darkness and Confusion reach Max and wear off */
    c[0].game.specials[0] = (unsigned char)tn_sb_cell(TN_SB_DARKNESS);
    c[0].game.specials[1] = (unsigned char)tn_sb_cell(TN_SB_CONFUSION);
    c[0].game.nspecials = 2;
    tn_client_special(&c[0], c[2].me);
    tn_client_special(&c[0], c[2].me);
    pump(&s, c, N, 40);
    CHECK(c[2].dark_ms > 0 && c[2].confused_ms > 0);
    CHECK(c[0].dark_ms == 0 && c[0].confused_ms == 0);
    int same = 1;
    for (int i = 0; i < 4; i++) same &= c[2].confusion[i] == i;
    CHECK(!same);
    /* a confused "left" does what the map says */
    for (int t = 0; t < 200 && (c[2].game.block == TN_NO_BLOCK || c[2].spawn_wait); t++) tn_client_update(&c[2], 50);
    int bx = c[2].game.bx, orient = c[2].game.orient;
    tn_client_action(&c[2], TN_ACT_LEFT);
    if (c[2].confusion[TN_ACT_LEFT] == TN_ACT_RIGHT) CHECK(c[2].game.bx == bx + 1);
    else if (c[2].confusion[TN_ACT_LEFT] == TN_ACT_LEFT) CHECK(c[2].game.bx == bx - 1);
    else CHECK(c[2].game.bx == bx && (c[2].game.orient != orient || tn_block_orients(c[2].game.block) == 1));
    tn_client_update(&c[2], TN_DARKNESS_MS);
    CHECK(c[2].dark_ms == 0 && c[2].confused_ms == 0);

    /* Mutate goes over the wire, hold works for a client */
    c[1].game.specials[0] = (unsigned char)tn_sb_cell(TN_SB_MUTATE);
    c[1].game.nspecials = 1;
    tn_client_special(&c[1], c[0].me);
    pump(&s, c, N, 40);
    CHECK(c[0].game.next == TN_MUTANT_NEXT);
    for (int t = 0; t < 200 && (c[0].game.block == TN_NO_BLOCK || c[0].spawn_wait); t++) tn_client_update(&c[0], 50);
    int was = c[0].game.block;
    tn_client_action(&c[0], TN_ACT_HOLD);
    CHECK(c[0].game.hold != TN_NO_BLOCK && (was == TN_MUTANT_CUR ? c[0].game.hold == TN_MUTANT_HOLD : c[0].game.hold == was));

    /* a TN2 field reaches the others whole */
    c[1].game.field[TN_FIELD_H - 1][0] = (char)tn_sb_cell(TN_SB_CLEARCOLUMN);
    c[1].game.specials[0] = 6;
    c[1].game.nspecials = 1;
    tn_client_special(&c[1], c[1].me); /* any special makes Vic send his field */
    pump(&s, c, N, 40);
    CHECK(memcmp(c[2].slot[c[1].me].field, c[1].game.field, sizeof(tn_field)) == 0);

    for (int i = 0; i < N; i++) tn_client_disconnect(&c[i]);
    tn_server_stop(&s);

    /* a client on a classic server ignores TN2 specials */
    tn_client cl;
    tn_client_init(&cl, "Classic", "", 9);
    tn_client_handle(&cl, "playernum 1");
    tn_client_handle(&cl, "playerjoin 2 Other");
    tn_rules d;
    tn_rules_default(&d);
    char args[512], msg[600];
    tn_rules_format(&d, args, sizeof args);
    snprintf(msg, sizeof msg, "newgame %s", args);
    tn_client_handle(&cl, msg);
    CHECK(cl.playing && !cl.rules.tn2);
    tn_client_handle(&cl, "sb 1 d 2");
    tn_client_handle(&cl, "sb 1 f 2");
    CHECK(cl.dark_ms == 0 && cl.confused_ms == 0 && cl.attacks.count == 0);
}

/* does the resolver give an address of this family for the name? */
static int resolves_to(const char *host, int family)
{
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = family;
    if (getaddrinfo(host, "31457", &hints, &res) != 0 || !res) return 0;
    freeaddrinfo(res);
    return 1;
}

static int connect_one(tn_server *s, tn_client *c, const char *nick, const char *host, int port)
{
    tn_client_init(c, nick, "", 5);
    tn_client_connect(c, host, port, 0);
    /* By time, not by rounds: a refused ::1 costs Windows about two seconds of SYN retries, and
     * a 1 ms sleep can take 15 ms there, so a fixed round count was flaky (CI 39356936200). */
    long t0 = clock_ms();
    while (c->state != TN_CONNECTED && c->state != TN_FAILED && clock_ms() - t0 < 10000) pump(s, c, 1, 4);
    if (c->state != TN_CONNECTED) fprintf(stderr, "%s -> %s: %s\n", nick, host, c->err);
    return c->state == TN_CONNECTED;
}

static void test_ipv6_and_lookup(void)
{
    char err[160] = "", host[160];
    int port;
    tn_client c, d;

    /* the address field: [v6]:port, a bare v6 address, host:port */
    CHECK(tn_query_split("[::1]:31460", host, sizeof host, &port) == 0 && !strcmp(host, "::1") && port == 31460);
    CHECK(tn_query_split("::1", host, sizeof host, &port) == 0 && !strcmp(host, "::1") && port == TN_PORT);
    CHECK(tn_query_split("[2001:db8::7]", host, sizeof host, &port) == 0 && !strcmp(host, "2001:db8::7") && port == TN_PORT);
    CHECK(tn_query_split("tetrinet.example:9000", host, sizeof host, &port) == 0 && port == 9000);

    /* a LAN server takes IPv6 and IPv4 clients on one port */
    tn_server s;
    CHECK(tn_server_start(&s, 0, 0, 8, err, sizeof err) == 0);
    if (!resolves_to("::1", AF_INET6)) {
        fprintf(stderr, "no IPv6 here: skipping the IPv6 checks\n");
    } else {
        CHECK(connect_one(&s, &c, "Six", "::1", s.port));
        CHECK(connect_one(&s, &d, "Four", "127.0.0.1", s.port));
        CHECK(c.me >= 1 && d.me >= 1 && c.me != d.me);
        pump(&s, &c, 1, 40);
        pump(&s, &d, 1, 40);
        CHECK(c.slot[d.me].present && d.slot[c.me].present); /* one lobby */
        tn_client_disconnect(&c);
        tn_client_disconnect(&d);
    }
    tn_server_stop(&s);

    /* an address that refuses is passed over for the next one */
    CHECK(tn_server_start(&s, 0, 1, 8, err, sizeof err) == 0); /* 127.0.0.1 only */
    if (resolves_to("localhost", AF_INET6) && resolves_to("localhost", AF_INET)) {
        tn_net_test_prefer_ipv6 = 1; /* ::1 first, where nobody listens */
        CHECK(connect_one(&s, &c, "Fallback", "localhost", s.port));
        tn_client_disconnect(&c);
        tn_net_test_prefer_ipv6 = 0;
    } else {
        fprintf(stderr, "localhost lacks IPv4 or IPv6: skipping the fallback check\n");
    }

    /* a slow name server holds up nobody: connect and update return at once */
    tn_net_test_resolve_delay_ms = 1200;
    long t0 = clock_ms();
    tn_client_init(&c, "Patient", "", 6);
    tn_client_connect(&c, "127.0.0.1", s.port, 0);
    tn_client_update(&c, 2);
    long spent = clock_ms() - t0;
    CHECK(spent < 300);
    CHECK(c.state == TN_CONNECTING && tn_connector_resolving(c.connector));
    int frames = 0;
    while (c.state == TN_CONNECTING && clock_ms() - t0 < 5000) {
        long f0 = clock_ms();
        tn_server_update(&s, 2);
        tn_client_update(&c, 2);
        if (clock_ms() - f0 > 200) { CHECK(!"a frame waited for the lookup"); break; }
        frames++;
        sleep_ms(5);
    }
    for (int t = 0; t < 200 && c.state != TN_CONNECTED; t++) pump(&s, &c, 1, 4);
    CHECK(c.state == TN_CONNECTED);
    /* Frames kept coming while the lookup slept. Not 50: a 5 ms sleep takes 20+ ms on a busy
     * macOS runner, which is still a frame loop that never blocked. */
    fprintf(stderr, "slow lookup: connected after %ld ms, %d frames meanwhile\n", clock_ms() - t0, frames);
    CHECK(clock_ms() - t0 >= 1200 && frames > 20);
    tn_client_disconnect(&c);

    /* backing out mid-lookup: the lookup finishes on its own and frees itself */
    tn_client_init(&d, "Impatient", "", 7);
    tn_client_connect(&d, "127.0.0.1", s.port, 0);
    tn_client_update(&d, 2);
    tn_client_disconnect(&d);
    CHECK(d.state == TN_OFFLINE && d.connector == NULL);
    tn_net_test_resolve_delay_ms = 0;
    tn_server_stop(&s);

    /* a name that does not exist fails with a message, not a hang */
    tn_client_init(&d, "Lost", "", 8);
    tn_client_connect(&d, "no-such-server.invalid", 0, 0);
    for (int t = 0; t < 3000 && d.state == TN_CONNECTING; t++) { tn_client_update(&d, 2); sleep_ms(2); }
    CHECK(d.state == TN_FAILED && strstr(d.err, "cannot find server"));
    tn_client_disconnect(&d);
    for (int t = 0; t < 150; t++) sleep_ms(10); /* let the abandoned lookup end before the leak check */
}

int main(void)
{
    if (tn_net_init() != 0) { fprintf(stderr, "network init failed\n"); return 1; }
    test_login();
    test_field_codec();
    test_rules();
    test_game();
    test_network_game();
    test_solo_game();
    test_bot_game(0);
    test_bot_game(1);
    test_tn2_rules();
    test_tn2_game();
    test_tn2_network();
    test_ipv6_and_lookup();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
