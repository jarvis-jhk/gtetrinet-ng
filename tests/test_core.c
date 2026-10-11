/*
 * gtetrinet-ng — tests for the protocol, the rules and a real client/server game over
 * localhost. SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_client.h"
#include "tn_server.h"
#include "tn_bot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <time.h>
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


static void test_bot_game(void)
{
    /* four computer players over the real server: lines get cleared, specials fly, somebody wins */
    tn_server s;
    char err[160] = "";
    CHECK(tn_server_start(&s, 0, 1, 8, err, sizeof err) == 0);
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
    printf("bot game: %d steps (%d s of play), %u lines, %u hits, winner slot %d\n", steps, steps / 50, lines, hits, c[0].last_winner);
    CHECK(lines > 20);
    CHECK(hits > 0);
    CHECK(!c[0].ingame && c[0].last_winner >= 1);
    for (int i = 0; i < N; i++) tn_client_disconnect(&c[i]);
    tn_server_stop(&s);
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
    test_bot_game();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
