/* The form controls: Toggle, Checkbox, Slider, TextField, SegmentedControl,
 * ProgressBar, plus the Icon widget. Each mirrors web/src/components/<Name>.
 * Every call handles input in the EVENT pass and paints in the DRAW pass;
 * the return value says whether the bound value changed. */
#ifndef MARYUI_LP_CONTROLS_H
#define MARYUI_LP_CONTROLS_H

#include <stddef.h>

#include "maryui/components/lp_button.h"
#include "maryui/lp_icons.h"
#include "maryui/lp_ui.h"

/* Icon — a glyph from icons.json in any ink; size in px (24-unit viewBox). */
void lp_icon_widget(lp_ctx *ctx, lp_icon icon, float x, float y, float size, lp_color color);

/* Toggle — a 38×22 capsule whose knob is a LiquidBubble. */
#define LP_TOGGLE_W 38
#define LP_TOGGLE_H 22
int lp_toggle(lp_ctx *ctx, lp_id id, float x, float y, int *checked, int disabled);

/* Checkbox — a 15px raised box that floods with the accent, plus a label. */
lp_size lp_checkbox_measure(lp_ctx *ctx, const char *label);
int lp_checkbox(lp_ctx *ctx, lp_id id, float x, float y, int *checked, const char *label, int disabled);

/* Slider — label, inset rail with an accent fill, platinum thumb, optional value. */
typedef struct lp_slider_opts {
    float min, max, step;       /* defaults 0, 100, 1 when all zero */
    const char *label;
    int show_value;
    /* Formats the value for the trailing column; NULL prints it plainly. */
    void (*format)(float value, char *out, size_t n);
    int disabled;
} lp_slider_opts;
#define LP_SLIDER_H 16
int lp_slider(lp_ctx *ctx, lp_id id, lp_rect r, float *value, lp_slider_opts opts);

/* TextField — an inset well with an optional leading icon; typing edits the buffer. */
typedef struct lp_text_buffer {
    char text[256];
    int len;
    int cursor;                 /* byte offset */
    int all_selected;           /* the whole text is selected: the next character replaces it (a rename starts this way) */
} lp_text_buffer;
typedef struct lp_text_field_opts {
    const char *placeholder;
    lp_icon icon;               /* LP_ICON_COUNT for none */
    int round;
    int large;                  /* Spotlight's bar: text.lg, a 16px icon (the rect sets the height) */
    int disabled;
} lp_text_field_opts;
void lp_text_buffer_set(lp_text_buffer *b, const char *text);
/* Sets the text with all of it selected, the way the Finder starts a rename. */
void lp_text_buffer_set_selected(lp_text_buffer *b, const char *text);
int lp_text_field(lp_ctx *ctx, lp_id id, lp_rect r, lp_text_buffer *buffer, lp_text_field_opts opts);

/* SegmentedControl — a well holding a sliding platinum thumb. */
typedef struct lp_segment {
    const char *label;          /* NULL for icon-only */
    lp_icon icon;               /* LP_ICON_COUNT for none */
} lp_segment;
lp_size lp_segmented_measure(lp_ctx *ctx, const lp_segment *options, int n, enum lp_control_size size);
int lp_segmented(lp_ctx *ctx, lp_id id, float x, float y, const lp_segment *options, int n, int *index, enum lp_control_size size);

/* ProgressBar — an 8px inset rail with a liquid accent fill; value < 0 is indeterminate (barber pole). */
#define LP_PROGRESS_H 8
void lp_progress(lp_ctx *ctx, lp_rect r, float value);

#endif
