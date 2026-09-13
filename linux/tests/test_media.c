/* The Media Player's engine and the app. Everywhere: time labels and a
 * folder's queue. With GStreamer: real clips made here — VP8 in WebM, a WAV,
 * an Ogg with tags — opened to their first frame, played to the end, sought,
 * read for tags, refused when damaged; and the app playing one clip and then
 * the next in its folder. Audio goes to a fake sink. Linux only (PARITY D15). */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_files.h"
#include "maryui/lp_media.h"

#ifdef HAVE_GST
#include <gst/gst.h>
#endif

static char root[512];

static void path_of(const char *name, char *out, size_t n) { snprintf(out, n, "%s/%s", root, name); }

static void touch(const char *name) {
    char path[700];
    path_of(name, path, sizeof path);
    FILE *f = fopen(path, "w");
    if (f) { fputs("x", f); fclose(f); }
}

LP_TEST(labels_time_the_way_players_do) {
    char s[32];
    lp_media_format_time(7.9, s, sizeof s);
    LP_ASSERT_STR(s, "0:07");
    lp_media_format_time(205, s, sizeof s);
    LP_ASSERT_STR(s, "3:25");
    lp_media_format_time(3723, s, sizeof s);
    LP_ASSERT_STR(s, "1:02:03");
    lp_media_format_time(-4, s, sizeof s);
    LP_ASSERT_STR(s, "0:00");
}

LP_TEST(queues_the_audio_and_video_beside_a_file) {
    char dir[700];
    path_of("album", dir, sizeof dir);
    mkdir(dir, 0755);
    touch("album/01 intro.mp3");
    touch("album/02 song.flac");
    touch("album/cover.jpg");
    touch("album/notes.txt");
    touch("album/03 video.mkv");
    char path[800];
    snprintf(path, sizeof path, "%s/02 song.flac", dir);
    char **queue;
    int at;
    int count = lp_media_queue(path, &queue, &at);
    LP_ASSERT_EQ(count, 3);
    LP_ASSERT_EQ(at, 1);
    LP_ASSERT_STR(lp_files_basename(queue[0]), "01 intro.mp3");
    LP_ASSERT_STR(lp_files_basename(queue[2]), "03 video.mkv");
    lp_media_queue_free(queue, count);
    LP_ASSERT(lp_media_kind_supported(LP_FILE_VIDEO) && !lp_media_kind_supported(LP_FILE_IMAGE));
}

#ifndef HAVE_GST

LP_TEST(says_the_build_has_no_gstreamer) {
    lp_media *m = NULL;
    char path[700];
    path_of("album/02 song.flac", path, sizeof path);
    LP_ASSERT_EQ(lp_media_available(), 0);
    LP_ASSERT_EQ(lp_media_open(&m, path, LP_MEDIA_SILENT), -ENOTSUP);
    LP_ASSERT(m == NULL);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_media_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    LP_RUN(labels_time_the_way_players_do);
    LP_RUN(queues_the_audio_and_video_beside_a_file);
    LP_RUN(says_the_build_has_no_gstreamer);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}

#else

#include "lp_test_loop.h"

/* Runs a gst-launch style pipeline to its end. 1 when it finished cleanly. */
static int make_clip(const char *description) {
    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch(description, &error);
    if (!pipeline) { fprintf(stderr, "  pipeline: %s\n", error ? error->message : "?"); if (error) g_error_free(error); return 0; }
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 20 * GST_SECOND, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
    int ok = msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
    if (msg) gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return ok;
}

/* Waits on the engine's wake fd until cond holds or ms pass. */
static int wait_until(lp_media *m, int (*cond)(lp_media *), int ms) {
    for (int waited = 0; waited < ms; waited += 20) {
        if (cond(m)) return 1;
        struct pollfd pfd = { .fd = lp_media_wake_fd(m), .events = POLLIN };
        poll(&pfd, 1, 20);
        lp_media_update(m);
    }
    return cond(m);
}
static int has_frame_and_paused(lp_media *m) { return lp_media_frame(m) && lp_media_state(m) == LP_MEDIA_PAUSED && lp_media_info_of(m)->duration > 0; }
static int prerolled(lp_media *m) { return lp_media_state(m) == LP_MEDIA_PAUSED && lp_media_info_of(m)->duration > 0; }
static int ended(lp_media *m) { return lp_media_state(m) == LP_MEDIA_ENDED; }
static int failed(lp_media *m) { return lp_media_state(m) == LP_MEDIA_FAILED; }
static int tagged(lp_media *m) { return strcmp(lp_media_info_of(m)->artist, "MaryOS") == 0; }

static char video[700], sound[700], tagged_clip[700];

LP_TEST(opens_a_video_paused_on_its_first_frame) {
    lp_media *m = NULL;
    LP_ASSERT_EQ(lp_media_open(&m, video, LP_MEDIA_SILENT), 0);
    LP_ASSERT(wait_until(m, has_frame_and_paused, 10000));
    const lp_media_info *info = lp_media_info_of(m);
    LP_ASSERT(info->has_video);
    LP_ASSERT_EQ(info->width, 64);
    LP_ASSERT_EQ(info->height, 48);
    LP_ASSERT_NEAR(info->duration, 2.0, 0.25);
    LP_ASSERT_STR(info->title, "clip");                  /* no tags: the file's name */
    cairo_surface_t *frame = lp_media_frame(m);
    LP_ASSERT(frame && cairo_image_surface_get_width(frame) == 64);
    lp_media_close(m);
}

LP_TEST(plays_to_the_end_and_again_from_the_start) {
    lp_media *m = NULL;
    LP_ASSERT_EQ(lp_media_open(&m, sound, LP_MEDIA_SILENT), 0);
    LP_ASSERT(wait_until(m, prerolled, 10000));
    lp_media_play(m);
    LP_ASSERT_EQ(lp_media_state(m), LP_MEDIA_PLAYING);
    LP_ASSERT(wait_until(m, ended, 15000));
    LP_ASSERT_NEAR(lp_media_position(m), lp_media_info_of(m)->duration, 0.01);
    lp_media_play(m);                                       /* again, from the top */
    LP_ASSERT_EQ(lp_media_state(m), LP_MEDIA_PLAYING);
    LP_ASSERT(lp_media_position(m) < 1.0);
    lp_media_close(m);
}

LP_TEST(seeks_and_sets_the_volume) {
    lp_media *m = NULL;
    LP_ASSERT_EQ(lp_media_open(&m, sound, LP_MEDIA_SILENT), 0);
    LP_ASSERT(wait_until(m, prerolled, 10000));
    lp_media_seek(m, 1.0);
    wait_until(m, prerolled, 2000);
    LP_ASSERT_NEAR(lp_media_position(m), 1.0, 0.2);
    lp_media_seek(m, 99);                                   /* clamped to the end */
    LP_ASSERT(lp_media_position(m) <= lp_media_info_of(m)->duration + 0.01);
    lp_media_set_volume(m, 1.7);
    LP_ASSERT_NEAR(lp_media_volume(m), 1.0, 1e-9);
    lp_media_set_volume(m, 0.25);
    LP_ASSERT_NEAR(lp_media_volume(m), 0.25, 1e-9);
    lp_media_close(m);
}

LP_TEST(reads_the_tags_a_file_carries) {
    lp_media *m = NULL;
    LP_ASSERT_EQ(lp_media_open(&m, tagged_clip, LP_MEDIA_SILENT), 0);
    LP_ASSERT(wait_until(m, tagged, 10000));
    LP_ASSERT_STR(lp_media_info_of(m)->title, "Test Tone");
    LP_ASSERT_STR(lp_media_info_of(m)->artist, "MaryOS");
    LP_ASSERT(!lp_media_info_of(m)->has_video);
    lp_media_close(m);
}

LP_TEST(fails_on_a_damaged_file_and_says_why) {
    char bad[700];
    path_of("damaged.mp4", bad, sizeof bad);
    FILE *f = fopen(bad, "w");
    fputs("this is not a movie", f);
    fclose(f);
    lp_media *m = NULL;
    int rc = lp_media_open(&m, bad, LP_MEDIA_SILENT);
    if (rc == 0) {
        LP_ASSERT(wait_until(m, failed, 10000));
        LP_ASSERT(lp_media_info_of(m)->error[0] != 0);
        lp_media_play(m);                                   /* a failed file stays failed */
        LP_ASSERT_EQ(lp_media_state(m), LP_MEDIA_FAILED);
        lp_media_close(m);
    } else {
        LP_ASSERT(rc < 0);
    }
    path_of("missing.mp4", bad, sizeof bad);
    LP_ASSERT_EQ(lp_media_open(&m, bad, LP_MEDIA_SILENT), -ENOENT);
}

static lp_desktop desk;
static void *player_state;
static int second_playing(void) {
    int count = 0;
    return lp_player_queue_position(player_state, &count) == 1 && lp_player_state(player_state) == LP_MEDIA_PLAYING;
}

LP_TEST(the_app_plays_on_into_the_next_in_its_folder) {
    char dir[700], first[800], second[800];
    path_of("playlist", dir, sizeof dir);
    mkdir(dir, 0755);
    snprintf(first, sizeof first, "%s/a.wav", dir);
    snprintf(second, sizeof second, "%s/b.wav", dir);
    char pipeline[2048];
    snprintf(pipeline, sizeof pipeline, "audiotestsrc num-buffers=4 samplesperbuffer=2000 ! audio/x-raw,rate=8000,channels=1 ! wavenc ! filesink location=\"%s\"", first);
    LP_ASSERT(make_clip(pipeline));
    snprintf(pipeline, sizeof pipeline, "audiotestsrc num-buffers=40 samplesperbuffer=2000 ! audio/x-raw,rate=8000,channels=1 ! wavenc ! filesink location=\"%s\"", second);
    LP_ASSERT(make_clip(pipeline));
    lp_desktop_init(&desk, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&desk);
    lp_desktop_register_builtin_apps(&desk);
    LP_ASSERT_EQ(lp_desktop_open_path(&desk, first), 1);
    const lp_window_record *w = &desk.wm.windows[desk.wm.count - 1];
    LP_ASSERT_STR(w->app_id, "media");
    char id[12];
    snprintf(id, sizeof id, "%s", w->id);
    player_state = lp_desktop_instance(&desk, id)->state;
    int count = 0;
    LP_ASSERT_EQ(lp_player_queue_position(player_state, &count), 0);
    LP_ASSERT_EQ(count, 2);
    lp_test_loop_run(15000, second_playing);
    LP_ASSERT(second_playing());
    LP_ASSERT_STR(lp_files_basename(lp_player_path(player_state)), "b.wav");
    lp_desktop_close_window(&desk, id);
}

int main(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof root, "%s/lp_media_XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(root)) return 1;
    char config[600];
    snprintf(config, sizeof config, "%s/config", root);
    setenv("XDG_CONFIG_HOME", config, 1);
    setenv("MARYUI_MEDIA_SILENT", "1", 1);
    gst_init(NULL, NULL);
    path_of("clip.webm", video, sizeof video);
    path_of("tone.wav", sound, sizeof sound);
    path_of("tagged.ogg", tagged_clip, sizeof tagged_clip);
    char pipeline[2048];
    snprintf(pipeline, sizeof pipeline, "videotestsrc num-buffers=20 ! video/x-raw,width=64,height=48,framerate=10/1 ! vp8enc ! webmmux ! filesink location=\"%s\"", video);
    int clips = make_clip(pipeline);
    snprintf(pipeline, sizeof pipeline, "audiotestsrc num-buffers=12 samplesperbuffer=2000 ! audio/x-raw,rate=8000,channels=1 ! wavenc ! filesink location=\"%s\"", sound);
    clips &= make_clip(pipeline);
    snprintf(pipeline, sizeof pipeline, "audiotestsrc num-buffers=8 ! audioconvert ! vorbisenc ! oggmux ! filesink location=\"%s\"", tagged_clip);
    GstElement *probe = gst_element_factory_make("taginject", NULL);
    if (probe) {
        gst_object_unref(probe);
        snprintf(pipeline, sizeof pipeline, "audiotestsrc num-buffers=8 ! taginject tags=\"title=\\\"Test Tone\\\",artist=MaryOS\" ! audioconvert ! vorbisenc ! oggmux ! filesink location=\"%s\"", tagged_clip);
    }
    clips &= make_clip(pipeline);
    if (!clips) { fprintf(stderr, "could not make the test clips\n"); return 1; }
    LP_RUN(labels_time_the_way_players_do);
    LP_RUN(queues_the_audio_and_video_beside_a_file);
    LP_RUN(opens_a_video_paused_on_its_first_frame);
    LP_RUN(plays_to_the_end_and_again_from_the_start);
    LP_RUN(seeks_and_sets_the_volume);
    if (probe) LP_RUN(reads_the_tags_a_file_carries);
    LP_RUN(fails_on_a_damaged_file_and_says_why);
    LP_RUN(the_app_plays_on_into_the_next_in_its_folder);
    lp_files_delete_tree(root);
    LP_TEST_MAIN_END();
}

#endif
