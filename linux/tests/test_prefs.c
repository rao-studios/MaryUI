/* System Settings, driven headlessly with the job runner stood in for, so no
 * service is touched: Network reads the links and then scans the Wi-Fi it
 * finds, and joins with a passphrase; Sound reads and sets the volume, and a
 * change asked for while a job runs is sent when it ends; Date & Time reads the
 * zone and refuses one that does not exist; About refuses a bad name; Keyboard
 * & Mouse saves and applies the layout; the Dock pane changes what a blank
 * Spotlight shows; and every pane paints. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"
#include "maryui/lp_job.h"

static char root[512];
static lp_desktop d;
static int settings_calls;

static char ran[8][512];     /* the last few commands, newest last */
static int runs;
static lp_job_done_fn pending_done;
static void *pending_user;

static lp_job *fake_run(lp_desktop *desk, const char *const *argv, lp_job_done_fn done, void *user) {
    char line[512] = "";
    for (int i = 0; argv[i]; i++) {
        size_t len = strlen(line);
        snprintf(line + len, sizeof line - len, "%s%s", i ? " " : "", argv[i]);
    }
    snprintf(ran[runs % 8], sizeof ran[0], "%s", line);
    runs++;
    pending_done = done;
    pending_user = user;
    return (lp_job *)&pending_done;
}
static const char *last_run(void) { return runs ? ran[(runs - 1) % 8] : ""; }
static void finish(int status, const char *output) {
    lp_job_done_fn done = pending_done;
    pending_done = NULL;
    done(status, output, pending_user);
}
static void on_settings(lp_desktop *desk) { settings_calls++; }

static void *open_prefs(char *id) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    d.on_settings = on_settings;
    settings_calls = 0;
    if (lp_desktop_open_app_with(&d, "settings", NULL, NULL, id) != 1) return NULL;
    void *p = lp_desktop_instance(&d, id)->state;
    lp_prefs_set_runner(p, fake_run);
    runs = 0;
    return p;
}

LP_TEST(network_reads_the_links_then_scans_the_wifi) {
    char id[12];
    void *p = open_prefs(id);
    LP_ASSERT(p != NULL);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_NETWORK);
    LP_ASSERT_EQ(lp_prefs_pane(p), LP_PREFS_NETWORK);
    LP_ASSERT_STR(last_run(), "ip -brief address");
    finish(0, "lo UNKNOWN 127.0.0.1/8\nenp0s1 UP 192.168.64.2/24\nwlan0 UP 10.0.0.5/24\n");
    LP_ASSERT_EQ(lp_prefs_link_count(p), 2);
    LP_ASSERT_STR(last_run(), "iwctl station wlan0 scan");
    finish(0, "");
    LP_ASSERT_STR(last_run(), "iwctl station wlan0 get-networks");
    finish(0, "  Network name   Security   Signal\n----\n  > Home     psk    ****\n    Cafe     open   **\n");
    LP_ASSERT_EQ(lp_prefs_network_count(p), 2);
    lp_prefs_join(p, 0, "");
    LP_ASSERT_STR(lp_prefs_message(p), "“Home” needs its password.");
    lp_prefs_join(p, 0, "s3cret");
    LP_ASSERT_STR(last_run(), "iwctl --passphrase s3cret station wlan0 connect Home");
    finish(0, "");
    LP_ASSERT_STR(lp_prefs_message(p), "Joined “Home”.");
    LP_ASSERT_STR(last_run(), "ip -brief address");      /* and the links are read again */
    finish(0, "enp0s1 UP 192.168.64.2/24\n");
    lp_desktop_close_window(&d, id);
}

LP_TEST(sound_reads_and_sets_the_volume) {
    char id[12];
    void *p = open_prefs(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_SOUND);
    LP_ASSERT_STR(last_run(), "wpctl get-volume @DEFAULT_AUDIO_SINK@");
    LP_ASSERT_EQ(lp_prefs_volume(p), -1);
    finish(0, "Volume: 0.40\n");
    LP_ASSERT_EQ(lp_prefs_volume(p), 40);
    LP_ASSERT_STR(last_run(), "wpctl get-volume @DEFAULT_AUDIO_SOURCE@");   /* then the microphone's */
    finish(0, "Volume: 0.65 [MUTED]\n");
    LP_ASSERT_EQ(lp_prefs_input_volume(p), 65);
    lp_prefs_set_volume(p, 70);
    LP_ASSERT_STR(last_run(), "wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.70");
    lp_prefs_set_volume(p, 75);                                /* while that runs: queued */
    lp_prefs_set_volume(p, 80);
    LP_ASSERT_EQ(runs, 3);
    finish(0, "");
    LP_ASSERT_STR(last_run(), "wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.80");   /* only the newest */
    finish(0, "");
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_SOUND);
    finish(1, "Could not connect to PipeWire\n");
    LP_ASSERT_EQ(lp_prefs_volume(p), -1);
    lp_desktop_close_window(&d, id);
}

LP_TEST(date_and_time_reads_the_zone_and_refuses_one_that_does_not_exist) {
    char id[12];
    void *p = open_prefs(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_TIME);
    LP_ASSERT_STR(last_run(), "timedatectl show");
    finish(0, "Timezone=Europe/Paris\nLocalRTC=no\nNTP=yes\n");
    LP_ASSERT_STR(lp_prefs_timezone(p), "Europe/Paris");
    int before = runs;
    lp_prefs_set_zone(p, "Mars/Olympus_Mons");
    LP_ASSERT_EQ(runs, before);
    LP_ASSERT(strstr(lp_prefs_message(p), "is not a time zone") != NULL);
    lp_prefs_set_zone(p, "../../etc/passwd");
    LP_ASSERT_EQ(runs, before);
    if (access("/usr/share/zoneinfo/Etc/UTC", R_OK) == 0) {
        lp_prefs_set_zone(p, "Etc/UTC");
        LP_ASSERT_STR(last_run(), "timedatectl set-timezone Etc/UTC");
        finish(1, "Failed to set time zone: Access denied\n");
        LP_ASSERT_STR(lp_prefs_message(p), "Could not change the time zone: Failed to set time zone: Access denied");
        LP_ASSERT_STR(last_run(), "timedatectl show");        /* read again either way */
        finish(0, "Timezone=Europe/Paris\nNTP=yes\n");
    }
    lp_desktop_close_window(&d, id);
}

LP_TEST(about_refuses_a_name_a_computer_cannot_have) {
    char id[12];
    void *p = open_prefs(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_ABOUT);
    int before = runs;
    lp_prefs_set_hostname(p, "my computer!");
    LP_ASSERT_EQ(runs, before);
    lp_prefs_set_hostname(p, "-maryos");
    LP_ASSERT_EQ(runs, before);
    lp_prefs_set_hostname(p, "maryos-2");
    LP_ASSERT_STR(last_run(), "hostnamectl set-hostname maryos-2");
    finish(0, "");
    LP_ASSERT_STR(lp_prefs_message(p), "This computer is now called maryos-2.");
    lp_desktop_close_window(&d, id);
}

LP_TEST(keyboard_settings_are_saved_and_applied) {
    char id[12];
    void *p = open_prefs(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_KEYBOARD);
    lp_prefs_set_layout(p, "us(dvorak)");
    LP_ASSERT_STR(d.settings.keyboard_layout, "us(dvorak)");
    LP_ASSERT(settings_calls >= 1);
    lp_settings loaded = lp_settings_load();
    LP_ASSERT_STR(loaded.keyboard_layout, "us(dvorak)");
    lp_prefs_set_layout(p, "us; rm -rf /");
    LP_ASSERT_STR(d.settings.keyboard_layout, "us(dvorak)");
    LP_ASSERT(strstr(lp_prefs_message(p), "is not a keyboard layout") != NULL);
    lp_prefs_set_layout(p, "");
    lp_desktop_close_window(&d, id);
}

LP_TEST(the_dock_pane_changes_what_spotlight_shows) {
    char id[12];
    open_prefs(id);
    const lp_app *gallery = lp_desktop_find_app(&d, "gallery"), *calendar = lp_desktop_find_app(&d, "calendar");
    LP_ASSERT(!lp_desktop_in_dock(&d, gallery));
    LP_ASSERT(lp_desktop_in_dock(&d, calendar));
    lp_desktop_set_in_dock(&d, "gallery", 1);
    lp_desktop_set_in_dock(&d, "calendar", 0);
    LP_ASSERT(lp_desktop_in_dock(&d, gallery));
    LP_ASSERT(!lp_desktop_in_dock(&d, calendar));
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS], results[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_spotlight_items(&d, items, LP_SPOTLIGHT_MAX_ITEMS);
    int count = lp_spotlight_results(items, n, "", results, LP_SPOTLIGHT_MAX_RESULTS), has_gallery = 0, has_calendar = 0;
    for (int i = 0; i < count; i++) {
        has_gallery |= strcmp(results[i].id, "gallery") == 0;
        has_calendar |= strcmp(results[i].id, "calendar") == 0;
    }
    LP_ASSERT(has_gallery && !has_calendar);
    lp_settings loaded = lp_settings_load();
    LP_ASSERT(strstr(loaded.dock, "gallery") != NULL && strstr(loaded.dock, "calendar") == NULL);
    lp_desktop_set_in_dock(&d, "gallery", 0);
    lp_desktop_set_in_dock(&d, "calendar", 1);
    lp_desktop_close_window(&d, id);
}

LP_TEST(every_pane_paints) {
    char id[12];
    void *p = open_prefs(id);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 800, 498);
    cairo_t *cr = cairo_create(s);
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    lp_input in = { .mx = -1, .my = -1 };
    for (int pane = 0; pane <= LP_PREFS_MARY; pane++) {
        lp_desktop_run_command(&d, LP_CMD_APP, pane);
        if (pending_done) finish(1, "");
        if (pending_done) finish(1, "");
        for (int pass = 0; pass < 2; pass++) {
            lp_ctx_begin(&ctx, pass ? LP_PASS_DRAW : LP_PASS_EVENT, pass ? cr : NULL, &in, LP_RECT(0, 0, 800, 498), 1000 + pane * 10 + pass);
            lp_app_prefs.paint(p, &ctx, LP_RECT(0, 0, 800, 498), &d);
            lp_ctx_end(&ctx);
        }
    }
    cairo_surface_flush(s);
    uint32_t px;
    memcpy(&px, cairo_image_surface_get_data(s) + 250 * cairo_image_surface_get_stride(s) + 4 * 500, 4);
    LP_ASSERT((px >> 24) == 255);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    lp_desktop_close_window(&d, id);
}

LP_TEST(settings_keep_their_defaults_and_bounds) {
    lp_settings fresh = lp_settings_defaults();
    LP_ASSERT_EQ(fresh.key_repeat_rate, 25);
    LP_ASSERT_EQ(fresh.key_repeat_delay, 600);
    LP_ASSERT_STR(fresh.dock, "");
    LP_ASSERT_STR(fresh.mary_voice, "fr_marie_neutral");
    char path[700];
    snprintf(path, sizeof path, "%s/config/maryui", root);
    lp_files_mkdir_p(path);
    snprintf(path, sizeof path, "%s/config/maryui/settings.conf", root);
    const char *old = "accent=graphite\nclock=on\nkey_repeat_rate=0\nkey_repeat_delay=99999\npointer_speed=5\nclock_24h=on\ndock=finder,settings\nmary_voice=en_paul_neutral\n";
    lp_files_write(path, old, strlen(old));
    lp_settings s = lp_settings_load();
    LP_ASSERT_EQ(s.accent, LP_ACCENT_GRAPHITE);
    LP_ASSERT_EQ(s.key_repeat_rate, 1);                   /* 0 would stop the compositor's repeat timer dividing */
    LP_ASSERT_EQ(s.key_repeat_delay, 2000);
    LP_ASSERT_NEAR(s.pointer_speed, 1, 1e-6);
    LP_ASSERT(s.clock_24h);                               /* a key with digits in it */
    LP_ASSERT_STR(s.dock, "finder,settings");
    LP_ASSERT_STR(s.keyboard_layout, "");
    LP_ASSERT_STR(s.mary_voice, "en_paul_neutral");
    LP_ASSERT_EQ(lp_settings_save(&s), 0);
    lp_settings again = lp_settings_load();
    LP_ASSERT_EQ(again.key_repeat_delay, 2000);
    LP_ASSERT_STR(again.dock, "finder,settings");
    LP_ASSERT_STR(again.mary_voice, "en_paul_neutral");
    const char *odd = "mary_voice=../../etc\n";                         /* not a voice: the default stands */
    lp_files_write(path, odd, strlen(odd));
    LP_ASSERT_STR(lp_settings_load().mary_voice, "fr_marie_neutral");
    unlink(path);
}

LP_TEST(mary_hands_the_key_over_once_and_keeps_no_copy) {
    char id[12];
    void *p = open_prefs(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_MARY);
    LP_ASSERT_EQ(lp_prefs_pane(p), LP_PREFS_MARY);
    size_t n = 0;
    const char *bytes;
    int clean = 1;
    lp_prefs_mary_set_key_text(p, "abcDEF1234567890ghij");
    lp_prefs_mary_save_key(p);                                  /* maryd is away */
    LP_ASSERT(strstr(lp_prefs_mary_status(p), "isn’t running") != NULL);
    bytes = lp_prefs_mary_key_bytes(p, &n);
    for (size_t i = 0; i < n; i++) clean &= bytes[i] == 0;
    LP_ASSERT(clean && n >= 64);
    if (lp_mary_available()) {
        int sv[2];
        LP_ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
        d.mary.fd = sv[0];
        lp_prefs_mary_set_key_text(p, "not a key!");
        lp_prefs_mary_save_key(p);
        LP_ASSERT(strstr(lp_prefs_mary_status(p), "doesn’t look like") != NULL);
        lp_prefs_mary_set_key_text(p, "abcDEF1234567890ghij");
        lp_prefs_mary_save_key(p);
        LP_ASSERT(strstr(lp_prefs_mary_status(p), "Saved") != NULL);
        char buf[256];
        ssize_t got = recv(sv[1], buf, sizeof buf - 1, MSG_DONTWAIT);
        buf[got > 0 ? got : 0] = 0;
        LP_ASSERT(strstr(buf, "\"type\":\"key.set\"") && strstr(buf, "abcDEF1234567890ghij"));
        LP_ASSERT(strstr(buf, "not a key") == NULL);
        bytes = lp_prefs_mary_key_bytes(p, &n);
        clean = 1;
        for (size_t i = 0; i < n; i++) clean &= bytes[i] == 0;
        LP_ASSERT(clean);
        lp_mary_free(&d.mary);
        close(sv[1]);
    }
    lp_desktop_close_window(&d, id);
}

static void pass(void *p, lp_input in, double now) {
    static lp_ctx ctx;
    ctx.settings = &d.settings;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 800, 300), now);
    lp_app_prefs.paint(p, &ctx, LP_RECT(0, 0, 800, 300), &d);
    lp_ctx_end(&ctx);
}

static int fake_displays(lp_desktop *desk, lp_display *out, int max) {
    if (max < 1) return 0;
    *out = (lp_display){ "Virtual-1", "Apple Virtualization display", 1280, 800, 60, 1 };
    return 1;
}

struct laid_out {
    int n;
    struct { enum lp_prefs_part part; lp_rect r; char text[48]; } parts[256];
};

static void on_part(enum lp_prefs_part part, lp_rect r, const char *text, void *user) {
    struct laid_out *t = user;
    if (t->n >= 256) return;
    t->parts[t->n].part = part;
    t->parts[t->n].r = r;
    snprintf(t->parts[t->n].text, sizeof t->parts[0].text, "%s", text ? text : "");
    t->n++;
}

static int off(float a, float b) { return a - b > 0.5f || b - a > 0.5f; }

LP_TEST(every_pane_lays_out_in_two_flush_left_columns) {
    char id[12];
    void *p = open_prefs(id);
    d.displays = fake_displays;
    const lp_audio_device speakers[1] = { { .id = 41, .name = "alsa_output.virtio", .description = "VirtIO SoundCard", .kind = "Virtual" } };
    d.audio.connected = 1;
    lp_audio_set_devices(&d.audio, LP_AUDIO_OUTPUT, speakers, 1, speakers[0].name);
    lp_audio_set_devices(&d.audio, LP_AUDIO_INPUT, speakers, 1, speakers[0].name);
    static struct laid_out t;
    lp_prefs_trace(p, on_part, &t);
    for (int pane = 0; pane <= LP_PREFS_MARY; pane++) {
        lp_desktop_run_command(&d, LP_CMD_APP, pane);
        if (pane == LP_PREFS_SOUND) {
            finish(0, "Volume: 0.40\n");
            finish(0, "Volume: 0.50\n");
        }
        if (pane == LP_PREFS_NETWORK) {
            finish(0, "enp0s1 UP 192.168.64.2/24\nwlan0 UP 10.0.0.5/24\n");
            finish(0, "");
            finish(0, "  Network name   Security   Signal\n----\n  > Home     psk    ****\n    Cafe     open   **\n");
        }
        if (pane == LP_PREFS_TIME) finish(0, "Timezone=Europe/Paris\nNTP=yes\n");
        t.n = 0;
        pass(p, (lp_input){ .mx = -1, .my = -1 }, 3000 + pane);
        float first = -1;
        int groups = 0, controls = 0;
        for (int i = 0; i < t.n; i++) if (t.parts[i].part == LP_PREFS_PART_HEADING) first = t.parts[i].r.x;
        if (first < 0) { LP_FAIL("pane %d has no heading", pane); continue; }
        float second = first + LP_PREFS_LABEL_W;
        for (int i = 0; i < t.n; i++) {
            float at = t.parts[i].r.x;
            const char *text = t.parts[i].text;
            switch (t.parts[i].part) {
            case LP_PREFS_PART_HEADING:
                break;
            case LP_PREFS_PART_SECTION:
                groups++;
                if (off(at, first)) LP_FAIL("pane %d: the group \"%s\" starts at %.1f, not %.1f", pane, text, at, first);
                break;
            case LP_PREFS_PART_LABEL:
                if (off(at, first)) LP_FAIL("pane %d: the label \"%s\" starts at %.1f, not %.1f", pane, text, at, first);
                if (t.parts[i].r.w > LP_PREFS_LABEL_W - LP_SPACE_3 + 0.5f) LP_FAIL("pane %d: the label \"%s\" reaches the controls", pane, text);
                break;
            case LP_PREFS_PART_CONTROL:
            case LP_PREFS_PART_TABLE:
                controls++;
                if (off(at, second)) LP_FAIL("pane %d: a control (%s) starts at %.1f, not %.1f", pane, text, at, second);
                break;
            case LP_PREFS_PART_NOTE:
                if (off(at, first) && off(at, second)) LP_FAIL("pane %d: the note \"%s\" starts at %.1f", pane, text, at);
                break;
            }
        }
        if (!groups) LP_FAIL("pane %d has no groups", pane);
        if (!controls) LP_FAIL("pane %d has no controls", pane);
    }
    lp_prefs_trace(p, NULL, NULL);
    d.displays = NULL;
    LP_ASSERT_EQ(lp_prefs_pane_named("sound"), LP_PREFS_SOUND);
    LP_ASSERT_EQ(lp_prefs_pane_named("Date & Time"), LP_PREFS_TIME);
    LP_ASSERT_EQ(lp_prefs_pane_named("bluetooth"), -1);
    LP_ASSERT_STR(lp_prefs_pane_slug(LP_PREFS_MARY), "mary");
    LP_ASSERT(lp_prefs_pane_slug(LP_PREFS_MARY + 1) == NULL);
    lp_desktop_close_window(&d, id);
}

LP_TEST(a_pane_taller_than_the_window_scrolls) {
    char id[12];
    void *p = open_prefs(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_MARY);
    pass(p, (lp_input){ .mx = 500, .my = 150 }, 1000);                     /* lays the pane out once */
    LP_ASSERT_NEAR(lp_prefs_scroll(p), 0, 0.01);
    pass(p, (lp_input){ .mx = 500, .my = 150, .scroll_y = 120 }, 1010);
    LP_ASSERT_NEAR(lp_prefs_scroll(p), 120, 0.01);
    pass(p, (lp_input){ .mx = 500, .my = 150, .scroll_y = 100000 }, 1020);
    LP_ASSERT(lp_prefs_scroll(p) > 120 && lp_prefs_scroll(p) < 1000);    /* it stops at the pane's end */
    pass(p, (lp_input){ .mx = 90, .my = 150, .scroll_y = -50 }, 1030);    /* the sidebar does not scroll it */
    LP_ASSERT(lp_prefs_scroll(p) > 120);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_ABOUT);
    LP_ASSERT_NEAR(lp_prefs_scroll(p), 0, 0.01);
    lp_desktop_close_window(&d, id);
}

LP_TEST(sound_lists_the_devices_and_switches_the_default) {
    char id[12];
    void *p = open_prefs(id);
    const lp_audio_device outputs[2] = {
        { .id = 41, .name = "alsa_output.virtio", .description = "VirtIO SoundCard", .kind = "Virtual" },
        { .id = 52, .name = "bluez_output.aa", .description = "AirPods", .kind = "Bluetooth" },
    };
    const lp_audio_device inputs[1] = { { .id = 60, .name = "alsa_input.virtio", .description = "VirtIO SoundCard", .kind = "Virtual" } };
    d.audio.connected = 1;
    lp_audio_set_devices(&d.audio, LP_AUDIO_OUTPUT, outputs, 2, "alsa_output.virtio");
    lp_audio_set_devices(&d.audio, LP_AUDIO_INPUT, inputs, 1, "alsa_input.virtio");
    LP_ASSERT_EQ(runs, 0);                                     /* General does not show them */
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_SOUND);
    LP_ASSERT(lp_prefs_metering(p));
    LP_ASSERT_EQ(d.audio.leases, 1);
    finish(0, "Volume: 0.40\n");
    finish(0, "Volume: 0.50\n");
    int before = runs;
    lp_prefs_choose_device(p, 0, 0);                          /* already the default */
    LP_ASSERT_EQ(runs, before);
    lp_prefs_choose_device(p, 0, 1);
    LP_ASSERT_STR(last_run(), "wpctl set-default 52");
    finish(1, "Object 52 not found\n");
    LP_ASSERT_STR(lp_prefs_message(p), "Could not switch to “AirPods”: Object 52 not found");
    lp_prefs_choose_device(p, 0, 1);
    finish(0, "");
    LP_ASSERT_STR(last_run(), "wpctl get-volume @DEFAULT_AUDIO_SINK@");   /* the new default's volume */
    finish(0, "Volume: 0.30\n");
    finish(0, "Volume: 0.50\n");
    before = runs;
    lp_audio_set_devices(&d.audio, LP_AUDIO_OUTPUT, outputs, 2, "bluez_output.aa");   /* WirePlumber moves it */
    LP_ASSERT_EQ(runs, before + 1);
    LP_ASSERT_STR(last_run(), "wpctl get-volume @DEFAULT_AUDIO_SINK@");
    finish(0, "Volume: 0.30\n");
    finish(0, "Volume: 0.50\n");
    lp_prefs_set_input_volume(p, 80);
    LP_ASSERT_STR(last_run(), "wpctl set-volume @DEFAULT_AUDIO_SOURCE@ 0.80");
    lp_prefs_set_input_volume(p, 85);                         /* while that runs: queued */
    finish(0, "");
    LP_ASSERT_STR(last_run(), "wpctl set-volume @DEFAULT_AUDIO_SOURCE@ 0.85");
    finish(0, "");
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_ABOUT);
    LP_ASSERT(!lp_prefs_metering(p));
    LP_ASSERT_EQ(d.audio.leases, 0);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_SOUND);
    finish(1, "Could not connect to PipeWire\n");            /* only the speaker's volume is asked for then */
    LP_ASSERT_EQ(lp_prefs_volume(p), -1);
    lp_desktop_close_window(&d, id);
    LP_ASSERT_EQ(d.audio.leases, 0);                          /* closing gives the lease back */
}

LP_TEST(sound_says_when_there_is_no_microphone) {
    char id[12];
    void *p = open_prefs(id);
    const lp_audio_device outputs[1] = { { .id = 41, .name = "alsa_output.virtio", .description = "VirtIO SoundCard", .kind = "Virtual" } };
    d.audio.connected = 1;
    lp_audio_set_devices(&d.audio, LP_AUDIO_OUTPUT, outputs, 1, outputs[0].name);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_SOUND);
    finish(0, "Volume: 0.40\n");
    finish(1, "Translate ID error: '@DEFAULT_AUDIO_SOURCE@' is not a valid ID\n");
    LP_ASSERT_EQ(lp_prefs_input_volume(p), -1);
    static struct laid_out t;
    lp_prefs_trace(p, on_part, &t);
    t.n = 0;
    pass(p, (lp_input){ .mx = -1, .my = -1 }, 4000);
    int said = 0, metered = 0, tables = 0;
    for (int i = 0; i < t.n; i++) {
        said |= t.parts[i].part == LP_PREFS_PART_NOTE && strstr(t.parts[i].text, "No microphone") != NULL;
        metered |= t.parts[i].part == LP_PREFS_PART_LABEL && strcmp(t.parts[i].text, "Level") == 0;
        tables += t.parts[i].part == LP_PREFS_PART_TABLE;
    }
    LP_ASSERT(said && !metered);
    LP_ASSERT_EQ(tables, 1);                                  /* the speakers' table, and no microphones' */
    lp_prefs_trace(p, NULL, NULL);
    lp_desktop_close_window(&d, id);
}

static void give_voices(void) {
    static const struct { const char *id, *name, *language; int custom; } V[] = {
        { "fr_marie_neutral", "Marie", "fr", 0 }, { "fr_marie_happy", "Marie", "fr", 0 }, { "en_paul_neutral", "Paul", "en", 0 },
        { "en_paul_sad", "Paul", "en", 0 }, { "gb_jane_neutral", "Jane", "en", 0 }, { "019b2bd7-96e7-7219", "My voice", "fr", 1 },
    };
    d.mary.voice_count = 0;
    for (size_t i = 0; i < sizeof V / sizeof *V; i++) {
        lp_mary_voice *v = &d.mary.voices[d.mary.voice_count++];
        memset(v, 0, sizeof *v);
        snprintf(v->id, sizeof v->id, "%s", V[i].id);
        snprintf(v->name, sizeof v->name, "%s", V[i].name);
        snprintf(v->language, sizeof v->language, "%s", V[i].language);
        v->custom = V[i].custom;
    }
    d.mary.voices_state = LP_MARY_VOICES_LISTED;
}

static int entry_named(const char *label) {
    const lp_menu_model *menu = &d.menus[LP_DESKTOP_MENU_POPUP];
    for (int i = 0; i < menu->count; i++) if (strcmp(menu->entries[i].label, label) == 0) return i;
    return -1;
}

static const char *heard(int fd) {
    static char buf[4096];
    size_t n = 0;
    ssize_t got;
    while (n < sizeof buf - 1 && (got = recv(fd, buf + n, sizeof buf - 1 - n, MSG_DONTWAIT)) > 0) n += (size_t)got;
    buf[n] = 0;
    return buf;
}

LP_TEST(choosing_a_voice_saves_it_and_tells_maryd) {
    char id[12];
    void *p = open_prefs(id);
    lp_desktop_run_command(&d, LP_CMD_APP, LP_PREFS_MARY);
    if (!lp_mary_available()) {
        lp_desktop_close_window(&d, id);
        return;
    }
    int sv[2];
    LP_ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    d.mary.fd = sv[0];
    d.mary.key_present = 1;
    give_voices();
    LP_ASSERT_STR(d.settings.mary_voice, "fr_marie_neutral");
    lp_prefs_open_voice_menu(p, 0);
    LP_ASSERT_EQ(d.open_menu, LP_DESKTOP_MENU_POPUP);
    const lp_menu_model *menu = &d.menus[LP_DESKTOP_MENU_POPUP];
    LP_ASSERT_STR(menu->entries[0].label, "Marie — French");
    LP_ASSERT(menu->entries[0].checked);
    int jane = entry_named("Jane — English"), paul = entry_named("Paul — English"), yours = entry_named("Your voices"), mine = entry_named("My voice — French");
    LP_ASSERT(jane > 0 && paul > jane && yours > paul && mine > yours);   /* by language and name, the account's own last */
    LP_ASSERT(yours > 0 && menu->entries[yours].disabled);
    settings_calls = 0;
    lp_desktop_select_menu_entry(&d, paul);
    LP_ASSERT_STR(d.settings.mary_voice, "en_paul_neutral");            /* the mood carries over when the voice has it */
    LP_ASSERT(settings_calls >= 1);
    const char *out = heard(sv[1]);
    LP_ASSERT(strstr(out, "\"type\":\"config\"") && strstr(out, "\"voice\":\"en_paul_neutral\""));
    LP_ASSERT_STR(lp_settings_load().mary_voice, "en_paul_neutral");

    lp_prefs_open_voice_menu(p, 1);
    LP_ASSERT_EQ(d.menus[LP_DESKTOP_MENU_POPUP].count, 2);            /* Paul has Neutral and Sad */
    int sad = entry_named("Sad"), neutral = entry_named("Neutral");
    LP_ASSERT(sad >= 0 && neutral >= 0 && d.menus[LP_DESKTOP_MENU_POPUP].entries[neutral].checked);
    lp_desktop_select_menu_entry(&d, sad);
    LP_ASSERT_STR(d.settings.mary_voice, "en_paul_sad");

    lp_prefs_open_voice_menu(p, 0);
    jane = entry_named("Jane — English");
    d.mary.voice_count = 1;                                            /* the list shrank while the menu was open */
    lp_desktop_select_menu_entry(&d, jane);
    LP_ASSERT_STR(d.settings.mary_voice, "gb_jane_neutral");           /* still what the menu showed: Jane has no Sad */
    give_voices();
    lp_prefs_open_voice_menu(p, 0);
    lp_desktop_select_menu_entry(&d, 0);
    LP_ASSERT_STR(d.settings.mary_voice, "fr_marie_neutral");
    lp_prefs_open_voice_menu(p, 1);
    LP_ASSERT_EQ(d.menus[LP_DESKTOP_MENU_POPUP].count, 6);            /* Marie's six moods */
    lp_desktop_close_menu(&d);

    heard(sv[1]);
    lp_prefs_mary_play_sample(p);
    out = heard(sv[1]);
    LP_ASSERT(strstr(out, "\"type\":\"voice.sample\"") && strstr(out, "\"voice_id\":\"fr_marie_neutral\"") && strstr(out, "Bonjour"));
    d.mary.sample_state = LP_MARY_SAMPLE_PLAYING;
    lp_prefs_mary_play_sample(p);                                      /* pressed again while it speaks: Stop */
    LP_ASSERT(strstr(heard(sv[1]), "\"type\":\"stop\"") != NULL);
    snprintf(d.settings.mary_voice, sizeof d.settings.mary_voice, "en_paul_neutral");
    d.mary.sample_state = LP_MARY_SAMPLE_DONE;
    lp_prefs_mary_play_sample(p);
    LP_ASSERT(strstr(heard(sv[1]), "Hello! This is how I will sound.") != NULL);   /* in the voice's language */
    lp_mary_free(&d.mary);
    close(sv[1]);
    snprintf(d.settings.mary_voice, sizeof d.settings.mary_voice, "fr_marie_neutral");
    lp_desktop_settings_changed(&d);
    lp_desktop_close_window(&d, id);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_prefs_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    char config[600];
    snprintf(config, sizeof config, "%s/config", root);
    setenv("XDG_CONFIG_HOME", config, 1);
    setenv("XDG_DATA_HOME", root, 1);
    LP_RUN(network_reads_the_links_then_scans_the_wifi);
    LP_RUN(sound_reads_and_sets_the_volume);
    LP_RUN(date_and_time_reads_the_zone_and_refuses_one_that_does_not_exist);
    LP_RUN(about_refuses_a_name_a_computer_cannot_have);
    LP_RUN(keyboard_settings_are_saved_and_applied);
    LP_RUN(the_dock_pane_changes_what_spotlight_shows);
    LP_RUN(every_pane_paints);
    LP_RUN(mary_hands_the_key_over_once_and_keeps_no_copy);
    LP_RUN(a_pane_taller_than_the_window_scrolls);
    LP_RUN(every_pane_lays_out_in_two_flush_left_columns);
    LP_RUN(sound_lists_the_devices_and_switches_the_default);
    LP_RUN(sound_says_when_there_is_no_microphone);
    LP_RUN(choosing_a_voice_saves_it_and_tells_maryd);
    LP_RUN(settings_keep_their_defaults_and_bounds);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
