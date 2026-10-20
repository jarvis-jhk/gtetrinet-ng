/* gtetrinet-ng — SPDX-License-Identifier: GPL-2.0-or-later
 * The game screen opens with the first local client that gets "newgame". Linux delivers
 * loopback data before send() returns, so all seats are in by then; macOS and Windows queue it,
 * and the others follow a few updates later. Tests that drive every seat wait for all of them. */
static int all_playing(const gtn_app *a)
{
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].client.state == TN_CONNECTED && !a->pl[i].client.playing) return 0;
    return 1;
}
