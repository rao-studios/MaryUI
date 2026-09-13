/* The desktop's view of PipeWire's speakers and microphones (PARITY D23): the default metadata read, a device's
 * kind told from its names, the meter's scale and its release, leases on the meter, lists set as PipeWire would
 * report them repainting only a window that shows them (and Mary's pane repainting when her key is checked), and
 * a start with no PipeWire to reach failing cleanly and trying again. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lp_test.h"
#include "lp_test_loop.h"
#include "maryui/lp_audio.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"
#include "maryui/lp_job.h"

LP_TEST(the_default_metadata_names_a_node) {
    char name[128];
    LP_ASSERT_EQ(lp_audio_parse_default("{\"name\":\"alsa_output.pci-0000_00_1f.3.analog-stereo\"}", name, sizeof name), 0);
    LP_ASSERT_STR(name, "alsa_output.pci-0000_00_1f.3.analog-stereo");
    LP_ASSERT_EQ(lp_audio_parse_default("{ \"name\" :\t\"odd \\\"one\\\"\" }", name, sizeof name), 0);
    LP_ASSERT_STR(name, "odd \"one\"");
    LP_ASSERT_EQ(lp_audio_parse_default("{\"name\":\"\"}", name, sizeof name), -EINVAL);
    LP_ASSERT_EQ(lp_audio_parse_default("{\"device\":\"x\"}", name, sizeof name), -EINVAL);
    LP_ASSERT_EQ(lp_audio_parse_default("{\"name\":\"unterminated", name, sizeof name), -EINVAL);
    LP_ASSERT_EQ(lp_audio_parse_default(NULL, name, sizeof name), -EINVAL);
    char tiny[4];
    LP_ASSERT_EQ(lp_audio_parse_default("{\"name\":\"longer\"}", tiny, sizeof tiny), -ENAMETOOLONG);
}

LP_TEST(a_devices_kind_is_read_from_its_names) {
    LP_ASSERT_STR(lp_audio_kind_of("alsa_input.usb-Blue_Microphones_Yeti-00.analog-stereo", "Yeti Stereo Microphone"), "USB");
    LP_ASSERT_STR(lp_audio_kind_of("bluez_output.AA_BB_CC.1", "AirPods"), "Bluetooth");
    LP_ASSERT_STR(lp_audio_kind_of("alsa_output.platform-107c706400.hdmi.hdmi-stereo", "Built-in Audio Digital Stereo (HDMI)"), "HDMI");
    LP_ASSERT_STR(lp_audio_kind_of("alsa_output.platform-a003e00.virtio_mmio.analog-stereo", "VirtIO SoundCard Analog Stereo"), "Virtual");
    LP_ASSERT_STR(lp_audio_kind_of("alsa_output.pci-0000_00_1f.3.analog-stereo", "Built-in Audio Analog Stereo"), "Built-in");
    LP_ASSERT_STR(lp_audio_kind_of(NULL, NULL), "Built-in");
}

LP_TEST(the_meter_reads_decibels_and_falls_back_over_300_ms) {
    LP_ASSERT_NEAR(lp_audio_meter_value(1.0f), 1, 1e-6);
    LP_ASSERT_NEAR(lp_audio_meter_value(-1.0f), 1, 1e-6);
    LP_ASSERT_NEAR(lp_audio_meter_value(0.0316228f), 0.5, 1e-3);     /* -30 dBFS */
    LP_ASSERT_NEAR(lp_audio_meter_value(0.001f), 0, 1e-6);           /* -60 dBFS and below */
    LP_ASSERT_NEAR(lp_audio_meter_value(0), 0, 1e-6);
    float shown = lp_audio_release(0, 1.0f, 0.02f);
    LP_ASSERT_NEAR(shown, 1, 1e-6);                                  /* up at once */
    shown = lp_audio_release(shown, 0, 0.15f);
    LP_ASSERT_NEAR(shown, 0.5, 1e-3);                                /* half-way down after 150 ms of quiet */
    shown = lp_audio_release(shown, 0.0316228f, 0.02f);
    LP_ASSERT_NEAR(shown, 0.5, 1e-3);                                /* a sound as loud as the needle holds it */
    LP_ASSERT_NEAR(lp_audio_release(shown, 0, 1.0f), 0, 1e-6);
}

LP_TEST(the_meter_runs_while_anyone_watches) {
    lp_audio a;
    lp_audio_init(&a, NULL);
    lp_audio_meter(&a, 1);
    lp_audio_meter(&a, 1);
    LP_ASSERT_EQ(a.leases, 2);
    lp_audio_set_level(&a, 0.7f);
    LP_ASSERT_NEAR(lp_audio_level(&a), 0.7, 1e-6);
    lp_audio_meter(&a, 0);
    LP_ASSERT_EQ(a.leases, 1);
    LP_ASSERT_NEAR(lp_audio_level(&a), 0.7, 1e-6);
    lp_audio_meter(&a, 0);
    lp_audio_meter(&a, 0);                                          /* never fewer than none */
    LP_ASSERT_EQ(a.leases, 0);
    LP_ASSERT_NEAR(lp_audio_level(&a), 0, 1e-6);
    lp_audio_set_level(&a, 3);
    LP_ASSERT_NEAR(lp_audio_level(&a), 1, 1e-6);
    lp_audio_free(&a);
}

static lp_desktop d;
static int dirtied;
static char dirty_window[12];

static void on_dirty(lp_desktop *desk, const char *window_id) {
    dirtied++;
    snprintf(dirty_window, sizeof dirty_window, "%s", window_id);
}

static lp_job *no_job(lp_desktop *desk, const char *const *argv, void (*done)(int, const char *, void *), void *user) { return NULL; }

LP_TEST(devices_repaint_only_a_window_that_shows_them) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    d.on_app_dirty = on_dirty;
    char settings[12], calculator[12];
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "settings", NULL, NULL, settings), 1);
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "calculator", NULL, NULL, calculator), 1);
    void *p = lp_desktop_instance(&d, settings)->state;
    lp_prefs_set_runner(p, no_job);

    const lp_audio_device outputs[2] = {
        { .id = 41, .serial = 41, .name = "alsa_output.platform-a003e00.virtio_mmio.analog-stereo", .description = "VirtIO SoundCard", .kind = "Virtual" },
        { .id = 52, .serial = 90, .name = "bluez_output.AA_BB_CC.1", .description = "AirPods", .kind = "Bluetooth" },
    };
    dirtied = 0;
    lp_audio_set_devices(&d.audio, LP_AUDIO_OUTPUT, outputs, 2, "bluez_output.AA_BB_CC.1");
    LP_ASSERT_EQ(dirtied, 0);                                        /* Settings shows General, which shows no devices */
    LP_ASSERT_STR(lp_audio_default(&d.audio, LP_AUDIO_OUTPUT)->description, "AirPods");
    LP_ASSERT(lp_audio_default(&d.audio, LP_AUDIO_INPUT) == NULL);   /* no microphone */

    lp_app_prefs.command(p, &d, LP_PREFS_SOUND);
    lp_audio_set_devices(&d.audio, LP_AUDIO_OUTPUT, outputs, 1, outputs[0].name);
    LP_ASSERT_EQ(dirtied, 1);
    LP_ASSERT_STR(dirty_window, settings);
    LP_ASSERT_EQ(d.audio.count[LP_AUDIO_OUTPUT], 1);
    LP_ASSERT(lp_audio_is_default(&d.audio, LP_AUDIO_OUTPUT, &d.audio.devices[LP_AUDIO_OUTPUT][0]));
    lp_audio_set_devices(&d.audio, LP_AUDIO_OUTPUT, outputs, 1, outputs[0].name);
    LP_ASSERT_EQ(dirtied, 1);                                        /* the same again changes nothing */

    lp_desktop_close_window(&d, calculator);
    lp_desktop_close_window(&d, settings);
}

LP_TEST(the_mary_pane_repaints_when_the_key_is_checked) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    d.on_app_dirty = on_dirty;
    char settings[12];
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "settings", NULL, NULL, settings), 1);
    void *p = lp_desktop_instance(&d, settings)->state;
    lp_prefs_set_runner(p, no_job);
    static const char status[] = "{\"type\":\"key.status\",\"present\":true,\"ok\":true}\n";
    static const char level[] = "{\"type\":\"level\",\"rms\":0.3}\n";
    lp_app_prefs.command(p, &d, LP_PREFS_MARY);
    dirtied = 0;
    lp_mary_feed(&d.mary, status, sizeof status - 1);
    lp_mary_feed(&d.mary, level, sizeof level - 1);                  /* the microphone's level repaints Spotlight only */
    LP_ASSERT_EQ(dirtied, lp_mary_available() ? 1 : 0);
    lp_app_prefs.command(p, &d, LP_PREFS_ABOUT);
    dirtied = 0;
    lp_mary_feed(&d.mary, status, sizeof status - 1);
    LP_ASSERT_EQ(dirtied, 0);
    lp_desktop_close_window(&d, settings);
}

LP_TEST(starting_with_no_pipewire_to_reach_fails_cleanly) {
    lp_audio a;
    lp_audio_init(&a, NULL);
    LP_ASSERT_EQ(lp_audio_start(&a), -ENOSYS);                       /* no event loop to live on */

    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&d);
    char dir[64];
    snprintf(dir, sizeof dir, "/tmp/lp-audio-XXXXXX");
    LP_ASSERT(mkdtemp(dir) != NULL);
    setenv("PIPEWIRE_RUNTIME_DIR", dir, 1);                          /* a runtime directory with no PipeWire in it */
    unsetenv("PIPEWIRE_REMOTE");
    int rc = lp_audio_start(&d.audio);
    lp_test_loop_run(50, NULL);                                      /* the loop turns; nothing is there to find */
    if (!lp_audio_available()) {
        LP_ASSERT_EQ(rc, -ENOSYS);
    } else {
        LP_ASSERT_EQ(rc, 0);
        LP_ASSERT(!d.audio.connected && d.audio.backend == NULL);
        LP_ASSERT(d.audio.retry != NULL);                            /* it will look again */
    }
    lp_audio_free(&d.audio);
    LP_ASSERT(d.audio.retry == NULL && d.audio.wake_source == NULL && d.audio.backend == NULL);
    rmdir(dir);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    char root[512], config[600];
    snprintf(root, sizeof root, "%s/lp_audio_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    snprintf(config, sizeof config, "%s/config", root);
    setenv("XDG_CONFIG_HOME", config, 1);
    setenv("XDG_DATA_HOME", root, 1);
    LP_RUN(the_default_metadata_names_a_node);
    LP_RUN(a_devices_kind_is_read_from_its_names);
    LP_RUN(the_meter_reads_decibels_and_falls_back_over_300_ms);
    LP_RUN(the_meter_runs_while_anyone_watches);
    LP_RUN(devices_repaint_only_a_window_that_shows_them);
    LP_RUN(the_mary_pane_repaints_when_the_key_is_checked);
    LP_RUN(starting_with_no_pipewire_to_reach_fails_cleanly);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}
