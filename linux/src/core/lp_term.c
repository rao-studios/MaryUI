/* Terminal's screen (lp_term.h). */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_term.h"

#ifndef HAVE_VTERM

int lp_term_available(void) { return 0; }
lp_term *lp_term_new(int rows, int cols, lp_term_output_fn output, void *user) { return NULL; }
void lp_term_free(lp_term *t) {}
void lp_term_resize(lp_term *t, int rows, int cols) {}
void lp_term_size(const lp_term *t, int *rows, int *cols) { *rows = 0; *cols = 0; }
void lp_term_feed(lp_term *t, const char *bytes, size_t len) {}
int lp_term_key(lp_term *t, uint32_t keysym, const char *utf8, uint32_t mods) { return 0; }
void lp_term_paste(lp_term *t, const char *text, size_t len) {}
void lp_term_cell_at(const lp_term *t, int row, int col, lp_term_cell *out) { memset(out, 0, sizeof *out); out->width = 1; out->bg_default = 1; }
int lp_term_scrollback_lines(const lp_term *t) { return 0; }
void lp_term_cursor(const lp_term *t, int *row, int *col, int *visible) { *row = 0; *col = 0; *visible = 0; }
int lp_term_take_damage(lp_term *t, unsigned char *rows, int n) { memset(rows, 0, (size_t)(n > 0 ? n : 0)); return 0; }
const char *lp_term_title(lp_term *t, int *changed) { if (changed) *changed = 0; return ""; }
size_t lp_term_text(const lp_term *t, int row0, int col0, int row1, int col1, char *out, size_t n) { if (n) out[0] = 0; return 0; }
int lp_term_altscreen(const lp_term *t) { return 0; }
int lp_term_take_bell(lp_term *t) { return 0; }

#else

#include <vterm.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/lp_tokens.h"
#include "maryui/lp_ui.h"

struct sb_line {
    int cols;
    VTermScreenCell *cells;
};

struct lp_term {
    VTerm *vt;
    VTermScreen *screen;
    int rows, cols;
    lp_term_output_fn output;
    void *user;
    unsigned char *damage;       /* one flag per screen row */
    int any_damage;
    struct sb_line *sb;          /* a ring of LP_TERM_SCROLLBACK lines */
    int sb_head, sb_count;       /* sb_head is the oldest */
    VTermPos cursor;
    int cursor_visible;
    int altscreen;
    char title[256], title_build[256];
    int title_changed;
    int bell;
};

int lp_term_available(void) { return 1; }

static void mark_rows(lp_term *t, int from, int to) {
    if (from < 0) from = 0;
    if (to > t->rows) to = t->rows;
    for (int r = from; r < to; r++) t->damage[r] = 1;
    t->any_damage = 1;
}

static int on_damage(VTermRect rect, void *user) {
    mark_rows(user, rect.start_row, rect.end_row);
    return 1;
}

static int on_movecursor(VTermPos pos, VTermPos old, int visible, void *user) {
    lp_term *t = user;
    mark_rows(t, old.row, old.row + 1);
    mark_rows(t, pos.row, pos.row + 1);
    t->cursor = pos;
    t->cursor_visible = visible;
    return 1;
}

static int on_settermprop(VTermProp prop, VTermValue *val, void *user) {
    lp_term *t = user;
    switch (prop) {
    case VTERM_PROP_TITLE: {
        /* the title arrives in fragments; keep it once the last one lands */
        VTermStringFragment frag = val->string;
        if (frag.initial) t->title_build[0] = 0;
        size_t len = strlen(t->title_build), room = sizeof t->title_build - 1 - len;
        size_t take = frag.len < room ? frag.len : room;
        memcpy(t->title_build + len, frag.str, take);
        t->title_build[len + take] = 0;
        if (frag.final && strcmp(t->title, t->title_build) != 0) {
            snprintf(t->title, sizeof t->title, "%s", t->title_build);
            t->title_changed = 1;
        }
        return 1;
    }
    case VTERM_PROP_CURSORVISIBLE:
        t->cursor_visible = val->boolean;
        mark_rows(t, t->cursor.row, t->cursor.row + 1);
        return 1;
    case VTERM_PROP_ALTSCREEN:
        t->altscreen = val->boolean;
        mark_rows(t, 0, t->rows);
        return 1;
    default:
        return 0;
    }
}

static int on_bell(void *user) {
    ((lp_term *)user)->bell = 1;
    return 1;
}

static int on_resize(int rows, int cols, void *user) { return 1; }

static int on_sb_pushline(int cols, const VTermScreenCell *cells, void *user) {
    lp_term *t = user;
    int slot;
    if (t->sb_count < LP_TERM_SCROLLBACK) {
        slot = (t->sb_head + t->sb_count++) % LP_TERM_SCROLLBACK;
    } else {
        slot = t->sb_head;
        t->sb_head = (t->sb_head + 1) % LP_TERM_SCROLLBACK;
    }
    struct sb_line *line = &t->sb[slot];
    int keep = cols;   /* trailing blanks cost memory and say nothing */
    while (keep > 0 && cells[keep - 1].chars[0] == 0 && VTERM_COLOR_IS_DEFAULT_BG(&cells[keep - 1].bg)) keep--;
    VTermScreenCell *copy = realloc(line->cells, sizeof *copy * (size_t)(keep > 0 ? keep : 1));
    if (!copy) { line->cols = 0; return 1; }
    memcpy(copy, cells, sizeof *copy * (size_t)keep);
    line->cells = copy;
    line->cols = keep;
    return 1;
}

static void blank_cell(const lp_term *t, VTermScreenCell *cell) {
    memset(cell, 0, sizeof *cell);
    cell->width = 1;
    vterm_state_get_default_colors(vterm_obtain_state(t->vt), &cell->fg, &cell->bg);
}

static int on_sb_popline(int cols, VTermScreenCell *cells, void *user) {
    lp_term *t = user;
    if (t->sb_count == 0) return 0;
    struct sb_line *line = &t->sb[(t->sb_head + t->sb_count - 1) % LP_TERM_SCROLLBACK];
    for (int c = 0; c < cols; c++) {
        if (c < line->cols) cells[c] = line->cells[c];
        else blank_cell(t, &cells[c]);
    }
    t->sb_count--;
    return 1;
}

static int on_sb_clear(void *user) {
    lp_term *t = user;
    t->sb_count = 0;
    return 1;
}

static void on_output(const char *bytes, size_t len, void *user) {
    lp_term *t = user;
    if (t->output) t->output(bytes, len, t->user);
}

static uint8_t byte_of(float channel) { return (uint8_t)lroundf(channel * 255); }

static void rgb(VTermColor *c, lp_color from) { vterm_color_rgb(c, byte_of(from.r), byte_of(from.g), byte_of(from.b)); }

/* The sixteen ANSI colours, darkened to read on the platinum well: "white"
 * becomes a mid grey and the bright white stays pale for reverse video. */
static const uint32_t PALETTE[16] = {
    0x1f2228, 0xb8322f, 0x3c7d2e, 0x9a6a00, 0x2e5fa8, 0x93409f, 0x1d7c85, 0x767b85,
    0x5a5f69, 0xd4453f, 0x4f9a3c, 0xb58400, 0x3f76c9, 0xad55ba, 0x2a959f, 0xaeb2ba,
};

lp_term *lp_term_new(int rows, int cols, lp_term_output_fn output, void *user) {
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    lp_term *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->sb = calloc(LP_TERM_SCROLLBACK, sizeof *t->sb);
    t->damage = calloc((size_t)rows, 1);
    t->vt = t->sb && t->damage ? vterm_new(rows, cols) : NULL;
    if (!t->vt) { lp_term_free(t); return NULL; }
    t->rows = rows;
    t->cols = cols;
    t->output = output;
    t->user = user;
    t->cursor_visible = 1;
    vterm_set_utf8(t->vt, 1);
    vterm_output_set_callback(t->vt, on_output, t);
    t->screen = vterm_obtain_screen(t->vt);
    static const VTermScreenCallbacks callbacks = {
        .damage = on_damage, .movecursor = on_movecursor, .settermprop = on_settermprop, .bell = on_bell,
        .resize = on_resize, .sb_pushline = on_sb_pushline, .sb_popline = on_sb_popline, .sb_clear = on_sb_clear,
    };
    vterm_screen_set_callbacks(t->screen, &callbacks, t);
    vterm_screen_enable_altscreen(t->screen, 1);
    vterm_screen_enable_reflow(t->screen, true);
    vterm_screen_reset(t->screen, 1);
    VTermState *state = vterm_obtain_state(t->vt);
    for (int i = 0; i < 16; i++) {
        VTermColor c;
        vterm_color_rgb(&c, (uint8_t)(PALETTE[i] >> 16), (uint8_t)(PALETTE[i] >> 8), (uint8_t)PALETTE[i]);
        vterm_state_set_palette_color(state, i, &c);
    }
    VTermColor fg, bg;
    rgb(&fg, LP_INK_PRIMARY);
    rgb(&bg, LP_SURFACE_WELL);
    vterm_screen_set_default_colors(t->screen, &fg, &bg);
    mark_rows(t, 0, rows);
    return t;
}

void lp_term_free(lp_term *t) {
    if (!t) return;
    if (t->vt) vterm_free(t->vt);
    if (t->sb) for (int i = 0; i < LP_TERM_SCROLLBACK; i++) free(t->sb[i].cells);
    free(t->sb);
    free(t->damage);
    free(t);
}

void lp_term_resize(lp_term *t, int rows, int cols) {
    if (!t || rows < 1 || cols < 1 || (rows == t->rows && cols == t->cols)) return;
    unsigned char *damage = realloc(t->damage, (size_t)rows);
    if (!damage) return;
    t->damage = damage;
    t->rows = rows;
    t->cols = cols;
    vterm_set_size(t->vt, rows, cols);
    vterm_screen_flush_damage(t->screen);
    mark_rows(t, 0, rows);
}

void lp_term_size(const lp_term *t, int *rows, int *cols) {
    *rows = t ? t->rows : 0;
    *cols = t ? t->cols : 0;
}

void lp_term_feed(lp_term *t, const char *bytes, size_t len) {
    if (!t || !len) return;
    vterm_input_write(t->vt, bytes, len);
    vterm_screen_flush_damage(t->screen);
}

static uint32_t utf8_first(const char *s) {
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x80) return u[0];
    if ((u[0] & 0xE0) == 0xC0 && u[1]) return (uint32_t)(u[0] & 0x1F) << 6 | (u[1] & 0x3F);
    if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2]) return (uint32_t)(u[0] & 0x0F) << 12 | (uint32_t)(u[1] & 0x3F) << 6 | (u[2] & 0x3F);
    if ((u[0] & 0xF8) == 0xF0 && u[1] && u[2] && u[3])
        return (uint32_t)(u[0] & 0x07) << 18 | (uint32_t)(u[1] & 0x3F) << 12 | (uint32_t)(u[2] & 0x3F) << 6 | (u[3] & 0x3F);
    return 0;
}

int lp_term_key(lp_term *t, uint32_t keysym, const char *utf8, uint32_t mods) {
    if (!t) return 0;
    int m = VTERM_MOD_NONE;
    if (mods & LP_MOD_SHIFT) m |= VTERM_MOD_SHIFT;
    if (mods & LP_MOD_ALT) m |= VTERM_MOD_ALT;
    if (mods & LP_MOD_CTRL) m |= VTERM_MOD_CTRL;
    VTermKey key = VTERM_KEY_NONE;
    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F12) key = (VTermKey)VTERM_KEY_FUNCTION(keysym - XKB_KEY_F1 + 1);
    switch (keysym) {
    case XKB_KEY_Return: key = VTERM_KEY_ENTER; break;
    case XKB_KEY_KP_Enter: key = VTERM_KEY_KP_ENTER; break;
    case XKB_KEY_Tab: case XKB_KEY_ISO_Left_Tab: key = VTERM_KEY_TAB; break;
    case XKB_KEY_BackSpace: key = VTERM_KEY_BACKSPACE; break;
    case XKB_KEY_Escape: key = VTERM_KEY_ESCAPE; break;
    case XKB_KEY_Up: key = VTERM_KEY_UP; break;
    case XKB_KEY_Down: key = VTERM_KEY_DOWN; break;
    case XKB_KEY_Left: key = VTERM_KEY_LEFT; break;
    case XKB_KEY_Right: key = VTERM_KEY_RIGHT; break;
    case XKB_KEY_Insert: key = VTERM_KEY_INS; break;
    case XKB_KEY_Delete: key = VTERM_KEY_DEL; break;
    case XKB_KEY_Home: key = VTERM_KEY_HOME; break;
    case XKB_KEY_End: key = VTERM_KEY_END; break;
    case XKB_KEY_Page_Up: key = VTERM_KEY_PAGEUP; break;
    case XKB_KEY_Page_Down: key = VTERM_KEY_PAGEDOWN; break;
    default: break;
    }
    if (key != VTERM_KEY_NONE) {
        vterm_keyboard_key(t->vt, key, (VTermModifier)m);
        return 1;
    }
    uint32_t cp = xkb_keysym_to_utf32(keysym);
    if (!cp && utf8 && utf8[0]) cp = utf8_first(utf8);
    if (!cp) return 0;   /* a modifier on its own, a dead key */
    /* Shift is already in the character (A rather than a); Ctrl and Alt are the terminal's to encode. */
    if (cp >= 0x20 && cp != 0x7f) m &= ~VTERM_MOD_SHIFT;
    vterm_keyboard_unichar(t->vt, cp, (VTermModifier)m);
    return 1;
}

void lp_term_paste(lp_term *t, const char *text, size_t len) {
    if (!t || !text) return;
    vterm_keyboard_start_paste(t->vt);
    size_t start = 0;
    static const char RETURN = '\r';
    for (size_t i = 0; i < len; i++) {
        if (text[i] != '\n') continue;
        if (i > start) on_output(text + start, i - start - (text[i - 1] == '\r' ? 1 : 0), t);
        on_output(&RETURN, 1, t);
        start = i + 1;
    }
    if (len > start) on_output(text + start, len - start, t);
    vterm_keyboard_end_paste(t->vt);
}

static lp_color color_of(const lp_term *t, VTermColor c) {
    vterm_screen_convert_color_to_rgb(t->screen, &c);
    return (lp_color){ c.rgb.red / 255.0f, c.rgb.green / 255.0f, c.rgb.blue / 255.0f, 1 };
}

static size_t utf8_put(uint32_t cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) { out[0] = (char)(0xC0 | cp >> 6); out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | cp >> 12); out[1] = (char)(0x80 | (cp >> 6 & 0x3F)); out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp < 0x110000) {
        out[0] = (char)(0xF0 | cp >> 18); out[1] = (char)(0x80 | (cp >> 12 & 0x3F));
        out[2] = (char)(0x80 | (cp >> 6 & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

static void fill_cell(const lp_term *t, const VTermScreenCell *vc, lp_term_cell *out) {
    memset(out, 0, sizeof *out);
    size_t o = 0;
    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && vc->chars[i] && vc->chars[i] != (uint32_t)-1; i++) {
        if (o + 5 > sizeof out->text) break;
        o += utf8_put(vc->chars[i], out->text + o);
    }
    out->text[o] = 0;
    out->width = vc->width;
    lp_color fg = color_of(t, vc->fg), bg = color_of(t, vc->bg);
    out->reverse = vc->attrs.reverse;
    if (out->reverse) {
        out->fg = bg;
        out->bg = fg;
        out->bg_default = 0;
    } else {
        out->fg = fg;
        out->bg = bg;
        out->bg_default = VTERM_COLOR_IS_DEFAULT_BG(&vc->bg);
    }
    out->bold = vc->attrs.bold;
    out->italic = vc->attrs.italic;
    out->underline = vc->attrs.underline;
    out->strike = vc->attrs.strike;
}

void lp_term_cell_at(const lp_term *t, int row, int col, lp_term_cell *out) {
    VTermScreenCell vc;
    if (row >= 0 && row < t->rows && col >= 0 && col < t->cols && vterm_screen_get_cell(t->screen, (VTermPos){ row, col }, &vc)) {
        fill_cell(t, &vc, out);
        return;
    }
    if (row < 0 && -row <= t->sb_count && col >= 0) {
        const struct sb_line *line = &t->sb[(t->sb_head + t->sb_count + row) % LP_TERM_SCROLLBACK];
        if (col < line->cols) { fill_cell(t, &line->cells[col], out); return; }
    }
    blank_cell(t, &vc);
    fill_cell(t, &vc, out);
}

int lp_term_scrollback_lines(const lp_term *t) { return t ? t->sb_count : 0; }

void lp_term_cursor(const lp_term *t, int *row, int *col, int *visible) {
    *row = t->cursor.row;
    *col = t->cursor.col;
    *visible = t->cursor_visible;
}

int lp_term_take_damage(lp_term *t, unsigned char *rows, int n) {
    int any = t->any_damage;
    for (int r = 0; r < n; r++) rows[r] = r < t->rows ? t->damage[r] : 0;
    memset(t->damage, 0, (size_t)t->rows);
    t->any_damage = 0;
    return any;
}

const char *lp_term_title(lp_term *t, int *changed) {
    if (changed) *changed = t->title_changed;
    t->title_changed = 0;
    return t->title;
}

size_t lp_term_text(const lp_term *t, int row0, int col0, int row1, int col1, char *out, size_t n) {
    if (!n) return 0;
    if (row1 < row0 || (row1 == row0 && col1 < col0)) {
        int r = row0, c = col0;
        row0 = row1; col0 = col1; row1 = r; col1 = c;
    }
    size_t o = 0;
    for (int r = row0; r <= row1; r++) {
        int from = r == row0 ? col0 : 0, to = r == row1 ? col1 : t->cols - 1;
        if (from < 0) from = 0;
        size_t line_start = o, inked = o;
        for (int c = from; c <= to && c < t->cols; c++) {
            lp_term_cell cell;
            lp_term_cell_at(t, r, c, &cell);
            const char *s = cell.text[0] ? cell.text : " ";
            size_t len = strlen(s);
            if (o + len + 2 > n) break;
            memcpy(out + o, s, len);
            o += len;
            if (cell.text[0] && strcmp(cell.text, " ") != 0) inked = o;
            if (cell.width > 1) c++;   /* the right half of a wide character has no text of its own */
        }
        o = inked > line_start ? inked : line_start;
        if (r < row1 && o + 2 <= n) out[o++] = '\n';
    }
    out[o < n ? o : n - 1] = 0;
    return o;
}

int lp_term_altscreen(const lp_term *t) { return t ? t->altscreen : 0; }

int lp_term_take_bell(lp_term *t) {
    int rang = t->bell;
    t->bell = 0;
    return rang;
}

#endif
