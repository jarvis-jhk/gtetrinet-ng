/*
 * gtetrinet-ng — the server list: servers played on recently and known public ones, each
 * asked in the background who is there (the TetriNET query protocol, src/core/tn_query.c).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "app.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

/* Public servers that answered in October 2026. GTN_SERVER_LIST (comma separated, may be
 * empty) replaces them, so tests never depend on the internet. */
static const char *const public_servers = "tetrinet.de,tetrinet.fr,tetrinet.us";

#define PROBE_TIMEOUT_MS 4000

/* Owned by the thread and the list together; whoever lets go last frees it, so closing the
 * list never waits for a slow server. */
typedef struct gtn_probe {
    char host[128];
    int port;
    tn_query_result res;
    SDL_atomic_t refs, done;
} gtn_probe;

static void let_go(gtn_probe *p)
{
    if (p && SDL_AtomicDecRef(&p->refs)) free(p);
}

static int probe_thread(void *data)
{
    gtn_probe *p = data;
    tn_query(p->host, p->port, PROBE_TIMEOUT_MS, &p->res);
    SDL_AtomicSet(&p->done, 1);
    let_go(p);
    return 0;
}

static void ask(gtn_server_entry *e)
{
    let_go(e->probe);
    e->probe = NULL;
    memset(&e->res, 0, sizeof e->res);
    gtn_probe *p = calloc(1, sizeof *p);
    if (!p || tn_query_split(e->addr, p->host, sizeof p->host, &p->port) != 0) {
        free(p);
        e->state = GTN_PROBE_DONE;
        snprintf(e->res.err, sizeof e->res.err, "not a server address");
        return;
    }
    SDL_AtomicSet(&p->refs, 2);
    SDL_Thread *t = SDL_CreateThread(probe_thread, "server-query", p);
    if (!t) {
        free(p);
        e->state = GTN_PROBE_DONE;
        snprintf(e->res.err, sizeof e->res.err, "%s", SDL_GetError());
        return;
    }
    SDL_DetachThread(t);
    e->probe = p;
    e->state = GTN_PROBE_ASKING;
}

static void add(gtn_browser *b, const char *addr, int known)
{
    if (!addr[0] || b->n >= GTN_BROWSER_MAX) return;
    for (int i = 0; i < b->n; i++)
        if (!SDL_strcasecmp(b->e[i].addr, addr)) return;
    gtn_server_entry *e = &b->e[b->n++];
    memset(e, 0, sizeof *e);
    snprintf(e->addr, sizeof e->addr, "%s", addr);
    e->known = known;
}

void gtn_browser_open(gtn_app *a)
{
    gtn_browser *b = &a->browser;
    gtn_browser_close(a);
    for (int i = 0; i < GTN_RECENT; i++) add(b, a->settings.recent[i], 0);
    const char *list = getenv("GTN_SERVER_LIST");
    if (!list) list = public_servers;
    while (*list) {
        size_t n = strcspn(list, ", ");
        char addr[128];
        snprintf(addr, sizeof addr, "%.*s", (int)(n < sizeof addr ? n : sizeof addr - 1), list);
        add(b, addr, 1);
        list += n;
        list += strspn(list, ", ");
    }
    b->sel = 0;
    gtn_browser_refresh(a);
}

void gtn_browser_refresh(gtn_app *a)
{
    for (int i = 0; i < a->browser.n; i++) ask(&a->browser.e[i]);
}

void gtn_browser_update(gtn_app *a)
{
    for (int i = 0; i < a->browser.n; i++) {
        gtn_server_entry *e = &a->browser.e[i];
        if (e->state != GTN_PROBE_ASKING || !SDL_AtomicGet(&e->probe->done)) continue;
        e->res = e->probe->res;
        let_go(e->probe);
        e->probe = NULL;
        e->state = GTN_PROBE_DONE;
    }
}

int gtn_browser_pending(const gtn_app *a)
{
    int n = 0;
    for (int i = 0; i < a->browser.n; i++) n += a->browser.e[i].state == GTN_PROBE_ASKING;
    return n;
}

void gtn_browser_close(gtn_app *a)
{
    for (int i = 0; i < a->browser.n; i++) let_go(a->browser.e[i].probe);
    memset(&a->browser, 0, sizeof a->browser);
}
