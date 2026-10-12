/* SPDX-License-Identifier: GPL-2.0-or-later
 * Software rendering checks the actual field/queue colors and classic letters.
 * This is a layout fixture, not physical Deck/controller verification. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#undef main
#include "font8x8_basic.h"

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

static int pixel_is(SDL_Surface *surface, int x, int y, gtn_color expected)
{
    Uint8 r, g, b, alpha;
    SDL_GetRGBA(((Uint32 *)((char *)surface->pixels + y * surface->pitch))[x],
                surface->format, &r, &g, &b, &alpha);
    return r == expected.r && g == expected.g && b == expected.b && alpha == expected.a;
}

static int count_color(SDL_Surface *surface, gtn_color color)
{
    int count = 0;
    for (int y = 0; y < surface->h; y++)
        for (int x = 0; x < surface->w; x++) count += pixel_is(surface, x, y, color);
    return count;
}

static void check_text(SDL_Surface *surface, int x0, int y0, const char *text, gtn_color color)
{
    int before = failures;
    for (int ch = 0; text[ch]; ch++)
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
            gtn_color expected = (font8x8_basic[(int)text[ch]][y] >> x) & 1 ? color : gtn_rgb(0x16, 0x19, 0x24);
            CHECK(pixel_is(surface, x0 + ch * 8 + x, y0 + y, expected));
        }
    if (failures != before) fprintf(stderr, "%d mismatched pixels for '%s' at %d,%d\n", failures - before, text, x0, y0);
}

/* Also used by the local screenshot harness; no sockets or bot behavior needed. */
static void layout(gtn_app *a, int players, int self)
{
    memset(a->pl, 0, sizeof a->pl);
    a->screen = SCR_GAME;
    for (int i = 0; i < players; i++) {
        gtn_local *p = &a->pl[i];
        p->active = 1;
        snprintf(p->nick, sizeof p->nick, "Player%d", i + 1);
        p->target = self ? i + 1 : (i + 1) % players + 1;
        tn_client *c = &p->client;
        c->state = TN_CONNECTED;
        c->me = i + 1;
        c->ingame = c->playing = 1;
        c->game.block = TN_NO_BLOCK;
        c->game.next = 2;
        c->game.level = 1;
        c->game.nspecials = 40; /* queue must leave the category label visible */
        for (int k = 0; k < 40; k++) c->game.specials[k] = (unsigned char)(6 + (i + k) % 9);
        for (int k = 0; k < 9; k++) c->game.field[TN_FIELD_H - 1][k] = (char)(6 + k);
        for (int k = 1; k <= players; k++) {
            c->slot[k].present = c->slot[k].playing = 1;
            snprintf(c->slot[k].nick, sizeof c->slot[k].nick, "Player%d", k);
        }
    }
    gtn_draw(a);
    SDL_RenderPresent(a->ren);
}

int main(void)
{
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_EVENTS) == 0);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 800, 32, SDL_PIXELFORMAT_RGBA32);
    CHECK(surface != NULL);
    if (!surface) return 1;
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    CHECK(renderer != NULL);
    if (!renderer) { SDL_FreeSurface(surface); SDL_Quit(); return 1; }
    CHECK(gtn_render_init(renderer) == 0);

    static const enum gtn_effect expected[] = {
        GTN_EFFECT_HURT, GTN_EFFECT_HELP, GTN_EFFECT_HELP, GTN_EFFECT_MIXED,
        GTN_EFFECT_MIXED, GTN_EFFECT_MIXED, GTN_EFFECT_HELP, GTN_EFFECT_MIXED, GTN_EFFECT_MIXED
    };
    const gtn_color help = {0x73, 0xdf, 0xa0, 255}, hurt = {0xff, 0x87, 0x87, 255};
    const gtn_color mixed = {0xff, 0xd0, 0x70, 255}, ink = {0x20, 0x20, 0x40, 255};
    const char *letters = "ACNRSBGQO";
    for (int i = 0; i < 9; i++) {
        int sb = tn_sb_from_cell(6 + i);
        CHECK(gtn_special_effect(sb) == expected[i]);
        const char *cue = expected[i] == GTN_EFFECT_HELP ? "+HELP" : expected[i] == GTN_EFFECT_HURT ? "-HURT" : "?MIX";
        CHECK(strcmp(gtn_special_cue(sb), cue) == 0);
        gtn_color color = expected[i] == GTN_EFFECT_HELP ? help : expected[i] == GTN_EFFECT_HURT ? hurt : mixed;
        for (int size = 6; size <= 24; size += 2) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            gtn_cell(10, 10, size, 6 + i);
            SDL_RenderPresent(renderer);
            CHECK(count_color(surface, color) > 0);
            CHECK(pixel_is(surface, 9, 10, gtn_rgb(0, 0, 0)));
            CHECK(pixel_is(surface, 10 + size, 10, gtn_rgb(0, 0, 0)));
            int sc = size / 8, origin = 10 + (size - 8 * sc) / 2;
            for (int y = 0; y < 8 && sc; y++) for (int x = 0; x < 8; x++) {
                if ((font8x8_basic[(int)letters[i]][y] >> x) & 1)
                    CHECK(pixel_is(surface, origin + x * sc, origin + y * sc, ink));
            }
        }
    }
    CHECK(strcmp(gtn_special_target_hint(TN_SB_NUKE, 1), "+You") == 0);
    CHECK(strcmp(gtn_special_target_hint(TN_SB_NUKE, 0), "+Them") == 0);
    CHECK(strcmp(gtn_special_target_hint(TN_SB_ADDLINE, 1), "-You") == 0);
    CHECK(strcmp(gtn_special_target_hint(TN_SB_ADDLINE, 0), "-Them") == 0);
    CHECK(strcmp(gtn_special_target_hint(TN_SB_SWITCH, 0), "?Both") == 0);
    CHECK(strcmp(gtn_special_target_hint(TN_SB_SWITCH, 1), "?Self") == 0);
    CHECK(strcmp(gtn_special_target_hint(TN_SB_BOMB, 0), "?MIX") == 0);
    for (int sb = TN_SB_ADDALL1; sb <= TN_SB_ADDALL4; sb++) CHECK(gtn_special_effect(sb) == GTN_EFFECT_HURT);
    CHECK(gtn_special_effect(-1) == GTN_EFFECT_UNKNOWN);
    CHECK(gtn_special_effect(TN_SB_COUNT) == GTN_EFFECT_UNKNOWN);

    static gtn_app a;
    a.ren = renderer;
    for (int small = 0; small < 2; small++) {
        a.w = small ? 800 : 1280;
        a.h = small ? 600 : 800;
        SDL_RenderSetViewport(renderer, &(SDL_Rect){0, 0, a.w, a.h});
        for (int players = 1; players <= 8; players += 7) {
            for (int self = 0; self <= 1; self++) {
                layout(&a, players, self);
                CHECK(count_color(surface, help) > 0);
                CHECK(count_color(surface, hurt) > 0);
                CHECK(count_color(surface, mixed) > 0);
                /* Fixed reference coordinates assert visible, complete cue/name text
                 * below a full queue, independently of the layout implementation. */
                static const char *labels[] = {
                    "-HURT Add Line", "+HELP Clear Line", "+HELP Nuke Field", "?MIX Clear Random",
                    "?MIX Switch Fields", "?MIX Clear Specials", "+HELP Block Gravity", "?MIX Blockquake"
                };
                static const char *other_hints[] = {"-Them", "+Them", "+Them", "?MIX", "?Both", "?MIX", "+Them", "?MIX"};
                static const char *self_hints[] = {"-You", "+You", "+You", "?MIX", "?Self", "?MIX", "+You", "?MIX"};
                for (int i = 0; i < players; i++) {
                    gtn_color color = expected[i] == GTN_EFFECT_HELP ? help : expected[i] == GTN_EFFECT_HURT ? hurt : mixed;
                    int x = players == 1 ? (small ? 198 : 377) : (small ? 12 : 28) + (i % 4) * (small ? 200 : 320);
                    int y = players == 1 ? (small ? 568 : 751) : (small ? 264 : 374) + (i / 4) * (small ? 300 : 400);
                    check_text(surface, x, y, labels[i], color);
                    if (players == 8) {
                        int hx = (small ? 137 : 215) + (i % 4) * (small ? 200 : 320);
                        int hy = (small ? 143 : 159) + (i / 4) * (small ? 300 : 400);
                        check_text(surface, hx, hy, self ? self_hints[i] : other_hints[i], color);
                    }
                }
            }
        }
    }
    gtn_render_quit();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    printf("%d effect checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
