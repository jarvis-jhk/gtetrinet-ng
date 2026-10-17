/*
 * gtetrinet-ng-server — standalone TetriNET server (the same one the game embeds).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_server.h"
#include "tn_query.h"
#include "version.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
static unsigned now_ms(void) { return (unsigned)GetTickCount(); }
#else
#include <time.h>
static void sleep_ms(int ms)
{
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}
static unsigned now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned)((unsigned long long)t.tv_sec * 1000 + (unsigned long long)t.tv_nsec / 1000000);
}
#endif

static volatile sig_atomic_t stop;
static void on_signal(int sig) { (void)sig; stop = 1; }

static void usage(void)
{
    printf("%s-server %s\n"
           "usage: gtetrinet-ng-server [--port N] [--slots 6|7|8] [--local] [--tn2] [--quiet]\n"
           "                           [--pure] [--special-lines N] [--garbage N] [--pieces MIX]\n"
           "       gtetrinet-ng-server --query HOST[:PORT]\n"
           "  --port N    listen on N (default %d)\n"
           "  --slots N   player slots; classic clients only show 6 (default 6)\n"
           "  --local     only accept connections from this machine\n"
           "  --tn2       TetriNET 2 rules (hold piece, 14 specials); gtetrinet-ng clients only\n"
           "  --pure      no specials at all\n"
           "  --special-lines N  lines to clear for each special (default 1)\n"
           "  --garbage N rows of garbage everyone starts with (default 0, at most 21)\n"
           "  --pieces MIX  classic, nosz (no S/Z pieces) or bars (more long bars)\n"
           "  --query     ask a running server who is there, as the game's server list does\n",
           TN_APP_NAME, TN_VERSION, TN_PORT);
}

/* Exit status: 0 answered the query protocol, 3 reachable without it, 1 not reachable. */
static int query(const char *addr)
{
    char host[128];
    int port;
    if (tn_query_split(addr, host, sizeof host, &port) != 0) { usage(); return 2; }
    tn_query_result r;
    if (!tn_query(host, port, 4000, &r)) { printf("%s: not reachable: %s\n", addr, r.err); return 1; }
    if (!r.answered) { printf("%s: reachable in %d ms, does not answer queries\n", addr, r.ms); return 3; }
    printf("%s: reachable in %d ms, %s, %d logged in, %d channel(s)\n", addr, r.ms, r.version[0] ? r.version : "no version", r.players, r.nchan);
    for (int i = 0; i < r.nchan && i < TN_QUERY_CHANNELS; i++)
        printf("  #%s %d/%d status %d \"%s\"\n", r.chan[i].name, r.chan[i].players, r.chan[i].max, r.chan[i].status, r.chan[i].desc);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--query")) return query(argv[2]);
    int port = TN_PORT, slots = TN_CLASSIC_SLOTS, local = 0, quiet = 0, tn2 = 0;
    int pure = 0, special_lines = 1, garbage = 0, pieces = TN_PIECES_CLASSIC;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--slots") && i + 1 < argc) slots = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--local")) local = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--tn2")) tn2 = 1;
        else if (!strcmp(argv[i], "--pure")) pure = 1;
        else if (!strcmp(argv[i], "--special-lines") && i + 1 < argc && atoi(argv[i + 1]) >= 1 && atoi(argv[i + 1]) <= 100)
            special_lines = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--garbage") && i + 1 < argc && atoi(argv[i + 1]) >= 0 && atoi(argv[i + 1]) < TN_FIELD_H)
            garbage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pieces") && i + 1 < argc && tn_piece_mix_parse(argv[i + 1]) >= 0)
            pieces = tn_piece_mix_parse(argv[++i]);
        else { usage(); return strcmp(argv[i], "--help") ? 2 : 0; }
    }
    if (tn_net_init() != 0) { fprintf(stderr, "network init failed\n"); return 1; }
    tn_server s;
    char err[160] = "";
    if (tn_server_start(&s, port, local, slots, err, sizeof err) != 0) {
        fprintf(stderr, "cannot start: %s\n", err);
        return 1;
    }
    s.log = !quiet;
    if (tn2) tn_rules_tn2(&s.rules);
    if (pure) s.rules.specials_added = 0;
    s.rules.lines_per_special = special_lines;
    s.rules.stack = garbage;
    tn_rules_pieces(&s.rules, pieces);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    if (!quiet) fprintf(stderr, "[server] %s %s listening on port %d, %d slots\n", TN_APP_NAME, TN_VERSION, s.port, s.max_slots);
    /* real elapsed time: the login timeout and the message rate limit count in it */
    unsigned last = now_ms();
    while (!stop) {
        unsigned t = now_ms(), dt = t - last; /* unsigned: survives the counter wrapping */
        tn_server_update(&s, (int)(dt > 1000 ? 1000 : dt));
        last = t;
        sleep_ms(10);
    }
    tn_server_stop(&s);
    return 0;
}
