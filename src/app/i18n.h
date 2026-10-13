/*
 * gtetrinet-ng — the game's own UI language. English strings are the keys; a missing
 * translation shows the English text. Protocol, server and operating-system texts are not
 * ours and pass through unchanged.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef GTN_I18N_H
#define GTN_I18N_H

#include <stddef.h>

enum gtn_lang { GTN_LANG_EN, GTN_LANG_DE, GTN_LANG_COUNT };
/* What the player chose in the options; stored as lang=auto|en|de. */
enum gtn_lang_choice { LANG_AUTO, LANG_EN, LANG_DE, LANG_CHOICES };

/* Marks a literal for translation where it is stored, not looked up (tests scan for it). */
#define N_(s) s

void gtn_lang_set(enum gtn_lang l);
enum gtn_lang gtn_lang_get(void);
/* The system's preferred supported language, English when none matches. */
enum gtn_lang gtn_lang_detect(void);
/* Sets the language a stored choice stands for. */
void gtn_lang_apply(int choice);
const char *gtn_lang_code(int choice);      /* "auto", "en", "de" */
int gtn_lang_parse(const char *code);       /* LANG_AUTO for anything unknown */
/* Endonym: a language is always named in itself, so it can be found from any other. */
const char *gtn_lang_endonym(enum gtn_lang l);

const char *gtn_tr(const char *en);
/* Special names from the core, which stays language-free. */
const char *gtn_tr_special(int sb);
/* Lines the core client writes into its chat and attack logs. */
void gtn_tr_log(const char *line, char *out, size_t n);

/* For tests: the translation table. */
int gtn_i18n_count(void);
const char *gtn_i18n_key(int i);
const char *gtn_i18n_value(int i, enum gtn_lang l);

#endif
