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
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_layout_components.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_job.h"
#include "maryui/lp_popup.h"
#include "maryui/lp_proc.h"
#include "maryui/lp_sysinfo.h"
#include "maryui/lp_text.h"
#include "maryui/lp_thread.h"
#include "maryui/lp_tokens.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

#define LABEL_W LP_PREFS_LABEL_W
#define FOOTER_H 28
#define ROW_H 30
#define NETWORK_REFRESH_MS 5000

enum job_kind {
    JOB_NONE, JOB_LINKS, JOB_WIFI_SCAN, JOB_WIFI_LIST, JOB_WIFI_CONNECT, JOB_WIFI_DISCONNECT,
    JOB_VOLUME_GET, JOB_VOLUME_SET, JOB_MUTE_SET, JOB_INPUT_VOLUME_GET, JOB_INPUT_VOLUME_SET, JOB_SET_DEFAULT, JOB_TIME_GET, JOB_TIME_SET_ZONE, JOB_TIME_SET_NTP, JOB_HOSTNAME_SET,
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
    int queued_input_volume;        /* the same for the microphone */
    lp_scroll_state scroll;         /* a pane taller than the window scrolls */
    float content_h;                /* the pane's height as the last pass laid it out */
    lp_source *ticker;
    char message[256];
    /* Sound: the default output's and input's volumes (wpctl), and lp_audio's devices */
    float volume, input_volume;
    int muted, sound_known, input_known;
    int metering;                   /* holds a lease on lp_audio's meter while Sound shows */
    char switching[128];            /* the device a set-default is choosing */
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
    /* Mary: the key only until Save hands it to maryd, then zeroed */
    lp_text_buffer mary_key;
    char mary_status[160];
    char voice_menu[LP_MENU_MAX_ENTRIES][64];   /* the voice each Voice menu entry chooses, as the menu was opened */
    char mood_menu[16][64];                     /* and each Mood entry */
    lp_rect voice_popup, mood_popup;            /* where the menus open, as the last pass laid them out */
    lp_prefs_trace_fn trace;        /* tests: where each part of a pane is laid out */
    void *trace_user;
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
    { "Mary", LP_ICON_STAR, "Assistant" },
};
#define PANE_COUNT ((int)(sizeof PANES / sizeof PANES[0]))

/* MARK: - Jobs */

static void job_done(int status, const char *output, void *user);
static void refresh_pane(struct prefs *p);

static void dirty(struct prefs *p) {
    if (p->desk && p->desk->on_app_dirty) p->desk->on_app_dirty(p->desk, p->window_id);
}

/* A lease on lp_audio's meter while Sound shows: its capture stream runs only then. */
static void meter(struct prefs *p, int on) {
    if (!p->desk || p->metering == on) return;
    p->metering = on;
    lp_audio_meter(&p->desk->audio, on);
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
        if (p->sound_known) {
            p->volume = (float)fmin(100, round(volume * 100));
            p->muted = muted;
            const char *const argv[] = { "wpctl", "get-volume", "@DEFAULT_AUDIO_SOURCE@", NULL };
            start(p, JOB_INPUT_VOLUME_GET, argv);    /* and then the microphone's */
        }
        break;
    }
    case JOB_INPUT_VOLUME_GET: {
        double volume;
        int muted;
        p->input_known = status == 0 && lp_sysinfo_parse_volume(output, &volume, &muted);
        if (p->input_known) p->input_volume = (float)fmin(100, round(volume * 100));
        break;
    }
    case JOB_VOLUME_SET:
    case JOB_MUTE_SET:
        if (status) snprintf(p->message, sizeof p->message, "Could not change the sound: %s", reason);
        break;
    case JOB_INPUT_VOLUME_SET:
        if (status) snprintf(p->message, sizeof p->message, "Could not change the microphone's volume: %s", reason);
        break;
    case JOB_SET_DEFAULT:
        if (status) snprintf(p->message, sizeof p->message, "Could not switch to “%s”: %s", p->switching, reason);
        else p->refresh_pending = 1;                /* the new default has volumes of its own */
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
    /* A volume asked for while a job ran is sent once nothing runs; only the newest of each. */
    if (p->queued_volume >= 0 && !p->job) {
        char level[16];
        snprintf(level, sizeof level, "%.2f", p->queued_volume / 100.0);
        p->queued_volume = -1;
        const char *const argv[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", level, NULL };
        start(p, JOB_VOLUME_SET, argv);
    }
    if (p->queued_input_volume >= 0 && !p->job) {
        char level[16];
        snprintf(level, sizeof level, "%.2f", p->queued_input_volume / 100.0);
        p->queued_input_volume = -1;
        const char *const argv[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SOURCE@", level, NULL };
        start(p, JOB_INPUT_VOLUME_SET, argv);
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

/* Mistral's voices, once each time the Mary pane shows, when maryd can ask for them. */
static void ask_voices(struct prefs *p) {
    lp_mary *m = p->desk ? &p->desk->mary : NULL;
    if (m && lp_mary_connected(m) && m->key_present && (m->voices_state == LP_MARY_VOICES_UNKNOWN || m->voices_state == LP_MARY_VOICES_FAILED))
        lp_mary_list_voices(m);
}

/* The Mary pane's Network activity and Memory sections: sewnd's ledger through maryd, the Thread's counts. */
static void ask_activity(struct prefs *p) {
    if (!p->desk) return;
    if (lp_mary_connected(&p->desk->mary)) lp_mary_list_calls(&p->desk->mary, 12);
    if (lp_thread_connected(&p->desk->thread)) lp_thread_stats(&p->desk->thread);
}

static void show_pane(struct prefs *p, int pane) {
    if (pane < 0 || pane >= PANE_COUNT) return;
    p->pane = pane;
    p->message[0] = 0;
    p->scroll.y = 0;
    if (pane == LP_PREFS_ABOUT) { lp_sysinfo_about(&p->about); lp_text_buffer_set(&p->hostname_field, p->about.hostname); }
    if (pane == LP_PREFS_KEYBOARD && p->desk) lp_text_buffer_set(&p->layout_field, p->desk->settings.keyboard_layout);
    meter(p, pane == LP_PREFS_SOUND);
    if (pane == LP_PREFS_MARY) { ask_voices(p); ask_activity(p); }
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

static void set_input_volume(struct prefs *p, int volume) {
    p->input_volume = (float)volume;
    char level[16];
    snprintf(level, sizeof level, "%.2f", volume / 100.0);
    const char *const argv[] = { "wpctl", "set-volume", "@DEFAULT_AUDIO_SOURCE@", level, NULL };
    if (!start(p, JOB_INPUT_VOLUME_SET, argv)) p->queued_input_volume = volume;
}

/* Makes a device the default. WirePlumber remembers it, and maryd, whose streams follow the defaults, listens and
 * speaks through it. Choosing the default again runs nothing. */
static void choose_device(struct prefs *p, enum lp_audio_direction dir, int index) {
    const lp_audio *a = p->desk ? &p->desk->audio : NULL;
    if (!a || index < 0 || index >= a->count[dir] || lp_audio_is_default(a, dir, &a->devices[dir][index])) return;
    const lp_audio_device *device = &a->devices[dir][index];
    char id[16];
    snprintf(id, sizeof id, "%u", (unsigned)device->id);
    snprintf(p->switching, sizeof p->switching, "%s", device->description);
    const char *const argv[] = { "wpctl", "set-default", id, NULL };
    if (!start(p, JOB_SET_DEFAULT, argv)) snprintf(p->message, sizeof p->message, "Still busy; try again in a moment.");
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

static int voice_command(struct prefs *p, lp_desktop *d, int cmd);

/* A pane by its number (the View menu, open_pane), or a choice from the Voice and Mood menus. */
static void prefs_command(void *state, lp_desktop *d, int cmd) {
    struct prefs *p = state;
    if (p && !voice_command(p, d, cmd)) show_pane(p, cmd);
}

/* MARK: - Painting helpers */

/* Every pane is two flush-left columns: labels, and each group's title, start at the heading's edge, and the
 * controls line up LABEL_W in. What a pane lays out is reported to lp_prefs_trace as it goes. */
static struct prefs *painting;          /* the pane being laid out */
static int group_open;                  /* a group has begun since the heading */

static void trace_part(enum lp_prefs_part part, lp_rect r, const char *text) {
    if (painting && painting->trace) painting->trace(part, r, text, painting->trace_user);
}

static void heading(lp_ctx *ctx, const char *text, float x, float *y, float w) {
    lp_rect r = LP_RECT(x, *y, w, 28);
    trace_part(LP_PREFS_PART_HEADING, r, text);
    group_open = 0;
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XL;
        st.weight = LP_TEXT_WEIGHT_BOLD;
        st.emboss = 1;
        lp_text_draw(ctx->cr, text, r, &st, LP_ALIGN_START);
    }
    *y += 28 + LP_SPACE_4;
}

/* A group's title, set as the sidebar sets its own (lp_sidebar_section): small, semibold, upper case. */
static void section(lp_ctx *ctx, const char *title, float x, float *y, float w) {
    if (group_open) *y += LP_SPACE_4;
    group_open = 1;
    lp_rect r = LP_RECT(x, *y, w, 16);
    trace_part(LP_PREFS_PART_SECTION, r, title);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_XS;
        st.weight = LP_TEXT_WEIGHT_SEMIBOLD;
        st.letter_spacing = LP_TEXT_XS * 0.04f;
        st.uppercase = 1;
        st.color = LP_INK_TERTIARY;
        lp_text_draw(ctx->cr, title, r, &st, LP_ALIGN_START);
    }
    *y += 16 + LP_SPACE_1;
}

/* A smaller group inside a section: one app among Mary's skills. */
static void subheading(lp_ctx *ctx, const char *text, float x, float *y, float w) {
    *y += LP_SPACE_2;
    lp_rect r = LP_RECT(x, *y, w, 20);
    trace_part(LP_PREFS_PART_SECTION, r, text);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_text_style st = lp_text_style_default();
        st.weight = LP_TEXT_WEIGHT_BOLD;
        st.emboss = 1;
        lp_text_draw(ctx->cr, text, r, &st, LP_ALIGN_START);
    }
    *y += 20 + LP_SPACE_1;
}

static void label(lp_ctx *ctx, const char *text, float x, float y) {
    lp_rect r = LP_RECT(x, y, LABEL_W - LP_SPACE_3, ROW_H);
    trace_part(LP_PREFS_PART_LABEL, r, text);
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.color = LP_INK_SECONDARY;
    st.ellipsize = 1;
    lp_text_draw(ctx->cr, text, r, &st, LP_ALIGN_START);
}

/* A row's first control, in the second column. */
static lp_rect control_at(lp_rect r) {
    trace_part(LP_PREFS_PART_CONTROL, r, NULL);
    return r;
}

static void value_text(lp_ctx *ctx, const char *text, float x, float y, float w) {
    lp_rect r = LP_RECT(x, y, w, ROW_H);
    trace_part(LP_PREFS_PART_CONTROL, r, text);
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.ellipsize = 1;
    lp_text_draw(ctx->cr, text, r, &st, LP_ALIGN_START);
}

/* Quiet words after a row's control: what a skill does, what a switch listens for. */
static void aside(lp_ctx *ctx, const char *text, float x, float y, float w) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr || w <= 0) return;
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS;
    st.color = LP_INK_TERTIARY;
    st.ellipsize = 1;
    lp_text_draw(ctx->cr, text, LP_RECT(x, y, w, ROW_H), &st, LP_ALIGN_START);
}

static void note(lp_ctx *ctx, const char *text, float x, float *y, float w) {
    if (!text[0]) return;
    trace_part(LP_PREFS_PART_NOTE, LP_RECT(x, *y, w, ROW_H), text);
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
    lp_rect r = control_at(LP_RECT(x + LABEL_W, *y + (ROW_H - s.h) / 2, s.w, s.h));
    int changed = lp_segmented(ctx, id, r.x, r.y, options, n, index, LP_CONTROL_SM);
    *y += ROW_H + LP_SPACE_1;
    return changed;
}

static int toggle_row(lp_ctx *ctx, lp_id id, const char *name, float x, float *y, int *on) {
    label(ctx, name, x, *y);
    lp_rect r = control_at(LP_RECT(x + LABEL_W, *y + (ROW_H - 20) / 2, LP_TOGGLE_W, LP_TOGGLE_H));
    int changed = lp_toggle(ctx, id, r.x, r.y, on, 0);
    *y += ROW_H + LP_SPACE_1;
    return changed;
}

static int slider_row(lp_ctx *ctx, lp_id id, const char *name, float x, float *y, float w, float *value, lp_slider_opts opts) {
    label(ctx, name, x, *y);
    lp_rect r = control_at(LP_RECT(x + LABEL_W, *y + (ROW_H - LP_SLIDER_H) / 2, fmin(320, w - LABEL_W), LP_SLIDER_H));
    int changed = lp_slider(ctx, id, r, value, opts);
    *y += ROW_H + LP_SPACE_1;
    return changed;
}

static void run(lp_desktop *d, enum lp_command command, int arg) { if (d) lp_desktop_run_command(d, command, arg); }

/* MARK: - Panes */

static float pane_general(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    lp_settings s = d ? d->settings : lp_settings_defaults();
    heading(ctx, "General", x, &y, w);
    section(ctx, "Appearance", x, &y, w);
    static const lp_segment ACCENTS[2] = { { "Blue", LP_ICON_COUNT }, { "Graphite", LP_ICON_COUNT } };
    int accent = s.accent;
    if (segmented_row(ctx, lp_id_index(base, 1), "Accent", x, &y, ACCENTS, 2, &accent)) run(d, LP_CMD_SET_ACCENT, accent);
    static const lp_segment FOLDERS[2] = { { "Slate", LP_ICON_COUNT }, { "Manila", LP_ICON_COUNT } };
    int folders = s.folders == LP_FOLDER_MANILA;
    if (segmented_row(ctx, lp_id_index(base, 2), "Folders", x, &y, FOLDERS, 2, &folders)) run(d, LP_CMD_SET_FOLDERS, folders ? LP_FOLDER_MANILA : LP_FOLDER_SLATE);
    static const lp_segment WALLPAPERS[3] = { { "Lava", LP_ICON_COUNT }, { "Molten", LP_ICON_COUNT }, { "Procedural", LP_ICON_COUNT } };
    static const enum lp_wallpaper_mode WALLPAPER_MODES[3] = { LP_WALLPAPER_LAVA, LP_WALLPAPER_MOLTEN, LP_WALLPAPER_PROCEDURAL };
    int wallpaper = s.wallpaper == LP_WALLPAPER_MOLTEN ? 1 : s.wallpaper == LP_WALLPAPER_LAVA ? 0 : 2;
    if (segmented_row(ctx, lp_id_index(base, 3), "Wallpaper", x, &y, WALLPAPERS, 3, &wallpaper)) run(d, LP_CMD_SET_WALLPAPER, WALLPAPER_MODES[wallpaper]);
    static const lp_segment TONES[2] = { { "Platinum", LP_ICON_COUNT }, { "Faithful", LP_ICON_COUNT } };
    int tone = s.molten_tone == LP_MOLTEN_FAITHFUL;
    if (segmented_row(ctx, lp_id_index(base, 4), "Molten tone", x, &y, TONES, 2, &tone)) run(d, LP_CMD_SET_MOLTEN_TONE, tone ? LP_MOLTEN_FAITHFUL : LP_MOLTEN_PLATINUM);
    int reduced = s.reduced_motion, goo = s.goo, clock = s.clock, hours24 = s.clock_24h;
    section(ctx, "Motion", x, &y, w);
    if (toggle_row(ctx, lp_id_index(base, 5), "Reduce motion", x, &y, &reduced)) run(d, LP_CMD_TOGGLE_REDUCED_MOTION, 0);
    if (toggle_row(ctx, lp_id_index(base, 6), "Liquid merge", x, &y, &goo)) run(d, LP_CMD_TOGGLE_GOO, 0);
    section(ctx, "Clock", x, &y, w);
    if (toggle_row(ctx, lp_id_index(base, 7), "Show the clock", x, &y, &clock)) run(d, LP_CMD_TOGGLE_CLOCK, 0);
    if (toggle_row(ctx, lp_id_index(base, 8), "24-hour time", x, &y, &hours24) && d) { d->settings.clock_24h = hours24; lp_desktop_settings_changed(d); }
    return y;
}

static float pane_dock(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Dock", x, &y, w);
    section(ctx, "Apps in the Dock", x, &y, w);
    note(ctx, "A blank Spotlight shows these apps. Every app is still one search away.", x, &y, w);
    if (!d) return y;
    label(ctx, "Show", x, y);
    for (int i = 0; i < d->app_count; i++) {
        const lp_app *app = d->apps[i];
        if (app->internal) continue;
        const char *name = app->name ? app->name : app->title;
        int on = lp_desktop_in_dock(d, app);
        lp_rect r = control_at(LP_RECT(x + LABEL_W, y + (ROW_H - 16) / 2, lp_checkbox_measure(ctx, name).w, 16));
        if (lp_checkbox(ctx, lp_id_index(base, 20 + i), r.x, r.y, &on, name, 0)) {
            lp_desktop_set_in_dock(d, app->id, on);
            ctx->dirty = 1;
        }
        y += ROW_H - 4;
    }
    return y;
}

static float pane_displays(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Displays", x, &y, w);
    lp_display displays[8];
    int n = d && d->displays ? d->displays(d, displays, 8) : 0;
    if (n == 0) { note(ctx, "The desktop reports no displays here.", x, &y, w); return y; }
    for (int i = 0; i < n; i++) {
        char text[160];
        section(ctx, displays[i].name, x, &y, w);
        if (displays[i].description[0]) {
            label(ctx, "Model", x, y);
            value_text(ctx, displays[i].description, x + LABEL_W, y, w - LABEL_W);
            y += ROW_H;
        }
        label(ctx, "Resolution", x, y);
        snprintf(text, sizeof text, "%d × %d at %.0f Hz", displays[i].width, displays[i].height, displays[i].refresh_hz);
        value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
        y += ROW_H;
        label(ctx, "Scale", x, y);
        snprintf(text, sizeof text, "%.0f%%", displays[i].scale * 100);
        value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
        y += ROW_H;
    }
    y += LP_SPACE_2;
    note(ctx, "Each display runs at its preferred mode. A virtual machine's window has one fixed size.", x, &y, w);
    return y;
}

static void format_repeat(float value, char *out, size_t n) { snprintf(out, n, "%.0f a second", value); }
static void format_ms(float value, char *out, size_t n) { snprintf(out, n, "%.0f ms", value); }
static void format_speed(float value, char *out, size_t n) { snprintf(out, n, "%+.1f", value); }

static float pane_keyboard(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Keyboard & Mouse", x, &y, w);
    lp_settings s = d ? d->settings : lp_settings_defaults();
    float rate = (float)s.key_repeat_rate, delay = (float)s.key_repeat_delay, speed = s.pointer_speed;
    section(ctx, "Keyboard", x, &y, w);
    if (slider_row(ctx, lp_id_index(base, 40), "Key repeat", x, &y, w, &rate, (lp_slider_opts){ .min = 1, .max = 50, .step = 1, .show_value = 1, .format = format_repeat }) && d) {
        d->settings.key_repeat_rate = (int)lround(rate);
        lp_desktop_settings_changed(d);
    }
    if (slider_row(ctx, lp_id_index(base, 41), "Delay until repeat", x, &y, w, &delay, (lp_slider_opts){ .min = 150, .max = 1200, .step = 50, .show_value = 1, .format = format_ms }) && d) {
        d->settings.key_repeat_delay = (int)lround(delay);
        lp_desktop_settings_changed(d);
    }
    label(ctx, "Layout", x, y);
    lp_text_field(ctx, lp_id_index(base, 42), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - LP_SIZE_CONTROL_HEIGHT) / 2, 140, LP_SIZE_CONTROL_HEIGHT)), &p->layout_field,
                  (lp_text_field_opts){ .placeholder = "us", .icon = LP_ICON_COUNT });
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 };
    lp_size bs = lp_button_measure(ctx, "Use", bo);
    if (lp_button(ctx, lp_id_index(base, 43), LP_RECT(x + LABEL_W + 140 + LP_SPACE_2, y + (ROW_H - bs.h) / 2, bs.w, bs.h), "Use", bo) && d)
        lp_prefs_set_layout(p, p->layout_field.text);
    y += ROW_H;
    note(ctx, "An XKB layout: us, gb, de, fr, or a variant such as us(dvorak). Empty is the system's own.", x + LABEL_W, &y, w - LABEL_W);
    section(ctx, "Pointer", x, &y, w);
    if (slider_row(ctx, lp_id_index(base, 44), "Pointer speed", x, &y, w, &speed, (lp_slider_opts){ .min = -1, .max = 1, .step = 0.1f, .show_value = 1, .format = format_speed }) && d) {
        d->settings.pointer_speed = speed;
        lp_desktop_settings_changed(d);
    }
    int natural = s.natural_scroll;
    if (toggle_row(ctx, lp_id_index(base, 45), "Natural scrolling", x, &y, &natural) && d) {
        d->settings.natural_scroll = natural;
        lp_desktop_settings_changed(d);
    }
    return y;
}

/* One direction's devices as a table in the second column, the default selected; choosing one makes it the default. */
static float device_table(lp_ctx *ctx, struct prefs *p, const lp_audio *a, enum lp_audio_direction dir, float x, float y, float w, lp_id base) {
    float tx = x + LABEL_W, tw = w - LABEL_W;
    label(ctx, "Device", x, y - (ROW_H - LP_LIST_ROW_H) / 2);
    trace_part(LP_PREFS_PART_TABLE, LP_RECT(tx, y, tw, LP_LIST_ROW_H * a->count[dir]), NULL);
    for (int i = 0; i < a->count[dir]; i++) {
        const lp_audio_device *device = &a->devices[dir][i];
        const char *const columns[1] = { device->kind };
        if (lp_list_row(ctx, lp_id_index(base, (dir == LP_AUDIO_OUTPUT ? 120 : 150) + i), LP_RECT(tx, y, tw, LP_LIST_ROW_H), LP_ICON_COUNT,
                        device->description, columns, 1, lp_audio_is_default(a, dir, device), i % 2)) {
            choose_device(p, dir, i);
            ctx->dirty = 1;
        }
        y += LP_LIST_ROW_H;
    }
    return y + LP_SPACE_2;
}

/* The microphone's level as a row of cells lit in the accent colour; while it is live it asks for frames. */
static void level_meter(lp_ctx *ctx, lp_rect r, float level, int live) {
    control_at(r);
    if (ctx->pass == LP_PASS_EVENT) {
        if (live) lp_want_frame_rect(ctx, r);
        return;
    }
    if (!ctx->cr) return;
    enum { CELLS = 16 };
    const float gap = 3;
    float cell = (r.w - gap * (CELLS - 1)) / CELLS;
    int lit = (int)lround(level * CELLS);
    lp_accent accent = lp_settings_accent(ctx->settings);
    for (int i = 0; i < CELLS; i++)
        lp_fill_solid(ctx->cr, LP_RECT(r.x + i * (cell + gap), r.y, cell, r.h), i < lit ? accent.base : LP_PLATINUM_3, LP_RADIUS_XS / 2);
}

static float pane_sound(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Sound", x, &y, w);
    const lp_audio *a = d ? &d->audio : NULL;
    int listed = a && a->connected;
    lp_slider_opts percent = { .min = 0, .max = 100, .step = 1, .show_value = 1 };
    section(ctx, "Output", x, &y, w);
    if (a && !listed) note(ctx, lp_audio_available() ? "Looking for PipeWire’s speakers and microphones…" : "This build of the desktop cannot list devices.", x, &y, w);
    if (listed && a->count[LP_AUDIO_OUTPUT]) y = device_table(ctx, p, a, LP_AUDIO_OUTPUT, x, y, w, base);
    else if (listed) note(ctx, "No speakers or headphones.", x, &y, w);
    if (p->sound_known) {
        float volume = p->volume;
        if (slider_row(ctx, lp_id_index(base, 50), "Volume", x, &y, w, &volume, percent)) set_volume(p, (int)lround(volume));
        int muted = p->muted;
        if (toggle_row(ctx, lp_id_index(base, 51), "Mute", x, &y, &muted)) set_mute(p, muted);
    } else {
        note(ctx, p->job ? "Asking PipeWire…" : "PipeWire is not answering, so sound cannot be changed here.", x, &y, w);
    }

    section(ctx, "Input", x, &y, w);
    if (listed && !a->count[LP_AUDIO_INPUT]) {
        note(ctx, "No microphone. In the VM, start it with maryos vm run --microphone (ui.sh does),\nand allow maryos under the Mac’s Privacy & Security › Microphone.", x, &y, w);
        return y + 14;                              /* the note's second line */
    }
    if (listed) y = device_table(ctx, p, a, LP_AUDIO_INPUT, x, y, w, base);
    if (p->input_known) {
        float volume = p->input_volume;
        if (slider_row(ctx, lp_id_index(base, 52), "Volume", x, &y, w, &volume, percent)) set_input_volume(p, (int)lround(volume));
    }
    if (listed && lp_audio_default(a, LP_AUDIO_INPUT)) {
        label(ctx, "Level", x, y);
        level_meter(ctx, LP_RECT(x + LABEL_W, y + (ROW_H - 10) / 2, fmin(320, w - LABEL_W), 10), lp_audio_level(a), p->metering);
        y += ROW_H + LP_SPACE_1;
    }
    return y;
}

static float pane_network(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Network", x, &y, w);
    section(ctx, "Connections", x, &y, w);
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
    section(ctx, "Wi-Fi", x, &y, w);
    if (!wifi) { note(ctx, "This computer has no Wi-Fi.", x, &y, w); return y; }
    float tx = x + LABEL_W, tw = w - LABEL_W;
    if (p->nnetworks == 0) {
        label(ctx, "Networks", x, y);
        value_text(ctx, p->job ? "Looking…" : "None in range.", tx, y, tw);
        return y + ROW_H;
    }
    label(ctx, "Networks", x, y - (ROW_H - LP_LIST_ROW_H) / 2);
    trace_part(LP_PREFS_PART_TABLE, LP_RECT(tx, y, tw, LP_LIST_ROW_H * p->nnetworks), NULL);
    for (int i = 0; i < p->nnetworks; i++) {
        const lp_wifi_network *n = &p->networks[i];
        /* In words: the UI font has no block glyphs to draw bars with. */
        static const char *const SIGNAL[] = { "Weak", "Weak", "Fair", "Good", "Excellent" };
        char right[48];
        snprintf(right, sizeof right, "%s · %s", n->connected ? "Joined" : strcmp(n->security, "open") == 0 ? "Open" : "Secured",
                 SIGNAL[n->signal < 0 ? 0 : n->signal > 4 ? 4 : n->signal]);
        const char *const columns[1] = { right };
        if (lp_list_row(ctx, lp_id_index(base, 60 + i), LP_RECT(tx, y, tw, LP_LIST_ROW_H), LP_ICON_WIFI, n->ssid, columns, 1, p->wifi_selected == i, i % 2)) {
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
            if (lp_button(ctx, lp_id_index(base, 100), control_at(LP_RECT(tx, y, bs.w, bs.h)), "Disconnect", bo)) {
                const char *const argv[] = { "iwctl", "station", wifi->name, "disconnect", NULL };
                start(p, JOB_WIFI_DISCONNECT, argv);
            }
        } else {
            float fx = tx;
            if (strcmp(n->security, "open") != 0) {
                lp_text_field(ctx, lp_id_index(base, 101), control_at(LP_RECT(tx, y, 220, LP_SIZE_CONTROL_HEIGHT)), &p->passphrase, (lp_text_field_opts){ .placeholder = "Password", .icon = LP_ICON_LOCK });
                fx += 220 + LP_SPACE_2;
            }
            lp_size bs = lp_button_measure(ctx, "Join", bo);
            lp_rect join_r = LP_RECT(fx, y, bs.w, bs.h);
            if (fx == tx) control_at(join_r);
            if (lp_button(ctx, lp_id_index(base, 102), join_r, "Join", bo) && !p->job) join(p, p->wifi_selected, p->passphrase.text);
        }
        y += LP_SIZE_CONTROL_HEIGHT;
    }
    return y;
}

static float pane_time(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Date & Time", x, &y, w);
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char text[96];
    strftime(text, sizeof text, "%A, %B %d, %Y  %H:%M", &tm);
    section(ctx, "Time", x, &y, w);
    label(ctx, "Now", x, y);
    value_text(ctx, text, x + LABEL_W, y, w - LABEL_W);
    y += ROW_H;
    int ntp = p->ntp;
    if (p->time_known && toggle_row(ctx, lp_id_index(base, 72), "Set automatically", x, &y, &ntp)) set_ntp(p, ntp);
    section(ctx, "Time Zone", x, &y, w);
    label(ctx, "Time zone", x, y);
    lp_text_field(ctx, lp_id_index(base, 70), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - LP_SIZE_CONTROL_HEIGHT) / 2, 220, LP_SIZE_CONTROL_HEIGHT)), &p->zone_field,
                  (lp_text_field_opts){ .placeholder = "Europe/London", .icon = LP_ICON_COUNT });
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, !p->time_known || p->job != NULL };
    lp_size bs = lp_button_measure(ctx, "Set", bo);
    if (lp_button(ctx, lp_id_index(base, 71), LP_RECT(x + LABEL_W + 220 + LP_SPACE_2, y + (ROW_H - bs.h) / 2, bs.w, bs.h), "Set", bo) && !bo.disabled) apply_zone(p);
    y += ROW_H + LP_SPACE_1;
    if (d) {
        section(ctx, "Clock", x, &y, w);
        int hours24 = d->settings.clock_24h;
        if (toggle_row(ctx, lp_id_index(base, 73), "24-hour time", x, &y, &hours24)) { d->settings.clock_24h = hours24; lp_desktop_settings_changed(d); }
    }
    return y;
}

static float pane_users(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Users", x, &y, w);
    struct passwd *pw = getpwuid(getuid());
    char full[128] = "", admin[8] = "No";
    if (pw && pw->pw_gecos) { snprintf(full, sizeof full, "%s", pw->pw_gecos); full[strcspn(full, ",")] = 0; }
    struct group *sudo = getgrnam("sudo");
    if (sudo && pw) for (char **m = sudo->gr_mem; *m; m++) if (strcmp(*m, pw->pw_name) == 0) snprintf(admin, sizeof admin, "Yes");
    section(ctx, "Account", x, &y, w);
    label(ctx, "Name", x, y); value_text(ctx, full[0] ? full : (pw ? pw->pw_name : "?"), x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Account", x, y); value_text(ctx, pw ? pw->pw_name : "?", x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Home", x, y); value_text(ctx, pw ? pw->pw_dir : "?", x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Administrator", x, y); value_text(ctx, admin, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_LOCK, 0, !(d && d->spawn) };
    lp_size bs = lp_button_measure(ctx, "Change Password…", bo);
    label(ctx, "Password", x, y);
    if (lp_button(ctx, lp_id_index(base, 80), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - bs.h) / 2, bs.w, bs.h)), "Change Password…", bo) && d && d->spawn)
        d->spawn(d, "foot --title='Change Password' passwd");   /* passwd wants a terminal of its own */
    return y + ROW_H;
}

static float pane_about(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "About", x, &y, w);
    const lp_about *a = &p->about;
    char text[200], memory[32];
    section(ctx, "This Computer", x, &y, w);
    label(ctx, "System", x, y); value_text(ctx, a->os, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    if (a->model[0]) { label(ctx, "Computer", x, y); value_text(ctx, a->model, x + LABEL_W, y, w - LABEL_W); y += ROW_H; }
    snprintf(text, sizeof text, "%s × %d", a->cpu, a->cores);
    label(ctx, "Processor", x, y); value_text(ctx, text, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    lp_proc_format_bytes(a->memory, memory, sizeof memory);
    label(ctx, "Memory", x, y); value_text(ctx, memory, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    label(ctx, "Kernel", x, y); value_text(ctx, a->kernel, x + LABEL_W, y, w - LABEL_W); y += ROW_H;
    section(ctx, "Name", x, &y, w);
    label(ctx, "Computer name", x, y);
    lp_text_field(ctx, lp_id_index(base, 90), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - LP_SIZE_CONTROL_HEIGHT) / 2, 200, LP_SIZE_CONTROL_HEIGHT)), &p->hostname_field,
                  (lp_text_field_opts){ .placeholder = "maryos", .icon = LP_ICON_COUNT });
    lp_button_opts bo = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, p->job != NULL };
    lp_size bs = lp_button_measure(ctx, "Rename", bo);
    if (lp_button(ctx, lp_id_index(base, 91), LP_RECT(x + LABEL_W + 200 + LP_SPACE_2, y + (ROW_H - bs.h) / 2, bs.w, bs.h), "Rename", bo) && !p->job) apply_hostname(p);
    return y + ROW_H;
}

/* MARK: - Mary (PARITY D19, D20) */

static void wipe(char *bytes, size_t n) {
    volatile char *b = bytes;
    while (n--) *b++ = 0;
}

/* Hands the key to maryd, which hands it to sewnd, and forgets it here whatever happened. */
static void save_mary_key(struct prefs *p) {
    int rc = p->desk ? lp_mary_set_key(&p->desk->mary, p->mary_key.text, (size_t)p->mary_key.len) : -ENOTCONN;
    wipe(p->mary_key.text, sizeof p->mary_key.text);
    p->mary_key.len = p->mary_key.cursor = 0;
    p->mary_key.all_selected = 0;
    snprintf(p->mary_status, sizeof p->mary_status, "%s",
             rc == 0 ? "Saved. Verify asks Mistral whether it works."
             : rc == -EINVAL ? "That doesn’t look like a Mistral API key."
             : rc == -ENOTCONN ? "Mary isn’t running, so the key was not saved."
                               : "The key could not be handed to Mary.");
}

static void mary_key_status(const struct prefs *p, const lp_mary *m, char *out, size_t n) {
    char when[40] = "";
    if (m && m->key_verified_at > 0) {
        time_t t = (time_t)(m->key_verified_at / 1000);
        struct tm tm;
        strftime(when, sizeof when, " Verified %B %d, %Y.", localtime_r(&t, &tm));
    }
    if (!m || !lp_mary_connected(m)) snprintf(out, n, "Mary isn’t running. The key can be saved once she is back.");
    else if (m->key_check == 1) snprintf(out, n, "Mistral accepted the key.%s", when);
    else if (m->key_check == 0) snprintf(out, n, "%s", m->key_message[0] ? m->key_message : "Mistral refused the key.");
    else if (p->mary_status[0]) snprintf(out, n, "%s", p->mary_status);
    else if (m->key_present) snprintf(out, n, "A key is stored.%s", when);
    else snprintf(out, n, "No key yet: Mary needs one to talk to Mistral. It is kept by sewnd, never here.");
}

/* MARK: Mary's voice */

/* Mistral names its preset voices <language>_<name>_<mood>, one voice per mood: fr_marie_neutral, en_paul_cheerful.
 * The Voice menu offers each character once (Marie, Jane, Oliver, Paul) and the Mood menu the moods that character
 * has, each spoken by its own id. A list whose ids do not say the mood is grouped by name ("Oliver (Confident)").
 * Marie always has Mary's own six moods, which Sewn speaks as fr_marie_<mood>. */
static const char *const MARIE_MOODS[] = { "neutral", "sad", "happy", "excited", "curious", "angry" };

#define MOODS_MAX 16

typedef struct voice_moods {
    int count;
    char mood[MOODS_MAX][32];
    char id[MOODS_MAX][64];         /* what each mood is spoken by */
} voice_moods;

static void lowercase(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* Where a name such as "Paul - Cheerful" or "Oliver (confident)" gives way to its mood; NULL when it has none. */
static const char *mood_separator(const char *name, size_t *skip) {
    static const char *const SEPARATORS[] = { " - ", " \xE2\x80\x93 ", " \xE2\x80\x94 ", " (", ": ", ", " };
    const char *first = NULL;
    for (size_t i = 0; i < sizeof SEPARATORS / sizeof *SEPARATORS; i++) {
        const char *at = strstr(name, SEPARATORS[i]);
        if (at && (!first || at < first)) {
            first = at;
            *skip = strlen(SEPARATORS[i]);
        }
    }
    return first;
}

/* A listed voice as its character and mood: by its id when that spells both; else by its name, keyed with its
 * language; else a character of its own with no mood. */
static void voice_parts(const lp_mary_voice *v, char *character, size_t cn, char *mood, size_t mn) {
    if (lp_mary_voice_split(v->id, character, cn, mood, mn)) return;
    size_t skip = 0;
    const char *cut = mood_separator(v->name, &skip);
    if (!cut) {
        snprintf(character, cn, "%s", v->id);
        if (mn) mood[0] = 0;
        return;
    }
    snprintf(character, cn, "%s:%.*s", v->language, (int)(cut - v->name), v->name);
    lowercase(character);
    snprintf(mood, mn, "%s", cut + skip);
    size_t len = strlen(mood);
    while (len && (mood[len - 1] == ')' || mood[len - 1] == ' ')) mood[--len] = 0;
    lowercase(mood);
}

/* The character and mood of the voice Mary speaks in. */
static void voice_of(const lp_mary *m, const char *voice_id, char *character, size_t cn, char *mood, size_t mn) {
    for (int i = 0; m && i < m->voice_count; i++) {
        if (strcmp(m->voices[i].id, voice_id) != 0) continue;
        voice_parts(&m->voices[i], character, cn, mood, mn);
        return;
    }
    lp_mary_voice_split(voice_id, character, cn, mood, mn);
}

static void add_mood(voice_moods *out, const char *mood, const char *id) {
    for (int i = 0; i < out->count; i++) if (strcmp(out->mood[i], mood) == 0) return;
    if (out->count == MOODS_MAX) return;
    snprintf(out->mood[out->count], sizeof out->mood[0], "%s", mood);
    if (snprintf(out->id[out->count], sizeof out->id[0], "%s", id) >= (int)sizeof out->id[0]) return;
    out->count++;
}

/* A character's moods, neutral first, then Marie's own, then the rest of the list's. */
static void character_moods(const lp_mary *m, const char *character, voice_moods *out) {
    out->count = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; m && i < m->voice_count; i++) {
            char c[96], mood[32];
            voice_parts(&m->voices[i], c, sizeof c, mood, sizeof mood);
            if (strcmp(c, character) != 0 || !mood[0] || (pass == 0) != (strcmp(mood, "neutral") == 0)) continue;
            add_mood(out, mood, m->voices[i].id);
        }
        for (size_t k = 0; pass == 0 && strcmp(character, "fr_marie") == 0 && k < sizeof MARIE_MOODS / sizeof *MARIE_MOODS; k++) {
            char id[64];
            snprintf(id, sizeof id, "fr_marie_%s", MARIE_MOODS[k]);
            add_mood(out, MARIE_MOODS[k], id);
        }
    }
}

/* The id a character is spoken by in the mood asked for, else neutral, else its first mood; a voice with no moods
 * by its own id. */
static void compose_voice(const lp_mary *m, const char *character, const char *want, char *out, size_t n) {
    voice_moods moods;
    character_moods(m, character, &moods);
    int pick = -1;
    for (int i = 0; i < moods.count && pick < 0; i++) if (strcmp(moods.mood[i], want) == 0) pick = i;
    for (int i = 0; i < moods.count && pick < 0; i++) if (strcmp(moods.mood[i], "neutral") == 0) pick = i;
    if (pick < 0 && moods.count) pick = 0;
    if (pick >= 0) {
        snprintf(out, n, "%s", moods.id[pick]);
        return;
    }
    for (int i = 0; m && i < m->voice_count; i++) {
        char c[96], mood[32];
        voice_parts(&m->voices[i], c, sizeof c, mood, sizeof mood);
        if (strcmp(c, character) == 0) {
            snprintf(out, n, "%s", m->voices[i].id);
            return;
        }
    }
    if (snprintf(out, n, "%s", character) >= (int)n) out[0] = 0;   /* too long to be a voice id */
}

/* The language a character speaks: a listed voice's, else its id's prefix (fr_marie: fr; gb_jane: en). */
static void character_language(const lp_mary *m, const char *character, char *out, size_t n) {
    for (int i = 0; m && i < m->voice_count; i++) {
        char c[96], mood[32];
        voice_parts(&m->voices[i], c, sizeof c, mood, sizeof mood);
        if (strcmp(c, character) == 0 && m->voices[i].language[0]) {
            snprintf(out, n, "%s", m->voices[i].language);
            return;
        }
    }
    const char *cut = strchr(character, '_');
    if (cut && cut - character == 2) snprintf(out, n, "%.2s", strncmp(character, "gb", 2) == 0 ? "en" : character);
    else snprintf(out, n, "%s", "");
}

static const char *language_name(const char *code) {
    static const struct { const char *code, *name; } NAMES[] = {
        { "fr", "French" }, { "en", "English" }, { "es", "Spanish" }, { "de", "German" }, { "it", "Italian" },
        { "pt", "Portuguese" }, { "nl", "Dutch" }, { "hi", "Hindi" }, { "ar", "Arabic" },
    };
    for (size_t i = 0; code[0] && i < sizeof NAMES / sizeof *NAMES; i++) if (strncasecmp(code, NAMES[i].code, 2) == 0) return NAMES[i].name;
    return NULL;
}

/* A character as a person reads it: the listed name without its mood ("Paul - Cheerful" is Paul), else the middle
 * of its id (fr_marie is Marie). */
static void character_name(const lp_mary *m, const char *character, char *out, size_t n) {
    out[0] = 0;
    for (int i = 0; m && i < m->voice_count && !out[0]; i++) {
        char c[96], mood[32];
        voice_parts(&m->voices[i], c, sizeof c, mood, sizeof mood);
        if (strcmp(c, character) != 0) continue;
        size_t skip = 0;
        const char *name = m->voices[i].name, *cut = mood_separator(name, &skip);
        snprintf(out, n, "%.*s", cut ? (int)(cut - name) : (int)strlen(name), name);
        size_t len = strlen(out), ml = strlen(mood);
        if (!cut && ml && len > ml && out[len - ml - 1] == ' ' && strcasecmp(out + len - ml, mood) == 0) out[len - ml - 1] = 0;
    }
    if (!out[0]) {
        const char *cut = strpbrk(character, "_:");
        snprintf(out, n, "%s", cut && cut[1] ? cut + 1 : character);
        out[0] = (char)toupper((unsigned char)out[0]);
    }
}

static void character_label(const lp_mary *m, const char *character, char *out, size_t n) {
    char name[64], language[16];
    character_name(m, character, name, sizeof name);
    character_language(m, character, language, sizeof language);
    const char *said = language_name(language);
    if (said) snprintf(out, n, "%s — %s", name, said);
    else snprintf(out, n, "%s", name);
}

/* "very_calm" as "Very calm". */
static void mood_label(const char *mood, char *out, size_t n) {
    snprintf(out, n, "%s", mood[0] ? mood : "neutral");
    for (char *c = out; *c; c++) if (*c == '_') *c = ' ';
    out[0] = (char)toupper((unsigned char)out[0]);
}

static void set_voice(struct prefs *p, lp_desktop *d, const char *voice) {
    if (!voice[0] || strcmp(d->settings.mary_voice, voice) == 0) return;
    snprintf(d->settings.mary_voice, sizeof d->settings.mary_voice, "%s", voice);
    lp_desktop_settings_changed(d);
    lp_desktop_publish_mary_config(d);             /* maryd speaks in it from the next sentence */
}

struct voice_choice {
    char character[96], voice[64], label[128], language[16];
    int custom;
};

/* Mistral's voices before the account's own, then by language and name. */
static int voice_after(const struct voice_choice *a, const struct voice_choice *b) {
    if (a->custom != b->custom) return a->custom > b->custom;
    int by_language = strcmp(a->language, b->language);
    return by_language ? by_language > 0 : strcmp(a->label, b->label) > 0;
}

/* The Voice (0) or Mood (1) menu under its pop-up. What each entry chooses is kept as the menu showed it, so a list
 * that changes while the menu is open cannot change what a click means. */
static void open_voice_menu(struct prefs *p, lp_desktop *d, int which) {
    if (!d) return;
    const lp_mary *m = &d->mary;
    char current[96], mood[32];
    voice_of(m, d->settings.mary_voice, current, sizeof current, mood, sizeof mood);
    lp_menu_model menu = { .id = "popup", .label = "", .count = 0 };
    memset(p->voice_menu, 0, sizeof p->voice_menu);
    memset(p->mood_menu, 0, sizeof p->mood_menu);
    if (which == 0) {
        struct voice_choice choices[LP_MENU_MAX_ENTRIES];
        int n = 0;
        for (int i = -1; i < m->voice_count; i++) {
            char character[96], ignored[32];
            if (i < 0) snprintf(character, sizeof character, "fr_marie");   /* Marie is always first */
            else voice_parts(&m->voices[i], character, sizeof character, ignored, sizeof ignored);
            int seen = -1;
            for (int k = 0; k < n && seen < 0; k++) if (strcmp(choices[k].character, character) == 0) seen = k;
            if (seen >= 0) {
                if (i >= 0 && m->voices[i].custom) choices[seen].custom = 1;
                continue;
            }
            if (n == LP_MENU_MAX_ENTRIES - 2) break;
            struct voice_choice *c = &choices[n++];
            snprintf(c->character, sizeof c->character, "%s", character);
            compose_voice(m, character, mood[0] ? mood : "neutral", c->voice, sizeof c->voice);
            character_label(m, character, c->label, sizeof c->label);
            character_language(m, character, c->language, sizeof c->language);
            c->custom = i >= 0 && m->voices[i].custom;
        }
        for (int a = 2; a < n; a++) {
            struct voice_choice key = choices[a];
            int b = a - 1;
            while (b >= 1 && voice_after(&choices[b], &key)) {
                choices[b + 1] = choices[b];
                b--;
            }
            choices[b + 1] = key;
        }
        for (int i = 0, headed = 0; i < n && menu.count < LP_MENU_MAX_ENTRIES; i++) {
            if (choices[i].custom && !headed) {
                if (menu.count + 3 > LP_MENU_MAX_ENTRIES) break;
                menu.entries[menu.count++] = (lp_menu_entry){ .separator = 1 };
                lp_menu_entry *header = &menu.entries[menu.count++];
                *header = (lp_menu_entry){ .disabled = 1 };
                snprintf(header->label, sizeof header->label, "Your voices");
                headed = 1;
            }
            int slot = menu.count;
            lp_menu_entry *e = &menu.entries[menu.count++];
            *e = (lp_menu_entry){ .command = LP_CMD_APP, .arg = LP_PREFS_CHOOSE_VOICE + slot, .checked = strcmp(choices[i].character, current) == 0 };
            snprintf(e->label, sizeof e->label, "%s", choices[i].label);
            snprintf(p->voice_menu[slot], sizeof p->voice_menu[slot], "%s", choices[i].voice);
        }
    } else {
        voice_moods moods;
        character_moods(m, current, &moods);
        if (!moods.count) return;
        for (int i = 0; i < moods.count && i < LP_MENU_MAX_ENTRIES && i < (int)(sizeof p->mood_menu / sizeof *p->mood_menu); i++) {
            lp_menu_entry *e = &menu.entries[menu.count++];
            *e = (lp_menu_entry){ .command = LP_CMD_APP, .arg = LP_PREFS_CHOOSE_MOOD + i, .checked = strcmp(moods.mood[i], mood[0] ? mood : "neutral") == 0 };
            mood_label(moods.mood[i], e->label, sizeof e->label);
            snprintf(p->mood_menu[i], sizeof p->mood_menu[i], "%s", moods.id[i]);
        }
    }
    lp_rect at = which ? p->mood_popup : p->voice_popup;
    lp_desktop_open_popup(d, p->window_id, at.x, at.y + at.h + 2, &menu);
}

/* A choice from one of those menus: 1 when the command was one. */
static int voice_command(struct prefs *p, lp_desktop *d, int cmd) {
    int moods = (int)(sizeof p->mood_menu / sizeof *p->mood_menu);
    const char *voice;
    if (cmd >= LP_PREFS_CHOOSE_MOOD && cmd < LP_PREFS_CHOOSE_MOOD + moods) voice = p->mood_menu[cmd - LP_PREFS_CHOOSE_MOOD];
    else if (cmd >= LP_PREFS_CHOOSE_VOICE && cmd < LP_PREFS_CHOOSE_VOICE + LP_MENU_MAX_ENTRIES) voice = p->voice_menu[cmd - LP_PREFS_CHOOSE_VOICE];
    else return 0;
    if (d) set_voice(p, d, voice);
    return 1;
}

/* A sentence in the voice's own language, with nothing in it that sounds like her name. */
static const char *sample_text(const char *language) {
    static const struct { const char *code, *text; } SAMPLES[] = {
        { "fr", "Bonjour ! Voici la voix que j’aurai." }, { "en", "Hello! This is how I will sound." },
        { "es", "¡Hola! Así sonará mi voz." }, { "de", "Hallo! So werde ich klingen." },
        { "it", "Ciao! Questa sarà la mia voce." }, { "pt", "Olá! Esta será a minha voz." }, { "nl", "Hallo! Zo ga ik klinken." },
    };
    for (size_t i = 0; language[0] && i < sizeof SAMPLES / sizeof *SAMPLES; i++)
        if (strncasecmp(language, SAMPLES[i].code, 2) == 0) return SAMPLES[i].text;
    return "Hello! This is how I will sound.";
}

static void play_sample(struct prefs *p, lp_desktop *d) {
    if (!d) return;
    lp_mary *m = &d->mary;
    if (m->sample_state == LP_MARY_SAMPLE_ASKING || m->sample_state == LP_MARY_SAMPLE_PLAYING) {
        lp_mary_stop(m);                            /* Stop: maryd ends the sample */
        return;
    }
    char character[96], mood[32], language[16];
    voice_of(m, d->settings.mary_voice, character, sizeof character, mood, sizeof mood);
    character_language(m, character, language, sizeof language);
    lp_mary_sample_voice(m, d->settings.mary_voice, sample_text(language));
}

static void voice_status(const lp_mary *m, char *out, size_t n) {
    out[0] = 0;
    if (!m || !lp_mary_connected(m)) snprintf(out, n, "Her voices can be heard once Mary is running.");
    else if (m->sample_state == LP_MARY_SAMPLE_ASKING) snprintf(out, n, "Asking Mistral…");
    else if (m->sample_state == LP_MARY_SAMPLE_PLAYING) snprintf(out, n, "Speaking…");
    else if (m->sample_state == LP_MARY_SAMPLE_FAILED) snprintf(out, n, "Not spoken: %s", m->sample_message[0] ? m->sample_message : "it failed");
    else if (!m->key_present) snprintf(out, n, "Marie is Mary’s own voice. Add a key to hear Mistral’s others.");
    else if (m->voices_state == LP_MARY_VOICES_ASKING) snprintf(out, n, "Asking Mistral for its voices…");
    else if (m->voices_state == LP_MARY_VOICES_FAILED) snprintf(out, n, "Only Marie for now: %s", m->voices_message);
}

static float pane_mary(lp_ctx *ctx, struct prefs *p, lp_desktop *d, float x, float y, float w, lp_id base) {
    heading(ctx, "Mary", x, &y, w);
    const lp_mary *m = d ? &d->mary : NULL;
    float after_toggle = x + LABEL_W + LP_TOGGLE_W + LP_SPACE_3, aside_w = w - LABEL_W - LP_TOGGLE_W - LP_SPACE_3;
    section(ctx, "Mistral", x, &y, w);
    label(ctx, "API key", x, y);
    float field_w = fmin(280, w - LABEL_W - 170);
    lp_text_field(ctx, lp_id_index(base, 400), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - LP_SIZE_CONTROL_HEIGHT) / 2, field_w, LP_SIZE_CONTROL_HEIGHT)), &p->mary_key,
                  (lp_text_field_opts){ .placeholder = m && m->key_present ? "Paste a new key to replace it" : "Paste your key", .icon = LP_ICON_COUNT, .secure = 1 });
    lp_button_opts save = { LP_BUTTON_PRIMARY, LP_CONTROL_SM, LP_ICON_COUNT, 0, p->mary_key.len == 0 };
    lp_size ss = lp_button_measure(ctx, "Save", save);
    float bx = x + LABEL_W + field_w + LP_SPACE_2;
    if (lp_button(ctx, lp_id_index(base, 401), LP_RECT(bx, y + (ROW_H - ss.h) / 2, ss.w, ss.h), "Save", save) && p->mary_key.len) save_mary_key(p);
    lp_button_opts verify = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, !(m && lp_mary_connected(m) && m->key_present) };
    lp_size vs = lp_button_measure(ctx, "Verify", verify);
    if (lp_button(ctx, lp_id_index(base, 402), LP_RECT(bx + ss.w + LP_SPACE_2, y + (ROW_H - vs.h) / 2, vs.w, vs.h), "Verify", verify) && d && lp_mary_verify_key(&d->mary) == 0)
        snprintf(p->mary_status, sizeof p->mary_status, "Asking Mistral…");
    y += ROW_H;
    char status[240];
    mary_key_status(p, m, status, sizeof status);
    note(ctx, status, x + LABEL_W, &y, w - LABEL_W);

    section(ctx, "Voice", x, &y, w);
    {
        char character[96], mood[32], choice[128], feeling[40];
        voice_of(m, d ? d->settings.mary_voice : "fr_marie_neutral", character, sizeof character, mood, sizeof mood);
        character_label(m, character, choice, sizeof choice);
        label(ctx, "Voice", x, y);
        lp_size vs = lp_popup_button_measure(ctx, choice);
        p->voice_popup = control_at(LP_RECT(x + LABEL_W, y + (ROW_H - vs.h) / 2, fmax(180, vs.w), vs.h));
        if (lp_popup_button(ctx, lp_id_index(base, 410), p->voice_popup, choice, !d)) open_voice_menu(p, d, 0);
        y += ROW_H + LP_SPACE_1;
        voice_moods moods;
        character_moods(m, character, &moods);
        if (moods.count > 1) {
            mood_label(mood, feeling, sizeof feeling);
            label(ctx, "Mood", x, y);
            lp_size ms = lp_popup_button_measure(ctx, feeling);
            p->mood_popup = control_at(LP_RECT(x + LABEL_W, y + (ROW_H - ms.h) / 2, fmax(120, ms.w), ms.h));
            if (lp_popup_button(ctx, lp_id_index(base, 411), p->mood_popup, feeling, !d)) open_voice_menu(p, d, 1);
            y += ROW_H + LP_SPACE_1;
        }
        int sampling = m && (m->sample_state == LP_MARY_SAMPLE_ASKING || m->sample_state == LP_MARY_SAMPLE_PLAYING);
        lp_button_opts so = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, sampling ? LP_ICON_STOP : LP_ICON_PLAY, 0, !(m && lp_mary_connected(m) && m->key_present) };
        const char *sample_label = sampling ? "Stop" : "Play Sample";
        lp_size bs = lp_button_measure(ctx, sample_label, so);
        if (lp_button(ctx, lp_id_index(base, 412), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - bs.h) / 2, bs.w, bs.h)), sample_label, so)) play_sample(p, d);
        y += ROW_H;
        char said[240];
        voice_status(m, said, sizeof said);
        note(ctx, said, x + LABEL_W, &y, w - LABEL_W);
    }

    section(ctx, "Listening", x, &y, w);
    int wake = d ? d->settings.mary_wake : 1;
    float row_y = y;
    if (toggle_row(ctx, lp_id_index(base, 403), "“Hey Mary”", x, &y, &wake) && d) {
        d->settings.mary_wake = wake;
        lp_desktop_settings_changed(d);
        lp_desktop_publish_mary_config(d);
    }
    aside(ctx, "Listens for her name while the desktop is idle", after_toggle, row_y, aside_w);
    if (!d) return y;

    section(ctx, "Engines", x, &y, w);
    note(ctx, "Which model each lane runs on. Mistral is the one served; Thinking Machines is a toggle for a later implementation.", x, &y, w);
    {
        static const lp_segment ENGINES[2] = { { "Mistral", LP_ICON_COUNT }, { "Thinking Machines", LP_ICON_COUNT } };
        const char *const lanes[2] = { "Voice (Lane A)", "Skills (Lane B)" };
        char *fields[2] = { d->settings.mary_voice_engine, d->settings.mary_skill_engine };
        for (int lane = 0; lane < 2; lane++) {
            int engine = strcmp(fields[lane], "tinker") == 0 ? 1 : 0;
            label(ctx, lanes[lane], x, y);
            lp_size s = lp_segmented_measure(ctx, ENGINES, 2, LP_CONTROL_SM);
            lp_rect r = control_at(LP_RECT(x + LABEL_W, y + (ROW_H - s.h) / 2, s.w, s.h));
            row_y = y;
            if (lp_segmented_masked(ctx, lp_id_index(base, 420 + lane), r.x, r.y, ENGINES, 2, &engine, LP_CONTROL_SM, 2u)) {
                snprintf(fields[lane], 16, "%s", engine ? "tinker" : "mistral");
                lp_desktop_settings_changed(d);
                lp_desktop_publish_mary_config(d);
            }
            aside(ctx, lane == 0 ? "The reply, spoken with retrieval" : "The silent skills loop that acts", x + LABEL_W + s.w + LP_SPACE_3, row_y, w - LABEL_W - s.w - LP_SPACE_3);
            y += ROW_H + LP_SPACE_1;
        }
        note(ctx, "Thinking Machines \xE2\x80\x94 later: no key, no wire yet; the toggle is kept so the lanes need no redesign.", x + LABEL_W, &y, w - LABEL_W);
    }

    section(ctx, "Recall", x, &y, w);
    note(ctx, "Which of the Thread's two lanes a turn may retrieve from. Each is a set of record families on the drive.", x, &y, w);
    {
        struct { const char *name, *families; int *on; } lanes[2] = {
            { "Personal", "memory, file, style \xE2\x80\x94 what you know, wrote and keep", &d->settings.mary_recall_personal },
            { "Behavioral", "behavior, routing \xE2\x80\x94 what Mary did before, and how you ask", &d->settings.mary_recall_behavioral },
        };
        for (int i = 0; i < 2; i++) {
            row_y = y;
            int on = *lanes[i].on;
            if (toggle_row(ctx, lp_id_index(base, 430 + i), lanes[i].name, x, &y, &on)) {
                *lanes[i].on = on;
                lp_desktop_settings_changed(d);
                lp_desktop_publish_mary_config(d);
            }
            aside(ctx, lanes[i].families, after_toggle, row_y, aside_w);
        }
    }

    section(ctx, "Network activity", x, &y, w);
    note(ctx, "Every call sewnd made, newest first \xE2\x80\x94 the only process on the machine that reaches the network. Never a key, never a body.", x, &y, w);
    {
        lp_button_opts ro = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_RELOAD, 0, !(m && lp_mary_connected(m)) };
        lp_size rs = lp_button_measure(ctx, "Refresh", ro);
        if (lp_button(ctx, lp_id_index(base, 440), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - rs.h) / 2, rs.w, rs.h)), "Refresh", ro)) ask_activity(p);
        y += ROW_H;
#ifdef HAVE_JSONC
        struct json_object *calls = m ? m->calls : NULL, *rows = NULL;
        if (calls && json_object_object_get_ex(calls, "calls", &rows) && json_object_is_type(rows, json_type_array) && json_object_array_length(rows)) {
            static const char *const COLUMNS[5] = { "purpose", "provider", "path", "status", "ms" };
            static const enum lp_align ALIGN[5] = { LP_ALIGN_START, LP_ALIGN_START, LP_ALIGN_START, LP_ALIGN_END, LP_ALIGN_END };
            float table_w = w - LABEL_W;
            lp_list_header_aligned(ctx, lp_id_index(base, 441), LP_RECT(x + LABEL_W, y, table_w, LP_LIST_ROW_H), COLUMNS, ALIGN, 5, -1, 0);
            y += LP_LIST_ROW_H;
            double total_ms = 0;
            size_t n = json_object_array_length(rows);
            for (size_t i = 0; i < n && i < 12; i++) {
                struct json_object *row = json_object_array_get_idx(rows, i), *v;
                const char *purpose = json_object_object_get_ex(row, "purpose", &v) ? json_object_get_string(v) : "";
                const char *provider = json_object_object_get_ex(row, "provider", &v) ? json_object_get_string(v) : "";
                const char *path = json_object_object_get_ex(row, "path", &v) ? json_object_get_string(v) : "";
                long status = json_object_object_get_ex(row, "status", &v) ? json_object_get_int(v) : 0;
                long ms = json_object_object_get_ex(row, "ms", &v) ? json_object_get_int(v) : 0;
                total_ms += (double)ms;
                char when[40], status_s[16], ms_s[16];
                int64_t at = json_object_object_get_ex(row, "at_ms", &v) ? json_object_get_int64(v) : 0;
                time_t t = (time_t)(at / 1000);
                struct tm tm;
                if (at) strftime(when, sizeof when, "%H:%M:%S", localtime_r(&t, &tm));
                else snprintf(when, sizeof when, "\xE2\x80\x94");
                snprintf(status_s, sizeof status_s, status ? "%ld" : "\xE2\x80\x94", status);
                snprintf(ms_s, sizeof ms_s, "%ld", ms);
                const char *cols[5] = { purpose, provider, path, status_s, ms_s };
                lp_list_row_aligned(ctx, lp_id_index(base, 450 + (int)i), LP_RECT(x + LABEL_W, y, table_w, LP_LIST_ROW_H), LP_ICON_COUNT, when, cols, ALIGN, 5, 0, (int)(i & 1));
                y += LP_LIST_ROW_H;
            }
            char total[120];
            snprintf(total, sizeof total, "%zu call%s shown, %.0f ms in all \xC2\xB7 the whole ledger: sewnctl calls", n, n == 1 ? "" : "s", total_ms);
            note(ctx, total, x + LABEL_W, &y, w - LABEL_W);
        } else
#endif
        {
            note(ctx, m && lp_mary_connected(m) ? "No calls yet, or sewnd has not answered." : "maryd is not running.", x + LABEL_W, &y, w - LABEL_W);
        }
    }

    section(ctx, "Memory", x, &y, w);
    {
        char counts[240] = "The drive's memory is threadd; it has not answered yet.";
#ifdef HAVE_JSONC
        struct json_object *stats = lp_thread_answer(&d->thread, LP_THREAD_STATS), *v;
        if (stats) {
            long documents = json_object_object_get_ex(stats, "documents", &v) ? json_object_get_int(v) : 0;
            long files = json_object_object_get_ex(stats, "files", &v) ? json_object_get_int(v) : 0;
            long entities = json_object_object_get_ex(stats, "entities", &v) ? json_object_get_int(v) : 0;
            snprintf(counts, sizeof counts, "%ld record%s on the drive, %ld of them files; the rest what Sewn remembered and what Mary did \xC2\xB7 %ld entit%s in the graph",
                     documents, documents == 1 ? "" : "s", files, entities, entities == 1 ? "y" : "ies");
        }
#endif
        label(ctx, "The Thread", x, y);
        lp_button_opts to = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 };
        lp_size ts = lp_button_measure(ctx, "Open Threads", to);
        if (lp_button(ctx, lp_id_index(base, 460), control_at(LP_RECT(x + LABEL_W, y + (ROW_H - ts.h) / 2, ts.w, ts.h)), "Open Threads", to)) lp_desktop_open_app(d, "thread");
        y += ROW_H;
        note(ctx, counts, x + LABEL_W, &y, w - LABEL_W);
    }

    section(ctx, "Skills", x, &y, w);
    note(ctx, "What Mary may do with each app, and when she asks you first.", x, &y, w);
    int allow_all = lp_skill_allow_all(&d->skill_policy);
    if (toggle_row(ctx, lp_id_index(base, 490), "Allow without asking", x, &y, &allow_all)) {
        lp_skill_set_allow_all(&d->skill_policy, allow_all);
        lp_desktop_skill_policy_changed(d);
    }
    note(ctx, allow_all ? "Mary runs every command you give her straight away, with no card to allow \xE2\x80\x94 including ones that cannot be undone. Each app's \xE2\x80\x9C" "Ask first\xE2\x80\x9D below waits until this is off."
                        : "Off: each app's \xE2\x80\x9C" "Ask first\xE2\x80\x9D decides, and anything that cannot be undone always asks.",
         x, &y, w);
    static const lp_segment ASK[3] = { { "Never", LP_ICON_COUNT }, { "Before changes", LP_ICON_COUNT }, { "Always", LP_ICON_COUNT } };
    int group = 0;
    for (int a = 0; a < d->app_count; a++) {
        const lp_app *app = d->apps[a];
        if (!app->skill_count || !app->perform) continue;
        lp_id gid = lp_id_index(base, 500 + 20 * group++);
        subheading(ctx, app->name ? app->name : app->title, x, &y, w);
        int on = lp_skill_app_enabled(&d->skill_policy, app->id);
        if (toggle_row(ctx, gid, "Allow Mary", x, &y, &on)) {
            lp_skill_set_app_enabled(&d->skill_policy, app->id, on);
            lp_desktop_skill_policy_changed(d);
        }
        int ask = (int)lp_skill_app_ask(&d->skill_policy, app->id);
        if (segmented_row(ctx, lp_id_index(gid, 1), "Ask first", x, &y, ASK, 3, &ask)) {
            lp_skill_set_app_ask(&d->skill_policy, app->id, (lp_skill_ask)ask);
            lp_desktop_skill_policy_changed(d);
        }
        for (int s = 0; s < app->skill_count && s < 16; s++) {
            const lp_skill *sk = &app->skills[s];
            row_y = y;
            int enabled = lp_skill_enabled(&d->skill_policy, app->id, sk->id);
            if (toggle_row(ctx, lp_id_index(gid, 2 + s), sk->title, x, &y, &enabled)) {
                lp_skill_set_enabled(&d->skill_policy, app->id, sk->id, enabled);
                lp_desktop_skill_policy_changed(d);
            }
            aside(ctx, sk->effect == LP_SKILL_READ ? "Looks, changes nothing" : sk->effect == LP_SKILL_ACT ? "Changes something" : "Cannot be undone",
                  after_toggle, row_y, aside_w);
        }
    }

    section(ctx, "Ambient", x, &y, w);
    note(ctx, "What each app tells Mary about what is on its screen, so she knows what is in front of you without looking (PARITY D28).", x, &y, w);
    static const struct { const char *id, *publishes; } PUBLISHES[] = {
        { "textedit", "the document, a window of its text, what you have selected" },
        { "finder", "the folder, its entries, what is selected" },
        { "preview", "the image or PDF, the page, the zoom" },
        { "calendar", "the day and view shown, how many events, an event being edited" },
        { "media", "the file, whether it plays, its place in the folder" },
        { "terminal", "the last screen rows, as text" },
        { "settings", "the pane that is open" },
        { "calculator", "the display" },
    };
    for (int a = 0; a < d->app_count; a++) {
        const lp_app *app = d->apps[a];
        if (!app->surface) continue;
        const char *what = "its front window";
        for (size_t i = 0; i < sizeof PUBLISHES / sizeof *PUBLISHES; i++) if (strcmp(PUBLISHES[i].id, app->id) == 0) what = PUBLISHES[i].publishes;
        char every[160];
        snprintf(every, sizeof every, "%s \xC2\xB7 every %d s, and whenever a turn asks", what, app->surface_poll_s > 0 ? app->surface_poll_s : LP_WORLD_POLL_S);
        label(ctx, app->name ? app->name : app->title, x, y);
        aside(ctx, every, x + LABEL_W, y, w - LABEL_W);
        y += ROW_H;
    }
    return y;
}

static void prefs_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct prefs empty = { .wifi_selected = -1, .queued_volume = -1, .queued_input_volume = -1 };
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
    /* What the last action said sits in a footer under the pane, never over it. */
    lp_rect footer = p->message[0] ? lp_rect_cut_bottom(&area, FOOTER_H) : LP_RECT(0, 0, 0, 0);
    lp_rect in = lp_rect_inset(area, LP_SPACE_6, LP_SPACE_5);
    /* A pane taller than the window scrolls under the wheel; each pane says where it ended. */
    painting = p;
    lp_rect origin = lp_scroll_begin(ctx, lp_id_index(base, 900), area, (lp_size){ area.w, p->content_h }, &p->scroll);
    float x = in.x, y = in.y + (origin.y - area.y), w = in.w, end = y;
    switch (p->pane) {
    case LP_PREFS_GENERAL: end = pane_general(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_DOCK: end = pane_dock(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_DISPLAYS: end = pane_displays(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_KEYBOARD: end = pane_keyboard(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_SOUND: end = pane_sound(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_NETWORK: end = pane_network(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_TIME: end = pane_time(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_USERS: end = pane_users(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_ABOUT: end = pane_about(ctx, p, desk, x, y, w, base); break;
    case LP_PREFS_MARY: end = pane_mary(ctx, p, desk, x, y, w, base); break;
    }
    lp_scroll_end(ctx);
    painting = NULL;
    p->content_h = end - origin.y + LP_SPACE_5;
    if (footer.h > 0 && ctx->pass == LP_PASS_DRAW && ctx->cr) {
        lp_fill_solid(ctx->cr, LP_RECT(footer.x, footer.y, footer.w, 1), LP_EDGE_DIVIDER, 0);
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_SM;
        st.color = LP_INK_SECONDARY;
        st.ellipsize = 1;
        lp_text_draw(ctx->cr, p->message, LP_RECT(footer.x + LP_SPACE_6, footer.y + 1, footer.w - 2 * LP_SPACE_6, footer.h - 1), &st, LP_ALIGN_START);
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
    p->queued_input_volume = -1;
    lp_sysinfo_about(&p->about);
    return p;
}

static void prefs_destroy(void *state) {
    struct prefs *p = state;
    if (!p) return;
    meter(p, 0);
    if (p->ticker) lp_desktop_remove_source(p->desk, p->ticker);
    if (p->job && p->run == lp_job_run) lp_job_cancel(p->job);
    wipe(p->mary_key.text, sizeof p->mary_key.text);
    free(p);
}

/* What Mary sees of System Settings (PARITY D28): the pane that is open, and the others. */
static int prefs_surface(void *state, lp_desktop *d, lp_app_surface *out) {
    struct prefs *p = state;
    if (!p) return 0;
    int pane = p->pane >= 0 && p->pane < PANE_COUNT ? p->pane : 0;
    snprintf(out->document_name, sizeof out->document_name, "%s", PANES[pane].name);
    char text[120];
    snprintf(text, sizeof text, "The %s pane is open", PANES[pane].name);
    out->document_text = strdup(text);
    for (int i = 0; i < PANE_COUNT; i++) lp_app_surface_add(out, "row", "pane", PANES[i].name, i == pane, 1);
    return 1;
}

/* MARK: - Mary's skills (PARITY D20) */

/* LP_PREFS_* order: the words Mary names a pane by. */
static const char *const PANE_SLUGS[] = { "general", "dock", "displays", "keyboard", "sound", "network", "time", "users", "about", "mary" };

static const char *const PREFS_TOKENS[] = { "settings", "preferences", "pane" };
static const char *const PREFS_PHRASES[] = { "open settings", "open system settings", "show me the settings" };
static const char *const PREFS_CLASSES[] = { "pane", "setting" };
static const lp_skill prefs_skills[] = {
    { .id = "open_pane", .title = "Open a pane", .summary = "Opens System Settings on one of its panes; it changes no setting.",
      .params = "{\"type\":\"object\",\"properties\":{\"pane\":{\"type\":\"string\",\"enum\":[\"general\",\"dock\",\"displays\",\"keyboard\",\"sound\",\"network\",\"time\",\"users\",\"about\"]}},\"required\":[\"pane\"]}",
      .effect = LP_SKILL_READ, .kind = "cognitive", .access = "seamless", .triggers = PREFS_TOKENS, .trigger_count = 3,
      .phrases = PREFS_PHRASES, .phrase_count = 3, .target_classes = PREFS_CLASSES, .target_class_count = 2,
      .spoken = "{\"pane\":{\"general\":[\"general\"],\"dock\":[\"dock\",\"the dock\"],\"displays\":[\"displays\",\"display\",\"screen\"],\"keyboard\":[\"keyboard\",\"mouse\",\"keyboard and mouse\"],"
                "\"sound\":[\"sound\",\"audio\",\"volume\"],\"network\":[\"network\",\"wifi\",\"wi-fi\"],\"time\":[\"time\",\"date\",\"date and time\"],\"users\":[\"users\",\"accounts\"],\"about\":[\"about\"]}}" },
};

static int prefs_perform(void *state, lp_desktop *d, const char *skill, const char *args, char *result, size_t n) {
    char name[32];
    if (strcmp(skill, "open_pane") != 0) return -ENOENT;
    if (!lp_skill_arg_string(args, "pane", name, sizeof name)) {
        snprintf(result, n, "Which pane? general, dock, displays, keyboard, sound, network, time, users or about.");
        return -EINVAL;
    }
    int pane = -1;
    for (int i = 0; i < PANE_COUNT && pane < 0; i++)
        if (strcasecmp(name, PANE_SLUGS[i]) == 0 || strcasecmp(name, PANES[i].name) == 0) pane = i;
    if (pane < 0) {
        snprintf(result, n, "System Settings has no pane called %.31s.", name);
        return -EINVAL;
    }
    lp_desktop_open_app(d, "settings");     /* one window: opened, or brought forward */
    struct prefs *p = lp_desktop_app_state(d, "settings");
    if (!p) {
        snprintf(result, n, "System Settings would not open.");
        return -EIO;
    }
    prefs_command(p, d, pane);
    if (d->on_app_dirty) d->on_app_dirty(d, p->window_id);
    snprintf(result, n, "{\"pane\":\"%s\"}", PANE_SLUGS[pane]);
    return 0;
}

/* The Mary pane shows lp_mary and Sound shows lp_audio: a change to either repaints the window while it shows. */
static int prefs_model_changed(void *state, lp_desktop *d, unsigned model, unsigned what) {
    struct prefs *p = state;
    if (model == LP_MODEL_AUDIO && p->pane == LP_PREFS_SOUND) {
        /* a new default, chosen here or anywhere else, has volumes of its own */
        if (what & (LP_AUDIO_CHANGED_DEFAULTS | LP_AUDIO_CHANGED_CONNECTION)) refresh_pane(p);
        return 1;
    }
    if (model == LP_MODEL_MARY && p->pane == LP_PREFS_MARY) {
        if (what & (LP_MARY_CHANGED_CONNECTION | LP_MARY_CHANGED_KEY)) ask_voices(p);   /* maryd is back, or a key is in */
        if (what & LP_MARY_CHANGED_CONNECTION) ask_activity(p);
        return 1;
    }
    if (model == LP_MODEL_THREAD && p->pane == LP_PREFS_MARY) return (what & (LP_THREAD_CHANGED_STATS | LP_THREAD_CHANGED_CONNECTION)) != 0;
    return 0;
}

const lp_app lp_app_prefs = {
    .id = "settings", .title = "System Settings", .name = "Settings", .aka = "System Settings", .icon = LP_ICON_GEAR, .dock = 1,
    .default_rect = { NAN, NAN, 800, 540 }, .min_size = { 660, 440 }, .singleton = 1, .resizable = 1,
    .create = prefs_create, .paint = prefs_paint, .destroy = prefs_destroy,
    .command = prefs_command, .menu_entries = prefs_menu_entries,
    .skills = prefs_skills, .skill_count = 1, .perform = prefs_perform, .model_changed = prefs_model_changed,
    .surface = prefs_surface, .surface_poll_s = 120,
    .summary = "Every setting of the machine, pane by pane.", .discipline = "system-control", .paradigm = "systemControl",
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
void lp_prefs_set_input_volume(void *state, int volume) { set_input_volume(state, volume); }
void lp_prefs_choose_device(void *state, int direction, int index) { choose_device(state, direction ? LP_AUDIO_INPUT : LP_AUDIO_OUTPUT, index); }
int lp_prefs_input_volume(const void *state) { const struct prefs *p = state; return p->input_known ? (int)p->input_volume : -1; }
int lp_prefs_metering(const void *state) { return ((const struct prefs *)state)->metering; }
void lp_prefs_open_voice_menu(void *state, int which) { struct prefs *p = state; open_voice_menu(p, p->desk, which); }
void lp_prefs_mary_play_sample(void *state) { struct prefs *p = state; play_sample(p, p->desk); }
void lp_prefs_mary_set_key_text(void *state, const char *text) { lp_text_buffer_set(&((struct prefs *)state)->mary_key, text); }
void lp_prefs_mary_save_key(void *state) { save_mary_key(state); }
const char *lp_prefs_mary_status(const void *state) { return ((const struct prefs *)state)->mary_status; }
const char *lp_prefs_mary_key_bytes(const void *state, size_t *n) {
    const struct prefs *p = state;
    *n = sizeof p->mary_key.text;
    return p->mary_key.text;
}
float lp_prefs_scroll(const void *state) { return ((const struct prefs *)state)->scroll.y; }
void lp_prefs_trace(void *state, lp_prefs_trace_fn fn, void *user) {
    struct prefs *p = state;
    p->trace = fn;
    p->trace_user = user;
}
int lp_prefs_pane_named(const char *name) {
    for (int i = 0; name && i < PANE_COUNT; i++)
        if (strcasecmp(name, PANE_SLUGS[i]) == 0 || strcasecmp(name, PANES[i].name) == 0) return i;
    return -1;
}
const char *lp_prefs_pane_slug(int pane) { return pane >= 0 && pane < PANE_COUNT ? PANE_SLUGS[pane] : NULL; }
