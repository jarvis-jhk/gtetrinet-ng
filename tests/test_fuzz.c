/*
 * gtetrinet-ng — fuzzing and limits: every parser gets mangled input, and a real server on
 * this machine is attacked by hostile peers while honest players keep using it.
 * Run under AddressSanitizer/UBSan (CI does), where a bad read or overflow fails the test.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_client.h"
#include "tn_server.h"
#include "tn_query.h"

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

static uint32_t rng = 20261016u;
static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/* Valid messages of every kind, both directions: the seeds that get mangled. */
static const char *corpus[] = {
    "playernum 1", "playernum 8", ")#)(!@(*3 2", "playerjoin 2 Anna", "playerleave 2", "kick 3",
    "team 1 Rot", "team 2 ", "pline 0 Welcome", "pline 3 hello there", "plineact 2 waves",
    "playerlost 2", "playerwon 1", "ingame", "pause 1", "pause 0", "endgame",
    "newgame 0 1 2 1 1 1 18 11111111111111122222222222222333333333333334444444444444455555555555555666666666666667777777777777777 1111111111111111111111111111111122222222222222222234444444444455566666666666666788888899999999999999 0 1",
    "newgame 0 1 2 1 1 1 18 1234567 123456789 0 1 tn2 1 123456789ABCDE",
    "******* 3 50 2 1 1 1 18 1234567 12 1 0", "btrixnewgame 0 1",
    "f 2 000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
    "f 3 !34\"56#78/??", "f 1 0123acnrsbgqoivmdf", "sb 0 cs1 2", "sb 2 a 1", "sb 3 s 1", "sb 1 i 2",
    "sb 4 m 3", "sb 0 cs4 1", "lvl 0 0", "lvl 2 17", "gmsg <Anna> hi", "winlist pAnna;3 tRot;1",
    "noconnecting Server is Full!", "startgame 1 1", "startgame 0 1", "clientinfo x 1", "version",
    "playerquery", "listchan", "listuser", "Number of players logged in: 3",
    "\"#tetrinet\" \"desc\" 3 6 0 1", "+OK",
};
#define NCORPUS (int)(sizeof corpus / sizeof *corpus)

static const char *nasty[] = {"-1", "0", "9", "-2147483648", "2147483647", "99999999999999999999",
                              "999", "\x7f", "\xc3\xa4", "\x80", "\xfe", " ", "  ", "!", "/start", ";;;",
                              "\"", "\"\"", "tn2", "%s%n", "\t", "\r"};

/* A mangled copy of a corpus message; never contains NUL, 0xFF or '\n' (they end a message). */
static void mutate(char *out, size_t outsz)
{
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", corpus[rnd() % NCORPUS]);
    int edits = 1 + (int)(rnd() % 6);
    for (int e = 0; e < edits; e++) {
        size_t n = strlen(buf);
        size_t at = n ? rnd() % (n + 1) : 0;
        switch (rnd() % 7) {
        case 0: if (at < n) buf[at] = (char)(1 + rnd() % 254); break;            /* any byte */
        case 1: buf[at] = 0; break;                                               /* truncate */
        case 2: {                                                                 /* insert a nasty word */
            const char *w = nasty[rnd() % (sizeof nasty / sizeof *nasty)];
            size_t wl = strlen(w);
            if (n + wl + 1 < sizeof buf) { memmove(buf + at + wl, buf + at, n - at + 1); memcpy(buf + at, w, wl); }
            break;
        }
        case 3: {                                                                 /* a long run */
            size_t len = rnd() % 3000;
            char ch = (char)(" 0a!3\"9z"[rnd() % 8]);
            if (n + len + 1 < sizeof buf) { memmove(buf + at + len, buf + at, n - at + 1); memset(buf + at, ch, len); }
            break;
        }
        case 4: if (at < n) memmove(buf + at, buf + at + 1, n - at); break;      /* delete */
        case 5: {                                                                 /* splice another message */
            const char *o = corpus[rnd() % NCORPUS];
            snprintf(buf + at, sizeof buf - at, "%s", o + rnd() % (strlen(o) + 1));
            break;
        }
        default: if (n && at < n) buf[at] = (char)('0' + rnd() % 10); break;
        }
    }
    for (char *p = buf; *p; p++)
        if ((unsigned char)*p == 0xFF || *p == '\n') *p = '?';
    snprintf(out, outsz, "%s", buf);
}

static void random_bytes(char *out, size_t len)
{
    for (size_t i = 0; i < len; i++) out[i] = (char)(1 + rnd() % 254);
    out[len] = 0;
}

/* Every parser, half a million times between them. */
static void test_parsers(void)
{
    char m[4096], out[512], w[64], host[128];
    tn_client c;
    tn_client_init(&c, "Fuzz", "Team", 1);
    tn_client_handle(&c, "playernum 1");
    for (int it = 0; it < 120000; it++) {
        if (rnd() % 8 == 0) { size_t len = rnd() % 600; random_bytes(m, len); }
        else mutate(m, sizeof m);

        tn_login_decode(m, out, sizeof out);
        tn_login_decode(m, out, 1 + rnd() % 40);
        tn_word(m, w, 1 + rnd() % sizeof w);

        tn_field f;
        memset(f, (int)(rnd() % TN_CELLS), sizeof f);
        tn_field_apply(f, m);
        for (int k = 0; k < TN_FIELD_W * TN_FIELD_H; k++) CHECK((&f[0][0])[k] >= 0 && (&f[0][0])[k] < TN_CELLS);

        tn_rules r;
        tn_rules_parse(&r, m);
        if (it % 64 == 0) {
            /* whatever the rules say, a game can be played with them */
            char fmt[512];
            tn_rules_format(&r, fmt, sizeof fmt);
            tn_game g;
            tn_game_start(&g, &r, rnd());
            for (int p = 0; p < 30 && tn_game_spawn(&g) == 0; p++) {
                tn_game_rotate(&g, 1);
                tn_game_move(&g, (int)(rnd() % 3) - 1);
                tn_game_drop(&g);
                int lc;
                tn_game_solidify(&g);
                tn_game_clear_lines(&g, &lc);
                if (p % 5 == 0) tn_game_apply_special(&g, (int)(rnd() % TN_SB_COUNT), f);
            }
        }

        int players;
        tn_query_channel ch;
        int port;
        tn_query_parse_players(m, &players);
        tn_query_parse_channel(m, &ch);
        tn_query_split(m, host, 1 + rnd() % sizeof host, &port);

        tn_client_handle(&c, m);
        if (c.state == TN_FAILED) { tn_client_init(&c, "Fuzz", "Team", (uint32_t)it); tn_client_handle(&c, "playernum 1"); }
        if (it % 16 == 0) {
            tn_client_update(&c, (int)(rnd() % 400));
            tn_client_action(&c, (enum tn_action)(rnd() % (TN_ACT_HOLD + 1)));
            tn_client_special(&c, (int)(rnd() % 12) - 2);
            tn_field v;
            tn_client_view(&c, v);
            CHECK(tn_client_gravity_level(&c) >= 0 && tn_client_gravity_level(&c) <= 100);
        }
        if (it % 5000 == 0) tn_client_handle(&c, corpus[17]); /* back into a game now and then */
    }
    tn_conn_close(&c.conn);
    CHECK(checks > 0);
}

/* A peer that nobody reads from fills its queue up to the limit and is then dead. */
static void test_output_limit(void)
{
    tn_conn c;
    tn_conn_init(&c, TN_BAD_SOCKET);
    c.outmax = 64 * 1024;
    char line[1000];
    memset(line, 'x', sizeof line - 1);
    line[sizeof line - 1] = 0;
    int sent = 0;
    while (!c.dead && sent < 1000) { tn_conn_send(&c, line); sent++; }
    CHECK(c.dead && strstr(c.err, "overflow") != NULL);
    CHECK(sent > 30 && sent < 100);
    CHECK(c.outlen <= c.outmax);
    tn_conn_close(&c);
}

static void step(tn_server *s, tn_client *c, int n)
{
    tn_server_update(s, 2);
    for (int i = 0; i < n; i++) tn_client_update(&c[i], 2);
    sleep_ms(1);
}

/* A raw TCP peer that says exactly what the test wants. */
static int raw_open(tn_server *s, tn_client *honest, int nh, const char *host, tn_conn *out)
{
    tn_connector *k = tn_connector_start(host, s->port);
    tn_socket sock = TN_BAD_SOCKET;
    char err[160];
    int r = 0;
    for (int t = 0; t < 2000 && (r = tn_connector_poll(k, &sock, err, sizeof err)) == 0; t++) step(s, honest, nh);
    tn_connector_free(k);
    if (r != 1) return 0;
    tn_conn_init(out, sock);
    return 1;
}

static int raw_login(tn_server *s, tn_client *honest, int nh, tn_conn *cn, const char *nick)
{
    char plain[64], enc[256], msg[TN_MSG_MAX];
    unsigned char ip[4] = {127, 0, 0, 1};
    if (!raw_open(s, honest, nh, "127.0.0.1", cn)) return 0;
    snprintf(plain, sizeof plain, "tetrisstart %s 1.13", nick);
    tn_login_encode(plain, ip, 0, enc, sizeof enc);
    tn_conn_send(cn, enc);
    for (int t = 0; t < 500; t++) {
        step(s, honest, nh);
        tn_conn_pump(cn);
        while (tn_conn_next(cn, msg, sizeof msg))
            if (!strncmp(msg, "playernum ", 10)) return atoi(msg + 10);
    }
    return 0;
}

static int pending_count(const tn_server *s)
{
    int n = 0;
    for (int k = 0; k < TN_SERVER_PENDING; k++) n += s->pending[k].used;
    return n;
}

static int chat_has(const tn_client *c, const char *text)
{
    for (int i = 0; i < c->chat.count; i++)
        if (strstr(tn_log_get(&c->chat, i), text)) return 1;
    return 0;
}

static void test_hostile_server(void)
{
    tn_server s;
    char err[160] = "", msg[TN_MSG_MAX];
    /* every interface (IPv4 and IPv6), so ::1 counts as a second address below */
    CHECK(tn_server_start(&s, 0, 0, 8, err, sizeof err) == 0);
    tn_client h[2];
    tn_client_init(&h[0], "Honest", "", 1);
    tn_client_init(&h[1], "Fair", "", 2);
    tn_client_connect(&h[0], "127.0.0.1", s.port, 0);
    for (int t = 0; t < 2000 && h[0].state != TN_CONNECTED; t++) step(&s, h, 1);
    tn_client_connect(&h[1], "127.0.0.1", s.port, 0);
    for (int t = 0; t < 2000 && h[1].state != TN_CONNECTED; t++) step(&s, h, 2);
    CHECK(h[0].state == TN_CONNECTED && h[1].state == TN_CONNECTED);

    /* 1. a logged-in player sends mangled messages and random bytes */
    tn_conn evil;
    int slot = raw_login(&s, h, 2, &evil, "Evil");
    CHECK(slot >= 3);
    char line[4096];
    for (int burst = 0; burst < 400 && !evil.dead; burst++) {
        for (int i = 0; i < 20; i++) {
            if (rnd() % 4 == 0) { random_bytes(line, rnd() % 300); }
            else {
                char m[4000];
                mutate(m, sizeof m);
                /* own slot most of the time, so the message gets past the slot check */
                if (rnd() % 3) {
                    char cmd[32];
                    const char *rest = tn_word(m, cmd, sizeof cmd);
                    char skip[16];
                    rest = tn_word(rest, skip, sizeof skip); /* their slot word */
                    snprintf(line, sizeof line, "%s %d %s", cmd, slot, rest);
                } else {
                    snprintf(line, sizeof line, "%s", m);
                }
            }
            /* no /start: the honest moderator decides when to play */
            for (char *p = line; *p; p++) if (*p == '/') *p = '.';
            tn_conn_send(&evil, line);
        }
        tn_conn_pump(&evil);
        while (tn_conn_next(&evil, msg, sizeof msg)) {}
        step(&s, h, 2);
    }
    for (int t = 0; t < 600; t++) { step(&s, h, 2); tn_conn_pump(&evil); while (tn_conn_next(&evil, msg, sizeof msg)) {} }
    CHECK(h[0].state == TN_CONNECTED && h[1].state == TN_CONNECTED);
    /* what is still queued from it goes with the connection */
    tn_conn_close(&evil);
    for (int t = 0; t < 100 && s.p[slot].used; t++) step(&s, h, 2);
    CHECK(!s.p[slot].used);
    slot = raw_login(&s, h, 2, &evil, "Flood");
    CHECK(slot >= 3);
    for (int t = 0; t < 1000; t++) { step(&s, h, 2); tn_conn_pump(&evil); while (tn_conn_next(&evil, msg, sizeof msg)) {} }

    /* 2. a chat flood is read at the rate limit, not as fast as it arrives */
    tn_client_say(&h[0], "before the flood");
    for (int t = 0; t < 200; t++) step(&s, h, 2);
    unsigned seen0 = h[1].chat.serial;
    for (int i = 0; i < 600; i++) {   /* about 11 KiB: throttled, below the flood limit */
        snprintf(line, sizeof line, "pline %d flood %d", slot, i);
        tn_conn_send(&evil, line);
    }
    tn_conn_pump(&evil);
    int rounds = 0;
    for (; rounds < 500; rounds++) {   /* 1 s of server time at 2 ms per update */
        step(&s, h, 2);
        tn_conn_pump(&evil);
        while (tn_conn_next(&evil, msg, sizeof msg)) {}
    }
    unsigned got = h[1].chat.serial - seen0;
    fprintf(stderr, "flood: %u of 600 lines relayed in 1 s of server time\n", got);
    CHECK(got >= TN_SERVER_MSG_BURST && got <= TN_SERVER_MSG_BURST + TN_SERVER_MSG_RATE + 20);
    CHECK(s.p[slot].used); /* throttled, not thrown out */
    /* honest chat still gets through while the flood waits in its queue */
    tn_client_say(&h[0], "still here");
    for (int t = 0; t < 300 && !chat_has(&h[1], "still here"); t++) { step(&s, h, 2); tn_conn_pump(&evil); while (tn_conn_next(&evil, msg, sizeof msg)) {} }
    CHECK(chat_has(&h[1], "still here"));

    /* 3. a line without an end, longer than any message: that peer is dropped */
    memset(line, 'A', sizeof line - 1);
    line[sizeof line - 1] = 0;
    for (int i = 0; i < 6 && !evil.dead; i++) {
        size_t n = strlen(line);
        if (evil.outlen + n > evil.outcap) {
            char *p = realloc(evil.out, evil.outlen + n + 1);
            if (!p) break;
            evil.out = p;
            evil.outcap = evil.outlen + n + 1;
        }
        memcpy(evil.out + evil.outlen, line, n); /* raw bytes, no terminator */
        evil.outlen += n;
    }
    for (int t = 0; t < 4000 && s.p[slot].used; t++) { step(&s, h, 2); tn_conn_pump(&evil); while (tn_conn_next(&evil, msg, sizeof msg)) {} }
    CHECK(!s.p[slot].used);
    tn_conn_close(&evil);

    /* 4. connection spam from one address fills its share of the waiting room, no more */
    tn_conn spam[40];
    int opened = 0;
    for (int i = 0; i < 40; i++) opened += raw_open(&s, h, 2, "127.0.0.1", &spam[opened]) ;
    for (int t = 0; t < 50; t++) step(&s, h, 2);
    CHECK(opened == 40);
    CHECK(pending_count(&s) <= TN_SERVER_PER_ADDR);
    /* a player from another address still gets in (::1, where this machine has IPv6) */
    tn_client six;
    tn_client_init(&six, "Six", "", 3);
    tn_client_connect(&six, "::1", s.port, 0);
    for (int t = 0; t < 2000 && six.state != TN_CONNECTED && six.state != TN_FAILED; t++) { step(&s, h, 2); tn_client_update(&six, 2); }
    if (six.state == TN_CONNECTED) CHECK(six.me >= 1);
    else fprintf(stderr, "no IPv6 here: skipping the second-address check (%s)\n", six.err);
    tn_client_disconnect(&six);
    for (int i = 0; i < opened; i++) tn_conn_close(&spam[i]);
    for (int t = 0; t < 50; t++) step(&s, h, 2);
    CHECK(pending_count(&s) == 0);

    /* 5. a query client gets its answers up to the limit, then the door */
    tn_conn q;
    CHECK(raw_open(&s, h, 2, "127.0.0.1", &q));
    for (int i = 0; i < 200; i++) tn_conn_send_line(&q, "playerquery");
    int answers = 0;
    for (int t = 0; t < 1000 && !q.dead; t++) {
        step(&s, h, 2);
        tn_conn_pump(&q);
        while (tn_conn_next(&q, msg, sizeof msg)) answers += !strncmp(msg, "Number of players", 17);
    }
    CHECK(q.dead);
    CHECK(answers > 0 && answers <= TN_SERVER_QUERIES);
    tn_conn_close(&q);

    /* after all that, the honest players play a game */
    CHECK(h[0].state == TN_CONNECTED && h[1].state == TN_CONNECTED);
    CHECK(tn_client_moderator(&h[0]));
    tn_client_start_game(&h[0]);
    for (int t = 0; t < 300 && !(h[0].ingame && h[1].ingame); t++) step(&s, h, 2);
    CHECK(h[0].ingame && h[1].ingame && h[0].playing && h[1].playing);
    for (int t = 0; t < 300; t++) { if (t % 10 == 0) tn_client_action(&h[t % 20 ? 0 : 1], TN_ACT_DROP); step(&s, h, 2); }
    CHECK(h[0].state == TN_CONNECTED && h[1].state == TN_CONNECTED);
    tn_client_disconnect(&h[0]);
    tn_client_disconnect(&h[1]);
    tn_server_stop(&s);
}

int main(void)
{
    if (tn_net_init() != 0) { fprintf(stderr, "network init failed\n"); return 1; }
    test_output_limit();
    test_parsers();
    test_hostile_server();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
