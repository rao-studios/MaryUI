/* The Media Player's engine (lp_media.h). */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "maryui/lp_image.h"
#include "maryui/lp_media.h"

void lp_media_format_time(double seconds, char *out, size_t n) {
    if (!(seconds >= 0)) seconds = 0;   /* NaN as well */
    long total = (long)floor(seconds + 1e-4);
    long h = total / 3600, m = total / 60 % 60, s = total % 60;
    if (h) snprintf(out, n, "%ld:%02ld:%02ld", h, m, s);
    else snprintf(out, n, "%ld:%02ld", m, s);
}

int lp_media_kind_supported(enum lp_file_kind kind) { return kind == LP_FILE_MUSIC || kind == LP_FILE_VIDEO; }

int lp_media_queue(const char *path, char ***out, int *at) {
    *out = NULL;
    if (at) *at = -1;
    char dir[LP_FILES_PATH_MAX];
    lp_files_parent(path, dir, sizeof dir);
    const char *name = lp_files_basename(path);
    lp_file_list l = { 0 };
    int count = 0;
    if (lp_files_list(dir, 0, &l) == 0) {
        char **queue = calloc((size_t)(l.count > 0 ? l.count : 1), sizeof *queue);
        for (int i = 0; queue && i < l.count; i++) {
            if (l.entries[i].is_dir || !lp_media_kind_supported(l.entries[i].kind)) continue;
            char full[LP_FILES_PATH_MAX];
            lp_files_join(dir, l.entries[i].name, full, sizeof full);
            if (at && strcmp(l.entries[i].name, name) == 0) *at = count;
            queue[count++] = strdup(full);
        }
        *out = queue;
    }
    lp_files_list_free(&l);
    return count;
}

void lp_media_queue_free(char **queue, int count) {
    if (!queue) return;
    for (int i = 0; i < count; i++) free(queue[i]);
    free(queue);
}

#ifndef HAVE_GST

int lp_media_available(void) { return 0; }
int lp_media_open(lp_media **out, const char *path, int flags) { *out = NULL; return -ENOTSUP; }
void lp_media_close(lp_media *m) {}
int lp_media_wake_fd(const lp_media *m) { return -1; }
int lp_media_update(lp_media *m) { return 0; }
enum lp_media_state lp_media_state(const lp_media *m) { return LP_MEDIA_FAILED; }
const lp_media_info *lp_media_info_of(const lp_media *m) { static const lp_media_info none; return &none; }
void lp_media_play(lp_media *m) {}
void lp_media_pause(lp_media *m) {}
double lp_media_position(lp_media *m) { return 0; }
void lp_media_seek(lp_media *m, double seconds) {}
void lp_media_set_volume(lp_media *m, double volume) {}
double lp_media_volume(const lp_media *m) { return 1; }
cairo_surface_t *lp_media_frame(lp_media *m) { return NULL; }
cairo_surface_t *lp_media_cover(lp_media *m) { return NULL; }

#else

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

struct lp_media {
    GstElement *playbin, *video_sink;
    GstBus *bus;
    int wake[2];                /* GStreamer's threads write here; the host watches wake[0] */
    GMutex lock;
    GstSample *pending;         /* the newest frame, left by the streaming thread */
    cairo_surface_t *frame, *cover;
    enum lp_media_state state;
    int want_playing;
    double last_position;
    lp_media_info info;
    double volume;
    char file_title[128];       /* the file's name without its extension */
    char stream_title[128];     /* a stream's title tag, which may only name the track */
    int global_title;           /* the container carried a title of its own */
};

int lp_media_available(void) { return 1; }

static void wake(lp_media *m) {
    char c = 1;
    ssize_t ignored = write(m->wake[1], &c, 1);   /* a full pipe already means "come and look" */
    (void)ignored;
}

static GstFlowReturn keep_sample(lp_media *m, GstSample *sample) {
    if (!sample) return GST_FLOW_OK;
    g_mutex_lock(&m->lock);
    int was_empty = m->pending == NULL;
    if (m->pending) gst_sample_unref(m->pending);   /* the desktop has not taken the last one: this one is newer */
    m->pending = sample;
    g_mutex_unlock(&m->lock);
    if (was_empty) wake(m);
    return GST_FLOW_OK;
}

static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer data) { return keep_sample(data, gst_app_sink_pull_sample(sink)); }
static GstFlowReturn on_new_preroll(GstAppSink *sink, gpointer data) { return keep_sample(data, gst_app_sink_pull_preroll(sink)); }

static GstBusSyncReply on_bus_message(GstBus *bus, GstMessage *message, gpointer data) {
    wake(data);
    return GST_BUS_PASS;   /* queued for lp_media_update, which pops it on the desktop's thread */
}

int lp_media_open(lp_media **out, const char *path, int flags) {
    *out = NULL;
    static int initialised;
    if (!initialised) { gst_init(NULL, NULL); initialised = 1; }
    char absolute[PATH_MAX];
    if (!realpath(path, absolute)) return -errno;
    gchar *uri = gst_filename_to_uri(absolute, NULL);
    if (!uri) return -EINVAL;
    lp_media *m = calloc(1, sizeof *m);
    if (!m) { g_free(uri); return -ENOMEM; }
    if (pipe(m->wake) != 0) { int e = errno; g_free(uri); free(m); return -e; }
    for (int i = 0; i < 2; i++) {
        fcntl(m->wake[i], F_SETFL, fcntl(m->wake[i], F_GETFL) | O_NONBLOCK);
        fcntl(m->wake[i], F_SETFD, FD_CLOEXEC);
    }
    g_mutex_init(&m->lock);
    m->volume = 1;
    m->state = LP_MEDIA_LOADING;
    const char *base = lp_files_basename(path), *dot = strrchr(base, '.');
    snprintf(m->info.title, sizeof m->info.title, "%.*s", dot && dot != base ? (int)(dot - base) : (int)strlen(base), base);
    snprintf(m->file_title, sizeof m->file_title, "%s", m->info.title);

    m->playbin = gst_element_factory_make("playbin", NULL);
    m->video_sink = gst_element_factory_make("appsink", NULL);
    if (!m->playbin || !m->video_sink) {
        g_free(uri);
        if (m->video_sink) gst_object_unref(m->video_sink);
        lp_media_close(m);
        return -ENOTSUP;
    }
    /* BGRx is Cairo's RGB24 byte for byte on a little-endian machine: a frame is a memcpy away */
    GstCaps *caps = gst_caps_from_string("video/x-raw,format=BGRx");
    g_object_set(m->video_sink, "caps", caps, "sync", TRUE, "max-buffers", 1, "drop", TRUE, NULL);
    gst_caps_unref(caps);
    GstAppSinkCallbacks callbacks = { .new_preroll = on_new_preroll, .new_sample = on_new_sample };
    gst_app_sink_set_callbacks(GST_APP_SINK(m->video_sink), &callbacks, m, NULL);
    g_object_set(m->playbin, "uri", uri, "video-sink", m->video_sink, NULL);
    g_free(uri);
    if (flags & LP_MEDIA_SILENT) {
        GstElement *quiet = gst_element_factory_make("fakesink", NULL);
        if (quiet) {
            g_object_set(quiet, "sync", TRUE, NULL);
            g_object_set(m->playbin, "audio-sink", quiet, NULL);
        }
    }
    m->bus = gst_element_get_bus(m->playbin);
    gst_bus_set_sync_handler(m->bus, on_bus_message, m, NULL);
    if (gst_element_set_state(m->playbin, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
        lp_media_close(m);
        return -EINVAL;
    }
    *out = m;
    return 0;
}

void lp_media_close(lp_media *m) {
    if (!m) return;
    if (m->playbin) {
        gst_element_set_state(m->playbin, GST_STATE_NULL);
        gst_object_unref(m->playbin);   /* it owns the sinks */
    }
    if (m->bus) {
        gst_bus_set_sync_handler(m->bus, NULL, NULL, NULL);
        gst_object_unref(m->bus);
    }
    if (m->pending) gst_sample_unref(m->pending);
    if (m->frame) cairo_surface_destroy(m->frame);
    if (m->cover) cairo_surface_destroy(m->cover);
    if (m->wake[0] > 0) close(m->wake[0]);
    if (m->wake[1] > 0) close(m->wake[1]);
    g_mutex_clear(&m->lock);
    free(m);
}

int lp_media_wake_fd(const lp_media *m) { return m ? m->wake[0] : -1; }
enum lp_media_state lp_media_state(const lp_media *m) { return m ? m->state : LP_MEDIA_FAILED; }
const lp_media_info *lp_media_info_of(const lp_media *m) { return &m->info; }
cairo_surface_t *lp_media_frame(lp_media *m) { return m ? m->frame : NULL; }
cairo_surface_t *lp_media_cover(lp_media *m) { return m ? m->cover : NULL; }
double lp_media_volume(const lp_media *m) { return m ? m->volume : 1; }

/* The container's title tag, else a stream's once the file has proved to have no picture (a WebM names its
 * tracks "Video" and "Audio"; an Ogg's Vorbis comments are the song's own), else the file's name. */
static int settle_title(lp_media *m) {
    if (m->global_title) return 0;
    const char *title = m->stream_title[0] && m->state != LP_MEDIA_LOADING && !m->info.has_video ? m->stream_title : m->file_title;
    if (strcmp(title, m->info.title) == 0) return 0;
    snprintf(m->info.title, sizeof m->info.title, "%s", title);
    return LP_MEDIA_CHANGED_INFO;
}

static int query_info(lp_media *m) {
    int changed = 0;
    gint64 duration;
    if (gst_element_query_duration(m->playbin, GST_FORMAT_TIME, &duration) && duration > 0) {
        double seconds = (double)duration / GST_SECOND;
        if (fabs(seconds - m->info.duration) > 0.01) { m->info.duration = seconds; changed = LP_MEDIA_CHANGED_INFO; }
    }
    gint videos = 0, audios = 0;
    g_object_get(m->playbin, "n-video", &videos, "n-audio", &audios, NULL);
    if ((videos > 0) != m->info.has_video || (audios > 0) != m->info.has_audio) {
        m->info.has_video = videos > 0 || m->frame != NULL;
        m->info.has_audio = audios > 0;
        changed = LP_MEDIA_CHANGED_INFO;
    }
    return changed | settle_title(m);
}

static void read_tags(lp_media *m, GstTagList *tags) {
    gchar *text = NULL;
    if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &text)) {
        if (gst_tag_list_get_scope(tags) == GST_TAG_SCOPE_GLOBAL) {
            snprintf(m->info.title, sizeof m->info.title, "%s", text);
            m->global_title = 1;
        } else {
            snprintf(m->stream_title, sizeof m->stream_title, "%s", text);
        }
        g_free(text);
        settle_title(m);
    }
    if (gst_tag_list_get_string(tags, GST_TAG_ARTIST, &text)) { snprintf(m->info.artist, sizeof m->info.artist, "%s", text); g_free(text); }
    if (gst_tag_list_get_string(tags, GST_TAG_ALBUM, &text)) { snprintf(m->info.album, sizeof m->info.album, "%s", text); g_free(text); }
    GstSample *image = NULL;
    if (!m->cover && (gst_tag_list_get_sample(tags, GST_TAG_IMAGE, &image) || gst_tag_list_get_sample(tags, GST_TAG_PREVIEW_IMAGE, &image))) {
        GstBuffer *buffer = gst_sample_get_buffer(image);
        GstMapInfo map;
        if (buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            m->cover = lp_image_decode(map.data, map.size);
            gst_buffer_unmap(buffer, &map);
        }
        gst_sample_unref(image);
    }
}

static void take_frame(lp_media *m, GstSample *sample) {
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, gst_sample_get_caps(sample))) return;
    GstVideoFrame frame;
    if (!gst_video_frame_map(&frame, &info, gst_sample_get_buffer(sample), GST_MAP_READ)) return;
    int w = GST_VIDEO_FRAME_WIDTH(&frame), h = GST_VIDEO_FRAME_HEIGHT(&frame);
    if (!m->frame || cairo_image_surface_get_width(m->frame) != w || cairo_image_surface_get_height(m->frame) != h) {
        if (m->frame) cairo_surface_destroy(m->frame);
        m->frame = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    }
    if (cairo_surface_status(m->frame) == CAIRO_STATUS_SUCCESS) {
        cairo_surface_flush(m->frame);
        unsigned char *dst = cairo_image_surface_get_data(m->frame);
        int dst_stride = cairo_image_surface_get_stride(m->frame), src_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
        const guint8 *src = GST_VIDEO_FRAME_PLANE_DATA(&frame, 0);
        size_t row = (size_t)w * 4;
        for (int y = 0; y < h; y++) memcpy(dst + y * dst_stride, src + y * src_stride, row);
        cairo_surface_mark_dirty(m->frame);
    }
    gst_video_frame_unmap(&frame);
    m->info.width = w;
    m->info.height = h;
    m->info.has_video = 1;
}

int lp_media_update(lp_media *m) {
    if (!m) return 0;
    char drain[64];
    while (read(m->wake[0], drain, sizeof drain) > 0) {}
    int changed = 0;
    GstMessage *message;
    while ((message = gst_bus_pop(m->bus))) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error = NULL;
            gchar *debug = NULL;
            gst_message_parse_error(message, &error, &debug);
            snprintf(m->info.error, sizeof m->info.error, "%s", error ? error->message : "GStreamer failed");
            if (error) g_error_free(error);
            g_free(debug);
            m->state = LP_MEDIA_FAILED;
            m->want_playing = 0;
            changed |= LP_MEDIA_CHANGED_STATE;
            break;
        }
        case GST_MESSAGE_EOS:
            m->state = LP_MEDIA_ENDED;
            m->want_playing = 0;
            changed |= LP_MEDIA_CHANGED_STATE;
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(m->playbin) && m->state != LP_MEDIA_FAILED && m->state != LP_MEDIA_ENDED) {
                GstState old, now, pending;
                gst_message_parse_state_changed(message, &old, &now, &pending);
                enum lp_media_state was = m->state;
                if (now == GST_STATE_PLAYING) m->state = LP_MEDIA_PLAYING;
                else if (now == GST_STATE_PAUSED) m->state = m->want_playing ? LP_MEDIA_PLAYING : LP_MEDIA_PAUSED;
                if (m->state != was) changed |= LP_MEDIA_CHANGED_STATE;
                if (now >= GST_STATE_PAUSED) changed |= query_info(m);
            }
            break;
        case GST_MESSAGE_DURATION_CHANGED:
        case GST_MESSAGE_ASYNC_DONE:
            changed |= query_info(m);
            break;
        case GST_MESSAGE_TAG: {
            GstTagList *tags = NULL;
            gst_message_parse_tag(message, &tags);
            if (tags) { read_tags(m, tags); gst_tag_list_unref(tags); }
            changed |= LP_MEDIA_CHANGED_INFO;
            break;
        }
        default:
            break;
        }
        gst_message_unref(message);
    }
    g_mutex_lock(&m->lock);
    GstSample *sample = m->pending;
    m->pending = NULL;
    g_mutex_unlock(&m->lock);
    if (sample) {
        take_frame(m, sample);
        gst_sample_unref(sample);
        changed |= LP_MEDIA_CHANGED_FRAME;
    }
    return changed;
}

void lp_media_play(lp_media *m) {
    if (!m || m->state == LP_MEDIA_FAILED) return;
    if (m->state == LP_MEDIA_ENDED) {
        gst_element_seek_simple(m->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
        m->last_position = 0;
        m->state = LP_MEDIA_PAUSED;
    }
    m->want_playing = 1;
    gst_element_set_state(m->playbin, GST_STATE_PLAYING);
    if (m->state != LP_MEDIA_LOADING) m->state = LP_MEDIA_PLAYING;   /* say so now; the bus confirms it */
}

void lp_media_pause(lp_media *m) {
    if (!m || m->state == LP_MEDIA_FAILED) return;
    m->want_playing = 0;
    gst_element_set_state(m->playbin, GST_STATE_PAUSED);
    if (m->state == LP_MEDIA_PLAYING) m->state = LP_MEDIA_PAUSED;
}

double lp_media_position(lp_media *m) {
    if (!m) return 0;
    if (m->state == LP_MEDIA_ENDED) return m->info.duration;
    gint64 position;
    if (gst_element_query_position(m->playbin, GST_FORMAT_TIME, &position) && position >= 0) m->last_position = (double)position / GST_SECOND;
    return m->last_position;
}

void lp_media_seek(lp_media *m, double seconds) {
    if (!m || m->state == LP_MEDIA_FAILED) return;
    if (seconds < 0) seconds = 0;
    if (m->info.duration > 0 && seconds > m->info.duration) seconds = m->info.duration;
    gst_element_seek_simple(m->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE, (gint64)(seconds * GST_SECOND));
    m->last_position = seconds;
    if (m->state == LP_MEDIA_ENDED) m->state = m->want_playing ? LP_MEDIA_PLAYING : LP_MEDIA_PAUSED;
}

void lp_media_set_volume(lp_media *m, double volume) {
    if (!m) return;
    m->volume = volume < 0 ? 0 : volume > 1 ? 1 : volume;
    g_object_set(m->playbin, "volume", m->volume, NULL);
}

#endif
