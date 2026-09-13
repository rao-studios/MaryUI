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
    lp_prefs_set_volume(p, 70);
    LP_ASSERT_STR(last_run(), "wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.70");
    lp_prefs_set_volume(p, 75);                                /* while that runs: queued */
    lp_prefs_set_volume(p, 80);
    LP_ASSERT_EQ(runs, 2);
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
    for (int pane = 0; pane <= LP_PREFS_ABOUT; pane++) {
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
    char path[700];
    snprintf(path, sizeof path, "%s/config/maryui", root);
    lp_files_mkdir_p(path);
    snprintf(path, sizeof path, "%s/config/maryui/settings.conf", root);
    const char *old = "accent=graphite\nclock=on\nkey_repeat_rate=0\nkey_repeat_delay=99999\npointer_speed=5\nclock_24h=on\ndock=finder,settings\n";
    lp_files_write(path, old, strlen(old));
    lp_settings s = lp_settings_load();
    LP_ASSERT_EQ(s.accent, LP_ACCENT_GRAPHITE);
    LP_ASSERT_EQ(s.key_repeat_rate, 1);                   /* 0 would stop the compositor's repeat timer dividing */
    LP_ASSERT_EQ(s.key_repeat_delay, 2000);
    LP_ASSERT_NEAR(s.pointer_speed, 1, 1e-6);
    LP_ASSERT(s.clock_24h);                               /* a key with digits in it */
    LP_ASSERT_STR(s.dock, "finder,settings");
    LP_ASSERT_STR(s.keyboard_layout, "");
    LP_ASSERT_EQ(lp_settings_save(&s), 0);
    lp_settings again = lp_settings_load();
    LP_ASSERT_EQ(again.key_repeat_delay, 2000);
    LP_ASSERT_STR(again.dock, "finder,settings");
    unlink(path);
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
    LP_RUN(settings_keep_their_defaults_and_bounds);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
