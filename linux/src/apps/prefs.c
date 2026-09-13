/* System Settings — the desktop's preferences in one window: General (accent,
 * folders, wallpaper, motion, the clock), Dock (what a blank Spotlight shows),
 * Displays, Keyboard & Mouse, Sound, Network (links and Wi-Fi), Date & Time,
 * Users and About. The desktop's own settings are lp_settings, saved and
 * applied at once — the compositor re-reads them for every keyboard and
 * pointer. The system's are asked of its services through their own commands,
 * run as lp_jobs and read by lp_sysinfo: wpctl, ip, iwctl, timedatectl,
 * hostnamectl; polkit decides, and MaryOS grants those actions to the sudo
 * group. One job runs at a time; a pane that wanted fresh numbers while one
 * ran gets them when it ends. Linux only (PARITY D15). */
#define _DARWIN_C_SOURCE 1
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_job.h"
#include "maryui/lp_proc.h"
#include "maryui/lp_sysinfo.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define LABEL_W 170
#define ROW_H 30
#define NETWORK_REFRESH_MS 5000

enum job_kind {
    JOB_NONE, JOB_LINKS, JOB_WIFI_SCAN, JOB_WIFI_LIST, JOB_WIFI_CONNECT, JOB_WIFI_DISCONNECT,
    JOB_VOLUME_GET, JOB_VOLUME_SET, JOB_MUTE_SET, JOB_TIME_GET, JOB_TIME_SET_ZONE, JOB_TIME_SET_NTP, JOB_HOSTNAME_SET,
};

typedef lp_job *(*job_runner)(lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user);

struct prefs {
    char window_id[12];
    lp_desktop *desk;
    int pane;
    job_runner run;
    lp_job *job;
    enum job_kind job_kind;
    int refresh_pending;            /* the pane wanted a reading while a job ran */
    int queued_volume;              /* a volume the slider asked for while a job ran; -1 none */
    lp_source *ticker;
    char message[256];
    /* Sound */
    float volume;
    int muted, sound_known;
    /* Network */
    lp_net_link links[16];
    int nlinks;
    lp_wifi_network networks[32];
    int nnetworks, wifi_selected;
    char joining[64];
    lp_text_buffer passphrase;
    /* Date & Time */
    char timezone[64];
    int ntp, time_known;
    lp_text_buffer zone_field;
    /* Keyboard & Mouse */
    lp_text_buffer layout_field;
    /* About */
    lp_about about;
    lp_text_buffer hostname_field;
};

static const struct { const char *name; lp_icon icon; const char *section; } PANES[] = {
    { "General", LP_ICON_GEAR, "Personal" },
    { "Dock", LP_ICON_GRID, NULL },
    { "Displays", LP_ICON_DESKTOP, "Hardware" },
    { "Keyboard & Mouse", LP_ICON_INPUT_SOURCE, NULL },
    { "Sound", LP_ICON_VOLUME, NULL },
    { "Network", LP_ICON_WIFI, "System" },
    { "Date & Time", LP_ICON_CLOCK, NULL },
    { "Users", LP_ICON_USER, NULL },
    { "About", LP_ICON_INFO, NULL },
};
#define PANE_COUNT ((int)(sizeof PANES / sizeof PANES[0]))

/* MARK: - Jobs */

static void job_done(int status, const char *output, void *user);
static void refresh_pane(struct prefs *p);

static void dirty(struct prefs *p) {
    if (p->desk && p->desk->on_app_dirty) p->desk->on_app_dirty(p->desk, p->window_id);
}

/* Starts a job unless one runs; returns whether it started. */
static int start(struct prefs *p, enum job_kind kind, const char *const *argv) {
    if (p->job) return 0;
    p->job_kind = kind;
    p->job = (lp_job *)1;   /* busy from here: without an event loop the job ends inside run */
    lp_job *job = p->run(p->desk, argv, job_done, p);
    if (p->job) p->job = job;
    return 1;
}

static void reason_of(const char *output, char *out, size_t n) {
    char line[256];
    snprintf(line, sizeof line, "%s", output ? output : "");
    line[strcspn(line, "\n")] = 0;
    snprintf(out, n, "%s", line[0] ? line : "it did not say why");
}

static const lp_net_link *wireless_link(const struct prefs *p) {
    for (int i = 0; i < p->nlinks; i++) if (p->links[i].wireless) return &p->links[i];
    return NULL;
}

static void job_done(int status, const char *output, void *user) {
    struct prefs *p = user;
    enum job_kind kind = p->job_kind;
    p->job = NULL;
    p->job_kind = JOB_NONE;
    char reason[256];
    reason_of(output, reason, sizeof reason);
    const lp_net_link *wifi;
    switch (kind) {
    case JOB_LINKS:
        p->nlinks = status == 0 ? lp_sysinfo_parse_links(output, p->links, 16) : 0;
        if (status) snprintf(p->message, sizeof p->message, "Could not list the network links: %s", reason);
        if ((wifi = wireless_link(p))) {
            const char *const argv[] = { "iwctl", "station", wifi->name, "scan", NULL };
            start(p, JOB_WIFI_SCAN, argv);
        }
        break;
    case JOB_WIFI_SCAN:
        if ((wifi = wireless_link(p))) {
            const char *const argv[] = { "iwctl", "station", wifi->name, "get-networks", NULL };
            start(p, JOB_WIFI_LIST, argv);
        }
        break;
    case JOB_WIFI_LIST:
        p->nnetworks = status == 0 ? lp_sysinfo_parse_wifi(output, p->networks, 32) : 0;
        if (p->wifi_selected >= p->nnetworks) p->wifi_selected = -1;
        break;
    case JOB_WIFI_CONNECT:
        if (status) snprintf(p->message, sizeof p->message, "Could not join “%s”: %s", p->joining, reason);
        else snprintf(p->message, sizeof p->message, "Joined “%s”.", p->joining);
        lp_text_buffer_set(&p->passphrase, "");
        p->refresh_pending = 1;
        break;
    case JOB_WIFI_DISCONNECT:
        p->refresh_pending = 1;
        break;
    case JOB_VOLUME_GET: {
        double volume;
        int muted;
        p->sound_known = status == 0 && lp_sysinfo_parse_volume(output, &volume, &muted);
        if (p->sound_known) { p->volume = (float)fmin(100, round(volume * 100)); p->muted = muted; }
        else snprintf(p->message, sizeof p->message, "PipeWire is not answering, so sound cannot be changed here.");
        break;
    }
    case JOB_VOLUME_SET:
    case JOB_MUTE_SET:
        if (status) snprintf(p->message, sizeof p->message, "Could not change the sound: %s", reason);
        break;
    case JOB_TIME_GET:
        p->time_known = status == 0 && lp_sysinfo_value(output, "Timezone", p->timezone, sizeof p->timezone);
        if (p->time_known) {
            char ntp[16];
            lp_sysinfo_value(output, "NTP", ntp, sizeof ntp);
            p->ntp = strcmp(ntp, "yes") == 0;
            lp_text_buffer_set(&p->zone_field, p->timezone);
        } else {
            snprintf(p->message, sizeof p->message, "Could not read the date and time settings: %s", reason);
        }
        break;
    case JOB_TIME_SET_ZONE:
        if (status) snprintf(p->message, sizeof p->message, "Could not change the time zone: %s", reason);
        else { tzset(); snprintf(p->message, sizeof p->message, "The time zone is now %s.", p->zone_field.text); }
        p->refresh_pending = 1;
        break;
    case JOB_TIME_SET_NTP:
        if (status) snprintf(p->message, sizeof p->message, "Could not change how the time is set: %s", reason);
        p->refresh_pending = 1;
        break;
    case JOB_HOSTNAME_SET:
        if (status) snprintf(p->message, sizeof p->message, "Could not rename this computer: %s", reason);
        else { lp_sysinfo_about(&p->about); snprintf(p->message, sizeof p->message, "This computer is now called %s.", p->hostname_field.text); }
        break;
    case JOB_NONE:
        break;
    }
    if (kind == JOB_VOLUME_SET || kind == JOB_MUTE_SET || kind == JOB_VOLUME_GET) {
        if (p->queued_volume >= 0 && !p->job) {
            char level[16];
            snprintf(level, sizeof level, "%.2f", p->queued_volume / 100.0);
            p->queued_volume = -1;
            const char *const argv[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", level, NULL };
            start(p, JOB_VOLUME_SET, argv);
        }
    }
    if (p->refresh_pending && !p->job) { p->refresh_pending = 0; refresh_pane(p); }
    dirty(p);
}

static int on_tick(int fd, uint32_t mask, void *data) {
    struct prefs *p = data;
    int w = p->desk ? lp_wm_find(&p->desk->wm, p->window_id) : -1;
    if (p->pane != LP_PREFS_NETWORK || w < 0) return 0;   /* not re-armed: nothing to watch */
    if (p->desk->wm.windows[w].state != LP_WIN_SHADED) refresh_pane(p);
    lp_desktop_update_timer(p->desk, p->ticker, NETWORK_REFRESH_MS);
    return 0;
}

/* Asks the pane's service for its numbers again; queued when a job runs. */
static void refresh_pane(struct prefs *p) {
    int started = 1;
    switch (p->pane) {
    case LP_PREFS_SOUND: { const char *const argv[] = { "wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@", NULL }; started = start(p, JOB_VOLUME_GET, argv); break; }
    case LP_PREFS_NETWORK: { const char *const argv[] = { "ip", "-brief", "address", NULL }; started = start(p, JOB_LINKS, argv); break; }
    case LP_PREFS_TIME: { const char *const argv[] = { "timedatectl", "show", NULL }; started = start(p, JOB_TIME_GET, argv); break; }
    case LP_PREFS_ABOUT: lp_sysinfo_about(&p->about); break;
    default: break;
    }
    if (!started) p->refresh_pending = 1;
}

static void show_pane(struct prefs *p, int pane) {
    if (pane < 0 || pane >= PANE_COUNT) return;
    p->pane = pane;
    p->message[0] = 0;
    if (pane == LP_PREFS_ABOUT) { lp_sysinfo_about(&p->about); lp_text_buffer_set(&p->hostname_field, p->about.hostname); }
    if (pane == LP_PREFS_KEYBOARD && p->desk) lp_text_buffer_set(&p->layout_field, p->desk->settings.keyboard_layout);
    refresh_pane(p);
    if (pane == LP_PREFS_NETWORK && p->desk) {
        if (!p->ticker) p->ticker = lp_desktop_add_timer(p->desk, NETWORK_REFRESH_MS, on_tick, p);
        else lp_desktop_update_timer(p->desk, p->ticker, NETWORK_REFRESH_MS);
    }
}

/* MARK: - Actions */

static int valid_zone(const char *zone) {
    if (!zone[0] || strstr(zone, "..") || zone[0] == '/') return 0;
    for (const char *c = zone; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("/_+-", *c)) return 0;
    char path[256];
    snprintf(path, sizeof path, "/usr/share/zoneinfo/%s", zone);
    return access(path, R_OK) == 0;
}

static void apply_zone(struct prefs *p) {
    if (!valid_zone(p->zone_field.text)) {
        snprintf(p->message, sizeof p->message, "“%s” is not a time zone. Try one like Europe/London or America/New_York.", p->zone_field.text);
        return;
    }
    const char *const argv[] = { "timedatectl", "set-timezone", p->zone_field.text, NULL };
    if (!start(p, JOB_TIME_SET_ZONE, argv)) snprintf(p->message, sizeof p->message, "Still busy; try again in a moment.");
}

static void set_ntp(struct prefs *p, int on) {
    const char *const argv[] = { "timedatectl", "set-ntp", on ? "true" : "false", NULL };
    if (start(p, JOB_TIME_SET_NTP, argv)) p->ntp = on;
}

static int valid_hostname(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n > 63 || name[0] == '-' || name[n - 1] == '-') return 0;
    for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)name[i]) && name[i] != '-') return 0;
    return 1;
}

static void apply_hostname(struct prefs *p) {
    if (!valid_hostname(p->hostname_field.text)) {
        snprintf(p->message, sizeof p->message, "A computer's name is letters, digits and hyphens, not starting or ending with one.");
        return;
    }
    const char *const argv[] = { "hostnamectl", "set-hostname", p->hostname_field.text, NULL };
    if (!start(p, JOB_HOSTNAME_SET, argv)) snprintf(p->message, sizeof p->message, "Still busy; try again in a moment.");
}

static void set_volume(struct prefs *p, int volume) {
    p->volume = (float)volume;
    char level[16];
    snprintf(level, sizeof level, "%.2f", volume / 100.0);
    const char *const argv[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", level, NULL };
    if (!start(p, JOB_VOLUME_SET, argv)) p->queued_volume = volume;   /* the newest wins once the job ends */
}

static void set_mute(struct prefs *p, int muted) {
    const char *const argv[] = { "wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", muted ? "1" : "0", NULL };
    if (start(p, JOB_MUTE_SET, argv)) p->muted = muted;
}

static void join(struct prefs *p, int network, const char *passphrase) {
    const lp_net_link *wifi = wireless_link(p);
    if (!wifi || network < 0 || network >= p->nnetworks) return;
    const lp_wifi_network *w = &p->networks[network];
    if (strcmp(w->security, "open") != 0 && (!passphrase || !*passphrase)) {
        snprintf(p->message, sizeof p->message, "“%s” needs its password.", w->ssid);
        return;
    }
    snprintf(p->joining, sizeof p->joining, "%s", w->ssid);
    if (strcmp(w->security, "open") == 0) {
        const char *const argv[] = { "iwctl", "station", wifi->name, "connect", w->ssid, NULL };
        start(p, JOB_WIFI_CONNECT, argv);
    } else {
        /* iwctl takes the passphrase as an argument: on a machine one person uses, that is where it goes */
        const char *const argv[] = { "iwctl", "--passphrase", passphrase, "station", wifi->name, "connect", w->ssid, NULL };
        start(p, JOB_WIFI_CONNECT, argv);
    }
}

static void prefs_command(void *state, lp_desktop *d, int cmd) {
    struct prefs *p = state;
    if (p) show_pane(p, cmd);
}

/* MARK: - Painting helpers */

static void heading(lp_ctx *ctx, const char *text, float x, float *y, float w) {
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XL;
        st.weight = LP_TEXT_WEIGHT_BOLD;
        st.emboss = 1;
        lp_text_draw(ctx->cr, text, LP_RECT(x, *y, w, 28), &st, LP_ALIGN_START);
    }
    *y += 28 + LP_SPACE_4;
}

static void label(lp_ctx *ctx, const char *text, float x, float y) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.color = LP_INK_SECONDARY;
    lp_text_draw(ctx->cr, text, LP_RECT(x, y, LABEL_W - LP_SPACE_3, ROW_H), &st, LP_ALIGN_END);
}

static void value_text(lp_ctx *ctx, const char *text, float x, float y, float w) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.ellipsize = 1;
    lp_text_draw(ctx->cr, text, LP_RECT(x, y, w, ROW_H), &st, LP_ALIGN_START);
}

static void note(lp_ctx *ctx, const char *text, float x, float *y, float w) {
    if (!text[0]) return;
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS;
        st.color = LP_INK_TERTIARY;
        lp_text_layout *l = lp_text_layout_new(ctx->cr, text, (int)strlen(text), &st, w);
        if (l) { lp_text_layout_draw(ctx->cr, l, x, *y + 6, LP_INK_TERTIARY); lp_text_layout_free(l); }
    }
    *y += ROW_H;
}

static int segmented_row(lp_ctx *ctx, lp_id id, const char *name, float x, float *y, const lp_segment *options, int n, int *index) {
    label(ctx, name, x, *y);
    lp_size s = lp_segmented_measure(ctx, options, n, LP_CONTROL_SM);
    int changed = lp_segmented(ctx, id, x + LABEL_W, *y + (ROW_H - s.h) / 2, options, n, index, LP_CONTROL_SM);
    *y += ROW_H + LP_SPACE_1;
    return changed;
}

static int toggle_row(lp_ctx *ctx, lp_id id, const char *name, float x, float *y, int *on) {
    label(ctx, name, x, *y);
    int changed = lp_toggle(ctx, id, x + LABEL_W, *y + (ROW_H - 20) / 2, on, 0);
    *y += ROW_H + LP_SPACE_1;
    return changed;
}

static int slider_row(lp_ctx *ctx, lp_id id, const char *name, float x, float *y, float w, float *value, lp_slider_opts opts) {
    label(ctx, name, x, *y);
    int changed = lp_slider(ctx, id, LP_RECT(x + LABEL_W, *y + (ROW_H - LP_SLIDER_H) / 2, fmin(320, w - LABEL_W), LP_SLIDER_H), value, opts);
    *y += ROW_H + LP_SPACE_1;
    return changed;
}

static void run(lp_desktop *d, enum lp_command command, int arg) { if (d) lp_desktop_run_command(d, command, arg); }

/* MARK: - Panes */

static void pane_general(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    lp_settings s = d ? d->settings : lp_settings_defaults();
    heading(ctx, "General", x, &y, w);
    static const lp_segment ACCENTS[2] = { { "Blue", LP_ICON_COUNT }, { "Graphite", LP_ICON_COUNT } };
    int accent = s.accent;
    if (segmented_row(ctx, lp_id_index(base, 1), "Accent", x, &y, ACCENTS, 2, &accent)) run(d, LP_CMD_SET_ACCENT, accent);
    static const lp_segment FOLDERS[2] = { { "Slate", LP_ICON_COUNT }, { "Manila", LP_ICON_COUNT } };
    int folders = s.folders == LP_FOLDER_MANILA;
    if (segmented_row(ctx, lp_id_index(base, 2), "Folders", x, &y, FOLDERS, 2, &folders)) run(d, LP_CMD_SET_FOLDERS, folders ? LP_FOLDER_MANILA : LP_FOLDER_SLATE);
    static const lp_segment WALLPAPERS[2] = { { "Molten", LP_ICON_COUNT }, { "Procedural", LP_ICON_COUNT } };
    int wallpaper = s.wallpaper == LP_WALLPAPER_PROCEDURAL;
    if (segmented_row(ctx, lp_id_index(base, 3), "Wallpaper", x, &y, WALLPAPERS, 2, &wallpaper)) run(d, LP_CMD_SET_WALLPAPER, wallpaper ? LP_WALLPAPER_PROCEDURAL : LP_WALLPAPER_MOLTEN);
    static const lp_segment TONES[2] = { { "Platinum", LP_ICON_COUNT }, { "Faithful", LP_ICON_COUNT } };
    int tone = s.molten_tone == LP_MOLTEN_FAITHFUL;
    if (segmented_row(ctx, lp_id_index(base, 4), "Molten tone", x, &y, TONES, 2, &tone)) run(d, LP_CMD_SET_MOLTEN_TONE, tone ? LP_MOLTEN_FAITHFUL : LP_MOLTEN_PLATINUM);
    y += LP_SPACE_2;
    int reduced = s.reduced_motion, goo = s.goo, clock = s.clock, hours24 = s.clock_24h;
    if (toggle_row(ctx, lp_id_index(base, 5), "Reduce motion", x, &y, &reduced)) run(d, LP_CMD_TOGGLE_REDUCED_MOTION, 0);
    if (toggle_row(ctx, lp_id_index(base, 6), "Liquid merge", x, &y, &goo)) run(d, LP_CMD_TOGGLE_GOO, 0);
    if (toggle_row(ctx, lp_id_index(base, 7), "Show the clock", x, &y, &clock)) run(d, LP_CMD_TOGGLE_CLOCK, 0);
    if (toggle_row(ctx, lp_id_index(base, 8), "24-hour time", x, &y, &hours24) && d) { d->settings.clock_24h = hours24; lp_desktop_settings_changed(d); }
}

static void pane_dock(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Dock", x, &y, w);
    note(ctx, "A blank Spotlight shows these apps. Every app is still one search away.", x, &y, w);
    if (!d) return;
    for (int i = 0; i < d->app_count; i++) {
        const lp_app *app = d->apps[i];
        if (app->internal) continue;
        int on = lp_desktop_in_dock(d, app);
        if (lp_checkbox(ctx, lp_id_index(base, 20 + i), x + LP_SPACE_2, y + (ROW_H - 16) / 2, &on, app->name ? app->name : app->title, 0)) {
            lp_desktop_set_in_dock(d, app->id, on);
            ctx->dirty = 1;
        }
        y += ROW_H - 4;
    }
}

static void pane_displays(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Displays", x, &y, w);
    lp_display displays[8];
    int n = d && d->displays ? d->displays(d, displays, 8) : 0;
    if (n == 0) { note(ctx, "The desktop reports no displays here.", x, &y, w); return; }
    for (int i = 0; i < n; i++) {
        char text[160];
        label(ctx, "Display", x, y);
        snprintf(text, sizeof text, "%s%s%s", displays[i].name, displays[i].description[0] ? " — " : "", displays[i].description);
        value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
        y += ROW_H;
        label(ctx, "Resolution", x, y);
        snprintf(text, sizeof text, "%d × %d at %.0f Hz", displays[i].width, displays[i].height, displays[i].refresh_hz);
        value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
        y += ROW_H;
        label(ctx, "Scale", x, y);
        snprintf(text, sizeof text, "%.0f%%", displays[i].scale * 100);
        value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
        y += ROW_H + LP_SPACE_3;
    }
    note(ctx, "Each display runs at its preferred mode. A virtual machine's window has one fixed size.", x, &y, w);
}

static void format_repeat(float value, char *out, size_t n) { snprintf(out, n, "%.0f a second", value); }
static void format_ms(float value, char *out, size_t n) { snprintf(out, n, "%.0f ms", value); }
static void format_speed(float value, char *out, size_t n) { snprintf(out, n, "%+.1f", value); }

static void pane_keyboard(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Keyboard & Mouse", x, &y, w);
    lp_settings s = d ? d->settings : lp_settings_defaults();
    float rate = (float)s.key_repeat_rate, delay = (float)s.key_repeat_delay, speed = s.pointer_speed;
    if (slider_row(ctx, lp_id_index(base, 40), "Key repeat", x, &y, w, &rate, (lp_slider_opts){ .min = 1, .max = 50, .step = 1, .show_value = 1, .format = format_repeat }) && d) {
        d->settings.key_repeat_rate = (int)lround(rate);
        lp_desktop_settings_changed(d);
    }
    if (slider_row(ctx, lp_id_index(base, 41), "Delay until repeat", x, &y, w, &delay, (lp_slider_opts){ .min = 150, .max = 1200, .step = 50, .show_value = 1, .format = format_ms }) && d) {
        d->settings.key_repeat_delay = (int)lround(delay);
        lp_desktop_settings_changed(d);
    }
    label(ctx, "Layout", x, y);
    lp_text_field(ctx, lp_id_index(base, 42), LP_RECT(x + LABEL_W, y + (ROW_H - LP_SIZE_CONTROL_HEIGHT) / 2, 140, LP_SIZE_CONTROL_HEIGHT), &p->layout_field,
                  (lp_text_field_opts){ .placeholder = "us", .icon = LP_ICON_COUNT });
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 };
    lp_size bs = lp_button_measure(ctx, "Use", bo);
    if (lp_button(ctx, lp_id_index(base, 43), LP_RECT(x + LABEL_W + 140 + LP_SPACE_2, y + (ROW_H - bs.h) / 2, bs.w, bs.h), "Use", bo) && d)
        lp_prefs_set_layout(p, p->layout_field.text);
    y += ROW_H + LP_SPACE_1;
    note(ctx, "An XKB layout: us, gb, de, fr, or a variant such as us(dvorak). Empty is the system's own.", x + LABEL_W, &y, w - LABEL_W);
    y += LP_SPACE_2;
    if (slider_row(ctx, lp_id_index(base, 44), "Pointer speed", x, &y, w, &speed, (lp_slider_opts){ .min = -1, .max = 1, .step = 0.1f, .show_value = 1, .format = format_speed }) && d) {
        d->settings.pointer_speed = speed;
        lp_desktop_settings_changed(d);
    }
    int natural = s.natural_scroll;
    if (toggle_row(ctx, lp_id_index(base, 45), "Natural scrolling", x, &y, &natural) && d) {
        d->settings.natural_scroll = natural;
        lp_desktop_settings_changed(d);
    }
}

static void pane_sound(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Sound", x, &y, w);
    if (!p->sound_known) { note(ctx, p->job ? "Asking PipeWire…" : p->message, x, &y, w); return; }
    float volume = p->volume;
    if (slider_row(ctx, lp_id_index(base, 50), "Output volume", x, &y, w, &volume, (lp_slider_opts){ .min = 0, .max = 100, .step = 1, .show_value = 1 }))
        set_volume(p, (int)lround(volume));
    int muted = p->muted;
    if (toggle_row(ctx, lp_id_index(base, 51), "Mute", x, &y, &muted)) set_mute(p, muted);
}

static void pane_network(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Network", x, &y, w);
    if (p->nlinks == 0) note(ctx, p->job ? "Looking…" : "No network links.", x, &y, w);
    for (int i = 0; i < p->nlinks; i++) {
        const lp_net_link *l = &p->links[i];
        char text[260];
        label(ctx, l->wireless ? "Wi-Fi" : "Ethernet", x, y);
        snprintf(text, sizeof text, "%s · %s%s%s", l->name, strcmp(l->state, "UP") == 0 ? "Connected" : "Not connected",
                 l->addresses[0] ? " · " : "", l->addresses);
        value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
        y += ROW_H;
    }
    const lp_net_link *wifi = wireless_link(p);
    y += LP_SPACE_3;
    if (!wifi) { note(ctx, "This computer has no Wi-Fi.", x, &y, w); return; }
    for (int i = 0; i < p->nnetworks; i++) {
        const lp_wifi_network *n = &p->networks[i];
        char bars[8] = "", right[48];
        for (int b = 0; b < 4; b++) strcat(bars, b < n->signal ? "▮" : "▯");
        snprintf(right, sizeof right, "%s  %s%s", bars, strcmp(n->security, "open") == 0 ? "open" : "secured", n->connected ? " · joined" : "");
        const char *const columns[1] = { right };
        if (lp_list_row(ctx, lp_id_index(base, 60 + i), LP_RECT(x, y, w, LP_LIST_ROW_H), LP_ICON_WIFI, n->ssid, columns, 1, p->wifi_selected == i, i % 2)) {
            p->wifi_selected = i;
            ctx->dirty = 1;
        }
        y += LP_LIST_ROW_H;
    }
    y += LP_SPACE_3;
    if (p->wifi_selected >= 0 && p->wifi_selected < p->nnetworks) {
        const lp_wifi_network *n = &p->networks[p->wifi_selected];
        lp_button_opts bo = { LP_BUTTON_PRIMARY, LP_CONTROL_SM, LP_ICON_COUNT, 0, p->job != NULL };
        if (n->connected) {
            bo.variant = LP_BUTTON_DEFAULT;
            lp_size bs = lp_button_measure(ctx, "Disconnect", bo);
            if (lp_button(ctx, lp_id_index(base, 100), LP_RECT(x, y, bs.w, bs.h), "Disconnect", bo)) {
                const char *const argv[] = { "iwctl", "station", wifi->name, "disconnect", NULL };
                start(p, JOB_WIFI_DISCONNECT, argv);
            }
        } else {
            float fx = x;
            if (strcmp(n->security, "open") != 0) {
                lp_text_field(ctx, lp_id_index(base, 101), LP_RECT(x, y, 220, LP_SIZE_CONTROL_HEIGHT), &p->passphrase, (lp_text_field_opts){ .placeholder = "Password", .icon = LP_ICON_LOCK });
                fx += 220 + LP_SPACE_2;
            }
            lp_size bs = lp_button_measure(ctx, "Join", bo);
            if (lp_button(ctx, lp_id_index(base, 102), LP_RECT(fx, y, bs.w, bs.h), "Join", bo) && !p->job) join(p, p->wifi_selected, p->passphrase.text);
        }
    }
}

static void pane_time(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Date & Time", x, &y, w);
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char text[96];
    strftime(text, sizeof text, "%A, %B %d, %Y  %H:%M", &tm);
    label(ctx, "Now", x, y);
    value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
    y += ROW_H + LP_SPACE_2;
    label(ctx, "Time zone", x, y);
    lp_text_field(ctx, lp_id_index(base, 70), LP_RECT(x + LABEL_W, y + (ROW_H - LP_SIZE_CONTROL_HEIGHT) / 2, 220, LP_SIZE_CONTROL_HEIGHT), &p->zone_field,
                  (lp_text_field_opts){ .placeholder = "Europe/London", .icon = LP_ICON_COUNT });
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, !p->time_known || p->job != NULL };
    lp_size bs = lp_button_measure(ctx, "Set", bo);
    if (lp_button(ctx, lp_id_index(base, 71), LP_RECT(x + LABEL_W + 220 + LP_SPACE_2, y + (ROW_H - bs.h) / 2, bs.w, bs.h), "Set", bo) && !bo.disabled) apply_zone(p);
    y += ROW_H + LP_SPACE_1;
    int ntp = p->ntp;
    if (p->time_known && toggle_row(ctx, lp_id_index(base, 72), "Set automatically", x, &y, &ntp)) set_ntp(p, ntp);
    if (d) {
        int hours24 = d->settings.clock_24h;
        if (toggle_row(ctx, lp_id_index(base, 73), "24-hour time", x, &y, &hours24)) { d->settings.clock_24h = hours24; lp_desktop_settings_changed(d); }
    }
}

static void pane_users(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Users", x, &y, w);
    struct passwd *pw = getpwuid(getuid());
    char full[128] = "", admin[8] = "No";
    if (pw && pw->pw_gecos) { snprintf(full, sizeof full, "%s", pw->pw_gecos); full[strcspn(full, ",")] = 0; }
    struct group *sudo = getgrnam("sudo");
    if (sudo && pw) for (char **m = sudo->gr_mem; *m; m++) if (strcmp(*m, pw->pw_name) == 0) snprintf(admin, sizeof admin, "Yes");
    label(ctx, "Name", x, y); value_text(ctx, full[0] ? full : (pw ? pw->pw_name : "?"), x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Account", x, y); value_text(ctx, pw ? pw->pw_name : "?", x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Home", x, y); value_text(ctx, pw ? pw->pw_dir : "?", x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Administrator", x, y); value_text(ctx, admin, x + LABEL_W, y, w - LABEL_W); y += ROW_H + LP_SPACE_3;
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_LOCK, 0, !(d && d->spawn) };
    lp_size bs = lp_button_measure(ctx, "Change Password…", bo);
    if (lp_button(ctx, lp_id_index(base, 80), LP_RECT(x + LABEL_W, y, bs.w, bs.h), "Change Password…", bo) && d && d->spawn)
        d->spawn(d, "foot --title='Change Password' passwd");   /* passwd wants a terminal of its own */
}

static void pane_about(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "About", x, &y, w);
    const lp_about *a = &p->about;
    char text[200], memory[32];
    label(ctx, "System", x, y); value_text(ctx, a->os, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    if (a->model[0]) { label(ctx, "Computer", x, y); value_text(ctx, a->model, x + LABEL_W, y, w - LABEL_W); y += ROW_H; }
    snprintf(text, sizeof text, "%s × %d", a->cpu, a->cores);
    label(ctx, "Processor", x, y); value_text(ctx, text, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    lp_proc_format_bytes(a->memory, memory, sizeof memory);
    label(ctx, "Memory", x, y); value_text(ctx, memory, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Kernel", x, y); value_text(ctx, a->kernel, x + LABEL_W, y, w - LABEL_W); y += ROW_H + LP_SPACE_3;
    label(ctx, "Computer name", x, y);
    lp_text_field(ctx, lp_id_index(base, 90), LP_RECT(x + LABEL_W, y + (ROW_H - LP_SIZE_CONTROL_HEIGHT) / 2, 200, LP_SIZE_CONTROL_HEIGHT), &p->hostname_field,
                  (lp_text_field_opts){ .placeholder = "maryos", .icon = LP_ICON_COUNT });
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, p->job != NULL };
    lp_size bs = lp_button_measure(ctx, "Rename", bo);
    if (lp_button(ctx, lp_id_index(base, 91), LP_RECT(x + LABEL_W + 200 + LP_SPACE_2, y + (ROW_H - bs.h) / 2, bs.w, bs.h), "Rename", bo) && !p->job) apply_hostname(p);
}

static void prefs_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct prefs empty = { .wifi_selected = -1, .queued_volume = -1 };
    struct prefs *p = state ? state : &empty;
    lp_desktop *desk = state ? d : NULL;
    lp_id base = LP_ID("prefs");
    lp_rect area = body;
    lp_rect cursor = lp_sidebar(ctx, &area);
    for (int i = 0; i < PANE_COUNT; i++) {
        if (PANES[i].section) { if (i) cursor.y += LP_SPACE_3; lp_sidebar_section(ctx, &cursor, PANES[i].section); }
        if (lp_sidebar_item(ctx, lp_id_index(base, 200 + i), &cursor, PANES[i].icon, PANES[i].name, p->pane == i) && state) {
            show_pane(p, i);
            ctx->dirty = 1;
        }
    }
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) lp_fill_solid(ctx->cr, area, LP_SURFACE_BODY, 0);
    lp_rect in = lp_rect_inset(area, LP_SPACE_6, LP_SPACE_5);
    float x = in.x, y = in.y, w = in.w;
    switch (p->pane) {
    case LP_PREFS_GENERAL: pane_general(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_DOCK: pane_dock(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_DISPLAYS: pane_displays(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_KEYBOARD: pane_keyboard(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_SOUND: pane_sound(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_NETWORK: pane_network(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_TIME: pane_time(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_USERS: pane_users(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_ABOUT: pane_about(ctx, p, desk, x, y, w, base); break;
    }
    if (ctx->pass == LP_PASS_DRAW && ctx->cr && p->message[0] && p->pane != LP_PREFS_SOUND) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_SM;
        st.color = LP_INK_SECONDARY;
        st.ellipsize = 1;
        lp_text_draw(ctx->cr, p->message, LP_RECT(in.x, in.y + in.h - 20, in.w, 20), &st, LP_ALIGN_START);
    }
}

static void prefs_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    const struct prefs *p = state;
    if (!p || menu != LP_MENU_VIEW) return;
    for (int i = 0; i < PANE_COUNT && m->count < LP_MENU_MAX_ENTRIES; i++) {
        lp_menu_entry *e = &m->entries[m->count++];
        memset(e, 0, sizeof *e);
        snprintf(e->label, sizeof e->label, "%s", PANES[i].name);
        e->command = LP_CMD_APP;
        e->arg = i;
        e->checked = p->pane == i;
    }
}

static void *prefs_create(lp_desktop *d, const char *window_id) {
    struct prefs *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    snprintf(p->window_id, sizeof p->window_id, "%s", window_id);
    p->desk = d;
    p->run = lp_job_run;
    p->wifi_selected = -1;
    p->queued_volume = -1;
    lp_sysinfo_about(&p->about);
    return p;
}

static void prefs_destroy(void *state) {
    struct prefs *p = state;
    if (!p) return;
    if (p->ticker) lp_desktop_remove_source(p->desk, p->ticker);
    if (p->job && p->run == lp_job_run) lp_job_cancel(p->job);
    free(p);
}

const lp_app lp_app_prefs = {
    .id = "settings", .title = "System Settings", .name = "System Settings", .icon = LP_ICON_GEAR, .dock = 1,
    .default_rect = { NAN, NAN, 800, 540 }, .min_size = { 660, 440 }, .singleton = 1, .resizable = 1,
    .create = prefs_create, .paint = prefs_paint, .destroy = prefs_destroy,
    .command = prefs_command, .menu_entries = prefs_menu_entries,
};

/* MARK: - Tests, and the pane's own actions */

void lp_prefs_set_layout(void *state, const char *layout) {
    struct prefs *p = state;
    if (!p->desk) return;
    for (const char *c = layout; *c; c++) {
        if (!isalnum((unsigned char)*c) && !strchr("(),_-", *c)) {
            snprintf(p->message, sizeof p->message, "“%s” is not a keyboard layout name.", layout);
            return;
        }
    }
    snprintf(p->desk->settings.keyboard_layout, sizeof p->desk->settings.keyboard_layout, "%s", layout);
    lp_text_buffer_set(&p->layout_field, layout);
    lp_desktop_settings_changed(p->desk);
    p->message[0] = 0;
}
void lp_prefs_set_runner(void *state, lp_job *(*run)(lp_desktop *d, const char *const *argv, lp_job_done_fn done, void *user)) { ((struct prefs *)state)->run = run; }
int lp_prefs_pane(const void *state) { return ((const struct prefs *)state)->pane; }
const char *lp_prefs_message(const void *state) { return ((const struct prefs *)state)->message; }
int lp_prefs_link_count(const void *state) { return ((const struct prefs *)state)->nlinks; }
int lp_prefs_network_count(const void *state) { return ((const struct prefs *)state)->nnetworks; }
int lp_prefs_volume(const void *state) { const struct prefs *p = state; return p->sound_known ? (int)p->volume : -1; }
const char *lp_prefs_timezone(const void *state) { return ((const struct prefs *)state)->timezone; }
void lp_prefs_set_zone(void *state, const char *zone) { struct prefs *p = state; lp_text_buffer_set(&p->zone_field, zone); apply_zone(p); }
void lp_prefs_set_hostname(void *state, const char *name) { struct prefs *p = state; lp_text_buffer_set(&p->hostname_field, name); apply_hostname(p); }
void lp_prefs_join(void *state, int network, const char *passphrase) { join(state, network, passphrase); }
void lp_prefs_set_volume(void *state, int volume) { set_volume(state, volume); }
