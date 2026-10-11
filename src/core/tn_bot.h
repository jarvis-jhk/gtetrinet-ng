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
} tn_bot;

void tn_bot_init(tn_bot *b, int think_ms);
void tn_bot_update(tn_bot *b, tn_client *c, int dt_ms);

#endif
