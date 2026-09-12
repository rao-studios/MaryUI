/* InfoApp — the Finder's Get Info window: the file's tile, name, kind, size,
 * where it lives, when it changed, who owns it, and an Open button. Internal:
 * opened by the Finder through lp_desktop_open_app_with("info", path). */
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "maryui/components/lp_button.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_files.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

struct info {
    char window_id[12];
    char path[LP_FILES_PATH_MAX];
    char name[LP_FILES_NAME_MAX];
    enum lp_file_kind kind;
    int is_dir, exists;
    char size[96], where[LP_FILES_PATH_MAX], modified[48], created[48], owner[96], perms[16];
    int items;               /* a folder's entry count */
};

static void *info_create(lp_desktop *d, const char *window_id) {
    struct info *s = calloc(1, sizeof *s);
    snprintf(s->window_id, sizeof s->window_id, "%s", window_id);
    return s;
}
static void info_destroy(void *state) { free(state); }

static void perms_string(unsigned mode, char *out) {
    const char *flags = "rwxrwxrwx";
    out[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    for (int i = 0; i < 9; i++) out[1 + i] = (mode & (0400 >> i)) ? flags[i] : '-';
    out[10] = 0;
}

static void info_open(void *state, lp_desktop *d, const char *path) {
    struct info *s = state;
    snprintf(s->path, sizeof s->path, "%s", path);
    lp_files_display_name(path, s->name, sizeof s->name);
    struct stat st;
    s->exists = stat(path, &st) == 0;
    if (!s->exists) return;
    s->is_dir = S_ISDIR(st.st_mode);
    s->kind = lp_files_kind(s->name, s->is_dir);
    time_t now = time(NULL);
    if (s->is_dir) {
        lp_file_list l = { 0 };
        s->items = lp_files_list(path, 0, &l) == 0 ? l.count : 0;
        lp_files_list_free(&l);
        snprintf(s->size, sizeof s->size, "%d item%s", s->items, s->items == 1 ? "" : "s");
    } else {
        char human[32];
        lp_files_format_size((int64_t)st.st_size, 0, human, sizeof human);
        if (st.st_size >= 1000) snprintf(s->size, sizeof s->size, "%s (%lld bytes)", human, (long long)st.st_size);
        else snprintf(s->size, sizeof s->size, "%s", human);
    }
    char parent[LP_FILES_PATH_MAX];
    lp_files_parent(path, parent, sizeof parent);
    lp_files_abbreviate(parent, s->where, sizeof s->where);
    lp_files_format_date(st.st_mtime, now, s->modified, sizeof s->modified);
    lp_files_format_date(st.st_ctime, now, s->created, sizeof s->created);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    snprintf(s->owner, sizeof s->owner, "%s (%s)", pw && pw->pw_name ? pw->pw_name : "?", gr && gr->gr_name ? gr->gr_name : "?");
    perms_string((unsigned)st.st_mode, s->perms);
}

static void row(cairo_t *cr, lp_rect *area, const char *label, const char *value) {
    lp_rect r = lp_rect_cut_top(area, 20);
    lp_text_style ls = lp_text_style_default();
    ls.size_px = LP_TEXT_SM; ls.color = LP_INK_TERTIARY;
    lp_text_draw(cr, label, LP_RECT(r.x, r.y, 80, r.h), &ls, LP_ALIGN_END);
    lp_text_style vs = lp_text_style_default();
    vs.size_px = LP_TEXT_SM; vs.ellipsize = 1;
    lp_text_draw(cr, value, LP_RECT(r.x + 88, r.y, r.w - 88, r.h), &vs, LP_ALIGN_START);
}

static void info_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct info preview;
    struct info *s = state ? state : &preview;
    if (!state && !preview.exists) {
        snprintf(preview.name, sizeof preview.name, "Design principles.md");
        preview.kind = LP_FILE_DOCUMENT;
        snprintf(preview.size, sizeof preview.size, "12 KB (12,288 bytes)");
        snprintf(preview.where, sizeof preview.where, "~/Documents");
        snprintf(preview.modified, sizeof preview.modified, "Aug 30, 2026");
        snprintf(preview.created, sizeof preview.created, "Aug 28, 2026");
        snprintf(preview.owner, sizeof preview.owner, "mary (mary)");
        snprintf(preview.perms, sizeof preview.perms, "-rw-r--r--");
        preview.exists = 1;
    }
    lp_id base = LP_ID("info");
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    lp_rect area = lp_rect_inset(body, LP_SPACE_4, LP_SPACE_4);

    /* the tile and the name */
    lp_rect head = lp_rect_cut_top(&area, 64);
    if (draw) {
        lp_file_icon_paint(cr, LP_RECT(head.x, head.y + 6, 52, 52), lp_files_kind_icon(s->kind), s->kind == LP_FILE_FOLDER, ctx->settings);
        lp_text_style ns = lp_text_style_default();
        ns.size_px = LP_TEXT_LG; ns.weight = LP_TEXT_WEIGHT_SEMIBOLD; ns.ellipsize = 1;
        lp_text_draw(cr, s->name, LP_RECT(head.x + 64, head.y + 10, head.w - 64, 22), &ns, LP_ALIGN_START);
        lp_text_style ks = lp_text_style_default();
        ks.size_px = LP_TEXT_SM; ks.color = LP_INK_SECONDARY;
        lp_text_draw(cr, s->exists ? lp_files_kind_label(s->kind) : "No such file", LP_RECT(head.x + 64, head.y + 34, head.w - 64, 18), &ks, LP_ALIGN_START);
    }
    area.y += LP_SPACE_2;
    if (draw) lp_draw_hairline(cr, LP_RECT(area.x, area.y - 1, area.w, 0), LP_EDGE_TOP, LP_EDGE_DIVIDER);
    area.y += LP_SPACE_3;
    if (draw && s->exists) {
        row(cr, &area, "Kind:", lp_files_kind_label(s->kind));
        row(cr, &area, "Size:", s->size);
        row(cr, &area, "Where:", s->where);
        row(cr, &area, "Modified:", s->modified);
        row(cr, &area, "Created:", s->created);
        row(cr, &area, "Owner:", s->owner);
        row(cr, &area, "Access:", s->perms);
    } else if (!draw) {
        area.y += 7 * 20;
    }

    /* Open, at the bottom right */
    lp_button_opts bo = { LP_BUTTON_PRIMARY, LP_CONTROL_MD, LP_ICON_COUNT, 0, !s->exists || !state };
    lp_size bs = lp_button_measure(ctx, "Open", bo);
    lp_rect br = LP_RECT(body.x + body.w - LP_SPACE_4 - bs.w, body.y + body.h - LP_SPACE_4 - bs.h, bs.w, bs.h);
    if (lp_button(ctx, lp_id_index(base, 1), br, "Open", bo) && state) {
        if (!lp_desktop_open_path(d, s->path)) ctx->dirty = 1;
    }
}

const lp_app lp_app_info = {
    .id = "info", .title = "Info", .name = "Info", .icon = LP_ICON_INFO, .hidden = 1, .internal = 1,
    .default_rect = { NAN, NAN, 320, 380 }, .min_size = { 320, 380 }, .singleton = 0, .resizable = 0,
    .create = info_create, .paint = info_paint, .destroy = info_destroy, .open = info_open,
};
