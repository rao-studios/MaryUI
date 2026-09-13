/* Media Player — a song or a movie, and the rest of its folder after it. Video
 * is fitted on black; audio shows its cover art and tags. A scrubber and the
 * time sit over previous / play / next and the volume; Space plays and pauses,
 * ←/→ skip five seconds, ↑/↓ change the volume, ⌘←/⌘→ step through the folder.
 * The engine is lp_media (GStreamer, woken through an fd the desktop watches).
 * The Finder opens every audio and video file here. Linux only (PARITY D15). */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_media.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define CONTROLS_H 76
#define TICK_MS 250
#define SKIP_SECONDS 5

struct player {
    char window_id[12];
    lp_desktop *desk;
    char path[LP_FILES_PATH_MAX];
    lp_media *media;
    int err;                    /* lp_media_open's, when the file would not open */
    lp_source *wake, *ticker;
    char **queue;
    int queue_count, queue_at;
    float scrub;                /* the scrubber's value: the position, or where it is being dragged */
    float volume;               /* 0 … 100 */
};

static void set_title(struct player *p) {
    if (!p->desk || !p->media || lp_wm_find(&p->desk->wm, p->window_id) < 0) return;
    lp_wm_action a = { .type = LP_WM_SET_TITLE, .id = p->window_id, .title = lp_media_info_of(p->media)->title };
    lp_desktop_dispatch(p->desk, &a);
}

static void dirty(struct player *p) {
    if (p->desk && p->desk->on_app_dirty) p->desk->on_app_dirty(p->desk, p->window_id);
}

static int on_tick(int fd, uint32_t mask, void *data);
static int on_wake(int fd, uint32_t mask, void *data);

static void ensure_ticking(struct player *p) {
    if (!p->desk) return;
    if (!p->ticker) p->ticker = lp_desktop_add_timer(p->desk, TICK_MS, on_tick, p);
    else lp_desktop_update_timer(p->desk, p->ticker, TICK_MS);
}

static void unload(struct player *p) {
    if (p->wake) lp_desktop_remove_source(p->desk, p->wake);
    p->wake = NULL;
    lp_media_close(p->media);
    p->media = NULL;
}

/* Opens path and plays it; within_queue keeps the queue it came from. */
static void load(struct player *p, const char *path, int within_queue) {
    unload(p);
    snprintf(p->path, sizeof p->path, "%s", path);
    if (!within_queue) {
        lp_media_queue_free(p->queue, p->queue_count);
        p->queue_count = lp_media_queue(path, &p->queue, &p->queue_at);
    }
    const char *silent = getenv("MARYUI_MEDIA_SILENT");   /* tests and previews play into a fake sink */
    p->err = lp_media_open(&p->media, path, silent && *silent == '1' ? LP_MEDIA_SILENT : 0);
    p->scrub = 0;
    if (p->err) return;
    lp_media_set_volume(p->media, p->volume / 100.0);
    if (p->desk) p->wake = lp_desktop_add_fd(p->desk, lp_media_wake_fd(p->media), LP_SOURCE_READABLE, on_wake, p);
    lp_media_play(p->media);
    ensure_ticking(p);
}

static void step(struct player *p, int delta) {
    int to = p->queue_at + delta;
    if (!p->queue || to < 0 || to >= p->queue_count) return;
    p->queue_at = to;
    load(p, p->queue[to], 1);
    set_title(p);
}

static int on_wake(int fd, uint32_t mask, void *data) {
    struct player *p = data;
    if (!p->media) return 0;
    int changed = lp_media_update(p->media);
    if ((changed & LP_MEDIA_CHANGED_STATE) && lp_media_state(p->media) == LP_MEDIA_ENDED && p->queue_at + 1 < p->queue_count) {
        step(p, 1);   /* the next one in the folder, as a record would */
        dirty(p);
        return 0;
    }
    if (changed & LP_MEDIA_CHANGED_INFO) set_title(p);
    if (changed) dirty(p);
    return 0;
}

static int on_tick(int fd, uint32_t mask, void *data) {
    struct player *p = data;
    if (!p->media || lp_media_state(p->media) != LP_MEDIA_PLAYING) return 0;   /* not re-armed: an idle player schedules nothing */
    dirty(p);
    lp_desktop_update_timer(p->desk, p->ticker, TICK_MS);
    return 0;
}

static void player_command(void *state, lp_desktop *d, int cmd) {
    struct player *p = state;
    if (!p) return;
    lp_media *m = p->media;
    switch ((enum lp_player_command)cmd) {
    case LP_PLAYER_PLAY_PAUSE:
        if (!m) break;
        if (lp_media_state(m) == LP_MEDIA_PLAYING) lp_media_pause(m);
        else { lp_media_play(m); ensure_ticking(p); }
        break;
    case LP_PLAYER_PREVIOUS:
        /* back to the start first, as players do; to the one before when already there */
        if (m && lp_media_position(m) > 3) lp_media_seek(m, 0);
        else step(p, -1);
        break;
    case LP_PLAYER_NEXT: step(p, 1); break;
    case LP_PLAYER_SKIP_BACK: if (m) lp_media_seek(m, lp_media_position(m) - SKIP_SECONDS); break;
    case LP_PLAYER_SKIP_FORWARD: if (m) lp_media_seek(m, lp_media_position(m) + SKIP_SECONDS); break;
    case LP_PLAYER_VOLUME_UP:
    case LP_PLAYER_VOLUME_DOWN:
        p->volume = fminf(100, fmaxf(0, p->volume + (cmd == LP_PLAYER_VOLUME_UP ? 10 : -10)));
        if (m) lp_media_set_volume(m, p->volume / 100.0);
        break;
    case LP_PLAYER_SHOW_IN_FINDER: {
        char dir[LP_FILES_PATH_MAX];
        if (p->path[0]) lp_files_parent(p->path, dir, sizeof dir);
        else lp_files_user_dir(LP_USER_HOME, dir, sizeof dir);
        if (d) lp_desktop_open_path(d, dir);
        break;
    }
    }
}

/* MARK: - Painting */

static void paint_message(cairo_t *cr, lp_rect r, const char *title, const char *detail) {
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_LG;
    st.weight = LP_TEXT_WEIGHT_SEMIBOLD;
    st.color = LP_INK_SECONDARY;
    float y = r.y + r.h / 2 - 20;
    lp_icon_draw(cr, LP_ICON_MUSIC, r.x + r.w / 2 - 24, y - 60, 48, 1.4f, LP_INK_TERTIARY);
    lp_text_draw(cr, title, LP_RECT(r.x, y, r.w, 22), &st, LP_ALIGN_CENTER);
    st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.color = LP_INK_TERTIARY;
    lp_text_draw(cr, detail, LP_RECT(r.x + LP_SPACE_4, y + 24, r.w - 2 * LP_SPACE_4, 18), &st, LP_ALIGN_CENTER);
}

static void paint_content(lp_ctx *ctx, struct player *p, lp_rect r) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    if (!p->path[0]) {
        lp_fill_solid(cr, r, LP_PLATINUM_2, 0);
        paint_message(cr, r, "Nothing playing", "Open a song or a movie from the Finder.");
        return;
    }
    if (p->err || lp_media_state(p->media) == LP_MEDIA_FAILED) {
        lp_fill_solid(cr, r, LP_PLATINUM_2, 0);
        const char *why = p->err == -ENOTSUP ? "This build of the desktop has no GStreamer to play it with."
                        : p->err ? strerror(-p->err) : lp_media_info_of(p->media)->error;
        paint_message(cr, r, "The Media Player can’t play this file", why);
        return;
    }
    const lp_media_info *info = lp_media_info_of(p->media);
    cairo_surface_t *frame = lp_media_frame(p->media);
    if (info->has_video || frame) {
        lp_fill_solid(cr, r, LP_RGBA(0, 0, 0, 1), 0);
        if (frame) {
            float fw = (float)cairo_image_surface_get_width(frame), fh = (float)cairo_image_surface_get_height(frame);
            float s = fminf(r.w / fw, r.h / fh);
            float w = roundf(fw * s), h = roundf(fh * s);
            cairo_save(cr);
            cairo_rectangle(cr, r.x, r.y, r.w, r.h);
            cairo_clip(cr);
            cairo_translate(cr, r.x + roundf((r.w - w) / 2), r.y + roundf((r.h - h) / 2));
            cairo_scale(cr, w / fw, h / fh);
            cairo_set_source_surface(cr, frame, 0, 0);
            /* moving pictures scale fast; a paused one is looked at, so it scales well */
            cairo_pattern_set_filter(cairo_get_source(cr), lp_media_state(p->media) == LP_MEDIA_PLAYING ? CAIRO_FILTER_FAST : CAIRO_FILTER_GOOD);
            cairo_paint(cr);
            cairo_restore(cr);
        }
        return;
    }
    /* audio: the cover, or the music object, beside the tags */
    lp_fill_solid(cr, r, LP_PLATINUM_2, 0);
    float art = fminf(200, fminf(r.h - 2 * LP_SPACE_6, r.w * 0.4f));
    lp_rect cover = LP_RECT(r.x + LP_SPACE_6, r.y + (r.h - art) / 2, art, art);
    cairo_surface_t *image = lp_media_cover(p->media);
    if (image) {
        float iw = (float)cairo_image_surface_get_width(image), ih = (float)cairo_image_surface_get_height(image);
        float s = fmaxf(art / iw, art / ih);
        cairo_save(cr);
        cairo_rectangle(cr, cover.x, cover.y, art, art);
        cairo_clip(cr);
        cairo_translate(cr, cover.x + (art - iw * s) / 2, cover.y + (art - ih * s) / 2);
        cairo_scale(cr, s, s);
        cairo_set_source_surface(cr, image, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
        cairo_paint(cr);
        cairo_restore(cr);
        lp_draw_focus_ring(cr, cover, 0, LP_EDGE_DIVIDER, 1);
    } else {
        lp_icon_paint_object(cr, LP_ICON_MUSIC, cover, 1.6f, LP_INK_SECONDARY, ctx->settings);
    }
    float tx = cover.x + art + LP_SPACE_6, tw = r.x + r.w - LP_SPACE_6 - tx, ty = r.y + r.h / 2 - 40;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XL;
    st.weight = LP_TEXT_WEIGHT_BOLD;
    st.emboss = 1;
    st.ellipsize = 1;
    lp_text_draw(cr, info->title, LP_RECT(tx, ty, tw, 28), &st, LP_ALIGN_START);
    st = lp_text_style_default();
    st.size_px = LP_TEXT_MD;
    st.color = LP_INK_SECONDARY;
    st.ellipsize = 1;
    lp_text_draw(cr, info->artist[0] ? info->artist : "Unknown artist", LP_RECT(tx, ty + 32, tw, 20), &st, LP_ALIGN_START);
    st.color = LP_INK_TERTIARY;
    lp_text_draw(cr, info->album, LP_RECT(tx, ty + 52, tw, 20), &st, LP_ALIGN_START);
    if (p->queue_count > 1) {
        char place[48];
        snprintf(place, sizeof place, "%d of %d in this folder", p->queue_at + 1, p->queue_count);
        st.size_px = LP_TEXT_SM;
        lp_text_draw(cr, place, LP_RECT(tx, ty + 80, tw, 18), &st, LP_ALIGN_START);
    }
}

static void paint_controls(lp_ctx *ctx, struct player *p, lp_desktop *d, lp_rect r, lp_id base) {
    cairo_t *cr = ctx->cr;
    int draw = ctx->pass == LP_PASS_DRAW && cr;
    if (draw) {
        lp_fill_vgradient(cr, r, LP_PLATINUM_2, LP_PLATINUM_3, 0);
        lp_draw_hairline(cr, r, LP_EDGE_TOP, LP_EDGE_DIVIDER);
    }
    lp_media *m = p->media;
    int ready = m && !p->err && lp_media_state(m) != LP_MEDIA_FAILED;
    double duration = ready ? lp_media_info_of(m)->duration : 0, position = ready ? lp_media_position(m) : 0;
    lp_rect inner = lp_rect_inset(r, LP_SPACE_4, LP_SPACE_2);

    /* the scrubber, with the time either side */
    lp_rect row = LP_RECT(inner.x, inner.y, inner.w, 20);
    lp_id scrubber = lp_id_index(base, 1);
    if (!lp_is_active(ctx, scrubber)) p->scrub = (float)position;
    char elapsed[16], remaining[24];
    lp_media_format_time(position, elapsed, sizeof elapsed);
    char left[16];
    lp_media_format_time(duration - position, left, sizeof left);
    snprintf(remaining, sizeof remaining, "-%s", left);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS;
    st.color = LP_INK_SECONDARY;
    st.tabular_nums = 1;
    if (draw) {
        lp_text_draw(cr, elapsed, LP_RECT(row.x, row.y, 48, row.h), &st, LP_ALIGN_START);
        lp_text_draw(cr, remaining, LP_RECT(row.x + row.w - 52, row.y, 52, row.h), &st, LP_ALIGN_END);
    }
    lp_slider_opts so = { .min = 0, .max = duration > 0 ? (float)duration : 1, .step = 0.001f, .disabled = !ready || duration <= 0 };
    if (lp_slider(ctx, scrubber, LP_RECT(row.x + 52, row.y + (row.h - LP_SLIDER_H) / 2, row.w - 108, LP_SLIDER_H), &p->scrub, so) && ready) {
        lp_media_seek(m, p->scrub);
        ctx->dirty = 1;
    }

    /* previous · play/pause · next, centred; the volume at the right */
    float cy = inner.y + 20 + (inner.h - 20) / 2;
    lp_button_opts round_opts = { LP_BUTTON_DEFAULT, LP_CONTROL_MD, LP_ICON_PREVIOUS, 1, !ready };
    lp_size bs = lp_button_measure(ctx, "", round_opts);
    float cx = inner.x + inner.w / 2;
    int playing = ready && lp_media_state(m) == LP_MEDIA_PLAYING;
    lp_button_opts play = { LP_BUTTON_PRIMARY, LP_CONTROL_MD, playing ? LP_ICON_PAUSE : LP_ICON_PLAY, 1, !ready };
    lp_button_opts next = round_opts;
    next.icon = LP_ICON_NEXT;
    next.disabled = !ready || p->queue_at + 1 >= p->queue_count;
    int cmd = -1;
    if (lp_button(ctx, lp_id_index(base, 2), LP_RECT(cx - bs.w * 1.5f - LP_SPACE_3, cy - bs.h / 2, bs.w, bs.h), "", round_opts) && ready) cmd = LP_PLAYER_PREVIOUS;
    if (lp_button(ctx, lp_id_index(base, 3), LP_RECT(cx - bs.w / 2, cy - bs.h / 2, bs.w, bs.h), "", play) && ready) cmd = LP_PLAYER_PLAY_PAUSE;
    if (lp_button(ctx, lp_id_index(base, 4), LP_RECT(cx + bs.w / 2 + LP_SPACE_3, cy - bs.h / 2, bs.w, bs.h), "", next) && !next.disabled) cmd = LP_PLAYER_NEXT;
    if (cmd >= 0 && ctx->pass == LP_PASS_EVENT) { player_command(p, d, cmd); ctx->dirty = 1; }

    lp_rect volume = LP_RECT(inner.x + inner.w - 120, cy - LP_SLIDER_H / 2, 120, LP_SLIDER_H);
    if (draw) lp_icon_draw(cr, p->volume > 0 ? LP_ICON_VOLUME : LP_ICON_MUTE, volume.x - 22, cy - 8, 16, 0, LP_INK_SECONDARY);
    if (lp_slider(ctx, lp_id_index(base, 5), volume, &p->volume, (lp_slider_opts){ .min = 0, .max = 100, .step = 1 }) && m) {
        lp_media_set_volume(m, p->volume / 100.0);
        ctx->dirty = 1;
    }
}

static void player_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct player empty = { .volume = 80, .queue_at = -1 };
    struct player *p = state ? state : &empty;
    lp_id base = LP_ID("player");
    if (ctx->pass == LP_PASS_EVENT && ctx->in.key_pressed && state) {
        int super = (ctx->in.mods & (LP_MOD_LOGO | LP_MOD_CTRL)) != 0, cmd = -1;
        switch (ctx->in.keysym) {
        case XKB_KEY_space: cmd = LP_PLAYER_PLAY_PAUSE; break;
        case XKB_KEY_Left: cmd = super ? LP_PLAYER_PREVIOUS : LP_PLAYER_SKIP_BACK; break;
        case XKB_KEY_Right: cmd = super ? LP_PLAYER_NEXT : LP_PLAYER_SKIP_FORWARD; break;
        case XKB_KEY_Up: cmd = LP_PLAYER_VOLUME_UP; break;
        case XKB_KEY_Down: cmd = LP_PLAYER_VOLUME_DOWN; break;
        default: break;
        }
        if (cmd >= 0) { player_command(p, d, cmd); ctx->dirty = 1; }
    }
    lp_rect area = body;
    lp_rect controls = lp_rect_cut_bottom(&area, CONTROLS_H);
    paint_content(ctx, p, area);
    paint_controls(ctx, p, state ? d : NULL, controls, base);
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

static void player_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    const struct player *p = state;
    if (!p) return;
    int ready = p->media && !p->err;
    if (menu == LP_MENU_FILE) {
        entry(m, lp_media_state(p->media) == LP_MEDIA_PLAYING ? "Pause" : "Play", "Space", LP_PLAYER_PLAY_PAUSE, !ready);
        entry(m, "Show in Finder", NULL, LP_PLAYER_SHOW_IN_FINDER, 0);
    } else if (menu == LP_MENU_GO) {
        entry(m, "Previous", "⌘←", LP_PLAYER_PREVIOUS, !ready);
        entry(m, "Next", "⌘→", LP_PLAYER_NEXT, !ready || p->queue_at + 1 >= p->queue_count);
        entry(m, "Back 5 Seconds", "←", LP_PLAYER_SKIP_BACK, !ready);
        entry(m, "Forward 5 Seconds", "→", LP_PLAYER_SKIP_FORWARD, !ready);
    }
}

static void *player_create(lp_desktop *d, const char *window_id) {
    struct player *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    snprintf(p->window_id, sizeof p->window_id, "%s", window_id);
    p->desk = d;
    p->volume = 80;
    p->queue_at = -1;
    return p;
}

static void player_open(void *state, lp_desktop *d, const char *path) {
    if (state && path && *path) load(state, path, 0);   /* the window's title comes with the open; tags may rename it */
}

static void player_destroy(void *state) {
    struct player *p = state;
    if (!p) return;
    if (p->ticker) lp_desktop_remove_source(p->desk, p->ticker);
    unload(p);
    lp_media_queue_free(p->queue, p->queue_count);
    free(p);
}

const lp_app lp_app_player = {
    .id = "media", .title = "Media Player", .name = "Media Player", .icon = LP_ICON_PLAY, .object = "appMusic", .hidden = 1, .dock = 1,
    .default_rect = { NAN, NAN, 640, 440 }, .min_size = { 420, 300 }, .singleton = 0, .resizable = 1,
    .create = player_create, .paint = player_paint, .destroy = player_destroy,
    .open = player_open, .command = player_command, .menu_entries = player_menu_entries,
};

/* MARK: - Tests */

const char *lp_player_path(const void *state) { return ((const struct player *)state)->path; }
int lp_player_state(const void *state) {
    const struct player *p = state;
    return p->err ? p->err : p->media ? (int)lp_media_state(p->media) : -ENOENT;
}
int lp_player_queue_position(const void *state, int *count) {
    const struct player *p = state;
    if (count) *count = p->queue_count;
    return p->queue_at;
}
