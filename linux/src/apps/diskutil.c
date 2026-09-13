/* Disk Utility — the drives, what is on them and where it is mounted, with
 * Mount, Unmount and Eject. The drives are lp_disks (sysfs, udev's database,
 * /proc/mounts), reread every three seconds and not while the window is
 * shaded. Each action is udisksctl run as an lp_job, so udisks2 and its polkit
 * rules decide who may, and the window never waits on a slow drive. Volumes the
 * system runs from (/, /boot…) are never offered for unmounting, nor their
 * drive for ejecting. Linux only (PARITY D15). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_disks.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_files.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_job.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define REFRESH_MS 3000
#define INFO_ROW_H 22

typedef lp_job *(*job_runner)(lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user);

struct diskutil {
    char window_id[12];
    lp_desktop *desk;
    lp_disks *disks;              /* on the heap: sixteen drives of sixteen volumes each */
    int err;
    char sys_block[512], udev_data[512], mounts_path[512];
    lp_source *timer;
    char selected[64];            /* the device selected, drive or volume: it survives a reread */
    lp_job *job;                  /* the action under way */
    int job_command;
    char job_name[96];
    char status[256];
    job_runner run;
};

static void refresh(struct diskutil *u) {
    char *mounts = NULL;
    size_t len = 0;
    if (lp_files_read(u->mounts_path, &mounts, &len) != 0) mounts = NULL;
    u->err = lp_disks_read_from(u->sys_block, u->udev_data, mounts ? mounts : "", u->disks);
    free(mounts);
}

static int find(const struct diskutil *u, const char *device, int *drive, int *volume) {
    for (int i = 0; i < u->disks->count; i++) {
        const lp_disk_drive *d = &u->disks->drives[i];
        if (strcmp(d->device, device) == 0) { *drive = i; *volume = -1; return 1; }
        for (int v = 0; v < d->nvolumes; v++) {
            if (strcmp(d->volumes[v].device, device) == 0) { *drive = i; *volume = v; return 1; }
        }
    }
    return 0;
}

static const lp_disk_drive *selected_drive(const struct diskutil *u, const lp_disk_volume **volume) {
    int d, v;
    *volume = NULL;
    if (!u->selected[0] || !find(u, u->selected, &d, &v)) return NULL;
    if (v >= 0) *volume = &u->disks->drives[d].volumes[v];
    return &u->disks->drives[d];
}

static int can(const struct diskutil *u, int command) {
    const lp_disk_volume *v;
    const lp_disk_drive *d = selected_drive(u, &v);
    if (!d) return 0;
    switch ((enum lp_diskutil_command)command) {
    case LP_DISKUTIL_MOUNT: return !u->job && v && !v->mount_point[0] && v->fs_type[0];
    case LP_DISKUTIL_UNMOUNT: return !u->job && v && v->mount_point[0] && !v->system;
    case LP_DISKUTIL_EJECT: return !u->job && !d->system && strcmp(d->kind, "Virtual") != 0;
    case LP_DISKUTIL_SHOW_IN_FINDER: return v && v->mount_point[0];
    }
    return 0;
}

/* udisksctl's errors end in the part a person can use: "…Error.DeviceBusy: target is busy". */
static void reason_of(const char *output, char *out, size_t n) {
    char line[512];
    snprintf(line, sizeof line, "%s", output);
    line[strcspn(line, "\n")] = 0;
    const char *text = line;
    if (strstr(line, "GDBus.Error:")) {
        const char *last = strrchr(line, ':');
        if (last && last[1]) text = last + 1;
    }
    while (*text == ' ') text++;
    snprintf(out, n, "%s", text[0] ? text : "udisksctl gave no reason");
}

static void job_done(int status, const char *output, void *user) {
    struct diskutil *u = user;
    static const char *const DONE[] = { "Mounted", "Unmounted", "Ejected" };
    static const char *const VERB[] = { "mount", "unmount", "eject" };
    int c = u->job_command < 3 ? u->job_command : 0;
    if (status == 0) {
        snprintf(u->status, sizeof u->status, "%s “%s”.", DONE[c], u->job_name);
    } else {
        char reason[256];
        reason_of(output, reason, sizeof reason);
        snprintf(u->status, sizeof u->status, "Could not %s “%s”: %s", VERB[c], u->job_name, reason);
    }
    u->job = NULL;
    refresh(u);
    if (u->desk && u->desk->on_app_dirty) u->desk->on_app_dirty(u->desk, u->window_id);
}

/* Unmounts every mounted volume, then powers the drive off: one job, stopping at the first refusal. */
static const char EJECT_SCRIPT[] =
    "drive=$1; shift; for volume; do udisksctl unmount --no-user-interaction -b \"$volume\" || exit $?; done; "
    "udisksctl power-off --no-user-interaction -b \"$drive\"";

static void start(struct diskutil *u, int command) {
    if (!can(u, command)) return;
    const lp_disk_volume *v;
    const lp_disk_drive *d = selected_drive(u, &v);
    const char *argv[LP_DISKS_MAX_VOLUMES + 8];
    if (command == LP_DISKUTIL_EJECT) {
        int n = 0;
        argv[n++] = "/bin/sh";
        argv[n++] = "-c";
        argv[n++] = EJECT_SCRIPT;
        argv[n++] = "eject";
        argv[n++] = d->device;
        for (int i = 0; i < d->nvolumes; i++) if (d->volumes[i].mount_point[0]) argv[n++] = d->volumes[i].device;
        argv[n] = NULL;
        snprintf(u->job_name, sizeof u->job_name, "%s", d->name);
    } else {
        lp_disks_action_argv(command == LP_DISKUTIL_MOUNT ? LP_DISK_MOUNT : LP_DISK_UNMOUNT, v->device, argv, 8);
        lp_disks_volume_name(d, v, u->job_name, sizeof u->job_name);
    }
    u->job_command = command;
    u->status[0] = 0;
    u->job = (lp_job *)1;   /* busy from here: a job without an event loop finishes inside run */
    lp_job *job = u->run(u->desk, argv, job_done, u);
    if (u->job) u->job = job;
}

static void diskutil_command(void *state, lp_desktop *d, int cmd) {
    struct diskutil *u = state;
    if (!u) return;
    if (cmd == LP_DISKUTIL_SHOW_IN_FINDER) {
        const lp_disk_volume *v;
        selected_drive(u, &v);
        if (v && v->mount_point[0] && d && !lp_desktop_open_path(d, v->mount_point))
            snprintf(u->status, sizeof u->status, "The Finder cannot open “%s”.", v->mount_point);
        return;
    }
    start(u, cmd);
}

static int on_tick(int fd, uint32_t mask, void *data) {
    struct diskutil *u = data;
    int w = lp_wm_find(&u->desk->wm, u->window_id);
    if (w >= 0 && u->desk->wm.windows[w].state != LP_WIN_SHADED && !u->job) {
        refresh(u);
        if (u->desk->on_app_dirty) u->desk->on_app_dirty(u->desk, u->window_id);
    }
    lp_desktop_update_timer(u->desk, u->timer, REFRESH_MS);
    return 0;
}

/* MARK: - Painting */

static void paint_toolbar(lp_ctx *ctx, struct diskutil *u, lp_desktop *d, lp_rect bar, lp_id base) {
    static const struct { const char *label; lp_icon icon; int command; } TOOLS[] = {
        { "Mount", LP_ICON_DRIVE, LP_DISKUTIL_MOUNT },
        { "Unmount", LP_ICON_STOP, LP_DISKUTIL_UNMOUNT },
        { "Eject", LP_ICON_EJECT, LP_DISKUTIL_EJECT },
        { "Show in Finder", LP_ICON_FOLDER, LP_DISKUTIL_SHOW_IN_FINDER },
    };
    float x = bar.x, cy = bar.y + bar.h / 2;
    for (int i = 0; i < 4; i++) {
        lp_button_opts o = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, TOOLS[i].icon, 0, !can(u, TOOLS[i].command) };
        lp_size s = lp_button_measure(ctx, TOOLS[i].label, o);
        if (lp_button(ctx, lp_id_index(base, 1 + i), LP_RECT(x, cy - s.h / 2, s.w, s.h), TOOLS[i].label, o) && can(u, TOOLS[i].command)) {
            diskutil_command(u, d, TOOLS[i].command);
            ctx->dirty = 1;
        }
        x += s.w + (i == 2 ? LP_SPACE_4 : LP_SPACE_2);
    }
}

static void paint_sidebar(lp_ctx *ctx, struct diskutil *u, lp_rect *area, lp_id base) {
    lp_rect cursor = lp_sidebar(ctx, area);
    for (int pass = 0; pass < 2; pass++) {
        int any = 0;
        for (int i = 0; i < u->disks->count; i++) {
            const lp_disk_drive *d = &u->disks->drives[i];
            int external = d->removable || strcmp(d->kind, "USB") == 0 || strcmp(d->kind, "SD card") == 0;
            if (external != pass) continue;
            if (!any) { if (pass) cursor.y += LP_SPACE_3; lp_sidebar_section(ctx, &cursor, pass ? "External" : "Internal"); any = 1; }
            if (lp_sidebar_item(ctx, lp_id_index(base, 100 + i * 32), &cursor, external ? LP_ICON_EJECT : LP_ICON_DRIVE, d->name, strcmp(u->selected, d->device) == 0)) {
                snprintf(u->selected, sizeof u->selected, "%s", d->device);
                ctx->dirty = 1;
            }
            for (int v = 0; v < d->nvolumes; v++) {
                char label[96], name[80];
                lp_disks_volume_name(d, &d->volumes[v], name, sizeof name);
                snprintf(label, sizeof label, "   %s", name);   /* a volume sits under its drive */
                if (lp_sidebar_item(ctx, lp_id_index(base, 100 + i * 32 + 1 + v), &cursor, LP_ICON_COUNT, label, strcmp(u->selected, d->volumes[v].device) == 0)) {
                    snprintf(u->selected, sizeof u->selected, "%s", d->volumes[v].device);
                    ctx->dirty = 1;
                }
            }
        }
    }
}

static void info_row(cairo_t *cr, float x, float *y, float w, const char *label, const char *value) {
    lp_text_style key = lp_text_style_default();
    key.size_px = LP_TEXT_SM;
    key.color = LP_INK_TERTIARY;
    lp_text_style val = key;
    val.color = LP_INK_PRIMARY;
    val.ellipsize = 1;
    lp_text_draw(cr, label, LP_RECT(x, *y, 110, INFO_ROW_H), &key, LP_ALIGN_END);
    lp_text_draw(cr, value, LP_RECT(x + 110 + LP_SPACE_3, *y, w - 110 - LP_SPACE_3, INFO_ROW_H), &val, LP_ALIGN_START);
    *y += INFO_ROW_H;
}

static void paint_detail(lp_ctx *ctx, struct diskutil *u, lp_rect area) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    lp_fill_solid(cr, area, LP_SURFACE_BODY, 0);
    lp_text_style st = lp_text_style_default();
    st.color = LP_INK_SECONDARY;
    if (u->err) {
        char text[160];
        snprintf(text, sizeof text, "Disk Utility cannot read the drives: %s", strerror(-u->err));
        lp_text_draw(cr, text, area, &st, LP_ALIGN_CENTER);
        return;
    }
    const lp_disk_volume *v;
    const lp_disk_drive *d = selected_drive(u, &v);
    if (!d) {
        lp_text_draw(cr, u->disks->count ? "Select a drive or a volume." : "No drives.", area, &st, LP_ALIGN_CENTER);
        return;
    }
    lp_rect inner = lp_rect_inset(area, LP_SPACE_6, LP_SPACE_5);
    lp_icon_paint_object(cr, LP_ICON_DRIVE, LP_RECT(inner.x, inner.y, 64, 64), 1.6f, LP_INK_SECONDARY, ctx->settings);
    char title[96], subtitle[160], size[32], free_text[32], used[32];
    if (v) lp_disks_volume_name(d, v, title, sizeof title);
    else snprintf(title, sizeof title, "%s", d->name);
    lp_files_format_size((int64_t)(v ? v->size : d->size), 0, size, sizeof size);
    if (v) snprintf(subtitle, sizeof subtitle, "%s%s volume · %s", v->fs_type[0] ? v->fs_type : "Unformatted", v->fs_type[0] ? "" : "", size);
    else snprintf(subtitle, sizeof subtitle, "%s drive · %s", d->kind, size);
    lp_text_style ts = lp_text_style_default();
    ts.size_px = LP_TEXT_XL;
    ts.weight = LP_TEXT_WEIGHT_BOLD;
    ts.emboss = 1;
    ts.ellipsize = 1;
    float tx = inner.x + 64 + LP_SPACE_4, tw = inner.x + inner.w - tx;
    lp_text_draw(cr, title, LP_RECT(tx, inner.y + 8, tw, 26), &ts, LP_ALIGN_START);
    lp_text_style ss = lp_text_style_default();
    ss.size_px = LP_TEXT_SM;
    ss.color = LP_INK_TERTIARY;
    lp_text_draw(cr, subtitle, LP_RECT(tx, inner.y + 36, tw, 18), &ss, LP_ALIGN_START);

    float y = inner.y + 64 + LP_SPACE_5;
    if (v && v->mount_point[0]) {
        uint64_t in_use = v->size > v->free_bytes ? v->size - v->free_bytes : 0;
        lp_files_format_size((int64_t)in_use, 0, used, sizeof used);
        lp_files_format_size((int64_t)v->free_bytes, 0, free_text, sizeof free_text);
        lp_progress(ctx, LP_RECT(inner.x, y, inner.w, LP_PROGRESS_H), v->size ? (float)((double)in_use / (double)v->size) : 0);
        y += LP_PROGRESS_H + LP_SPACE_2;
        char line[96];
        snprintf(line, sizeof line, "%s used · %s available", used, free_text);
        lp_text_draw(cr, line, LP_RECT(inner.x, y, inner.w, 16), &ss, LP_ALIGN_START);
        y += 16 + LP_SPACE_4;
    }
    if (v) {
        info_row(cr, inner.x, &y, inner.w, "Mount Point", v->mount_point[0] ? v->mount_point : "Not mounted");
        info_row(cr, inner.x, &y, inner.w, "Format", v->fs_type[0] ? v->fs_type : "Unknown");
        info_row(cr, inner.x, &y, inner.w, "Capacity", size);
        info_row(cr, inner.x, &y, inner.w, "Device", v->device);
        if (v->system) info_row(cr, inner.x, &y, inner.w, "Used by", "the system, which runs from it");
    } else {
        char count[32];
        snprintf(count, sizeof count, "%d", d->nvolumes);
        info_row(cr, inner.x, &y, inner.w, "Kind", d->kind);
        info_row(cr, inner.x, &y, inner.w, "Capacity", size);
        info_row(cr, inner.x, &y, inner.w, "Volumes", count);
        info_row(cr, inner.x, &y, inner.w, "Device", d->device);
        info_row(cr, inner.x, &y, inner.w, "Removable", d->removable ? "Yes" : "No");
    }
    y += LP_SPACE_4;
    if (u->job) {
        lp_progress(ctx, LP_RECT(inner.x, y, 160, LP_PROGRESS_H), -1);
        char working[128];
        snprintf(working, sizeof working, "Working on “%s”…", u->job_name);
        lp_text_draw(cr, working, LP_RECT(inner.x + 160 + LP_SPACE_3, y - 4, inner.w - 172, 16), &ss, LP_ALIGN_START);
    } else if (u->status[0]) {
        lp_text_draw(cr, u->status, LP_RECT(inner.x, y, inner.w, 16), &ss, LP_ALIGN_START);
    }
}

static void diskutil_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static lp_disks none;
    static struct diskutil empty = { .disks = &none };
    struct diskutil *u = state ? state : &empty;
    lp_id base = LP_ID("diskutil");
    lp_rect area = body;
    lp_rect bar = lp_toolbar(ctx, &area);
    paint_toolbar(ctx, u, state ? d : NULL, bar, base);
    paint_sidebar(ctx, u, &area, base);
    paint_detail(ctx, u, area);
}

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

static void diskutil_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    const struct diskutil *u = state;
    if (!u || menu != LP_MENU_FILE) return;
    entry(m, "Mount", NULL, LP_DISKUTIL_MOUNT, !can(u, LP_DISKUTIL_MOUNT));
    entry(m, "Unmount", NULL, LP_DISKUTIL_UNMOUNT, !can(u, LP_DISKUTIL_UNMOUNT));
    entry(m, "Eject", "⌘E", LP_DISKUTIL_EJECT, !can(u, LP_DISKUTIL_EJECT));
    entry(m, "Show in Finder", NULL, LP_DISKUTIL_SHOW_IN_FINDER, !can(u, LP_DISKUTIL_SHOW_IN_FINDER));
}

static void select_first(struct diskutil *u) {
    /* an external volume if there is one — it is what people open Disk Utility for — else the first drive */
    for (int i = 0; i < u->disks->count; i++) {
        const lp_disk_drive *d = &u->disks->drives[i];
        if (!d->system && d->nvolumes > 0 && strcmp(d->kind, "Virtual") != 0) { snprintf(u->selected, sizeof u->selected, "%s", d->volumes[0].device); return; }
    }
    if (u->disks->count) snprintf(u->selected, sizeof u->selected, "%s", u->disks->drives[0].device);
}

static void *diskutil_create(lp_desktop *d, const char *window_id) {
    struct diskutil *u = calloc(1, sizeof *u);
    if (!u) return NULL;
    u->disks = calloc(1, sizeof *u->disks);
    if (!u->disks) { free(u); return NULL; }
    snprintf(u->window_id, sizeof u->window_id, "%s", window_id);
    u->desk = d;
    snprintf(u->sys_block, sizeof u->sys_block, "/sys/block");
    snprintf(u->udev_data, sizeof u->udev_data, "/run/udev/data");
    snprintf(u->mounts_path, sizeof u->mounts_path, "/proc/mounts");
    u->run = lp_job_run;
    refresh(u);
    select_first(u);
    if (d) u->timer = lp_desktop_add_timer(d, REFRESH_MS, on_tick, u);
    return u;
}

static void diskutil_destroy(void *state) {
    struct diskutil *u = state;
    if (!u) return;
    if (u->timer) lp_desktop_remove_source(u->desk, u->timer);
    if (u->job && u->run == lp_job_run) lp_job_cancel(u->job);   /* the drive finishes what it started; nobody is told */
    free(u->disks);
    free(u);
}

const lp_app lp_app_diskutil = {
    .id = "diskutil", .title = "Disk Utility", .name = "Disk Utility", .icon = LP_ICON_DRIVE, .object = "volumeInternal",
    .default_rect = { NAN, NAN, 760, 480 }, .min_size = { 560, 340 }, .singleton = 1, .resizable = 1,
    .create = diskutil_create, .paint = diskutil_paint, .destroy = diskutil_destroy,
    .command = diskutil_command, .menu_entries = diskutil_menu_entries,
};

/* MARK: - Tests */

void lp_diskutil_set_paths(void *state, const char *sys_block, const char *udev_data, const char *mounts_path) {
    struct diskutil *u = state;
    snprintf(u->sys_block, sizeof u->sys_block, "%s", sys_block);
    snprintf(u->udev_data, sizeof u->udev_data, "%s", udev_data);
    snprintf(u->mounts_path, sizeof u->mounts_path, "%s", mounts_path);
    refresh(u);
    u->selected[0] = 0;
    select_first(u);
}
void lp_diskutil_set_runner(void *state, lp_job *(*run)(lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user)) {
    ((struct diskutil *)state)->run = run;
}
void lp_diskutil_select(void *state, const char *device) { snprintf(((struct diskutil *)state)->selected, 64, "%s", device); }
const char *lp_diskutil_selected(const void *state) { return ((const struct diskutil *)state)->selected; }
int lp_diskutil_can(const void *state, int command) { return can(state, command); }
int lp_diskutil_busy(const void *state) { return ((const struct diskutil *)state)->job != NULL; }
const char *lp_diskutil_status(const void *state) { return ((const struct diskutil *)state)->status; }
void lp_diskutil_refresh(void *state) { refresh(state); }
