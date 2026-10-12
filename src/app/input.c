/*
 * gtetrinet-ng — devices to buttons. Keyboard and every game controller become the same
 * twelve buttons; a device belongs to the local player who joined with it.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "app.h"

#define STICK_ON 16000
#define STICK_OFF 9000

int gtn_local_count(const gtn_app *a)
{
    int n = 0;
    for (int i = 0; i < GTN_MAX_LOCAL; i++) n += a->pl[i].active;
    return n;
}

static int owner(gtn_app *a, enum gtn_dev dev, SDL_JoystickID pad)
{
    for (int i = 0; i < GTN_MAX_LOCAL; i++)
        if (a->pl[i].active && a->pl[i].dev == dev && (dev == DEV_KEYBOARD || a->pl[i].pad == pad)) return i;
    return -1;
}

static void press(gtn_app *a, enum gtn_dev dev, SDL_JoystickID pad, enum gtn_btn b, int down)
{
    int who = owner(a, dev, pad);
    /* horizontal auto-repeat in game */
    if (who >= 0 && (b == BTN_LEFT || b == BTN_RIGHT)) {
        gtn_local *p = &a->pl[who];
        int dir = b == BTN_LEFT ? -1 : 1;
        if (down) { p->hold = dir; p->repeat_ms = gtn_repeat_delay(a->settings.repeat_profile); }
        else if (p->hold == dir) p->hold = 0;
    }
    gtn_button(a, who, dev, pad, b, down);
}

static int key_button(SDL_Keycode k, int in_game)
{
    switch (k) {
    case SDLK_LEFT: return BTN_LEFT;
    case SDLK_RIGHT: return BTN_RIGHT;
    case SDLK_DOWN: return BTN_DOWN;
    case SDLK_UP: return in_game ? BTN_A : BTN_UP;   /* classic TetriNET: up rotates */
    case SDLK_SPACE: return in_game ? BTN_UP : BTN_A; /* space drops */
    case SDLK_x: return BTN_A;
    case SDLK_z: case SDLK_LCTRL: return BTN_B;
    case SDLK_s: return BTN_X;
    case SDLK_TAB: case SDLK_e: return BTN_RB;
    case SDLK_q: return BTN_LB;
    case SDLK_d: return BTN_BACK;
    case SDLK_RETURN: case SDLK_KP_ENTER: return in_game ? BTN_START : BTN_A;
    case SDLK_p: return BTN_START;
    case SDLK_ESCAPE: return BTN_B;
    case SDLK_BACKSPACE: return in_game ? -1 : BTN_B;
    default: return -1;
    }
}

static int pad_button(Uint8 b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return BTN_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return BTN_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return BTN_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
    case SDL_CONTROLLER_BUTTON_A: return BTN_A;
    case SDL_CONTROLLER_BUTTON_B: return BTN_B;
    case SDL_CONTROLLER_BUTTON_X: return BTN_X;
    case SDL_CONTROLLER_BUTTON_Y: return BTN_Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return BTN_LB;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return BTN_RB;
    case SDL_CONTROLLER_BUTTON_START: return BTN_START;
    case SDL_CONTROLLER_BUTTON_BACK: return BTN_BACK;
    default: return -1;
    }
}

/* The left stick works as a d-pad, with hysteresis so it does not flicker. */
static void stick(gtn_app *a, SDL_JoystickID pad, int axis, int v)
{
    int who = owner(a, DEV_PAD, pad);
    static int free_x[64], free_y[64]; /* sticks of devices nobody joined with yet */
    int *cur;
    if (who >= 0) cur = axis == SDL_CONTROLLER_AXIS_LEFTX ? &a->pl[who].stick_x : &a->pl[who].stick_y;
    else cur = axis == SDL_CONTROLLER_AXIS_LEFTX ? &free_x[pad & 63] : &free_y[pad & 63];
    int nv = *cur;
    if (v > STICK_ON) nv = 1;
    else if (v < -STICK_ON) nv = -1;
    else if (v > -STICK_OFF && v < STICK_OFF) nv = 0;
    if (nv == *cur) return;
    int neg = axis == SDL_CONTROLLER_AXIS_LEFTX ? BTN_LEFT : BTN_UP;
    int pos = axis == SDL_CONTROLLER_AXIS_LEFTX ? BTN_RIGHT : BTN_DOWN;
    if (*cur < 0) press(a, DEV_PAD, pad, (enum gtn_btn)neg, 0);
    if (*cur > 0) press(a, DEV_PAD, pad, (enum gtn_btn)pos, 0);
    *cur = nv;
    if (nv < 0) press(a, DEV_PAD, pad, (enum gtn_btn)neg, 1);
    if (nv > 0) press(a, DEV_PAD, pad, (enum gtn_btn)pos, 1);
}

void gtn_input_event(gtn_app *a, const SDL_Event *e)
{
    int in_game = a->screen == SCR_GAME;
    switch (e->type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        if (e->key.repeat) {
            /* text entry wants repeats for backspace; the game does its own repeat */
            if (a->screen == SCR_ONLINE && e->key.keysym.sym == SDLK_BACKSPACE) gtn_text_input(a, "\b");
            return;
        }
        SDL_Keycode k = e->key.keysym.sym;
        int down = e->type == SDL_KEYDOWN;
        if (a->screen == SCR_ONLINE && down) {
            if (k == SDLK_BACKSPACE) { gtn_text_input(a, "\b"); return; }
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { gtn_text_input(a, "\n"); return; }
        }
        if (in_game && down && k >= SDLK_1 && k <= SDLK_8) {
            int who = owner(a, DEV_KEYBOARD, 0);
            if (who >= 0) tn_client_special(&a->pl[who].client, (int)(k - SDLK_0));
            return;
        }
        if (a->screen == SCR_ONLINE && k != SDLK_ESCAPE && k != SDLK_UP && k != SDLK_DOWN &&
            k != SDLK_LEFT && k != SDLK_RIGHT && k != SDLK_TAB)
            return; /* typed characters arrive as text input */
        int b = key_button(k, in_game);
        if (b >= 0) press(a, DEV_KEYBOARD, 0, (enum gtn_btn)b, down);
        break;
    }
    case SDL_TEXTINPUT:
        if (a->screen == SCR_ONLINE) gtn_text_input(a, e->text.text);
        break;
    case SDL_CONTROLLERDEVICEADDED:
        SDL_GameControllerOpen(e->cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED: {
        int who = owner(a, DEV_PAD, e->cdevice.which);
        if (who >= 0) gtn_set_status(a, "Controller of %s disconnected - press A on any controller to take over", a->pl[who].nick);
        SDL_GameController *gc = SDL_GameControllerFromInstanceID(e->cdevice.which);
        if (gc) SDL_GameControllerClose(gc);
        if (who >= 0) {
            /* the player stays in the game; the next unclaimed controller to press A takes over */
            gtn_button(a, who, DEV_PAD, e->cdevice.which, BTN_DOWN, 0);
            gtn_button(a, who, DEV_PAD, e->cdevice.which, BTN_BACK, 0);
            a->pl[who].stick_x = a->pl[who].stick_y = 0;
            a->pl[who].pad = -1;
            a->pl[who].hold = 0;
        }
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        int b = pad_button(e->cbutton.button);
        if (b >= 0) press(a, DEV_PAD, e->cbutton.which, (enum gtn_btn)b, e->type == SDL_CONTROLLERBUTTONDOWN);
        break;
    }
    case SDL_CONTROLLERAXISMOTION:
        if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
            stick(a, e->caxis.which, e->caxis.axis, e->caxis.value);
        else if (e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            /* triggers double as shoulder buttons, which some pads lack */
            int who = owner(a, DEV_PAD, e->caxis.which);
            static int held[64][2];
            int t = e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
            int *h = &held[e->caxis.which & 63][t];
            int nv = e->caxis.value > 20000 ? 1 : e->caxis.value < 8000 ? 0 : *h;
            if (nv != *h) {
                *h = nv;
                if (who >= 0) gtn_button(a, who, DEV_PAD, e->caxis.which, t ? BTN_RB : BTN_LB, nv);
            }
        }
        break;
    default: break;
    }
}

void gtn_input_update(gtn_app *a, int dt_ms)
{
    if (a->screen != SCR_GAME) {
        for (int i = 0; i < GTN_MAX_LOCAL; i++) a->pl[i].hold = 0;
        return;
    }
    for (int i = 0; i < GTN_MAX_LOCAL; i++) {
        gtn_local *p = &a->pl[i];
        if (!p->active || !p->hold) continue;
        p->repeat_ms -= dt_ms;
        while (p->repeat_ms <= 0) {
            tn_client_action(&p->client, p->hold < 0 ? TN_ACT_LEFT : TN_ACT_RIGHT);
            p->repeat_ms += gtn_repeat_interval(a->settings.repeat_profile);
        }
    }
}
