/* The pane kit: the shell and the vocabulary Mary's system apps share (Threads, Ambient, Abilities),
 * so that the three read as one family and line up — the same header, the same content column, the
 * same cards, one label column, capsules that are measured rather than guessed. An app cuts a sidebar
 * (lp_sidebar) for its sections, a header for the one it shows, and lays the rest out in the content
 * column. Every painter here skips what the clip cannot show and does nothing in the EVENT pass
 * beyond advancing the cursor, so a card's height is the same in both passes. */
#ifndef MARYUI_LP_PANE_H
#define MARYUI_LP_PANE_H

#include "maryui/lp_ui.h"

#define LP_PANE_INSET 20.0f          /* the content column's side inset, and the header's */
#define LP_PANE_HEADER_H 64.0f       /* the title and its subtitle */
#define LP_PANE_HEADER_NOTE_H 22.0f  /* the serif line under them, when there is one */
#define LP_PANE_BAR_H 40.0f          /* a strip of controls under the header */
#define LP_PANE_LABEL_W 120.0f       /* a row's label column, right-aligned */
#define LP_PANE_GUTTER 12.0f         /* between the label column and the value */
#define LP_PANE_ROW 20.0f            /* one label/value row */
#define LP_PANE_LEADING 17.0f        /* a paragraph's line at text.sm */
#define LP_PANE_CARD_PAD 16.0f       /* inside a card */
#define LP_PANE_CARD_TITLE_H 30.0f   /* a card's title band */
#define LP_PANE_CARD_GAP 12.0f       /* between cards */

extern const lp_color LP_PANE_GOLD, LP_PANE_SAGE, LP_PANE_BLUE, LP_PANE_MAUVE, LP_PANE_RED;

typedef struct lp_pane_header {
    const char *title;       /* text.xl bold, embossed, as Settings' headings */
    const char *subtitle;    /* one line under it, tertiary; NULL for none */
    const char *note;        /* a serif italic line under that (a package's summary); NULL for none */
    int live;                /* -1: no dot; 0 / 1: the dot at the subtitle's right, with `status` beside it */
    const char *status;      /* the word beside the dot ("threadd", "maryd is away"); NULL for none */
} lp_pane_header;

/* Cuts the header off the top of *area and paints it. Returns the rect at the right of the title line,
 * LP_PANE_INSET in from the edge, for the caller's controls (lay them out from its right end). */
lp_rect lp_pane_header_paint(lp_ctx *ctx, lp_rect *area, const lp_pane_header *h);
/* A strip of controls under the header: cuts LP_PANE_BAR_H off the top of *area, returns it inset. */
lp_rect lp_pane_bar(lp_ctx *ctx, lp_rect *area);
/* The content column of a pane: LP_PANE_INSET both sides, LP_SPACE_4 above and below. */
lp_rect lp_pane_content(lp_rect area);

/* A card's frame and title band over `box` (the body starts LP_PANE_CARD_TITLE_H down, LP_PANE_CARD_PAD in). */
void lp_pane_card_frame(lp_ctx *ctx, lp_rect box, const char *title);
/* A card cut off the top of *column with room for body_h under its title band, then LP_PANE_CARD_GAP.
 * Returns the body rect; *box (optional) receives the card's own rect, for accessories in its title band. */
lp_rect lp_pane_card(lp_ctx *ctx, lp_rect *column, const char *title, float body_h, lp_rect *box);

/* Lines a text takes across w at text.sm, estimated the same way in both passes; and the paragraph height. */
int lp_pane_lines_of(const char *text, float w);
float lp_pane_paragraph_h(const char *text, float w);
/* A label/value row at (x, *y) across w; advances *y by LP_PANE_ROW. A NULL or empty value reads "—". */
void lp_pane_row(lp_ctx *ctx, float x, float *y, float w, const char *label, const char *value);
/* A paragraph at (x, *y) across w; advances *y by its height (measured when drawing, estimated otherwise). */
void lp_pane_paragraph(lp_ctx *ctx, float x, float *y, float w, const char *text, lp_color color, int italic);
/* One line in the mono face, ellipsized to w. */
void lp_pane_mono(lp_ctx *ctx, float x, float y, float w, const char *text, lp_color color);
/* A capsule with a word in it, its left edge at x, measured; returns its width (estimated in the EVENT pass). */
float lp_pane_capsule(lp_ctx *ctx, float x, float y, const char *label, lp_color color);
/* The same with its right edge at `right`. */
float lp_pane_capsule_right(lp_ctx *ctx, float right, float y, const char *label, lp_color color);
/* A clickable filter chip; *size receives its size. 1 when clicked. */
int lp_pane_chip(lp_ctx *ctx, lp_id id, float x, float y, const char *label, int selected, lp_size *size);
void lp_pane_live_dot(lp_ctx *ctx, float x, float y, int on);
/* An empty state, centred in the area. */
void lp_pane_empty(lp_ctx *ctx, lp_rect area, const char *text);

#endif
