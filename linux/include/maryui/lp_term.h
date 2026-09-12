/* Terminal's screen: libvterm parsing what a program writes into a grid of
 * cells, keys turned into the bytes terminal programs expect, dirty rows for
 * the painter, a scrollback of the lines that fall off the top, the title a
 * program sets, and the text of a selection. The colours start from Liquid
 * Platinum's — ink on the well — with a palette that stays legible on it.
 * Without libvterm in the build (HAVE_VTERM) lp_term_new returns NULL and
 * lp_term_available says so. Linux only (PARITY D15). */
#ifndef MARYUI_LP_TERM_H
#define MARYUI_LP_TERM_H

#include <stddef.h>
#include <stdint.h>

#include "maryui/lp_types.h"

#define LP_TERM_SCROLLBACK 2000

typedef struct lp_term lp_term;

typedef struct lp_term_cell {
    char text[32];          /* UTF-8 (a base character and its combining marks); "" when empty */
    int width;              /* 2 for a wide character, whose right half is the next cell */
    lp_color fg, bg;        /* reverse video already applied */
    int bg_default;         /* the terminal's own background: let the well show */
    unsigned bold : 1, italic : 1, underline : 2, strike : 1, reverse : 1;
} lp_term_cell;

/* Bytes the terminal sends back — keys, replies to a program's queries: write them to the pty. */
typedef void (*lp_term_output_fn)(const char *bytes, size_t len, void *user);

int lp_term_available(void);
lp_term *lp_term_new(int rows, int cols, lp_term_output_fn output, void *user);
void lp_term_free(lp_term *t);
/* Rewraps the screen and scrollback to the new width. */
void lp_term_resize(lp_term *t, int rows, int cols);
void lp_term_size(const lp_term *t, int *rows, int *cols);
/* What the program wrote. */
void lp_term_feed(lp_term *t, const char *bytes, size_t len);
/* A key press: its xkb keysym, its text, LP_MOD_* modifiers. 1 when it sent something. */
int lp_term_key(lp_term *t, uint32_t keysym, const char *utf8, uint32_t mods);
/* Pasted text, bracketed when the program asked for that, newlines sent as Return. */
void lp_term_paste(lp_term *t, const char *text, size_t len);

/* A cell. Rows >= 0 are the screen; -1 is the newest line in the scrollback and
 * -lp_term_scrollback_lines() the oldest. Anything outside reads as an empty cell. */
void lp_term_cell_at(const lp_term *t, int row, int col, lp_term_cell *out);
int lp_term_scrollback_lines(const lp_term *t);
void lp_term_cursor(const lp_term *t, int *row, int *col, int *visible);
/* rows[r] = 1 for each screen row changed since the last call (n entries). Returns whether any did. */
int lp_term_take_damage(lp_term *t, unsigned char *rows, int n);
/* The title a program set (OSC 0 or 2), "" if none. *changed is 1 the first time a new one is read. */
const char *lp_term_title(lp_term *t, int *changed);
/* The text from (row0, col0) to (row1, col1) inclusive, rows as lp_term_cell_at, in either order:
 * lines joined with \n, trailing blanks dropped. NUL-terminated; returns the length. */
size_t lp_term_text(const lp_term *t, int row0, int col0, int row1, int col1, char *out, size_t n);
/* A full-screen program is on the alternate screen (no scrollback there). */
int lp_term_altscreen(const lp_term *t);
/* 1 when the bell rang since the last call. */
int lp_term_take_bell(lp_term *t);

#endif
