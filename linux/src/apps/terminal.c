/* Terminal — a shell in a window. lp_pty runs it, lp_term (libvterm) keeps its
 * screen, and this file draws the cells in the mono font on the platinum well,
 * sends keys and the wheel, selects and copies, pastes, and follows the title a
 * program sets (or else the job in front). Ctrl chords are the shell's
 * (lp_app.raw_ctrl): ⌘/Super reaches the desktop, and ⌘C / ⌘V — or Ctrl+Shift+C
 * and V — copy and paste. Without libvterm in the build the window says so.
 * Linux only (PARITY D15). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_text_area.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_pty.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_term.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define PAD LP_SPACE_2
#define READ_BUDGET (256 * 1024)   /* bytes per wake-up: a flood yields to the compositor between bites */

struct terminal {
    char window_id[12];
    lp_desktop *desk;
    lp_term *term;
    lp_pty pty;
    lp_source *source;
    int exited;
    int measured;
    float cell_w, cell_h;
    int rows, cols;
    int scroll;               /* lines scrolled back from the bottom */
    float scroll_acc;         /* wheel travel not yet a whole line */
    int selecting, has_selection;
    int sel_row0, sel_col0, sel_row1, sel_col1;   /* rows as lp_term_cell_at counts them */
    char title[160];
};

/* What a preview shows: a short session. */
static const char SAMPLE[] =
    "\x1b[1mmary@maryos\x1b[0m:\x1b[34m~\x1b[0m$ uname -sr\r\n"
    "Linux 6.8.0-139-generic\r\n"
    "\x1b[1mmary@maryos\x1b[0m:\x1b[34m~\x1b[0m$ ls\r\n"
    "\x1b[1;34mDesktop\x1b[0m  \x1b[1;34mDocuments\x1b[0m  \x1b[1;34mDownloads\x1b[0m  notes.txt  \x1b[32mbuild.sh\x1b[0m\r\n"
    "\x1b[1mmary@maryos\x1b[0m:\x1b[34m~\x1b[0m$ ";

static lp_text_style mono(void) {
    lp_text_style st = lp_text_style_default();
    st.font = LP_FONT_MONO;
    st.size_px = LP_TEXT_MD;
    st.color = LP_INK_PRIMARY;
    return st;
}

static void to_pty(const char *bytes, size_t len, void *user) {
    struct terminal *k = user;
    if (!k->exited && k->pty.fd >= 0) lp_pty_write(&k->pty, bytes, len);
}

static void sync_title(struct terminal *k) {
    if (!k->desk || !k->term) return;
    int changed = 0;
    const char *set = lp_term_title(k->term, &changed);
    char want[sizeof k->title], job[64];
    if (set[0]) {
        snprintf(want, sizeof want, "%s", set);
    } else {
        lp_pty_foreground(&k->pty, job, sizeof job);
        snprintf(want, sizeof want, "%s", job[0] ? job : "Terminal");
    }
    if (strcmp(want, k->title) == 0) return;
    memcpy(k->title, want, sizeof k->title);
    if (lp_wm_find(&k->desk->wm, k->window_id) < 0) return;
    lp_wm_action a = { .type = LP_WM_SET_TITLE, .id = k->window_id, .title = k->title };
    lp_desktop_dispatch(k->desk, &a);
}

static void finish(struct terminal *k) {
    if (k->exited) return;
    k->exited = 1;
    if (k->source) {
        lp_desktop_remove_source(k->desk, k->source);
        k->source = NULL;
    }
    lp_pty_poll_exit(&k->pty);
    static const char DONE[] = "\r\n[Process completed]";
    lp_term_feed(k->term, DONE, sizeof DONE - 1);
}

static int on_pty(int fd, uint32_t mask, void *data) {
    struct terminal *k = data;
    char buf[16384];
    size_t total = 0;
    ssize_t r = 0;
    while (total < READ_BUDGET && (r = lp_pty_read(&k->pty, buf, sizeof buf)) > 0) {
        lp_term_feed(k->term, buf, (size_t)r);
        total += (size_t)r;
    }
    if (r < 0 || ((mask & (LP_SOURCE_HANGUP | LP_SOURCE_ERROR)) && total == 0)) finish(k);
    if (total || k->exited) {
        sync_title(k);
        if (k->desk->on_app_dirty) k->desk->on_app_dirty(k->desk, k->window_id);
    }
    return 0;
}

static void *terminal_create(lp_desktop *d, const char *window_id) {
    struct terminal *k = calloc(1, sizeof *k);
    if (!k) return NULL;
    snprintf(k->window_id, sizeof k->window_id, "%s", window_id);
    k->desk = d;
    k->pty.fd = -1;
    k->cell_w = 8;
    k->cell_h = 17;
    k->rows = 24;
    k->cols = 80;
    snprintf(k->title, sizeof k->title, "Terminal");
    k->term = lp_term_new(k->rows, k->cols, to_pty, k);
    /* No libvterm, or a host without an event loop (lp-render): a screen with nothing behind it. */
    if (!k->term || !d || !d->add_fd) return k;
    if (lp_pty_spawn(&k->pty, NULL, NULL, k->rows, k->cols) != 0) {
        static const char FAILED[] = "Terminal could not start a shell.";
        lp_term_feed(k->term, FAILED, sizeof FAILED - 1);
        k->exited = 1;
        return k;
    }
    k->source = lp_desktop_add_fd(d, k->pty.fd, LP_SOURCE_READABLE, on_pty, k);
    if (!k->source) {
        lp_pty_close(&k->pty);
        k->exited = 1;
    }
    return k;
}

static void terminal_destroy(void *state) {
    struct terminal *k = state;
    if (!k) return;
    if (k->source) lp_desktop_remove_source(k->desk, k->source);
    lp_pty_close(&k->pty);   /* hangs the shell up */
    lp_term_free(k->term);
    free(k);
}

/* MARK: - Selection, clipboard, commands */

static void selection_bounds(const struct terminal *k, int *r0, int *c0, int *r1, int *c1) {
    long a = (long)k->sel_row0 * 100000 + k->sel_col0, b = (long)k->sel_row1 * 100000 + k->sel_col1;
    int swap = a > b;
    *r0 = swap ? k->sel_row1 : k->sel_row0;
    *c0 = swap ? k->sel_col1 : k->sel_col0;
    *r1 = swap ? k->sel_row0 : k->sel_row1;
    *c1 = swap ? k->sel_col0 : k->sel_col1;
}

static void copy_selection(struct terminal *k) {
    if (!k->has_selection) return;
    int r0, c0, r1, c1;
    selection_bounds(k, &r0, &c0, &r1, &c1);
    size_t cap = (size_t)(r1 - r0 + 1) * ((size_t)k->cols * sizeof(((lp_term_cell *)0)->text) + 1) + 1;
    char *text = malloc(cap);
    if (!text) return;
    size_t len = lp_term_text(k->term, r0, c0, r1, c1, text, cap);
    lp_text_clipboard_set(lp_text_clipboard_shared(), text, (int)len);
    free(text);
}

static void paste(struct terminal *k) {
    lp_text_clipboard *clip = lp_text_clipboard_shared();
    if (k->exited || !clip->text || clip->len <= 0) return;
    lp_term_paste(k->term, clip->text, (size_t)clip->len);
    k->scroll = 0;
}

static void terminal_command(void *state, lp_desktop *d, int cmd) {
    struct terminal *k = state;
    if (!k || !k->term) return;
    switch ((enum lp_terminal_command)cmd) {
    case LP_TERMINAL_COPY: copy_selection(k); break;
    case LP_TERMINAL_PASTE: paste(k); break;
    case LP_TERMINAL_CLEAR_SCROLLBACK:
        lp_term_feed(k->term, "\x1b[3J", 4);
        k->scroll = 0;
        k->has_selection = 0;
        to_pty("\x0c", 1, k);   /* and Ctrl+L, so the shell redraws its prompt at the top */
        break;
    }
}

/* MARK: - Input */

static void fit_grid(struct terminal *k, lp_rect grid) {
    int cols = (int)(grid.w / k->cell_w), rows = (int)(grid.h / k->cell_h);
    if (cols < 2) cols = 2;
    if (rows < 1) rows = 1;
    if (cols == k->cols && rows == k->rows) return;
    k->cols = cols;
    k->rows = rows;
    lp_term_resize(k->term, rows, cols);
    if (k->pty.fd >= 0 && !k->exited) lp_pty_resize(&k->pty, rows, cols, (int)grid.w, (int)grid.h);
    int sb = lp_term_scrollback_lines(k->term);
    if (k->scroll > sb) k->scroll = sb;
    k->has_selection = 0;
}

static void cell_of(const struct terminal *k, lp_rect grid, float x, float y, int *row, int *col) {
    int c = (int)floorf((x - grid.x) / k->cell_w), r = (int)floorf((y - grid.y) / k->cell_h);
    if (c < 0) c = 0;
    if (c >= k->cols) c = k->cols - 1;
    if (r < 0) r = 0;
    if (r >= k->rows) r = k->rows - 1;
    *col = c;
    *row = r - k->scroll;
}

static void handle_input(lp_ctx *ctx, struct terminal *k, lp_desktop *d, lp_rect body, lp_rect grid) {
    lp_input *in = &ctx->in;
    if (in->key_pressed && in->keysym) {
        uint32_t sym = in->keysym;
        int super = (in->mods & LP_MOD_LOGO) != 0;
        int ctrl_shift = (in->mods & (LP_MOD_CTRL | LP_MOD_SHIFT)) == (LP_MOD_CTRL | LP_MOD_SHIFT);
        int is_c = sym == XKB_KEY_c || sym == XKB_KEY_C, is_v = sym == XKB_KEY_v || sym == XKB_KEY_V;
        if ((super || ctrl_shift) && is_c) terminal_command(k, d, LP_TERMINAL_COPY);
        else if ((super || ctrl_shift) && is_v) terminal_command(k, d, LP_TERMINAL_PASTE);
        else if (super && (sym == XKB_KEY_k || sym == XKB_KEY_K)) terminal_command(k, d, LP_TERMINAL_CLEAR_SCROLLBACK);
        else if (!super && !k->exited && lp_term_key(k->term, sym, in->utf8, in->mods)) {
            k->scroll = 0;
            k->has_selection = 0;
        }
        ctx->dirty = 1;
    }
    if (in->scroll_y != 0 && lp_hit(ctx, body)) {
        k->scroll_acc += in->scroll_y / k->cell_h;
        int lines = (int)k->scroll_acc;
        k->scroll_acc -= (float)lines;
        if (lines && lp_term_altscreen(k->term)) {
            /* a full-screen program keeps no scrollback: the wheel is its arrow keys */
            for (int i = 0; i < abs(lines) && !k->exited; i++) lp_term_key(k->term, lines > 0 ? XKB_KEY_Down : XKB_KEY_Up, "", 0);
        } else if (lines) {
            int sb = lp_term_scrollback_lines(k->term);
            k->scroll -= lines;
            if (k->scroll < 0) k->scroll = 0;
            if (k->scroll > sb) k->scroll = sb;
        }
        if (lines) ctx->dirty = 1;
    }
    int inside = !isnan(in->mx) && lp_rect_contains(grid, in->mx, in->my);
    if (inside) ctx->cursor = LP_CURSOR_TEXT;
    if ((in->pressed & LP_BUTTON_LEFT) && inside) {
        cell_of(k, grid, in->mx, in->my, &k->sel_row0, &k->sel_col0);
        k->sel_row1 = k->sel_row0;
        k->sel_col1 = k->sel_col0;
        k->selecting = 1;
        k->has_selection = 0;
        ctx->dirty = 1;
    } else if (k->selecting && (in->buttons & LP_BUTTON_LEFT) && !isnan(in->mx)) {
        int r, c;
        cell_of(k, grid, in->mx, in->my, &r, &c);
        if (r != k->sel_row1 || c != k->sel_col1) {
            k->sel_row1 = r;
            k->sel_col1 = c;
            k->has_selection = 1;
            ctx->dirty = 1;
        }
    }
    if (k->selecting && (in->released & LP_BUTTON_LEFT)) k->selecting = 0;
}

/* MARK: - Drawing */

typedef struct run_style {
    lp_color fg, bg;
    int bg_default, bold, underline, strike;
} run_style;

static void paint_run(cairo_t *cr, const struct terminal *k, const char *text, const run_style *s, float x, float y, int cells) {
    float w = (float)cells * k->cell_w;
    if (!s->bg_default) lp_fill_solid(cr, LP_RECT(x, y, w, k->cell_h), s->bg, 0);
    if (text[0]) {
        lp_text_style st = mono();
        st.color = s->fg;
        st.weight = s->bold ? LP_TEXT_WEIGHT_BOLD : LP_TEXT_WEIGHT_REGULAR;
        lp_text_draw(cr, text, LP_RECT(x, y, w + k->cell_w, k->cell_h), &st, LP_ALIGN_START);
    }
    if (s->underline) lp_fill_solid(cr, LP_RECT(x, y + k->cell_h - 2, w, 1), s->fg, 0);
    if (s->strike) lp_fill_solid(cr, LP_RECT(x, y + roundf(k->cell_h / 2), w, 1), s->fg, 0);
}

/* One row as runs of cells that look the same, so a line is a handful of layouts rather than one per cell. */
static void paint_row(cairo_t *cr, const struct terminal *k, int row, float x0, float y) {
    char text[4096];
    size_t len = 0;
    int start = 0, cells = 0, have = 0;
    run_style run;
    memset(&run, 0, sizeof run);
    for (int c = 0; c <= k->cols; c++) {
        run_style s;
        memset(&s, 0, sizeof s);
        lp_term_cell cell = { .width = 1 };
        int end = c == k->cols;
        if (!end) {
            lp_term_cell_at(k->term, row, c, &cell);
            s.fg = cell.fg;
            s.bg = cell.bg;
            s.bg_default = cell.bg_default;
            s.bold = cell.bold;
            s.underline = cell.underline != 0;
            s.strike = cell.strike;
        }
        int wide = !end && cell.width > 1;
        if (have && (end || wide || memcmp(&s, &run, sizeof s) != 0 || len + sizeof cell.text >= sizeof text)) {
            text[len] = 0;
            paint_run(cr, k, text, &run, x0 + (float)start * k->cell_w, y, cells);
            have = 0;
            len = 0;
            cells = 0;
        }
        if (end) break;
        if (wide) {
            /* on its own, so a fallback font's advance cannot push the rest of the row off the grid */
            paint_run(cr, k, cell.text, &s, x0 + (float)c * k->cell_w, y, 2);
            c++;
            continue;
        }
        if (!have) { run = s; start = c; have = 1; }
        const char *t = cell.text[0] ? cell.text : " ";
        size_t tl = strlen(t);
        memcpy(text + len, t, tl);
        len += tl;
        cells++;
    }
}

static void paint_selection(cairo_t *cr, const struct terminal *k, lp_rect grid, lp_color color) {
    if (!k->has_selection) return;
    int r0, c0, r1, c1;
    selection_bounds(k, &r0, &c0, &r1, &c1);
    for (int y = 0; y < k->rows; y++) {
        int row = y - k->scroll;
        if (row < r0 || row > r1) continue;
        int from = row == r0 ? c0 : 0, to = row == r1 ? c1 : k->cols - 1;
        lp_fill_solid(cr, LP_RECT(grid.x + (float)from * k->cell_w, grid.y + (float)y * k->cell_h, (float)(to - from + 1) * k->cell_w, k->cell_h), color, 0);
    }
}

static void terminal_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct terminal *preview;
    struct terminal *k = state;
    if (!k) {
        if (!preview && (preview = terminal_create(NULL, "preview")) && preview->term) lp_term_feed(preview->term, SAMPLE, sizeof SAMPLE - 1);
        k = preview;
    }
    if (!k) return;
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    if (draw) lp_fill_solid(cr, body, LP_SURFACE_WELL, 0);
    if (!k->term) {
        if (draw) {
            lp_text_style st = lp_text_style_default();
            st.color = LP_INK_SECONDARY;
            lp_text_draw(cr, "Terminal needs libvterm, which this build of the desktop does not have.", body, &st, LP_ALIGN_CENTER);
        }
        return;
    }
    lp_rect grid = lp_rect_inset(body, PAD, PAD);
    if (draw && !k->measured) {
        lp_text_style st = mono();
        lp_size m = lp_text_measure(cr, "MMMMMMMMMM", &st);
        if (m.w > 0 && m.h > 0) {
            k->cell_w = m.w / 10;
            k->cell_h = ceilf(m.h) + 1;
        }
        k->measured = 1;
    }
    if (k->measured) fit_grid(k, grid);   /* never on the guessed metrics: that would send the shell two SIGWINCHes */
    if (ctx->pass == LP_PASS_EVENT && state) handle_input(ctx, k, d, body, grid);
    if (!draw) return;

    lp_settings fallback = lp_settings_defaults();
    lp_accent accent = lp_settings_accent(ctx->settings ? ctx->settings : &fallback);
    cairo_save(cr);
    cairo_rectangle(cr, body.x, body.y, body.w, body.h);
    cairo_clip(cr);
    for (int y = 0; y < k->rows; y++) paint_row(cr, k, y - k->scroll, grid.x, grid.y + (float)y * k->cell_h);
    paint_selection(cr, k, grid, lp_color_with_alpha(accent.base, 0.28f));
    int row, col, visible;
    lp_term_cursor(k->term, &row, &col, &visible);
    if (visible && !k->exited && row + k->scroll < k->rows) {
        lp_rect cursor = LP_RECT(grid.x + (float)col * k->cell_w, grid.y + (float)(row + k->scroll) * k->cell_h, k->cell_w, k->cell_h);
        if (ctx->active_window) lp_fill_solid(cr, cursor, lp_color_with_alpha(accent.base, 0.45f), 1.5f);
        else lp_draw_focus_ring(cr, lp_rect_inset(cursor, 0.5f, 0.5f), 1.5f, lp_color_with_alpha(accent.base, 0.6f), 1);
    }
    cairo_restore(cr);
    unsigned char rows[512];   /* the whole body repaints for now; take the damage so it does not pile up */
    lp_term_take_damage(k->term, rows, k->rows < 512 ? k->rows : 512);
}

/* MARK: - Menus and the app */

static void entry(lp_menu_model *m, const char *label, const char *shortcut, int arg, int disabled) {
    if (m->count >= LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "%s", label);
    e->shortcut = shortcut;
    e->command = LP_CMD_APP;
    e->arg = arg;
    e->disabled = disabled;
}

static void terminal_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    const struct terminal *k = state;
    if (!k || !k->term) return;
    if (menu == LP_MENU_EDIT) {
        entry(m, "Copy", "⌘C", LP_TERMINAL_COPY, !k->has_selection);
        entry(m, "Paste", "⌘V", LP_TERMINAL_PASTE, k->exited);
    } else if (menu == LP_MENU_VIEW) {
        entry(m, "Clear Scrollback", "⌘K", LP_TERMINAL_CLEAR_SCROLLBACK, 0);
    }
}

const lp_app lp_app_terminal = {
    .id = "terminal", .title = "Terminal", .name = "Terminal", .icon = LP_ICON_TERMINAL, .object = "terminal",
    .hidden = 1, .dock = 1, .raw_ctrl = 1,
    .default_rect = { NAN, NAN, 680, 460 }, .min_size = { 320, 200 }, .singleton = 0, .resizable = 1,
    .create = terminal_create, .paint = terminal_paint, .destroy = terminal_destroy,
    .command = terminal_command, .menu_entries = terminal_menu_entries,
};

/* MARK: - Previews and tests */

void lp_terminal_feed(void *state, const char *bytes) {
    struct terminal *k = state;
    if (!k || !k->term) return;
    if (!bytes) bytes = SAMPLE;
    lp_term_feed(k->term, bytes, strlen(bytes));
}

void lp_terminal_row_text(const void *state, int row, char *out, size_t n) {
    const struct terminal *k = state;
    if (n) out[0] = 0;
    if (k && k->term) lp_term_text(k->term, row, 0, row, k->cols - 1, out, n);
}

int lp_terminal_pid(const void *state) {
    const struct terminal *k = state;
    return k && !k->exited && !k->pty.exited ? (int)k->pty.pid : 0;
}

int lp_terminal_exited(const void *state) {
    const struct terminal *k = state;
    return k ? k->exited : 1;
}
