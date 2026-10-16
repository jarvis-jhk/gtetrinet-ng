/*
 * gtetrinet-ng — UI translations.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "i18n.h"
#include "tn_protocol.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static enum gtn_lang current = GTN_LANG_EN;

/* {English, Deutsch}. test_i18n draws every screen at 1280x800 and fails on any cut text, so
 * German stays as short as English where space is tight (STF/ZLN like LVL/LNS). */
static const char *const strings[][GTN_LANG_COUNT] = {
    /* title */
    {"Press A", "Mit A"},
    {"to join", "spielen"},
    {"Keyboard", "Tastatur"},
    {"No pad!", "Kein Pad!"},
    {"Gamepad", "Gamepad"},
    {"TetriNET for up to 8 players - version %s", "TetriNET für bis zu 8 Spieler - Version %s"},
    {"Play here (everyone on this screen)", "Hier spielen (alle an diesem Bildschirm)"},
    {"Play online", "Online spielen"},
    {"What's new", "Neuigkeiten"},
    {"Quit", "Beenden"},
    {"Everyone who wants to play: press A on your controller (or Enter)",
     "Wer mitspielen will: A am Controller drücken (oder Enter)"},
    {"A/Enter: join / choose   B: leave   Up/Down: menu   X/S: options",
     "A/Enter: beitreten/wählen   B: gehen   Hoch/Runter: Menü   X/S: Optionen"},
    {"Your name: press Y on your controller (or N on the keyboard)",
     "Dein Name: Y am Controller drücken (oder N auf der Tastatur)"},
    /* names */
    {"Name for P%d", "Name für P%d"},
    {"Everyone sees it, online too. One word, at most 15 characters.",
     "Alle sehen ihn, auch online. Ein Wort, höchstens 15 Zeichen."},
    {"Saved for this seat; the next game uses it.", "Gespeichert für diesen Platz; gilt ab dem nächsten Spiel."},
    {"D-pad/A: type   X: delete   Y: Aa   Start: save   B: back",
     "Kreuz/A: tippen   X: löschen   Y: Aa   Start: speichern   B: zurück"},
    {"A name needs at least one letter or digit", "Ein Name braucht mindestens einen Buchstaben oder eine Ziffer"},
    {"%s is already playing here", "%s spielt hier schon mit"},
    {"The name works for this run, but could not be saved",
     "Der Name gilt jetzt, konnte aber nicht gespeichert werden"},
    /* options */
    {"Options", "Optionen"},
    {"Movement repeat", "Bewegungswiederholung"},
    {"Holding Left/Right moves repeatedly after a short delay.",
     "Links/Rechts halten bewegt nach kurzer Pause wiederholt."},
    {"First repeat: %d ms   Next repeats: every %d ms", "Erste Wiederholung: %d ms   Danach: alle %d ms"},
    {"Normal", "Normal"},
    {"Relaxed", "Gemütlich"},
    {"Fast", "Schnell"},
    {"Language", "Sprache"},
    {"Automatic (%s)", "Automatisch (%s)"},
    {"Saved for all local players, keyboard and controllers.",
     "Gespeichert für alle Spieler hier, Tastatur und Controller."},
    {"Up/Down: choose   Left/Right/A: change   Y: default   B/Esc/Start: back",
     "Hoch/Runter: wählen   Links/Rechts/A: ändern   Y: Standard   B/Start: zurück"},
    {"Movement repeat works for this run, but could not be saved",
     "Bewegungswiederholung gilt jetzt, konnte aber nicht gespeichert werden"},
    {"The language works for this run, but could not be saved",
     "Die Sprache gilt jetzt, konnte aber nicht gespeichert werden"},
    {"Controller buttons", "Controller-Tasten"},
    {"Standard", "Standard"},
    {"Custom", "Eigene"},
    {"The controller buttons work for this run, but could not be saved",
     "Die Controller-Tasten gelten jetzt, konnten aber nicht gespeichert werden"},
    {"Rotate clockwise", "Im Uhrzeigersinn drehen"},
    {"Rotate counterclockwise", "Gegen Uhrzeigersinn drehen"},
    {"Hard drop", "Fallen lassen"},
    {"Special on target", "Spezial aufs Ziel"},
    {"Special on yourself", "Spezial auf dich"},
    {"Previous target", "Voriges Ziel"},
    {"Next target", "Nächstes Ziel"},
    {"Discard special", "Spezial wegwerfen"},
    {"In a game, for every controller. Left/Right/Down move, Start pauses.",
     "Im Spiel, für jeden Controller. Links/Rechts/Runter bewegen, Start pausiert."},
    {"Press the controller button for: %s", "Controller-Taste drücken für: %s"},
    {"A button that is taken swaps with the old one.", "Eine belegte Taste tauscht mit der alten."},
    {"Press a button on the controller   Start or any key: cancel",
     "Taste am Controller drücken   Start oder Tastatur: abbrechen"},
    {"Up/Down: choose   A: new button   Y: all standard   B/Start: back",
     "Hoch/Runter: wählen   A: neue Taste   Y: alle Standard   B/Start: zurück"},
    /* online */
    {"Server address (TetriNET 1.13 or gtetrinet-ng-server)",
     "Serveradresse (TetriNET 1.13 oder gtetrinet-ng-server)"},
    {"DEL", "ENTF"},
    {"GO", "LOS"},
    {"Mode: %s (Y to change)", "Modus: %s (Y zum Ändern)"},
    {"classic TetriNET", "klassisches TetriNET"},
    {"Everybody who joined plays on that server, each as their own player.",
     "Alle, die beigetreten sind, spielen dort, jeder als eigener Spieler."},
    {"D-pad/A: type   X: delete   Start/GO/Enter: connect   B/Esc: back",
     "Kreuz/A: tippen   X: löschen   Start/LOS/Enter: verbinden   B/Esc: zurück"},
    {"Connecting to %s ...", "Verbinde mit %s ..."},
    {"the game on this machine", "dem Spiel auf diesem Gerät"},
    {"B: cancel", "B: abbrechen"},
    /* lobby */
    {"Game on this machine (port %d) - others on your network can join it",
     "Spiel auf diesem Gerät (Port %d) - andere im Netzwerk können beitreten"},
    {"Online: %s", "Online: %s"},
    {"Local game", "Lokales Spiel"},
    {"Online game", "Online-Spiel"},
    {"- free -", "- frei -"},
    {"moderator", "Moderator"},
    {"%s wins!", "%s gewinnt!"},
    {"Winners so far:", "Bisherige Gewinner:"},
    {"Team ", "Team "},
    {"Press Start or A to begin the game (%s is the moderator).",
     "Start oder A beginnt das Spiel (%s ist Moderator)."},
    {"Waiting for the moderator (%s) to start the game.", "Warte, bis der Moderator (%s) das Spiel startet."},
    {"Start/A: start   Y: team   B: leave/end   Back: news   New pad A: join",
     "Start/A: los   Y: Team   B: gehen/Ende   Back: News   Neues Pad A: beitreten"},
    /* news */
    {"Up/Down: scroll   LB/RB: page   A/B: close", "Hoch/Runter: blättern   LB/RB: Seite   A/B: schließen"},
    {"The changelog could not be found next to the game.",
     "Die Änderungsliste wurde neben dem Spiel nicht gefunden."},
    {"(The changelog is only available in English.)", "(Die Änderungsliste gibt es nur auf Englisch.)"},
    /* game */
    {"WINNER", "SIEGER"},
    {"OUT", "RAUS"},
    {"NEXT", "DANACH"},
    {"LVL %d", "STF %d"},
    {"LNS %d", "ZLN %d"},
    {"TARGET", "ZIEL"},
    {"yourself", "du selbst"},
    {"+HELP", "+HILFT"},
    {"-HURT", "-SCHADET"},
    {"?MIX", "?GEMISCHT"},
    {"?UNKNOWN", "?UNBEKANNT"},
    {"+You", "+Dir"},
    {"+Them", "+Ziel"},
    {"-You", "-Dir"},
    {"-Them", "-Ziel"},
    {"?Self", "?Selbst"},
    {"?Both", "?Beide"},
    {"PAUSED", "PAUSE"},
    {"Start: continue   Back+Start: end the game", "Start: weiter   Back+Start: Spiel beenden"},
    /* status line */
    {"Controller of %s disconnected - press A on any controller to take over",
     "Controller von %s getrennt - A an einem Controller drücken übernimmt"},
    {"%s has a controller again", "%s hat wieder einen Controller"},
    {"Eight players is the most that fit", "Mehr als acht Spieler passen nicht"},
    {"Could not start a game here: %s", "Konnte hier kein Spiel starten: %s"},
    {"Enter a server address first", "Bitte zuerst eine Serveradresse eingeben"},
    {"Only the moderator (the first player on the server) can start",
     "Nur der Moderator (erster Spieler auf dem Server) kann starten"},
    {"%s could not connect: %s", "%s konnte nicht verbinden: %s"},
    {"Disconnected", "Verbindung getrennt"},
    /* errors the core reports in its own words */
    {"connection closed", "Verbindung geschlossen"},
    {"disconnected", "getrennt"},
    {"the server refused the connection", "der Server hat die Verbindung abgelehnt"},
    {"send buffer overflow", "Sendepuffer übergelaufen"},
    {"message too long", "Nachricht zu lang"},
};

/* Core log lines with one %s each; the most specific template comes first. */
static const char *const templates[][GTN_LANG_COUNT] = {
    {"*** The game has started", "*** Das Spiel hat begonnen"},
    {"*** A game is in progress", "*** Ein Spiel läuft gerade"},
    {"*** The game is paused", "*** Das Spiel ist pausiert"},
    {"*** The game continues", "*** Das Spiel geht weiter"},
    {"*** The game has ended", "*** Das Spiel ist beendet"},
    {"*** Connected to %s", "*** Verbunden mit %s"},
    {"*** Disconnected: %s", "*** Verbindung getrennt: %s"},
    {"*** Team %s has won the game", "*** Team %s hat das Spiel gewonnen"},
    {"*** %s has won the game", "*** %s hat das Spiel gewonnen"},
    {"*** %s has been kicked", "*** %s wurde entfernt"},
    {"*** %s has joined", "*** %s ist beigetreten"},
    {"*** %s has left", "*** %s ist gegangen"},
    {"cannot find server '%s'", "Server '%s' nicht gefunden"},
    {"cannot connect to '%s'", "keine Verbindung zu '%s'"},
};

static const char *const specials_de[TN_SB_COUNT] = {
    "1 Zeile dazu", "2 Zeilen dazu", "4 Zeilen dazu", "Zeile dazu", "Zeile weg",
    "Feld leeren", "Zufällig löschen", "Felder tauschen", "Specials löschen",
    "Schwerkraft", "Blockbeben", "Blockbombe",
};

void gtn_lang_set(enum gtn_lang l) { current = l >= 0 && l < GTN_LANG_COUNT ? l : GTN_LANG_EN; }
enum gtn_lang gtn_lang_get(void) { return current; }

enum gtn_lang gtn_lang_detect(void)
{
    enum gtn_lang found = GTN_LANG_EN;
    SDL_Locale *list = SDL_GetPreferredLocales();
    for (SDL_Locale *l = list; l && l->language; l++) {
        if (!SDL_strcasecmp(l->language, "de")) { found = GTN_LANG_DE; break; }
        if (!SDL_strcasecmp(l->language, "en")) break;
    }
    SDL_free(list);
    return found;
}

void gtn_lang_apply(int choice)
{
    gtn_lang_set(choice == LANG_DE ? GTN_LANG_DE : choice == LANG_EN ? GTN_LANG_EN : gtn_lang_detect());
}

const char *gtn_lang_code(int choice)
{
    return choice == LANG_EN ? "en" : choice == LANG_DE ? "de" : "auto";
}

int gtn_lang_parse(const char *code)
{
    return !strcmp(code, "en") ? LANG_EN : !strcmp(code, "de") ? LANG_DE : LANG_AUTO;
}

const char *gtn_lang_endonym(enum gtn_lang l) { return l == GTN_LANG_DE ? "Deutsch" : "English"; }

const char *gtn_tr(const char *en)
{
    if (current == GTN_LANG_EN || !en) return en;
    for (size_t i = 0; i < sizeof strings / sizeof *strings; i++)
        if (!strcmp(strings[i][0], en)) return strings[i][current];
    return en;
}

const char *gtn_tr_special(int sb)
{
    if (current == GTN_LANG_DE && sb >= 0 && sb < TN_SB_COUNT) return specials_de[sb];
    return tn_sb_name(sb);
}

/* Matches line against a template with at most one %s; the capture goes to cap. */
static int match(const char *tpl, const char *line, char *cap, size_t n)
{
    const char *hole = strstr(tpl, "%s");
    if (!hole) return !strcmp(tpl, line) && (cap[0] = 0, 1);
    size_t pre = (size_t)(hole - tpl), post = strlen(hole + 2), len = strlen(line);
    if (len < pre + post + 1 || strncmp(line, tpl, pre) || strcmp(line + len - post, hole + 2)) return 0;
    snprintf(cap, n, "%.*s", (int)(len - pre - post), line + pre);
    return 1;
}

static int special_named(const char *line, size_t *len)
{
    for (int sb = 0; sb < TN_SB_COUNT; sb++) {
        size_t k = strlen(tn_sb_name(sb));
        if (!strncmp(line, tn_sb_name(sb), k) && line[k] == ' ') { *len = k; return sb; }
    }
    return -1;
}

void gtn_tr_log(const char *line, char *out, size_t n)
{
    char cap[256];
    if (current != GTN_LANG_EN) {
        for (size_t i = 0; i < sizeof templates / sizeof *templates; i++)
            if (match(templates[i][0], line, cap, sizeof cap)) {
                snprintf(out, n, templates[i][current], cap);
                return;
            }
        /* "<special> on <nick> by <nick|server>" and "<special> to All by <nick|server>" */
        size_t k = 0;
        int sb = special_named(line, &k);
        const char *by = sb >= 0 ? strstr(line + k, " by ") : NULL;
        if (sb >= 0 && by) {
            for (const char *later; (later = strstr(by + 4, " by ")); ) by = later;
            const char *from = by + 4;
            if (!strcmp(from, "server")) from = "Server";
            const char *rest = line + k;
            if (!strncmp(rest, " to All by ", 11)) {
                snprintf(out, n, "%s an alle von %s", gtn_tr_special(sb), from);
                return;
            }
            if (!strncmp(rest, " on ", 4) && by > rest + 4) {
                snprintf(out, n, "%s auf %.*s von %s", gtn_tr_special(sb), (int)(by - rest - 4), rest + 4, from);
                return;
            }
        }
    }
    snprintf(out, n, "%s", line);
}

int gtn_i18n_count(void) { return (int)(sizeof strings / sizeof *strings); }
const char *gtn_i18n_key(int i) { return strings[i][0]; }
const char *gtn_i18n_value(int i, enum gtn_lang l) { return strings[i][l]; }
