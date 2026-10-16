/*
 * gtetrinet-ng — synthesized sound effects and a small mixer.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "sound.h"

#include <SDL.h>
#include <math.h>
#include <string.h>

#include "i18n.h"

#define VOICES 12
#define MAX_LEN (GTN_SFX_RATE * 2)

unsigned gtn_sfx_played[SFX_COUNT];

static int16_t *bank[SFX_COUNT];
static int bank_len[SFX_COUNT];
static struct { int sfx, pos; } voice[VOICES];
static SDL_AudioDeviceID dev;
static int gain256 = 166;          /* VOLUME_NORMAL */
static int volume = VOLUME_NORMAL;
static unsigned char asked[SFX_COUNT];

/* A note: frequency sweeping from f0 to f1, a square/triangle/noise mix, with a short attack and
 * an exponential decay. Written onto out at start, mixed with what is there. */
typedef struct note {
    double t0, len, f0, f1, amp;
    int wave;                      /* 0 triangle, 1 square, 2 noise */
    double decay;                  /* larger: shorter tail */
} note;

static int render_notes(const note *n, int count, int16_t *out, int max)
{
    int len = 0;
    for (int k = 0; k < count; k++) {
        int end = (int)((n[k].t0 + n[k].len) * GTN_SFX_RATE);
        if (end > len) len = end;
    }
    if (len > max) len = max;
    if (!out) return len;
    memset(out, 0, sizeof *out * (size_t)len);
    uint32_t rng = 0x2545f491u;
    for (int k = 0; k < count; k++) {
        const note *q = &n[k];
        int a = (int)(q->t0 * GTN_SFX_RATE), b = (int)((q->t0 + q->len) * GTN_SFX_RATE);
        double phase = 0, held = 0;
        if (b > len) b = len;
        for (int i = a; i < b; i++) {
            double t = (double)(i - a) / GTN_SFX_RATE, u = t / q->len;
            double f = q->f0 + (q->f1 - q->f0) * u;
            phase += f / GTN_SFX_RATE;
            phase -= floor(phase);
            double v;
            if (q->wave == 1) v = phase < 0.5 ? 0.6 : -0.6;
            else if (q->wave == 2) {
                /* sample-and-hold noise at the note's frequency: a pitched hiss */
                if (phase < f / GTN_SFX_RATE) { rng = rng * 1664525u + 1013904223u; held = (double)(rng >> 8) / (1 << 23) - 1.0; }
                v = held * 0.7;
            } else v = 4.0 * fabs(phase - 0.5) - 1.0;
            double env = (t < 0.004 ? t / 0.004 : 1.0) * exp(-q->decay * u);
            if (u > 0.9) env *= (1.0 - u) * 10.0;          /* no click at the end */
            int s = out[i] + (int)(v * env * q->amp * 32767.0);
            out[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        }
    }
    return len;
}

int gtn_sfx_render(enum gtn_sfx s, int16_t *out, int max)
{
    /* C5 E5 G5 C6 E6 */
    static const double up[] = {523.25, 659.25, 783.99, 1046.5, 1318.5};
    note n[8];
    int c = 0;
    switch (s) {
    case SFX_LOCK:
        n[c++] = (note){0, 0.07, 160, 90, 0.35, 1, 4};
        n[c++] = (note){0, 0.03, 2500, 2500, 0.10, 2, 3};
        break;
    case SFX_CLEAR1: case SFX_CLEAR2: case SFX_CLEAR3: case SFX_CLEAR4: {
        int lines = s - SFX_CLEAR1 + 1;
        for (int k = 0; k < lines; k++) n[c++] = (note){k * 0.055, 0.16, up[k], up[k], 0.40, 0, 3};
        if (lines == 4) {
            n[c++] = (note){4 * 0.055, 0.35, up[4], up[4] * 1.02, 0.40, 0, 2.5};
            n[c++] = (note){4 * 0.055, 0.30, up[4] * 2, up[4] * 2, 0.10, 1, 4};
        }
        break;
    }
    case SFX_SPECIAL:
        n[c++] = (note){0, 0.18, 330, 990, 0.30, 1, 1.5};
        n[c++] = (note){0, 0.18, 3000, 6000, 0.08, 2, 2};
        break;
    case SFX_HIT_HURT:
        n[c++] = (note){0, 0.25, 420, 110, 0.45, 1, 2};
        n[c++] = (note){0, 0.12, 1200, 400, 0.15, 2, 3};
        break;
    case SFX_HIT_HELP:
        n[c++] = (note){0, 0.12, 783.99, 783.99, 0.35, 0, 2};
        n[c++] = (note){0.07, 0.20, 1174.7, 1174.7, 0.35, 0, 2.5};
        break;
    case SFX_HIT_MIXED:
        n[c++] = (note){0, 0.22, 600, 300, 0.30, 0, 2};
        n[c++] = (note){0.05, 0.22, 450, 900, 0.25, 1, 2.5};
        break;
    case SFX_LOSE:
        n[c++] = (note){0, 0.22, 392.0, 392.0, 0.35, 1, 1.5};
        n[c++] = (note){0.2, 0.22, 311.1, 311.1, 0.35, 1, 1.5};
        n[c++] = (note){0.4, 0.55, 261.6, 220.0, 0.40, 1, 2.5};
        break;
    case SFX_WIN:
        for (int k = 0; k < 3; k++) n[c++] = (note){k * 0.11, 0.14, up[k], up[k], 0.35, 1, 2};
        n[c++] = (note){0.33, 0.55, up[3], up[3], 0.40, 1, 2};
        n[c++] = (note){0.33, 0.55, up[1] * 2, up[1] * 2, 0.15, 0, 2};
        break;
    case SFX_MENU:
        n[c++] = (note){0, 0.04, 1400, 1400, 0.25, 0, 5};
        break;
    default:
        return 0;
    }
    return render_notes(n, c, out, max);
}

static void SDLCALL mix(void *user, Uint8 *stream, int bytes)
{
    int16_t *out = (int16_t *)stream;
    int n = bytes / 2;
    memset(stream, 0, (size_t)bytes);
    for (int v = 0; v < VOICES; v++) {
        if (voice[v].sfx < 0) continue;
        const int16_t *src = bank[voice[v].sfx];
        int pos = voice[v].pos, len = bank_len[voice[v].sfx];
        for (int i = 0; i < n && pos < len; i++, pos++) {
            int s = out[i] + src[pos] * gain256 / 256;
            out[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        }
        voice[v].pos = pos;
        if (pos >= len) voice[v].sfx = -1;
    }
}

void gtn_sound_init(void)
{
    for (int v = 0; v < VOICES; v++) voice[v].sfx = -1;
    if (dev) return;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    for (int s = 0; s < SFX_COUNT; s++) {
        bank[s] = SDL_malloc(sizeof(int16_t) * MAX_LEN);
        if (!bank[s]) { gtn_sound_quit(); return; }
        bank_len[s] = gtn_sfx_render((enum gtn_sfx)s, bank[s], MAX_LEN);
    }
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = GTN_SFX_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;              /* about 23 ms: a lock should sound when it happens */
    want.callback = mix;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) { gtn_sound_quit(); return; }
    SDL_PauseAudioDevice(dev, 0);
}

void gtn_sound_quit(void)
{
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;
    for (int s = 0; s < SFX_COUNT; s++) { SDL_free(bank[s]); bank[s] = NULL; bank_len[s] = 0; }
    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

int gtn_sound_active(void) { return dev != 0; }

void gtn_sound_volume(int v)
{
    static const int gains[VOLUME_COUNT] = {0, 90, 166, 256};
    volume = v >= 0 && v < VOLUME_COUNT ? v : VOLUME_NORMAL;
    if (dev) SDL_LockAudioDevice(dev);
    gain256 = gains[volume];
    if (dev) SDL_UnlockAudioDevice(dev);
}

const char *gtn_volume_name(int v)
{
    static const char *names[VOLUME_COUNT] = {N_("Off"), N_("Quiet"), N_("Normal"), N_("Loud")};
    return names[v >= 0 && v < VOLUME_COUNT ? v : VOLUME_NORMAL];
}

void gtn_sound_play(enum gtn_sfx s)
{
    if ((unsigned)s >= SFX_COUNT || asked[s]) return;
    asked[s] = 1;
    gtn_sfx_played[s]++;
    if (!dev || volume == VOLUME_OFF || !bank_len[s]) return;
    SDL_LockAudioDevice(dev);
    int pick = 0, best = -1;
    for (int v = 0; v < VOICES; v++) {
        if (voice[v].sfx < 0) { pick = v; break; }
        if (voice[v].pos > best) { best = voice[v].pos; pick = v; }   /* else steal the oldest */
    }
    voice[pick].sfx = s;
    voice[pick].pos = 0;
    SDL_UnlockAudioDevice(dev);
}

void gtn_sound_frame(void) { memset(asked, 0, sizeof asked); }
