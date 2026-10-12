/*
 * gtetrinet-ng — drawing helpers on top of SDL_Renderer: rectangles, an 8x8 bitmap font,
 * TetriNET cells and fields.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GTN_RENDER_H
#define GTN_RENDER_H

#include <SDL.h>
#include "tn_protocol.h"

typedef struct gtn_color { Uint8 r, g, b, a; } gtn_color;

extern SDL_Renderer *gtn_ren;

int gtn_render_init(SDL_Renderer *r);
void gtn_render_quit(void);

gtn_color gtn_rgb(Uint8 r, Uint8 g, Uint8 b);
gtn_color gtn_player_color(int i); /* 0..7 */
/* Effects describe the recipient's field, not the sender's tactical advantage. */
enum gtn_effect { GTN_EFFECT_UNKNOWN, GTN_EFFECT_HELP, GTN_EFFECT_HURT, GTN_EFFECT_MIXED };
enum gtn_effect gtn_special_effect(int sb);
gtn_color gtn_special_color(int sb);
const char *gtn_special_cue(int sb); /* symbol + word, readable without color */
const char *gtn_special_target_hint(int sb, int self);
void gtn_fill(int x, int y, int w, int h, gtn_color c);
void gtn_frame(int x, int y, int w, int h, int thick, gtn_color c);

/* scale = pixel size of one font pixel (glyphs are 8*scale wide). Returns the width drawn. */
int gtn_text(int x, int y, int scale, gtn_color c, const char *s);
int gtn_text_width(int scale, const char *s);
void gtn_text_center(int cx, int y, int scale, gtn_color c, const char *s);
int gtn_text_scale(int scale, int maxw, const char *s);
void gtn_text_center_fit(int cx, int y, int scale, int maxw, gtn_color c, const char *s);
/* Reduces integer scale before abbreviating to maxw. */
void gtn_text_fit(int x, int y, int scale, int maxw, gtn_color c, const char *s);

void gtn_cell(int x, int y, int size, int cell);
/* Draws a whole field; ghost_y >= 0 draws the falling piece's landing place (block/orient). */
void gtn_field(int x, int y, int cellsize, const tn_field f, int dim);
void gtn_piece(int x, int y, int cellsize, int block, int orient, int alpha);

#endif
