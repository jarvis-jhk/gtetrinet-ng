/*
 * gtetrinet-ng — sound effects and controller rumble. The effects are synthesized at start-up,
 * so the game ships no audio files; without an audio device everything stays silent.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GTN_SOUND_H
#define GTN_SOUND_H

#include <stdint.h>

enum gtn_sfx {
    SFX_LOCK, SFX_CLEAR1, SFX_CLEAR2, SFX_CLEAR3, SFX_CLEAR4,
    SFX_SPECIAL, SFX_HIT_HURT, SFX_HIT_HELP, SFX_HIT_MIXED,
    SFX_LOSE, SFX_WIN, SFX_MENU, SFX_COUNT
};

enum gtn_volume { VOLUME_OFF, VOLUME_QUIET, VOLUME_NORMAL, VOLUME_LOUD, VOLUME_COUNT };

#define GTN_SFX_RATE 22050

/* Opens the audio device. Failure is not an error: the game simply has no sound. */
void gtn_sound_init(void);
void gtn_sound_quit(void);
int gtn_sound_active(void);
void gtn_sound_volume(int volume);
const char *gtn_volume_name(int volume);

/* Plays an effect. The same effect asked for again within one frame plays once, so eight
 * players clearing at the same moment do not stack into one loud burst. */
void gtn_sound_play(enum gtn_sfx s);
/* Ends a frame: from here on the same effect may start again. */
void gtn_sound_frame(void);

/* Writes up to max samples of an effect and returns its length; pure, for tests. */
int gtn_sfx_render(enum gtn_sfx s, int16_t *out, int max);

/* Effects asked for since start (also with no device or sound off), for tests. */
extern unsigned gtn_sfx_played[SFX_COUNT];

#endif
