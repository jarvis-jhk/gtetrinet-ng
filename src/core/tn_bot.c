/*
 * gtetrinet-ng — a simple computer player.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_bot.h"

#include <string.h>

void tn_bot_init(tn_bot *b, int think_ms)
{
    memset(b, 0, sizeof *b);
    b->think_ms = think_ms;
    b->last_block = TN_NO_BLOCK;
}

static int fits(const tn_field f, int block, int orient, int bx, int by)
{
    const char (*sh)[4] = tn_block_shape(block, orient);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            if (!sh[y][x]) continue;
            int fx = bx + x, fy = by + y;
            if (fx < 0 || fx >= TN_FIELD_W || fy < 0 || fy >= TN_FIELD_H || f[fy][fx]) return 0;
        }
    return 1;
}

static long score_field(const tn_field f)
{
    int heights[TN_FIELD_W], holes = 0, agg = 0, bump = 0, lines = 0;
    for (int y = 0; y < TN_FIELD_H; y++) {
        int full = 1;
        for (int x = 0; x < TN_FIELD_W; x++) if (!f[y][x]) full = 0;
        lines += full;
    }
    for (int x = 0; x < TN_FIELD_W; x++) {
        int y = 0;
        while (y < TN_FIELD_H && !f[y][x]) y++;
        heights[x] = TN_FIELD_H - y;
        agg += heights[x];
        for (; y < TN_FIELD_H; y++) if (!f[y][x]) holes++;
    }
    for (int x = 0; x + 1 < TN_FIELD_W; x++) bump += heights[x] > heights[x + 1] ? heights[x] - heights[x + 1] : heights[x + 1] - heights[x];
    /* weights after the well-known El-Tetris style tuning */
    return -510L * agg + 760L * lines - 357L * holes - 184L * bump;
}

static void choose(tn_bot *b, const tn_client *c)
{
    const tn_game *g = &c->game;
    long best = -0x7fffffffL;
    b->have_goal = 0;
    for (int o = 0; o < tn_block_orients(g->block); o++)
        for (int x = -3; x < TN_FIELD_W; x++) {
            if (!fits(g->field, g->block, o, x, 0)) continue;
            int y = 0;
            while (fits(g->field, g->block, o, x, y + 1)) y++;
            tn_field t;
            memcpy(t, g->field, sizeof t);
            const char (*sh)[4] = tn_block_shape(g->block, o);
            for (int r = 0; r < 4; r++)
                for (int q = 0; q < 4; q++)
                    if (sh[r][q]) t[y + r][x + q] = sh[r][q];
            long s = score_field(t);
            if (s > best) { best = s; b->goal_x = x; b->goal_orient = o; b->have_goal = 1; }
        }
}

static int stack_height(const tn_field f)
{
    for (int y = 0; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++)
            if (f[y][x]) return TN_FIELD_H - y;
    return 0;
}

static void use_special(tn_client *c)
{
    int sb = tn_sb_from_cell(c->game.specials[0]);
    int good = sb == TN_SB_CLEARLINE || sb == TN_SB_NUKE || sb == TN_SB_GRAVITY || sb == TN_SB_SWITCH;
    if (good) {
        /* helpful ones on ourselves when the stack is high, switch onto the lowest opponent */
        if (sb == TN_SB_SWITCH) {
            int best = 0, low = TN_FIELD_H + 1;
            for (int i = 1; i <= TN_MAX_SLOTS; i++)
                if (i != c->me && c->slot[i].playing && stack_height((const char (*)[TN_FIELD_W])c->slot[i].field) < low) {
                    low = stack_height((const char (*)[TN_FIELD_W])c->slot[i].field);
                    best = i;
                }
            if (best && low + 4 < stack_height(c->game.field)) tn_client_special(c, best);
            else if (c->game.nspecials > 4) tn_client_action(c, TN_ACT_DISCARD);
            return;
        }
        if (stack_height(c->game.field) > 8 || c->game.nspecials > 4) tn_client_special(c, c->me);
        return;
    }
    /* harmful ones on the opponent with the highest stack */
    int best = 0, high = -1;
    for (int i = 1; i <= TN_MAX_SLOTS; i++)
        if (i != c->me && c->slot[i].playing && stack_height((const char (*)[TN_FIELD_W])c->slot[i].field) > high) {
            high = stack_height((const char (*)[TN_FIELD_W])c->slot[i].field);
            best = i;
        }
    if (best) tn_client_special(c, best);
    else tn_client_action(c, TN_ACT_DISCARD);
}

void tn_bot_update(tn_bot *b, tn_client *c, int dt_ms)
{
    if (!c->playing || c->paused) return;
    b->timer -= dt_ms;
    if (b->timer > 0) return;
    b->timer = b->think_ms;
    if (c->game.nspecials > 0) { use_special(c); return; }
    tn_game *g = &c->game;
    if (g->block == TN_NO_BLOCK || c->spawn_wait > 0) { b->have_goal = 0; b->last_block = TN_NO_BLOCK; return; }
    /* a new piece (or the field changed under it): plan again */
    if (b->last_block != g->block || g->by < b->last_by || !b->have_goal) {
        choose(b, c);
        b->last_block = g->block;
    }
    b->last_by = g->by;
    if (!b->have_goal) { tn_client_action(c, TN_ACT_DROP); return; }
    if (g->orient != b->goal_orient) {
        int o = g->orient;
        tn_client_action(c, TN_ACT_ROT_CW);
        if (g->orient == o) { tn_client_action(c, TN_ACT_DROP); b->have_goal = 0; }
        return;
    }
    if (g->bx < b->goal_x) { int x = g->bx; tn_client_action(c, TN_ACT_RIGHT); if (g->bx == x) b->have_goal = 0; return; }
    if (g->bx > b->goal_x) { int x = g->bx; tn_client_action(c, TN_ACT_LEFT); if (g->bx == x) b->have_goal = 0; return; }
    tn_client_action(c, TN_ACT_DROP);
    b->have_goal = 0;
}
