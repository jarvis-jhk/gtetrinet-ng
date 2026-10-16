/*
 * gtetrinet-ng — plays our client against a TetriNET server we did not write.
 *
 *   test_interop HOST PORT [fast|fast-unsupported]
 *
 * Two clients log in to an already running server (tetrinetx, Andrew Church's tetrinet-server,
 * or our own for comparison), chat, change team, play a game with specials until one tops out
 * and the other wins, then play a second game. They also check that a taken nickname and a
 * full six-slot server are refused in words the game understands. "fast" logs in with the
 * TetriFast handshake; "fast-unsupported" expects the server to refuse it (neither tetrinetx
 * nor tetrinet-server decodes it) and checks that the player is told why.
 * tests/interop.sh builds and starts the third-party servers; CI runs it.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_client.h"

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
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
#endif

static int checks, failures;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } } while (0)

#define N 2
static tn_client c[N];

static void pump(int ms)
{
    for (int t = 0; t < ms / 5; t++) {
        for (int i = 0; i < N; i++) tn_client_update(&c[i], 5);
        sleep_ms(5);
    }
}

/* Pumps until cond holds or ms run out. */
#define WAIT_FOR(cond, ms) do { for (int w_ = 0; w_ < (ms) / 20 && !(cond); w_++) pump(20); } while (0)

static int chat_has(const tn_client *cl, const char *text)
{
    for (int i = 0; i < cl->chat.count; i++)
        if (strstr(tn_log_get(&cl->chat, i), text)) return 1;
    return 0;
}

static int attacks_have(const tn_client *cl, const char *text)
{
    for (int i = 0; i < cl->attacks.count; i++)
        if (strstr(tn_log_get(&cl->attacks, i), text)) return 1;
    return 0;
}

static int field_cells(const tn_field f)
{
    int n = 0;
    for (int y = 0; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++) n += f[y][x] != 0;
    return n;
}

static void dump(const char *why)
{
    for (int i = 0; i < N; i++) {
        fprintf(stderr, "-- %s: client %s state %d me %d ingame %d playing %d err '%s'\n", why, c[i].nick,
                c[i].state, c[i].me, c[i].ingame, c[i].playing, c[i].err);
        for (int k = c[i].chat.count - 1; k >= 0; k--) fprintf(stderr, "   chat: %s\n", tn_log_get(&c[i].chat, k));
    }
}

/* One game: the moderator starts it, both players see it; the moderator hands specials to the
 * other player, then the other player tops out by dropping pieces and the moderator wins. */
static void play_game(int mod, int round)
{
    int other = 1 - mod;
    unsigned wins_before = c[mod].ev.wins;
    tn_client_start_game(&c[mod]);
    WAIT_FOR(c[0].playing && c[1].playing, 5000);
    CHECK(c[0].ingame && c[1].ingame);
    CHECK(c[0].playing && c[1].playing);
    if (!(c[0].playing && c[1].playing)) { dump("game did not start"); return; }

    /* the opening field of each player reaches the other */
    for (int k = 0; k < 2; k++) {
        tn_client_action(&c[k], TN_ACT_DROP);
    }
    WAIT_FOR(field_cells(c[0].slot[c[1].me].field) > 0 && field_cells(c[1].slot[c[0].me].field) > 0, 5000);
    CHECK(field_cells(c[0].slot[c[1].me].field) > 0);
    CHECK(field_cells(c[1].slot[c[0].me].field) > 0);

    /* specials: an Add Line aimed at the other player, and a Clear Line on ourselves. The
     * inventory is filled directly; what is tested is how the server relays them. */
    int before = field_cells(c[other].game.field);
    int seen = c[other].attacks.count;
    c[mod].game.specials[0] = 6 + 0;   /* a: add line */
    c[mod].game.specials[1] = 6 + 1;   /* c: clear line */
    c[mod].game.nspecials = 2;
    tn_client_special(&c[mod], c[other].me);
    WAIT_FOR(c[other].attacks.count > seen, 5000);
    CHECK(attacks_have(&c[other], "Add Line on Ben by Anna") || attacks_have(&c[other], "Add Line on Anna by Ben"));
    CHECK(c[other].hits >= 1);
    CHECK(field_cells(c[other].game.field) > before);
    pump(300);
    /* and the changed field travels back */
    CHECK(field_cells(c[mod].slot[c[other].me].field) >= field_cells(c[other].game.field) - 4);
    tn_client_special(&c[mod], c[mod].me);
    WAIT_FOR(c[other].attacks.count > seen + 1, 5000);
    CHECK(c[other].attacks.count == seen + 2);

    /* the other player tops out; the moderator keeps playing and wins */
    for (int t = 0; t < 3000 && c[other].playing; t++) {
        if (c[other].game.block != TN_NO_BLOCK && c[other].spawn_wait == 0) tn_client_action(&c[other], TN_ACT_DROP);
        pump(10);
    }
    CHECK(!c[other].playing);
    WAIT_FOR(!c[mod].ingame, 8000);
    if (c[mod].ingame) dump("game did not end");
    CHECK(!c[0].ingame && !c[1].ingame);
    CHECK(c[0].last_winner == c[mod].me && c[1].last_winner == c[mod].me);
    CHECK(c[mod].ev.wins == wins_before + 1);
    CHECK(c[other].ev.losses == (unsigned)round);
    pump(300);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s HOST PORT [fast|fast-unsupported]\n", argv[0]);
        return 2;
    }
    const char *host = argv[1];
    int port = atoi(argv[2]);
    const char *mode = argc > 3 ? argv[3] : "";
    int fast = strncmp(mode, "fast", 4) == 0;
    tn_net_init();

    if (strcmp(mode, "fast-unsupported") == 0) {
        tn_client_init(&c[0], "Anna", "", 7);
        tn_client_connect(&c[0], host, port, 1);
        WAIT_FOR(c[0].state == TN_CONNECTED || c[0].state == TN_FAILED, 8000);
        CHECK(c[0].state == TN_FAILED);
        CHECK(strstr(c[0].err, "TetriFast") != NULL);
        tn_client_disconnect(&c[0]);
        printf("test_interop %s:%d TetriFast refused: '%s'; %d checks, %d failures\n", host, port, c[0].err, checks, failures);
        return failures ? 1 : 0;
    }

    const char *nicks[N] = { "Anna", "Ben" };
    for (int i = 0; i < N; i++) {
        tn_client_init(&c[i], nicks[i], "", 7 + (uint32_t)i);
        tn_client_connect(&c[i], host, port, fast);
        WAIT_FOR(c[i].state == TN_CONNECTED || c[i].state == TN_FAILED, 8000);
        CHECK(c[i].state == TN_CONNECTED);
    }
    WAIT_FOR(c[0].slot[c[1].me].present && c[1].slot[c[0].me].present, 3000);
    if (c[0].state != TN_CONNECTED || c[1].state != TN_CONNECTED) {
        dump("login");
        fprintf(stderr, "test_interop: %d of %d checks failed\n", failures + 1, checks);
        return 1;
    }
    CHECK(c[0].me >= 1 && c[1].me >= 1 && c[0].me != c[1].me);
    CHECK(strcmp(c[0].slot[c[1].me].nick, "Ben") == 0);
    CHECK(strcmp(c[1].slot[c[0].me].nick, "Anna") == 0);
    int mod = tn_client_moderator(&c[0]) ? 0 : 1;

    /* a taken nickname: the game retries with another name when the reason says "Nickname" */
    static tn_client extra[5];
    tn_client_init(&extra[0], "Anna", "", 1);
    tn_client_connect(&extra[0], host, port, fast);
    for (int w = 0; w < 400 && extra[0].state != TN_FAILED && extra[0].state != TN_CONNECTED; w++) { pump(10); tn_client_update(&extra[0], 10); }
    CHECK(extra[0].state == TN_FAILED);
    CHECK(strstr(extra[0].err, "Nickname") != NULL);
    tn_client_disconnect(&extra[0]);

    /* six slots: four more fit, the seventh player is refused with a reason */
    for (int i = 0; i < 5; i++) {
        char nick[16];
        snprintf(nick, sizeof nick, "Extra%d", i + 1);
        tn_client_init(&extra[i], nick, "", 1);
        tn_client_connect(&extra[i], host, port, fast);
        for (int w = 0; w < 400 && extra[i].state != TN_FAILED && extra[i].state != TN_CONNECTED; w++) {
            pump(10);
            for (int k = 0; k <= i; k++) tn_client_update(&extra[k], 10);
        }
    }
    for (int i = 0; i < 4; i++) CHECK(extra[i].state == TN_CONNECTED);
    /* tetrinet-server refuses the seventh player; tetrinetx seats them in its next channel */
    CHECK((extra[4].state == TN_FAILED && extra[4].err[0]) ||
          (extra[4].state == TN_CONNECTED && !c[0].slot[extra[4].me].present) ||
          (extra[4].state == TN_CONNECTED && strcmp(c[0].slot[extra[4].me].nick, "Extra5") != 0));
    if (extra[4].state == TN_FAILED) printf("seventh player refused: '%s'\n", extra[4].err);
    else printf("seventh player seated elsewhere (slot %d, not in our game)\n", extra[4].me);
    /* one at a time: tetrinet-server dies of SIGPIPE when it writes a playerleave to a client
     * that hung up in the same instant, which is why the game staggers its own departures */
    for (int i = 0; i < 5; i++) {
        tn_client_disconnect(&extra[i]);
        pump(100);
    }
    WAIT_FOR(!c[0].slot[extra[3].me ? extra[3].me : 6].present, 3000);
    pump(500);
    CHECK(tn_client_moderator(&c[mod]) && !tn_client_moderator(&c[1 - mod]));

    /* partyline */
    tn_client_say(&c[0], "hello from gtetrinet-ng");
    WAIT_FOR(chat_has(&c[1], "<Anna> hello from gtetrinet-ng"), 3000);
    CHECK(chat_has(&c[1], "<Anna> hello from gtetrinet-ng"));

    /* teams: each sees the other's team */
    tn_client_set_team(&c[0], "Red");
    tn_client_set_team(&c[1], "Blue");
    WAIT_FOR(strcmp(c[1].slot[c[0].me].team, "Red") == 0 && strcmp(c[0].slot[c[1].me].team, "Blue") == 0, 3000);
    CHECK(strcmp(c[1].slot[c[0].me].team, "Red") == 0);
    CHECK(strcmp(c[0].slot[c[1].me].team, "Blue") == 0);

    play_game(mod, 1);
    if (!failures) play_game(mod, 2);

    /* a player leaving is seen by the other */
    int gone = c[1].me;
    tn_client_disconnect(&c[1]);
    WAIT_FOR(!c[0].slot[gone].present, 3000);
    CHECK(!c[0].slot[gone].present);
    tn_client_disconnect(&c[0]);

    if (failures) dump("end");
    printf("test_interop %s:%d%s: %d checks, %d failures\n", host, port, fast ? " (TetriFast)" : "", checks, failures);
    return failures ? 1 : 0;
}
