/*
 * gtetrinet-ng — TetriNET protocol pieces shared by client and server.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The wire format is the one of TetriNET 1.13 (and its "tetrifast" variant): text messages
 * terminated by a 0xFF byte, the first one obfuscated with the server's IPv4 address.
 * Behaviour is taken from GTetrinet (Ka-shu Wong and contributors) so we stay compatible with
 * tetrinetx and other classic servers.
 */
#ifndef TN_PROTOCOL_H
#define TN_PROTOCOL_H

#include <stddef.h>

#define TN_FIELD_W 12
#define TN_FIELD_H 22
/* Classic servers have player slots 1..6. Our own server can open slots 7 and 8 for local
 * splitscreen; classic clients ignore those (they bounds-check the slot number). */
#define TN_MAX_SLOTS 8
#define TN_CLASSIC_SLOTS 6
#define TN_PORT 31457

#define TN_NICK_MAX 32
#define TN_TEAM_MAX 32
#define TN_MSG_MAX 2048

typedef char tn_field[TN_FIELD_H][TN_FIELD_W];

/* Cell values: 0 empty, 1..5 colours, 6..14 classic specials (a c n r s b g q o), 15..19 the
 * TetriNET 2 specials (i v m d f). Those five only exist when our own server announces the TN2
 * rules (see tn_rules.tn2); a classic server never sends them and we never send them to one. */
enum {
    TN_SB_ADDALL1, TN_SB_ADDALL2, TN_SB_ADDALL4,
    TN_SB_ADDLINE, TN_SB_CLEARLINE, TN_SB_NUKE, TN_SB_CLEARRANDOM, TN_SB_SWITCH,
    TN_SB_CLEARSPECIAL, TN_SB_GRAVITY, TN_SB_QUAKE, TN_SB_BOMB,
    TN_SB_IMMUNITY, TN_SB_CLEARCOLUMN, TN_SB_MUTATE, TN_SB_DARKNESS, TN_SB_CONFUSION,
    TN_SB_COUNT
};
#define TN_CELLS 20            /* cell values 0..19 */
#define TN_SPECIALS 14         /* field specials: 9 classic + 5 TetriNET 2 */
#define TN_CLASSIC_SPECIALS 9

/* protocol id ("cs1", "a", ...), cell value (-1 for the classic-mode line adds), display name */
const char *tn_sb_id(int sb);
int tn_sb_cell(int sb);
const char *tn_sb_name(int sb);
int tn_sb_from_id(const char *id);   /* -1 if unknown */
int tn_sb_from_cell(int cell);       /* -1 if not a special */
char tn_cell_char(int cell);         /* '0'..'5', 'a'.. */
int tn_cell_from_char(char c);       /* 0 for anything unknown */

/* Login: "tetrisstart <nick> 1.13" (or "tetrifaster ...") obfuscated with the server address. */
void tn_login_encode(const char *plain, const unsigned char ip[4], int seed, char *out, size_t outsz);
/* Recovers the plain text without knowing the address (only the known prefix is needed).
 * Returns 0 on success, -1 if it does not decode to a login. */
int tn_login_decode(const char *hex, char *out, size_t outsz);

/* Field updates: "f <slot> <data>" where <data> is either the whole field (264 cell chars) or
 * a diff of runs "!+cell" followed by (x+'3', y+'3') pairs. */
void tn_field_encode(const tn_field now, const tn_field sent, int full, char *out, size_t outsz);
/* Applies <data> to a field. Returns 0, or -1 if it was malformed (partially applied). */
int tn_field_apply(tn_field f, const char *data);

/* newgame parameters */
typedef struct tn_rules {
    int stack;              /* initial garbage height */
    int level;              /* starting level */
    int lines_per_level;
    int level_inc;
    int lines_per_special;
    int specials_added;
    int special_capacity;
    int blockfreq[7];       /* percentages, sum 100: I O J L Z S T as in the classic client */
    int specialfreq[TN_SPECIALS]; /* percentages, sum 100: a c n r s b g q o, then i v m d f */
    int average_levels;
    int classic;            /* 2/3/4-line clears add lines to everybody */
    /* TetriNET 2 rules, an extension of our own server: "newgame ... tn2 <hold> <freq>" after
     * the classic fields, which classic clients ignore. tn2 == 0 everywhere else. */
    int tn2;
    int hold;               /* the hold piece is allowed */
} tn_rules;

void tn_rules_default(tn_rules *r);
/* The TetriNET 2 set: hold on, all fourteen specials. */
void tn_rules_tn2(tn_rules *r);
/* Host settings beyond the rule set. Piece mixes: classic (tetrinetx), no S/Z pieces,
 * more long bars. Returns -1 for an unknown mix and leaves r alone. */
enum tn_piece_mix { TN_PIECES_CLASSIC, TN_PIECES_NO_SZ, TN_PIECES_LONG_BARS, TN_PIECE_MIXES };
int tn_rules_pieces(tn_rules *r, int mix);
/* Which mix r->blockfreq is, or -1 for any other. */
int tn_rules_piece_mix(const tn_rules *r);
const char *tn_piece_mix_code(int mix);
int tn_piece_mix_parse(const char *code);
/* "newgame" argument string and its parser (parser returns -1 on garbage). */
void tn_rules_format(const tn_rules *r, char *out, size_t outsz);
int tn_rules_parse(tn_rules *r, const char *args);

/* Splits off the first space-separated word: returns the rest (never NULL). */
const char *tn_word(const char *s, char *word, size_t wordsz);

#endif
