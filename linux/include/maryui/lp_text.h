/* Text through Pango: the three token font stacks (fontconfig picks the first
 * installed family, so the web's macOS names fall through to Inter, P052 and
 * JetBrains Mono on Linux), absolute pixel sizes, weights, the embossed
 * label idiom (`text-shadow: 0 1px 0 ink.emboss`), tabular figures,
 * letter-spacing, ellipsis. */
#ifndef MARYUI_LP_TEXT_H
#define MARYUI_LP_TEXT_H

#include <cairo.h>

#include "maryui/lp_types.h"

enum lp_font { LP_FONT_UI, LP_FONT_DISPLAY, LP_FONT_MONO };
enum lp_align { LP_ALIGN_START, LP_ALIGN_CENTER, LP_ALIGN_END };

typedef struct lp_text_style {
    enum lp_font font;
    float size_px;
    int weight;              /* 400, 500, 600, 700 */
    lp_color color;
    int emboss;              /* draw a 1px highlight below the ink */
    lp_color emboss_color;
    int tabular_nums;
    float letter_spacing;    /* px */
    int uppercase;
    int ellipsize;           /* clip long text with … at the rect's width */
} lp_text_style;

/* The body default: font.ui, text.md, regular, ink.primary. */
lp_text_style lp_text_style_default(void);

lp_size lp_text_measure(cairo_t *cr, const char *text, const lp_text_style *style);
/* Draws vertically centred in `r`, horizontally per `align`. */
void lp_text_draw(cairo_t *cr, const char *text, lp_rect r, const lp_text_style *style, enum lp_align align);
/* Draws with the baseline origin at (x, y) — for glyph-level layouts. */
void lp_text_draw_at(cairo_t *cr, const char *text, float x, float y, const lp_text_style *style);

/* The Pango family list for a font slot (e.g. "…, Inter, sans-serif"). */
const char *lp_font_families(enum lp_font font);

/* A retained multi-line layout for editors and paragraphs: wraps at
 * wrap_width (word, then character; <= 0 for no wrap), keeps paragraph breaks,
 * and maps byte indices to pixels and back. `cr` may be NULL (the EVENT pass
 * has no cairo context): a scratch image context with the same font options
 * is used, so hit-testing agrees with drawing. */
typedef struct lp_text_layout lp_text_layout;
lp_text_layout *lp_text_layout_new(cairo_t *cr, const char *text, int len, const lp_text_style *style, float wrap_width);
void lp_text_layout_free(lp_text_layout *l);
/* Logical pixel extents. */
lp_size lp_text_layout_size(const lp_text_layout *l);
int lp_text_layout_line_count(const lp_text_layout *l);
/* The caret rectangle before the grapheme at `index` (w is 1). */
lp_rect lp_text_layout_index_to_pos(const lp_text_layout *l, int index);
/* The byte index nearest to a point (layout-local pixels); the trailing half of a grapheme rounds up. */
int lp_text_layout_xy_to_index(const lp_text_layout *l, float x, float y);
/* The visual line holding `index`: its first byte and the byte after its last (before a paragraph break). */
void lp_text_layout_line_bounds(const lp_text_layout *l, int index, int *start, int *end);
/* The index on the line `delta` lines away closest to x (layout-local pixels), or -1 past the first/last line. */
int lp_text_layout_move_line(const lp_text_layout *l, int index, int delta, float x);
/* The rectangles covering bytes [start, end), one or more per line. Returns the count. */
int lp_text_layout_range_rects(const lp_text_layout *l, int start, int end, lp_rect *out, int max);
void lp_text_layout_draw(cairo_t *cr, const lp_text_layout *l, float x, float y, lp_color color);

#endif
