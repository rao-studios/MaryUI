/* TextArea — a multi-line inset well for editing text: a growable UTF-8
 * document with a caret and a selection, wrapped through Pango, scrolled by
 * ScrollArea. Mirrors web/src/components/TextArea (a native <textarea> there;
 * here the document, the clipboard and the editing keys are ours). */
#ifndef MARYUI_LP_TEXT_AREA_H
#define MARYUI_LP_TEXT_AREA_H

#include <stdint.h>

#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_ui.h"

/* The document: text[0..len) is NUL-terminated UTF-8; the selection is
 * [min(cursor, anchor), max(cursor, anchor)); preferred_x keeps the caret's
 * column while stepping through lines (-1 when unset). */
typedef struct lp_text_doc {
    char *text;
    int len, cap;
    int cursor, anchor;   /* byte offsets */
    float preferred_x;
} lp_text_doc;

void lp_text_doc_init(lp_text_doc *d);
void lp_text_doc_free(lp_text_doc *d);
void lp_text_doc_set(lp_text_doc *d, const char *text);
/* Replaces the selection (or inserts at the caret) with n bytes of UTF-8. */
void lp_text_doc_insert(lp_text_doc *d, const char *utf8, int n);
/* Removes the selection; returns 1 when there was one. */
int lp_text_doc_delete_selection(lp_text_doc *d);
void lp_text_doc_select_all(lp_text_doc *d);
int lp_text_doc_has_selection(const lp_text_doc *d);
void lp_text_doc_selection(const lp_text_doc *d, int *start, int *end);
/* Whitespace-separated words; code points; the 1-based line and column of a byte offset. */
int lp_text_doc_word_count(const lp_text_doc *d);
int lp_text_doc_char_count(const lp_text_doc *d);
void lp_text_doc_line_col(const lp_text_doc *d, int index, int *line, int *col);

/* The process-wide clipboard the text controls share (no wl_data_device yet). */
typedef struct lp_text_clipboard { char *text; int len; } lp_text_clipboard;
lp_text_clipboard *lp_text_clipboard_shared(void);
void lp_text_clipboard_set(lp_text_clipboard *c, const char *text, int len);

/* The keys that need no layout: BackSpace/Delete (selection-aware), ←/→ (Shift
 * extends), Home/End on the logical line, Return, Tab, Ctrl/⌘+A/C/X/V, and
 * printable text. Returns 1 when the text changed (caret moves return 0). */
int lp_text_doc_key(lp_text_doc *d, uint32_t keysym, uint32_t mods, const char *utf8, lp_text_clipboard *clip);

typedef struct lp_text_area_state {
    lp_text_doc doc;
    lp_scroll_state scroll;
    int dragging;
} lp_text_area_state;

typedef struct lp_text_area_opts {
    const char *placeholder;
    int mono;
    int disabled;
} lp_text_area_opts;

/* Handles input in the EVENT pass (click to place the caret, drag to select,
 * ↑/↓ by visual line, wheel to scroll) and paints in the DRAW pass. Returns 1
 * when the text changed. */
int lp_text_area(lp_ctx *ctx, lp_id id, lp_rect r, lp_text_area_state *state, lp_text_area_opts opts);

#endif
