/*
 * gtetrinet-ng — the TetriNET game rules for one player, without any I/O.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Ported from GTetrinet's tetris.c/tetrinet.c with the global state moved into a struct, so a
 * process can run up to eight players side by side.
 */
#ifndef TN_GAME_H
#define TN_GAME_H

#include <stdint.h>
#include "tn_protocol.h"

#define TN_NO_BLOCK (-1)

typedef struct tn_rng { uint32_t s; } tn_rng;
void tn_rng_seed(tn_rng *r, uint32_t seed);
int tn_rng_int(tn_rng *r, int n); /* 0..n-1 */

/* block shapes: 7 kinds, up to 4 orientations of 4x4 */
int tn_block_orients(int block);
const char (*tn_block_shape(int block, int orient))[4];

typedef struct tn_game {
    tn_field field;
    tn_rules rules;
    tn_rng rng;
    int alive;
    int block, orient, bx, by;  /* falling piece, block == TN_NO_BLOCK if none */
    int next, next_orient;
    int level, lines, slines, llines;
    unsigned char specials[256];
    int nspecials;
} tn_game;

void tn_game_start(tn_game *g, const tn_rules *rules, uint32_t seed);

int tn_game_random_block(tn_game *g);
/* Places a new piece at the top; returns -1 if it does not fit (the player has lost). */
int tn_game_spawn(tn_game *g);
/* 0 moved, 1 blocked */
int tn_game_down(tn_game *g);
void tn_game_move(tn_game *g, int dir);
void tn_game_rotate(tn_game *g, int dir);
void tn_game_drop(tn_game *g);
/* Writes the piece into the field. Returns -1 if there is no room (lost). */
int tn_game_solidify(tn_game *g);

/* Removes full lines, collects their specials into the player's inventory, adds new
 * specials to the field and advances the level. Returns the number of lines; *level_changed
 * tells whether a "lvl" message is due. */
int tn_game_clear_lines(tn_game *g, int *level_changed);

/* Adds garbage at the bottom: type 1 = add-line special, 2 = classic mode. -1 if pushed out. */
int tn_game_add_lines(tn_game *g, int count, int type);

/* Applies a special to this player's field. other is the sender's field (for switch).
 * Returns -1 if the player lost because of it. */
int tn_game_apply_special(tn_game *g, int sb, const tn_field other);

/* Removes and returns the first special in the inventory, -1 if none. */
int tn_game_pop_special(tn_game *g);

/* field with the falling piece drawn in */
void tn_game_view(const tn_game *g, tn_field out);
/* Row where the piece would land (for the ghost piece). */
int tn_game_drop_y(const tn_game *g);

/* Fills the field with random junk, as the classic client shows a lost player. */
void tn_game_fill_lost(tn_game *g);

int tn_gravity_ms(int level);

#endif
