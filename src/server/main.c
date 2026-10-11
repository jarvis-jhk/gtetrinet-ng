/*
 * gtetrinet-ng-server — standalone TetriNET server (the same one the game embeds).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_server.h"
#include "version.h"

#include <signal.h>
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
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}
#endif

static volatile sig_atomic_t stop;
static void on_signal(int sig) { (void)sig; stop = 1; }

static void usage(void)
{
    printf("%s-server %s\n"
           "usage: gtetrinet-ng-server [--port N] [--slots 6|7|8] [--local] [--quiet]\n"
           "  --port N    listen on N (default %d)\n"
           "  --slots N   player slots; classic clients only show 6 (default 6)\n"
           "  --local     only accept connections from this machine\n",
           TN_APP_NAME, TN_VERSION, TN_PORT);
}

int main(int argc, char **argv)
{
    int port = TN_PORT, slots = TN_CLASSIC_SLOTS, local = 0, quiet = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--slots") && i + 1 < argc) slots = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--local")) local = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
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
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    if (!quiet) fprintf(stderr, "[server] %s %s listening on port %d, %d slots\n", TN_APP_NAME, TN_VERSION, s.port, s.max_slots);
    while (!stop) {
        tn_server_update(&s, 10);
        sleep_ms(10);
    }
    tn_server_stop(&s);
    return 0;
}
