/*
 * gtetrinet-ng — drawing helpers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "render.h"
#include "tn_game.h"
#include "font8x8_basic.h"

#include <string.h>

SDL_Renderer *gtn_ren;
static SDL_Texture *font_tex;

gtn_color gtn_rgb(Uint8 r, Uint8 g, Uint8 b)
{
    gtn_color c = {r, g, b, 255};
    return c;
}

gtn_color gtn_player_color(int i)
{
    static const gtn_color pc[8] = {
        {0x4f, 0xa3, 0xff, 255}, {0xff, 0x6b, 0x5e, 255}, {0x6b, 0xdb, 0x6b, 255}, {0xff, 0xd0, 0x3f, 255},
        {0xc0, 0x7b, 0xff, 255}, {0x3f, 0xe0, 0xd8, 255}, {0xff, 0x9a, 0x3c, 255}, {0xff, 0x7b, 0xc8, 255},
    };
    return pc[((i % 8) + 8) % 8];
}

int gtn_render_init(SDL_Renderer *r)
{
    gtn_ren = r;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    /* 16x8 glyph atlas, white on transparent, tinted per call */
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 128, 64, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return -1;
    SDL_LockSurface(s);
    Uint32 *px = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4;
    Uint32 white = SDL_MapRGBA(s->format, 255, 255, 255, 255);
    Uint32 clear = SDL_MapRGBA(s->format, 0, 0, 0, 0);
    for (int ch = 0; ch < 128; ch++) {
        int gx = (ch % 16) * 8, gy = (ch / 16) * 8;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                px[(gy + y) * pitch + gx + x] = (font8x8_basic[ch][y] >> x) & 1 ? white : clear;
    }
    SDL_UnlockSurface(s);
    font_tex = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    if (!font_tex) return -1;
    SDL_SetTextureBlendMode(font_tex, SDL_BLENDMODE_BLEND);
    return 0;
}

void gtn_render_quit(void)
{
    if (font_tex) SDL_DestroyTexture(font_tex);
    font_tex = NULL;
}

void gtn_fill(int x, int y, int w, int h, gtn_color c)
{
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(gtn_ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(gtn_ren, &rc);
}

void gtn_frame(int x, int y, int w, int h, int t, gtn_color c)
{
    gtn_fill(x, y, w, t, c);
    gtn_fill(x, y + h - t, w, t, c);
    gtn_fill(x, y, t, h, c);
    gtn_fill(x + w - t, y, t, h, c);
}

int gtn_text_width(int scale, const char *s) { return (int)strlen(s) * 8 * scale; }

int gtn_text(int x, int y, int scale, gtn_color c, const char *s)
{
    int x0 = x;
    SDL_SetTextureColorMod(font_tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(font_tex, c.a);
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch >= 128) ch = '?';
        SDL_Rect src = {(ch % 16) * 8, (ch / 16) * 8, 8, 8};
        SDL_Rect dst = {x, y, 8 * scale, 8 * scale};
        SDL_RenderCopy(gtn_ren, font_tex, &src, &dst);
        x += 8 * scale;
    }
    return x - x0;
}

void gtn_text_center(int cx, int y, int scale, gtn_color c, const char *s)
{
    gtn_text(cx - gtn_text_width(scale, s) / 2, y, scale, c, s);
}

void gtn_text_fit(int x, int y, int scale, int maxw, gtn_color c, const char *s)
{
    int fit = maxw / (8 * scale);
    if (fit <= 0) return;
    if ((int)strlen(s) <= fit) { gtn_text(x, y, scale, c, s); return; }
    char buf[256];
    int n = fit - 2 < (int)sizeof buf - 3 ? fit - 2 : (int)sizeof buf - 3;
    if (n < 0) n = 0;
    memcpy(buf, s, (size_t)n);
    strcpy(buf + n, "..");
    gtn_text(x, y, scale, c, buf);
}

static const gtn_color cell_colors[6] = {
    {0x12, 0x14, 0x1c, 255},
    {0x2f, 0x6b, 0xff, 255}, /* blue */
    {0xff, 0xd2, 0x2e, 255}, /* yellow */
    {0x2e, 0xc8, 0x4a, 255}, /* green */
    {0xb0, 0x4a, 0xff, 255}, /* purple */
    {0xff, 0x3b, 0x3b, 255}, /* red */
};

static gtn_color shade(gtn_color c, int pct)
{
    gtn_color o = c;
    if (pct > 100) {
        o.r = (Uint8)(c.r + (255 - c.r) * (pct - 100) / 100);
        o.g = (Uint8)(c.g + (255 - c.g) * (pct - 100) / 100);
        o.b = (Uint8)(c.b + (255 - c.b) * (pct - 100) / 100);
    } else {
        o.r = (Uint8)(c.r * pct / 100);
        o.g = (Uint8)(c.g * pct / 100);
        o.b = (Uint8)(c.b * pct / 100);
    }
    return o;
}

void gtn_cell(int x, int y, int size, int cell)
{
    if (cell <= 0) return;
    if (cell < 6) {
        gtn_color c = cell_colors[cell];
        int b = size >= 8 ? size / 8 : 1;
        gtn_fill(x, y, size, size, c);
        if (size >= 4) {
            gtn_fill(x, y, size, b, shade(c, 150));
            gtn_fill(x, y, b, size, shade(c, 130));
            gtn_fill(x, y + size - b, size, b, shade(c, 55));
            gtn_fill(x + size - b, y, b, size, shade(c, 70));
        }
        return;
    }
    /* specials: a letter on a light tile, like the original's lettered blocks */
    gtn_fill(x, y, size, size, gtn_rgb(0xe8, 0xe8, 0xf0));
    gtn_frame(x, y, size, size, size >= 12 ? 2 : 1, gtn_rgb(0x50, 0x50, 0x70));
    char s[2] = {tn_cell_char(cell), 0};
    s[0] = (char)(s[0] - 'a' + 'A');
    int sc = size / 8;
    if (sc >= 1) gtn_text(x + (size - 8 * sc) / 2, y + (size - 8 * sc) / 2, sc, gtn_rgb(0x20, 0x20, 0x40), s);
}

void gtn_field(int x, int y, int cs, const tn_field f, int dim)
{
    gtn_fill(x, y, cs * TN_FIELD_W, cs * TN_FIELD_H, cell_colors[0]);
    if (cs >= 10) {
        gtn_color grid = gtn_rgb(0x1b, 0x1e, 0x29);
        for (int i = 1; i < TN_FIELD_W; i++) gtn_fill(x + i * cs, y, 1, cs * TN_FIELD_H, grid);
        for (int i = 1; i < TN_FIELD_H; i++) gtn_fill(x, y + i * cs, cs * TN_FIELD_W, 1, grid);
    }
    for (int r = 0; r < TN_FIELD_H; r++)
        for (int c = 0; c < TN_FIELD_W; c++) gtn_cell(x + c * cs, y + r * cs, cs, f[r][c]);
    if (dim) gtn_fill(x, y, cs * TN_FIELD_W, cs * TN_FIELD_H, (gtn_color){0, 0, 0, 150});
}

void gtn_piece(int x, int y, int cs, int block, int orient, int alpha)
{
    if (block < 0) return;
    const char (*sh)[4] = tn_block_shape(block, orient);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            if (!sh[r][c]) continue;
            if (alpha >= 255) {
                gtn_cell(x + c * cs, y + r * cs, cs, sh[r][c]);
            } else {
                gtn_color col = cell_colors[(int)sh[r][c]];
                col.a = (Uint8)alpha;
                gtn_fill(x + c * cs, y + r * cs, cs, cs, col);
            }
        }
}
