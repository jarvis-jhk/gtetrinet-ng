/*
 * gtetrinet-ng — the TetriNET game rules for one player, without any I/O.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 1999-2003 Ka-shu Wong and the GTetrinet contributors (original rules)
 * Copyright (C) 2026 gtetrinet-ng contributors
 */
#include "tn_game.h"

#include <string.h>

typedef char shape[4][4];

static const shape b1[2] = {
    {{1,1,1,1},{0,0,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}}};
static const shape b2[1] = {
    {{0,2,2,0},{0,2,2,0},{0,0,0,0},{0,0,0,0}}};
static const shape b3[4] = {
    {{0,0,3,0},{0,0,3,0},{0,3,3,0},{0,0,0,0}},
    {{0,3,0,0},{0,3,3,3},{0,0,0,0},{0,0,0,0}},
    {{0,3,3,0},{0,3,0,0},{0,3,0,0},{0,0,0,0}},
    {{0,3,3,3},{0,0,0,3},{0,0,0,0},{0,0,0,0}}};
static const shape b4[4] = {
    {{0,4,0,0},{0,4,0,0},{0,4,4,0},{0,0,0,0}},
    {{0,4,4,4},{0,4,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,4,4,0},{0,0,4,0},{0,0,4,0},{0,0,0,0}},
    {{0,0,0,4},{0,4,4,4},{0,0,0,0},{0,0,0,0}}};
static const shape b5[2] = {
    {{0,0,5,0},{0,5,5,0},{0,5,0,0},{0,0,0,0}},
    {{0,5,5,0},{0,0,5,5},{0,0,0,0},{0,0,0,0}}};
static const shape b6[2] = {
    {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
    {{0,0,1,1},{0,1,1,0},{0,0,0,0},{0,0,0,0}}};
static const shape b7[4] = {
    {{0,0,2,0},{0,2,2,0},{0,0,2,0},{0,0,0,0}},
    {{0,0,2,0},{0,2,2,2},{0,0,0,0},{0,0,0,0}},
    {{0,2,0,0},{0,2,2,0},{0,2,0,0},{0,0,0,0}},
    {{0,2,2,2},{0,0,2,0},{0,0,0,0},{0,0,0,0}}};

static const shape *blocks[7] = {b1, b2, b3, b4, b5, b6, b7};
static const int blockcount[7] = {2, 1, 4, 4, 2, 2, 4};

void tn_rng_seed(tn_rng *r, uint32_t seed) { r->s = seed ? seed : 0x9E3779B9u; }

int tn_rng_int(tn_rng *r, int n)
{
    uint32_t x = r->s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    r->s = x;
    return n > 0 ? (int)(x % (uint32_t)n) : 0;
}

int tn_block_orients(int block) { return block >= 0 && block < 7 ? blockcount[block] : 1; }
const char (*tn_block_shape(int block, int orient))[4] { return blocks[block][orient]; }

int tn_gravity_ms(int level) { return level <= 100 ? 1005 - level * 10 : 5; }

const char (*tn_game_shape(const tn_game *g, int block, int orient))[4]
{
    if (block >= TN_MUTANT_CUR && block < TN_BLOCK_KINDS) return g->mutant[block - TN_MUTANT_CUR][orient & 3];
    if (block < 0 || block >= 7) block = 0;
    return blocks[block][orient % blockcount[block]];
}

int tn_game_orients(const tn_game *g, int block)
{
    (void)g;
    return block >= TN_MUTANT_CUR && block < TN_BLOCK_KINDS ? 4 : tn_block_orients(block);
}

/* 0 free, 1 blocked by cells or floor/ceiling, 2 blocked only by a side wall */
static int obstructed_g(const tn_game *g, const tn_field f, int block, int orient, int bx, int by)
{
    int side = 0;
    const char (*sh)[4] = tn_game_shape(g, block, orient);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            if (!sh[y][x]) continue;
            int fx = bx + x, fy = by + y;
            if (fx < 0 || fx >= TN_FIELD_W) { side = 2; continue; }
            if (fy < 0 || fy >= TN_FIELD_H) return 1;
            if (f[fy][fx]) return 1;
        }
    return side;
}

#define obstructed(f, block, orient, bx, by) obstructed_g(g, f, block, orient, bx, by)

static void place_g(const tn_game *g, tn_field f, int block, int orient, int bx, int by)
{
    const char (*sh)[4] = tn_game_shape(g, block, orient);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (sh[y][x]) {
                int fx = bx + x, fy = by + y;
                if (fx >= 0 && fx < TN_FIELD_W && fy >= 0 && fy < TN_FIELD_H)
                    f[fy][fx] = sh[y][x];
            }
}
#define place(f, block, orient, bx, by) place_g(g, f, block, orient, bx, by)

/* A random connected shape of five or six cells inside 4x4, in one colour, with its four
 * rotations; each rotation is moved to the top row and centred like the classic pieces. */
static void make_mutant(tn_game *g, char out[4][4][4])
{
    char m[4][4];
    memset(m, 0, sizeof m);
    int cells = 5 + tn_rng_int(&g->rng, 2), n = 1;
    char colour = (char)(tn_rng_int(&g->rng, 5) + 1);
    m[1][1] = colour;
    while (n < cells) {
        int y = tn_rng_int(&g->rng, 4), x = tn_rng_int(&g->rng, 4);
        if (m[y][x]) continue;
        int touch = (y > 0 && m[y - 1][x]) || (y < 3 && m[y + 1][x]) || (x > 0 && m[y][x - 1]) || (x < 3 && m[y][x + 1]);
        if (!touch) continue;
        m[y][x] = colour;
        n++;
    }
    for (int o = 0; o < 4; o++) {
        char r[4][4];
        int top = 4, left = 4, right = -1;
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++) {
                r[y][x] = m[y][x];
                if (!m[y][x]) continue;
                if (y < top) top = y;
                if (x < left) left = x;
                if (x > right) right = x;
            }
        int dx = (4 - (right - left + 1)) / 2 - left;
        memset(out[o], 0, 16);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                if (r[y][x]) out[o][y - top][x + dx] = r[y][x];
        /* next orientation: a quarter turn clockwise */
        char t[4][4];
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++) t[x][3 - y] = m[y][x];
        memcpy(m, t, sizeof m);
    }
}

/* the next piece: normal, or a mutant while Mutate Pieces lasts */
static void draw_next(tn_game *g)
{
    if (g->mutate_left > 0) {
        g->mutate_left--;
        make_mutant(g, g->mutant[TN_MUTANT_NEXT - TN_MUTANT_CUR]);
        g->next = TN_MUTANT_NEXT;
        g->next_orient = 0;
        return;
    }
    g->next = tn_game_random_block(g);
    g->next_orient = tn_rng_int(&g->rng, blockcount[g->next]);
}

/* moves a piece's block number (and a mutant's shape) to the slot it now occupies */
static int move_block(tn_game *g, int block, int to)
{
    if (block < TN_MUTANT_CUR) return block;
    if (block != to) memcpy(g->mutant[to - TN_MUTANT_CUR], g->mutant[block - TN_MUTANT_CUR], sizeof g->mutant[0]);
    return to;
}

void tn_game_start(tn_game *g, const tn_rules *rules, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->rules = *rules;
    tn_rng_seed(&g->rng, seed);
    g->alive = 1;
    g->block = TN_NO_BLOCK;
    g->hold = TN_NO_BLOCK;
    g->level = rules->level;
    draw_next(g);
    if (rules->stack > 0) tn_game_add_lines(g, rules->stack, 1);
}

int tn_game_random_block(tn_game *g)
{
    int total = 0;
    for (int i = 0; i < 7; i++) total += g->rules.blockfreq[i];
    if (total <= 0) return tn_rng_int(&g->rng, 7);
    int n = tn_rng_int(&g->rng, total);
    for (int i = 0; i < 7; i++) {
        if (n < g->rules.blockfreq[i]) return i;
        n -= g->rules.blockfreq[i];
    }
    return 6;
}

static int random_special_cell(tn_game *g)
{
    int total = 0, kinds = g->rules.tn2 ? TN_SPECIALS : TN_CLASSIC_SPECIALS;
    for (int i = 0; i < kinds; i++) total += g->rules.specialfreq[i];
    int n = tn_rng_int(&g->rng, total > 0 ? total : 9);
    for (int i = 0; i < kinds; i++) {
        int w = total > 0 ? g->rules.specialfreq[i] : 1;
        if (n < w) return 6 + i;
        n -= w;
    }
    return 14;
}

int tn_game_spawn(tn_game *g)
{
    if (!g->alive) return -1;
    g->block = move_block(g, g->next, TN_MUTANT_CUR);
    g->orient = g->next_orient;
    g->bx = TN_FIELD_W / 2 - 2;
    g->by = 0;
    g->hold_used = 0;
    draw_next(g);
    if (obstructed(g->field, g->block, g->orient, g->bx, g->by)) {
        g->block = TN_NO_BLOCK;
        g->alive = 0;
        return -1;
    }
    return 0;
}

int tn_game_down(tn_game *g)
{
    if (g->block == TN_NO_BLOCK) return 0;
    if (obstructed(g->field, g->block, g->orient, g->bx, g->by + 1)) return 1;
    g->by++;
    return 0;
}

void tn_game_move(tn_game *g, int dir)
{
    if (g->block == TN_NO_BLOCK) return;
    if (!obstructed(g->field, g->block, g->orient, g->bx + dir, g->by)) g->bx += dir;
}

void tn_game_rotate(tn_game *g, int dir)
{
    if (g->block == TN_NO_BLOCK) return;
    int n = g->orient + dir, cnt = tn_game_orients(g, g->block);
    if (n >= cnt) n = 0;
    if (n < 0) n = cnt - 1;
    switch (obstructed(g->field, g->block, n, g->bx, g->by)) {
    case 1: return;
    case 2: {
        /* against a wall: shift away from it if that fits, like the original */
        static const int shifts[4] = {1, -1, 2, -2};
        for (int i = 0; i < 4; i++)
            if (!obstructed(g->field, g->block, n, g->bx + shifts[i], g->by)) {
                g->bx += shifts[i];
                g->orient = n;
                return;
            }
        return;
    }
    default: g->orient = n;
    }
}

void tn_game_drop(tn_game *g)
{
    if (g->block == TN_NO_BLOCK) return;
    while (tn_game_down(g) == 0) {}
}

int tn_game_drop_y(const tn_game *g)
{
    if (g->block == TN_NO_BLOCK) return 0;
    int y = g->by;
    while (!obstructed(g->field, g->block, g->orient, g->bx, y + 1)) y++;
    return y;
}

int tn_game_hold(tn_game *g)
{
    if (!g->rules.hold || g->block == TN_NO_BLOCK || g->hold_used) return 1;
    int cur = g->block;
    if (g->hold == TN_NO_BLOCK) {
        g->hold = move_block(g, cur, TN_MUTANT_HOLD);
        g->block = TN_NO_BLOCK;
        if (tn_game_spawn(g) < 0) return -1;
    } else {
        /* the shapes trade places through the spare current-piece slot */
        char spare[4][4][4];
        if (cur == TN_MUTANT_CUR) memcpy(spare, g->mutant[0], sizeof spare);
        g->block = move_block(g, g->hold, TN_MUTANT_CUR);
        g->orient = 0;
        if (cur == TN_MUTANT_CUR) {
            memcpy(g->mutant[TN_MUTANT_HOLD - TN_MUTANT_CUR], spare, sizeof spare);
            g->hold = TN_MUTANT_HOLD;
        } else {
            g->hold = cur;
        }
        g->bx = TN_FIELD_W / 2 - 2;
        g->by = 0;
        if (obstructed(g->field, g->block, g->orient, g->bx, g->by)) {
            g->block = TN_NO_BLOCK;
            g->alive = 0;
            return -1;
        }
    }
    g->hold_used = 1;
    return 0;
}

int tn_game_solidify(tn_game *g)
{
    if (g->block == TN_NO_BLOCK) return 0;
    if (obstructed(g->field, g->block, g->orient, g->bx, g->by)) {
        /* a special moved the field under the piece: lift it until it fits */
        int y = g->by - 1;
        for (; y >= 0; y--)
            if (!obstructed(g->field, g->block, g->orient, g->bx, y)) break;
        if (y < 0) {
            g->block = TN_NO_BLOCK;
            g->alive = 0;
            return -1;
        }
        g->by = y;
    }
    place(g->field, g->block, g->orient, g->bx, g->by);
    g->block = TN_NO_BLOCK;
    return 0;
}

static void add_special_to_inventory(tn_game *g, unsigned char sb)
{
    int cap = g->rules.special_capacity;
    if (cap > (int)sizeof g->specials - 1) cap = (int)sizeof g->specials - 1;
    if (g->nspecials >= cap) return;
    /* inserted at a random place among the ones already held, as the original does */
    int l = tn_rng_int(&g->rng, ++g->nspecials);
    for (int i = g->nspecials - 1; i > l; i--) g->specials[i] = g->specials[i - 1];
    g->specials[l] = sb;
}

static void add_specials_to_field(tn_game *g, int count)
{
    for (int i = 0; i < count; i++) {
        int sb = random_special_cell(g);
        int c = 0;
        for (int y = 0; y < TN_FIELD_H; y++)
            for (int x = 0; x < TN_FIELD_W; x++)
                if (g->field[y][x] > 0 && g->field[y][x] < 6) c++;
        if (c == 0) {
            /* nothing to turn into a special: drop it onto a column instead */
            int n = 0, y = 0, j;
            for (j = 0; j < 20; j++) {
                n = tn_rng_int(&g->rng, TN_FIELD_W);
                for (y = 0; y < TN_FIELD_H; y++)
                    if (g->field[y][n]) break;
                if (y == TN_FIELD_H || g->field[y][n] < 6) break;
            }
            if (j == 20 || y == 0) return;
            g->field[y - 1][n] = (char)sb;
        } else {
            int n = tn_rng_int(&g->rng, c);
            for (int y = 0; y < TN_FIELD_H; y++)
                for (int x = 0; x < TN_FIELD_W; x++)
                    if (g->field[y][x] > 0 && g->field[y][x] < 6 && n-- == 0) {
                        g->field[y][x] = (char)sb;
                        goto placed;
                    }
        placed:;
        }
    }
}

static int remove_full_lines(tn_field f, unsigned char *specials, int *nsp)
{
    int c = 0;
    for (int y = 0; y < TN_FIELD_H; y++) {
        int holes = 0;
        for (int x = 0; x < TN_FIELD_W; x++)
            if (f[y][x] == 0) holes++;
        if (holes) continue;
        c++;
        if (specials)
            for (int x = 0; x < TN_FIELD_W; x++)
                if (f[y][x] > 5 && *nsp < TN_FIELD_W * TN_FIELD_H) specials[(*nsp)++] = (unsigned char)f[y][x];
        for (int i = y - 1; i >= 0; i--) memcpy(f[i + 1], f[i], TN_FIELD_W);
        memset(f[0], 0, TN_FIELD_W);
    }
    return c;
}

int tn_game_clear_lines(tn_game *g, int *level_changed)
{
    unsigned char sp[TN_FIELD_W * TN_FIELD_H];
    int nsp = 0;
    *level_changed = 0;
    int lines = remove_full_lines(g->field, sp, &nsp);
    if (!lines) return 0;
    g->lines += lines;
    g->slines += lines;
    g->llines += lines;
    /* every special in a cleared line is collected once per line cleared, as the original */
    for (int i = 0; i < lines; i++)
        for (int j = 0; j < nsp; j++) add_special_to_inventory(g, sp[j]);
    int lps = g->rules.lines_per_special > 0 ? g->rules.lines_per_special : 1;
    int n = g->slines / lps;
    g->slines %= lps;
    add_specials_to_field(g, g->rules.specials_added * n);
    int lpl = g->rules.lines_per_level > 0 ? g->rules.lines_per_level : 1;
    while (g->llines >= lpl) {
        g->level += g->rules.level_inc;
        g->llines -= lpl;
        *level_changed = 1;
    }
    if (g->level > 100) g->level = 100;
    return lines;
}

int tn_game_add_lines(tn_game *g, int count, int type)
{
    for (int i = 0; i < count; i++) {
        for (int x = 0; x < TN_FIELD_W; x++)
            if (g->field[0][x]) {
                g->alive = 0;
                g->block = TN_NO_BLOCK;
                return -1;
            }
        for (int y = 0; y < TN_FIELD_H - 1; y++) memcpy(g->field[y], g->field[y + 1], TN_FIELD_W);
        char *row = g->field[TN_FIELD_H - 1];
        if (type == 1)
            for (int x = 0; x < TN_FIELD_W; x++) row[x] = (char)tn_rng_int(&g->rng, 6);
        else
            for (int x = 0; x < TN_FIELD_W; x++) row[x] = (char)(tn_rng_int(&g->rng, 5) + 1);
        row[tn_rng_int(&g->rng, TN_FIELD_W)] = 0;
    }
    return 0;
}

static void shift_line(char *row, int d)
{
    char tmp[TN_FIELD_W];
    memset(tmp, 0, sizeof tmp);
    for (int x = 0; x < TN_FIELD_W; x++) {
        int nx = x + d;
        if (nx >= 0 && nx < TN_FIELD_W) tmp[nx] = row[x];
    }
    memcpy(row, tmp, TN_FIELD_W);
}

int tn_game_apply_special(tn_game *g, int sb, const tn_field other)
{
    tn_field *f = &g->field;
    switch (sb) {
    case TN_SB_ADDALL1: if (tn_game_add_lines(g, 1, 2) < 0) return -1; break;
    case TN_SB_ADDALL2: if (tn_game_add_lines(g, 2, 2) < 0) return -1; break;
    case TN_SB_ADDALL4: if (tn_game_add_lines(g, 4, 2) < 0) return -1; break;
    case TN_SB_ADDLINE: if (tn_game_add_lines(g, 1, 1) < 0) return -1; break;
    case TN_SB_CLEARLINE:
        for (int y = TN_FIELD_H - 1; y > 0; y--) memcpy((*f)[y], (*f)[y - 1], TN_FIELD_W);
        memset((*f)[0], 0, TN_FIELD_W);
        break;
    case TN_SB_NUKE:
        memset(*f, 0, sizeof *f);
        break;
    case TN_SB_CLEARRANDOM:
        for (int i = 0; i < 10; i++) {
            int y = tn_rng_int(&g->rng, TN_FIELD_H);
            int x = tn_rng_int(&g->rng, TN_FIELD_W);
            (*f)[y][x] = 0;
        }
        break;
    case TN_SB_SWITCH: {
        if (!other) break;
        tn_field t;
        memcpy(t, other, sizeof t);
        /* keep the top six rows free so the switch does not kill outright */
        int y = 0;
        for (; y < 6; y++) {
            int any = 0;
            for (int x = 0; x < TN_FIELD_W; x++) if (t[y][x]) any = 1;
            if (any) break;
        }
        int i = 6 - y;
        if (i) {
            for (int yy = TN_FIELD_H - 1; yy >= i; yy--) memcpy(t[yy], t[yy - i], TN_FIELD_W);
            for (int yy = 0; yy < i; yy++) memset(t[yy], 0, TN_FIELD_W);
        }
        memcpy(*f, t, sizeof t);
        break;
    }
    case TN_SB_CLEARSPECIAL:
        for (int y = 0; y < TN_FIELD_H; y++)
            for (int x = 0; x < TN_FIELD_W; x++)
                if ((*f)[y][x] > 5) (*f)[y][x] = (char)(tn_rng_int(&g->rng, 5) + 1);
        break;
    case TN_SB_GRAVITY:
        for (int y = 0; y < TN_FIELD_H; y++)
            for (int x = 0; x < TN_FIELD_W; x++)
                if ((*f)[y][x] == 0) {
                    for (int i = y; i > 0; i--) (*f)[i][x] = (*f)[i - 1][x];
                    (*f)[0][x] = 0;
                }
        break;
    case TN_SB_QUAKE:
        for (int y = 0; y < TN_FIELD_H; y++) {
            int s = 0, i = tn_rng_int(&g->rng, 22);
            if (i < 1) s++;
            if (i < 4) s++;
            if (i < 11) s++;
            if (tn_rng_int(&g->rng, 2)) s = -s;
            shift_line((*f)[y], s);
        }
        break;
    case TN_SB_BOMB: {
        static const int ax[] = {-1, 0, 1, 1, 1, 0, -1, -1};
        static const int ay[] = {-1, -1, -1, 0, 1, 1, 1, 0};
        char debris[TN_FIELD_W * TN_FIELD_H * 8];
        int c = 0;
        for (int y = 0; y < TN_FIELD_H; y++)
            for (int x = 0; x < TN_FIELD_W; x++)
                if ((*f)[y][x] == 14) {
                    (*f)[y][x] = 0;
                    for (int i = 0; i < 8; i++) {
                        int nx = x + ax[i], ny = y + ay[i];
                        if (ny >= TN_FIELD_H || ny < 0 || nx >= TN_FIELD_W || nx < 0) continue;
                        char b = (*f)[ny][nx];
                        if (b == 14) b = 0;
                        else (*f)[ny][nx] = 0;
                        debris[c++] = b;
                    }
                }
        for (int i = 0; i < c; i++)
            (*f)[tn_rng_int(&g->rng, TN_FIELD_H - 6) + 6][tn_rng_int(&g->rng, TN_FIELD_W)] = debris[i];
        break;
    }
    case TN_SB_CLEARCOLUMN: {
        int x = tn_rng_int(&g->rng, TN_FIELD_W);
        for (int y = 0; y < TN_FIELD_H; y++) (*f)[y][x] = 0;
        break;
    }
    case TN_SB_MUTATE:
        /* the piece shown as next is the first of them */
        g->mutate_left = TN_MUTATE_PIECES;
        draw_next(g);
        break;
    default: break; /* immunity, darkness and confusion are timers of the client */
    }
    /* a special can complete lines; those count as removed but give nothing */
    remove_full_lines(g->field, NULL, NULL);
    return 0;
}

int tn_game_pop_special(tn_game *g)
{
    if (g->nspecials <= 0) return -1;
    int sb = tn_sb_from_cell(g->specials[0]);
    memmove(g->specials, g->specials + 1, (size_t)(g->nspecials - 1));
    g->nspecials--;
    return sb;
}

void tn_game_view(const tn_game *g, tn_field out)
{
    memcpy(out, g->field, sizeof(tn_field));
    if (g->block != TN_NO_BLOCK) place(out, g->block, g->orient, g->bx, g->by);
}

void tn_game_fill_lost(tn_game *g)
{
    for (int y = 0; y < TN_FIELD_H; y++)
        for (int x = 0; x < TN_FIELD_W; x++) g->field[y][x] = (char)(tn_rng_int(&g->rng, 5) + 1);
    g->block = TN_NO_BLOCK;
}
