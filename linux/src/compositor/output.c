/* Outputs: one scene output per wlr_output, the preferred mode, and the
 * wallpaper layer. The frame handler is the motion engine's clock: it steps
 * the springs and tweens, commits the scene, and schedules another frame only
 * while something still moves — an idle desktop draws nothing. */
#include <stdlib.h>
#include <time.h>

#include "chrome.h"
#include "window.h"

/* platinum.6 (#8b909b) behind the wallpaper. */
static const float BACKGROUND[4] = { 0.545f, 0.565f, 0.608f, 1.0f };

static double frame_interval_ms(const struct mui_output *output) {
    int refresh = output->wlr_output->refresh; /* mHz; 0 when unknown */
    return refresh > 0 ? 1000.0 / (refresh / 1000.0) : 1000.0 / 60;
}

static int frame_timer_fire(void *data) {
    struct mui_output *output = data;
    wlr_output_schedule_frame(output->wlr_output);
    return 0;
}

/* Asks for another frame, no sooner than one refresh interval after the last.
 * A virtual GPU under pixman completes page flips at once and skips commits
 * that change nothing, so wlr_output_schedule_frame alone would spin the loop. */
static void request_frame_in(struct mui_output *output, double now_ms, double wait_ms) {
    output->frame_wanted = 1;
    output->frame_due_ms = now_ms + (wait_ms > 0 ? wait_ms : 0);
    if (wait_ms <= 0.5) wlr_output_schedule_frame(output->wlr_output);
    else if (output->frame_timer) wl_event_source_timer_update(output->frame_timer, (int)(wait_ms + 0.5));
}

static void request_frame(struct mui_output *output, double now_ms) {
    request_frame_in(output, now_ms, frame_interval_ms(output) - (now_ms - output->last_frame_ms));
}

void mui_server_schedule_frame(struct mui_server *server) {
    double now_ms = mui_now_ms();
    struct mui_output *output;
    wl_list_for_each(output, &server->outputs, link) request_frame(output, now_ms);
}

/* Repaints the whole output on the next frame: the safety net when a scaled or
 * cropped node goes back to normal, so no strip of the old rendering survives. */
void mui_server_damage_all(struct mui_server *server) {
    struct mui_output *output;
    wl_list_for_each(output, &server->outputs, link) wlr_damage_ring_add_whole(&output->scene_output->damage_ring);
    mui_server_schedule_frame(server);
}

void mui_engine_wake(void *user) {
    struct mui_server *server = user;
    if (server->debug_frames) wlr_log(WLR_INFO, "motion: wake");
    mui_server_schedule_frame(server);
}

static int ambient_pending(struct mui_server *server) {
    struct mui_window *win;
    wl_list_for_each(win, &server->windows, link) if (mui_chrome_wants_frame(&win->chrome)) return 1;
    return 0;
}

/* MARYUI_DEBUG=frames: a histogram of the frame handler's own time, every 5 s while frames run. */
static void record_frame_time(struct mui_server *server, double ms, double now_ms) {
    struct mui_frame_stats *s = &server->frame_stats;
    int bucket = ms < 2 ? 0 : ms < 4 ? 1 : ms < 8 ? 2 : ms < 16 ? 3 : 4;
    s->buckets[bucket]++;
    s->frames++;
    s->total_ms += ms;
    if (ms > s->max_ms) s->max_ms = ms;
    if (s->report_ms == 0) s->report_ms = now_ms;
    if (now_ms - s->report_ms >= 5000) {
        wlr_log(WLR_INFO, "frames: %u in %.1f s (%u needs_frame, %u damaged, %u skipped), avg %.2f ms (commit avg %.2f, max %.1f; %u strip repaints avg %.2f, max %.1f; tick avg %.2f, max %.1f; transform avg %.2f, max %.1f; pre/mid/post avg %.2f/%.2f/%.2f, anim avg %.2f; cpu avg %.2f), max %.1f ms; <2: %u  <4: %u  <8: %u  <16: %u  16+: %u",
            s->frames, (now_ms - s->report_ms) / 1000, s->needs_frame, s->damaged, s->skipped, s->frames ? s->total_ms / s->frames : 0,
            s->frames ? s->commit_ms / s->frames : 0, s->commit_max_ms, s->paints, s->paints ? s->paint_ms / s->paints : 0, s->paint_max_ms,
            s->frames ? s->tick_ms / s->frames : 0, s->tick_max_ms, s->frames ? s->transform_ms / s->frames : 0, s->transform_max_ms,
            s->frames ? s->pre_ms / s->frames : 0, s->frames ? s->mid_ms / s->frames : 0, s->frames ? s->post_ms / s->frames : 0, s->frames ? s->anim_ms / s->frames : 0, s->frames ? s->cpu_ms / s->frames : 0, s->max_ms,
            s->buckets[0], s->buckets[1], s->buckets[2], s->buckets[3], s->buckets[4]);
        s->needs_frame = s->damaged = s->skipped = 0;
        s->commit_ms = s->commit_max_ms = 0;
        s->paint_ms = s->paint_max_ms = 0;
        s->tick_ms = s->tick_max_ms = s->transform_ms = s->transform_max_ms = 0;
        s->pre_ms = s->mid_ms = s->post_ms = s->cpu_ms = s->anim_ms = 0;
        s->paints = 0;
        memset(s->buckets, 0, sizeof s->buckets);
        s->frames = 0;
        s->total_ms = s->max_ms = 0;
        s->report_ms = now_ms;
    }
}

static void output_frame(struct wl_listener *listener, void *data) {
    struct mui_output *output = wl_container_of(listener, output, frame);
    struct mui_server *server = output->server;
    double now_ms = mui_now_ms();
    struct timespec cpu0;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu0);
    /* A page flip on a virtual GPU completes at once: hold frames that come
     * sooner than the refresh interval, so motion steps at the display's pace. */
    double since = now_ms - output->last_frame_ms;
    double due_in = output->frame_wanted ? output->frame_due_ms - now_ms : 0;
    if (output->last_frame_ms > 0 && (since < frame_interval_ms(output) - 1.5 || due_in > 1.5)) {
        /* Early: a flip completed at once, or the requested frame (30 Hz ambient) is not due yet.
         * Re-arm only when we asked for a frame: wlr_output_schedule_frame forces a commit, and its
         * flip would otherwise loop us here forever. */
        if (output->frame_wanted && output->frame_timer) {
            double wait = frame_interval_ms(output) - since;
            if (due_in > wait) wait = due_in;
            wl_event_source_timer_update(output->frame_timer, (int)(wait + 0.5));
        }
        return;
    }
    /* Seconds since the previous frame, before last_frame_ms moves on. */
    float frame_dt = (float)(since / 1000.0);
    if (frame_dt < 0 || frame_dt > LP_MOTION_MAX_DT) frame_dt = LP_MOTION_MAX_DT;
    output->last_frame_ms = now_ms;
    int wanted = output->frame_wanted;
    output->frame_wanted = 0;
    int needs_frame = output->wlr_output->needs_frame, damaged = pixman_region32_not_empty(&output->scene_output->damage_ring.current);

    /* Springs (windows) and tweens (close, shade, menu-in). */
    double t_tick = mui_now_ms();
    if (server->debug_frames) server->frame_stats.pre_ms += t_tick - now_ms;
    int motion = lp_motion_engine_tick(&server->engine, now_ms);
    double t_tick_end = mui_now_ms();
    if (server->debug_frames) {
        double ms = t_tick_end - t_tick;
        server->frame_stats.tick_ms += ms;
        if (ms > server->frame_stats.tick_max_ms) server->frame_stats.tick_max_ms = ms;
    }
    if (server->debug_frames && server->frame_stats.motion_active && !motion)
        wlr_log(WLR_INFO, "motion: idle (%u wakes, %u idles, %u frames)", server->engine.wakes, server->engine.idles, server->engine.frames);
    server->frame_stats.motion_active = motion;
    int active = motion;
    /* The molten wallpaper's clock is window motion. It never returns "active",
     * so it can extend a frame that was already happening but never start one. */
    mui_desktop_molten_tick(server, now_ms, frame_dt, motion);
    double t_anim = mui_now_ms();
    active |= mui_windows_animate(server, now_ms);
    active |= mui_desktop_animate(server, now_ms);
    if (server->debug_frames) server->frame_stats.anim_ms += mui_now_ms() - t_anim;

    /* Ambient animations (bubbles rolling, progress glints) repaint at most 30 times a second. */
    static double last_ambient = 0;
    if (ambient_pending(server) && now_ms - last_ambient >= 1000.0 / 30) {
        last_ambient = now_ms;
        mui_desktop_ambient_tick(server);
    }

    /* A frame the backend asked for with nothing new to show (no damage, no motion, pointer still)
     * would commit the same picture again — and on a virtual GPU that commit costs a vblank. */
    int cursor_moved = server->cursor && (server->cursor->x != output->last_cursor_x || server->cursor->y != output->last_cursor_y);
    int skip_commit = needs_frame && !damaged && !wanted && !active && !cursor_moved;
    double t_commit = mui_now_ms();
    if (server->debug_frames) server->frame_stats.mid_ms += t_commit - t_tick_end;
    if (!skip_commit) {
        wlr_scene_output_commit(output->scene_output, NULL);
        if (server->cursor) { output->last_cursor_x = server->cursor->x; output->last_cursor_y = server->cursor->y; }
    }
    double commit_ms = mui_now_ms() - t_commit;
    if (server->debug_frames && needs_frame && !wanted && !damaged && now_ms - server->frame_stats.last_diag_ms > 1000) {
        server->frame_stats.last_diag_ms = now_ms;
        wlr_log(WLR_INFO, "unrequested frame: needs_frame=%d frame_pending=%d cursor_moved=%d hw_cursor=%d sw_cursor_locks=%d since_last=%.1f ms%s",
            needs_frame, output->wlr_output->frame_pending, cursor_moved, output->wlr_output->hardware_cursor != NULL,
            output->wlr_output->software_cursor_locks, since, skip_commit ? " (skipped)" : "");
    }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(output->scene_output, &now);
    if (server->debug_frames) {
        server->frame_stats.needs_frame += needs_frame > 0;
        server->frame_stats.damaged += damaged > 0;
        server->frame_stats.skipped += skip_commit;
        server->frame_stats.commit_ms += commit_ms;
        if (commit_ms > server->frame_stats.commit_max_ms) server->frame_stats.commit_max_ms = commit_ms;
        double t_end = mui_now_ms();
        server->frame_stats.post_ms += t_end - t_commit - commit_ms;
        struct timespec cpu1;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu1);
        server->frame_stats.cpu_ms += (cpu1.tv_sec - cpu0.tv_sec) * 1000.0 + (cpu1.tv_nsec - cpu0.tv_nsec) / 1e6;
        record_frame_time(server, t_end - now_ms, now_ms);
    }
    if (active) request_frame(output, now_ms);
    else if (ambient_pending(server)) request_frame_in(output, now_ms, 1000.0 / 30 - (now_ms - last_ambient)); /* ambient animations run at 30 Hz */
}

static void output_fit_background(struct mui_output *output) {
    int width, height;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    wlr_scene_rect_set_size(output->background, width, height);
    struct wlr_box box;
    wlr_output_layout_get_box(output->server->output_layout, output->wlr_output, &box);
    wlr_scene_node_set_position(&output->background->node, box.x, box.y);
}

static void output_request_state(struct wl_listener *listener, void *data) {
    struct mui_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
    output_fit_background(output);
    int width, height;
    wlr_output_effective_resolution(output->wlr_output, &width, &height);
    if (width != output->width || height != output->height) {
        output->width = width;
        output->height = height;
        mui_desktop_output_ready(output);
    }
}

static void output_destroy(struct wl_listener *listener, void *data) {
    struct mui_output *output = wl_container_of(listener, output, destroy);
    if (output->wallpaper_buffer) wlr_buffer_drop(&output->wallpaper_buffer->base);
    if (output->frame_timer) wl_event_source_remove(output->frame_timer);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);
}

void mui_output_handle_new(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode) wlr_output_state_set_mode(&state, mode);
    if (!wlr_output_commit_state(wlr_output, &state)) {
        wlr_log(WLR_ERROR, "output %s: could not enable", wlr_output->name);
    }
    wlr_output_state_finish(&state);

    struct mui_output *output = calloc(1, sizeof(*output));
    output->server = server;
    output->wlr_output = wlr_output;
    output->frame_timer = wl_event_loop_add_timer(wl_display_get_event_loop(server->display), frame_timer_fire, output);
    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);
    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);
    wl_list_insert(&server->outputs, &output->link);

    struct wlr_output_layout_output *layout_output = wlr_output_layout_add_auto(server->output_layout, wlr_output);
    output->scene_output = wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_layout, layout_output, output->scene_output);

    output->background = wlr_scene_rect_create(server->layer_wallpaper, 1, 1, BACKGROUND);
    output_fit_background(output);

    int width, height;
    wlr_output_effective_resolution(wlr_output, &width, &height);
    output->width = width;
    output->height = height;
    wlr_log(WLR_INFO, "output %s: %dx%d%s", wlr_output->name, width, height,
        mode ? "" : " (no preferred mode)");
    mui_desktop_output_ready(output);
}
