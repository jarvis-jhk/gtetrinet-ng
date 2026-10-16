/*
 * gtetrinet-ng — TetriNET protocol pieces shared by client and server.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tn_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *id; int cell; const char *name; } sbinfo[TN_SB_COUNT] = {
    {"cs1", -1, "1 Line Added"},
    {"cs2", -1, "2 Lines Added"},
    {"cs4", -1, "4 Lines Added"},
    {"a",    6, "Add Line"},
    {"c",    7, "Clear Line"},
    {"n",    8, "Nuke Field"},
    {"r",    9, "Clear Random"},
    {"s",   10, "Switch Fields"},
    {"b",   11, "Clear Specials"},
    {"g",   12, "Block Gravity"},
    {"q",   13, "Blockquake"},
    {"o",   14, "Block Bomb"},
    {"i",   15, "Immunity"},
    {"v",   16, "Clear Column"},
    {"m",   17, "Mutate Pieces"},
    {"d",   18, "Darkness"},
    {"f",   19, "Confusion"},
};

static const char cellchars[] = "012345acnrsbgqoivmdf";

const char *tn_sb_id(int sb) { return sb >= 0 && sb < TN_SB_COUNT ? sbinfo[sb].id : "?"; }
int tn_sb_cell(int sb) { return sb >= 0 && sb < TN_SB_COUNT ? sbinfo[sb].cell : -1; }
const char *tn_sb_name(int sb) { return sb >= 0 && sb < TN_SB_COUNT ? sbinfo[sb].name : "?"; }

int tn_sb_from_id(const char *id)
{
    for (int i = 0; i < TN_SB_COUNT; i++)
        if (strcmp(id, sbinfo[i].id) == 0) return i;
    return -1;
}

int tn_sb_from_cell(int cell)
{
    for (int i = 0; i < TN_SB_COUNT; i++)
        if (sbinfo[i].cell == cell) return i;
    return -1;
}

char tn_cell_char(int cell) { return cell >= 0 && cell < TN_CELLS ? cellchars[cell] : '0'; }

int tn_cell_from_char(char c)
{
    const char *p = c ? strchr(cellchars, c) : NULL;
    return p ? (int)(p - cellchars) : 0;
}

static void iphash(const unsigned char ip[4], char *h, size_t hsz)
{
    snprintf(h, hsz, "%d", ip[0] * 54 + ip[1] * 41 + ip[2] * 29 + ip[3] * 17);
}

void tn_login_encode(const char *plain, const unsigned char ip[4], int seed, char *out, size_t outsz)
{
    char h[16];
    iphash(ip, h, sizeof h);
    size_t hl = strlen(h), n = strlen(plain), o = 0;
    unsigned int prev = (unsigned int)seed & 0xFF;
    /* GTetrinet starts with 0; real TetriNET uses a random first byte. Either decodes. */
    if (o + 3 > outsz) { if (outsz) out[0] = 0; return; }
    o += (size_t)snprintf(out + o, outsz - o, "%02X", prev);
    for (size_t i = 0; i < n && o + 3 <= outsz; i++) {
        unsigned int c = ((prev + ((unsigned char)plain[i])) % 255) ^ (unsigned char)h[i % hl];
        o += (size_t)snprintf(out + o, outsz - o, "%02X", c & 0xFF);
        prev = c & 0xFF;
    }
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int tn_login_decode(const char *hex, char *out, size_t outsz)
{
    static const char *prefixes[] = {"tetrisstart ", "tetrifaster "};
    unsigned char enc[512];
    size_t n = 0;
    for (const char *p = hex; p[0] && p[1] && n < sizeof enc; p += 2) {
        int a = hexval(p[0]), b = hexval(p[1]);
        if (a < 0 || b < 0) return -1;
        enc[n++] = (unsigned char)(a * 16 + b);
    }
    if (n < 13 || n - 1 >= outsz) return -1;
    for (int pi = 0; pi < 2; pi++) {
        const char *pre = prefixes[pi];
        size_t pl = strlen(pre);
        unsigned char key[16];
        for (size_t i = 0; i < pl; i++)
            key[i] = (unsigned char)(((enc[i] + (unsigned char)pre[i]) % 255) ^ enc[i + 1]);
        /* The key is the decimal hash string, 1..5 digits long, repeating. */
        for (size_t kl = 1; kl <= 6; kl++) {
            int ok = 1;
            for (size_t i = kl; i < pl && ok; i++)
                if (key[i] != key[i % kl]) ok = 0;
            for (size_t i = 0; i < kl && ok; i++)
                if (key[i] < '0' || key[i] > '9') ok = 0;
            if (!ok) continue;
            for (size_t i = 0; i + 1 < n; i++) {
                int v = ((enc[i + 1] ^ key[i % kl]) - enc[i] + 255) % 255;
                out[i] = (char)v;
            }
            out[n - 1] = 0;
            if (strncmp(out, pre, pl) == 0) return 0;
        }
    }
    return -1;
}

void tn_field_encode(const tn_field now, const tn_field sent, int full, char *out, size_t outsz)
{
    char buf[TN_FIELD_W * TN_FIELD_H * 3 + 64];
    size_t o = 0;
    if (!full) {
        /* A diff names its cell as '!' + cell, which only reaches the classic 15 before it
         * collides with the full-field characters; a TN2 special therefore sends it whole. */
        for (int y = 0; y < TN_FIELD_H && !full; y++)
            for (int x = 0; x < TN_FIELD_W; x++)
                if (now[y][x] != sent[y][x] && now[y][x] >= 15) { full = 1; break; }
    }
    if (!full) {
        for (int cell = 0; cell < 15; cell++) {
            int started = 0;
            for (int y = 0; y < TN_FIELD_H; y++)
                for (int x = 0; x < TN_FIELD_W; x++)
                    if (now[y][x] != sent[y][x] && now[y][x] == cell) {
                        if (!started) { buf[o++] = (char)('!' + cell); started = 1; }
                        buf[o++] = (char)(x + '3');
                        buf[o++] = (char)(y + '3');
                    }
        }
        if (o == 0) { if (outsz) out[0] = 0; return; }
        if (o >= TN_FIELD_W * TN_FIELD_H) full = 1;
    }
    if (full) {
        o = 0;
        for (int y = 0; y < TN_FIELD_H; y++)
            for (int x = 0; x < TN_FIELD_W; x++)
                buf[o++] = tn_cell_char(now[y][x]);
    }
    buf[o] = 0;
    snprintf(out, outsz, "%s", buf);
}

int tn_field_apply(tn_field f, const char *s)
{
    if (!s || !*s) return 0;
    if (*s >= '0') {
        char *p = &f[0][0];
        int i = 0;
        for (; *s && i < TN_FIELD_W * TN_FIELD_H; s++, i++) p[i] = (char)tn_cell_from_char(*s);
        return i == TN_FIELD_W * TN_FIELD_H ? 0 : -1;
    }
    int cell = 0;
    for (; *s; s++) {
        if (*s < '0' && *s >= '!') { cell = *s - '!'; continue; }
        if (!s[1]) return -1;
        int x = s[0] - '3', y = s[1] - '3';
        s++;
        if (x >= 0 && x < TN_FIELD_W && y >= 0 && y < TN_FIELD_H && cell < 15) f[y][x] = (char)cell;
    }
    return 0;
}

void tn_rules_default(tn_rules *r)
{
    /* tetrinetx's defaults */
    static const int bf[7] = {15, 14, 14, 14, 14, 14, 15};
    static const int sf[9] = {32, 18, 1, 11, 3, 14, 1, 6, 14};
    memset(r, 0, sizeof *r);
    r->stack = 0;
    r->level = 1;
    r->lines_per_level = 2;
    r->level_inc = 1;
    r->lines_per_special = 1;
    r->specials_added = 1;
    r->special_capacity = 18;
    memcpy(r->blockfreq, bf, sizeof bf);
    memcpy(r->specialfreq, sf, sizeof sf);
    r->average_levels = 0;
    r->classic = 1;
}

void tn_rules_tn2(tn_rules *r)
{
    /* the classic mix scaled to 79 %, the rest for the five new ones */
    static const int sf[TN_SPECIALS] = {24, 14, 1, 9, 3, 11, 1, 5, 11, 5, 6, 4, 3, 3};
    memcpy(r->specialfreq, sf, sizeof sf);
    r->tn2 = 1;
    r->hold = 1;
}

/* special frequency characters: '1'..'9' classic, 'A'..'E' TetriNET 2 */
static char sf_char(int i) { return i < 9 ? (char)('1' + i) : (char)('A' + i - 9); }
static int sf_index(char c)
{
    if (c >= '1' && c <= '9') return c - '1';
    if (c >= 'A' && c <= 'E') return c - 'A' + 9;
    return -1;
}

void tn_rules_format(const tn_rules *r, char *out, size_t outsz)
{
    char bf[128], sf[128];
    size_t o = 0;
    for (int i = 0; i < 7; i++)
        for (int k = 0; k < r->blockfreq[i] && o < 100; k++) bf[o++] = (char)('1' + i);
    bf[o] = 0;
    o = 0;
    /* classic clients get the classic specials only; the TN2 string carries all of them */
    for (int i = 0; i < TN_CLASSIC_SPECIALS; i++)
        for (int k = 0; k < r->specialfreq[i] && o < 100; k++) sf[o++] = sf_char(i);
    if (o == 0) sf[o++] = '1';
    sf[o] = 0;
    int n = snprintf(out, outsz, "%d %d %d %d %d %d %d %s %s %d %d", r->stack, r->level,
                     r->lines_per_level, r->level_inc, r->lines_per_special, r->specials_added,
                     r->special_capacity, bf, sf, r->average_levels, r->classic);
    if (!r->tn2 || n < 0 || (size_t)n >= outsz) return;
    o = 0;
    for (int i = 0; i < TN_SPECIALS; i++)
        for (int k = 0; k < r->specialfreq[i] && o < 100; k++) sf[o++] = sf_char(i);
    if (o == 0) sf[o++] = '1';
    sf[o] = 0;
    snprintf(out + n, outsz - (size_t)n, " tn2 %d %s", r->hold ? 1 : 0, sf);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int tn_rules_parse(tn_rules *r, const char *args)
{
    char bf[128] = "", sf[128] = "", ext[8] = "", xsf[128] = "";
    int hold = 0;
    tn_rules d;
    tn_rules_default(&d);
    *r = d;
    int n = sscanf(args, "%d %d %d %d %d %d %d %127s %127s %d %d %7s %d %127s", &r->stack, &r->level,
                   &r->lines_per_level, &r->level_inc, &r->lines_per_special, &r->specials_added,
                   &r->special_capacity, bf, sf, &r->average_levels, &r->classic, ext, &hold, xsf);
    if (n < 9) { *r = d; return -1; }
    if (n == 14 && strcmp(ext, "tn2") == 0) {
        r->tn2 = 1;
        r->hold = hold != 0;
        snprintf(sf, sizeof sf, "%s", xsf);
    }
    r->stack = clampi(r->stack, 0, TN_FIELD_H - 1);
    r->level = clampi(r->level, 0, 100);
    if (r->lines_per_level < 1) r->lines_per_level = 1;
    if (r->lines_per_special < 1) r->lines_per_special = 1;
    r->specials_added = clampi(r->specials_added, 0, 100);
    r->special_capacity = clampi(r->special_capacity, 0, 255);
    memset(r->blockfreq, 0, sizeof r->blockfreq);
    memset(r->specialfreq, 0, sizeof r->specialfreq);
    for (int i = 0; bf[i]; i++) if (bf[i] >= '1' && bf[i] <= '7') r->blockfreq[bf[i] - '1']++;
    for (int i = 0; sf[i]; i++) {
        int k = sf_index(sf[i]);
        if (k >= 0 && (r->tn2 || k < TN_CLASSIC_SPECIALS)) r->specialfreq[k]++;
    }
    int bs = 0, ss = 0;
    for (int i = 0; i < 7; i++) bs += r->blockfreq[i];
    for (int i = 0; i < TN_SPECIALS; i++) ss += r->specialfreq[i];
    if (bs == 0) memcpy(r->blockfreq, d.blockfreq, sizeof d.blockfreq);
    if (ss == 0) memcpy(r->specialfreq, d.specialfreq, sizeof d.specialfreq);
    return 0;
}

const char *tn_word(const char *s, char *word, size_t wordsz)
{
    while (*s == ' ') s++;
    size_t i = 0;
    while (*s && *s != ' ') {
        if (i + 1 < wordsz) word[i++] = *s;
        s++;
    }
    if (wordsz) word[i] = 0;
    if (*s == ' ') s++;
    return s;
}
