/* SPDX-License-Identifier: GPL-2.0-or-later
 * The block style: the Options row and its saved value, and a software-rendered field whose
 * pixels must come from GTetrinet's own blocks.png (decoded here, apart from the renderer)
 * when the original style is on, and from the modern drawing when it is not. */
#define SDL_MAIN_HANDLED
#define main gtn_app_entry
#include "../src/app/main.c"
#undef main

static const char *pref_path;
static char *test_pref_path(const char *org, const char *app) { return SDL_strdup(pref_path); }
#define SDL_GetPrefPath test_pref_path
#include "../src/app/settings.c"
#undef SDL_GetPrefPath

#define STBI_NO_STDIO
#include "stb_image.h" /* the decoder itself is compiled into render.c */
#include "blocks_png.h"

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)

static void pad(gtn_app *a, Uint8 button, int down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    e.cbutton.which = 42;
    e.cbutton.button = button;
    gtn_input_event(a, &e);
}

static void press(gtn_app *a, Uint8 button) { pad(a, button, 1); pad(a, button, 0); }

static unsigned char *sheet;
static int sheet_w, sheet_h;

static void rgb_at(SDL_Surface *s, int x, int y, Uint8 *r, Uint8 *g, Uint8 *b)
{
    Uint8 al;
    SDL_GetRGBA(((Uint32 *)((char *)s->pixels + y * s->pitch))[x], s->format, r, g, b, &al);
}

/* Pixel (x,y) of the screen equals sheet pixel (sx,sy), within rounding. */
static int from_sheet(SDL_Surface *s, int x, int y, int sx, int sy)
{
    Uint8 r, g, b;
    rgb_at(s, x, y, &r, &g, &b);
    const unsigned char *p = sheet + 4 * (sy * sheet_w + sx);
    return abs(r - p[0]) <= 2 && abs(g - p[1]) <= 2 && abs(b - p[2]) <= 2;
}

/* Draws a 20 px field at the top left: cell 3 (green) at row 21 column 0, special 'a' at column 1,
 * TN2 special 'i' at column 2, the rest empty. */
static void draw_field(SDL_Renderer *ren, int cs)
{
    tn_field f;
    memset(f, 0, sizeof f);
    f[21][0] = 3;
    f[21][1] = (char)tn_cell_from_char('a');
    f[21][2] = (char)tn_cell_from_char('i');
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    gtn_field(0, 0, cs, f, 0);
    SDL_RenderPresent(ren);
}

static void check_render(void)
{
    sheet = stbi_load_from_memory(gtn_blocks_png, (int)sizeof gtn_blocks_png, &sheet_w, &sheet_h, NULL, 4);
    CHECK(sheet && sheet_w == 360 && sheet_h == 470);
    if (!sheet) return;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 400, 480, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *ren = s ? SDL_CreateSoftwareRenderer(s) : NULL;
    CHECK(ren != NULL);
    if (!ren) return;
    CHECK(gtn_render_init(ren) == 0);
    CHECK(gtn_original_blocks_loaded());

    gtn_block_style = GTN_BLOCKS_ORIGINAL;
    draw_field(ren, 20);
    int same = 0, total = 0;
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++) {
            total += 3;
            same += from_sheet(s, x, 420 + y, 2 * 20 + x, y);          /* green block */
            same += from_sheet(s, 20 + 4 + x / 2, 420 + 4 + y / 2, 5 * 20 + 4 + x / 2, 4 + y / 2); /* 'a' inside its frame */
            same += from_sheet(s, 5 * 20 + x, 3 * 20 + y, 5 * 20 + x, 30 + 3 * 20 + y); /* backdrop */
        }
    CHECK(same == total);
    if (same != total) fprintf(stderr, "%d of %d pixels differ from blocks.png\n", total - same, total);
    /* the effect frame around the original 'a' (Add line: harmful) */
    Uint8 r, g, b;
    gtn_color hurt = gtn_special_color(tn_sb_from_cell(tn_cell_from_char('a')));
    rgb_at(s, 20, 430, &r, &g, &b);
    CHECK(r == hurt.r && g == hurt.g && b == hurt.b);
    /* the TN2 'i' has no picture in the sheet: modern tint, not a sheet block */
    gtn_color help = gtn_special_color(tn_sb_from_cell(tn_cell_from_char('i')));
    rgb_at(s, 40 + 3, 420 + 3, &r, &g, &b);
    CHECK(r == help.r && g == help.g && b == help.b);

    /* small fields use the half-size row and the small backdrop */
    draw_field(ren, 10);
    same = total = 0;
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++) {
            total += 2;
            same += from_sheet(s, x, 210 + y, 2 * 10 + x, 20 + y);
            same += from_sheet(s, 50 + x, 30 + y, 240 + 50 + x, 30 + 30 + y);
        }
    CHECK(same == total);

    /* modern: no sheet pixel in the block's centre, the flat green instead */
    gtn_block_style = GTN_BLOCKS_MODERN;
    draw_field(ren, 20);
    rgb_at(s, 10, 430, &r, &g, &b);
    CHECK(r == 0x2e && g == 0xc8 && b == 0x4a);
    CHECK(!from_sheet(s, 5 * 20 + 7, 3 * 20 + 7, 5 * 20 + 7, 30 + 3 * 20 + 7));

    gtn_render_quit();
    SDL_DestroyRenderer(ren);
    SDL_FreeSurface(s);
    stbi_image_free(sheet);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    pref_path = argv[1];
    static gtn_app a;
    char path[1200];
    snprintf(path, sizeof path, "%ssettings.ini", pref_path);
    remove(path);
    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    CHECK(SDL_Init(SDL_INIT_EVENTS) == 0);

    check_render();

    /* default modern; nonsense in the file stays modern */
    gtn_settings_load(&a.settings);
    CHECK(a.settings.blocks == GTN_BLOCKS_MODERN);
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (f) { fputs("blocks=ORIGINAL\n", f); fclose(f); }
    gtn_settings loaded;
    gtn_settings_load(&loaded);
    CHECK(loaded.blocks == GTN_BLOCKS_MODERN);
    remove(path);
    snprintf(a.settings.nick, sizeof a.settings.nick, "Blocks");
    snprintf(a.settings.last_version, sizeof a.settings.last_version, "fixture");

    /* Options: the last row, from a controller before anyone joined; saved at once */
    a.screen = SCR_TITLE;
    press(&a, SDL_CONTROLLER_BUTTON_X);
    CHECK(a.screen == SCR_INPUT && gtn_local_count(&a) == 0);
    press(&a, SDL_CONTROLLER_BUTTON_DPAD_UP);
    CHECK(a.options_row == 6);
    press(&a, SDL_CONTROLLER_BUTTON_A);
    gtn_settings_load(&loaded);
    CHECK(a.settings.blocks == GTN_BLOCKS_ORIGINAL && gtn_block_style == GTN_BLOCKS_ORIGINAL && loaded.blocks == GTN_BLOCKS_ORIGINAL);
    press(&a, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    gtn_settings_load(&loaded);
    CHECK(a.settings.blocks == GTN_BLOCKS_MODERN && gtn_block_style == GTN_BLOCKS_MODERN && loaded.blocks == GTN_BLOCKS_MODERN);
    press(&a, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    press(&a, SDL_CONTROLLER_BUTTON_Y);
    CHECK(a.settings.blocks == GTN_BLOCKS_MODERN);
    press(&a, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    CHECK(a.options_row == 0);

    SDL_Quit();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
