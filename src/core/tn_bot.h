/*
 * gtetrinet-ng — a simple computer player: picks the placement with the best heuristic
 * score (low stack, few holes, lines cleared) and uses specials on the strongest opponent.
 * Used by the demo/screenshot mode and the tests; the base for CPU opponents.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TN_BOT_H
#define TN_BOT_H

#include "tn_client.h"

typedef struct tn_bot {
    int think_ms;        /* time between moves; smaller is stronger */
    int timer;
    int goal_x, goal_orient, have_goal, piece_serial;
    int last_block, last_by;
    int mistake_pct;     /* chance a piece goes somewhere other than the best place */
    unsigned rng;
} tn_bot;

/* Strengths of the computer opponents offered in the lobby. */
enum tn_bot_level { TN_BOT_EASY, TN_BOT_NORMAL, TN_BOT_HARD, TN_BOT_LEVELS };

void tn_bot_init(tn_bot *b, int think_ms);
/* A lobby opponent: speed and mistakes from the level; seed varies the mistakes. */
void tn_bot_init_level(tn_bot *b, int level, unsigned seed);
void tn_bot_update(tn_bot *b, tn_client *c, int dt_ms);

#endif
