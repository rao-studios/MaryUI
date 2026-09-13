/* The Media Player's engine: one file played through GStreamer's playbin —
 * audio to the default sink (PipeWire on MaryOS), video frames handed over as
 * Cairo surfaces — with play, pause, seek, volume, and the tags and cover art a
 * file carries. GStreamer's threads never touch the app: they write to a pipe
 * whose read end the host watches (lp_desktop_add_fd), and lp_media_update does
 * the work on the desktop's thread. Plus what needs no GStreamer: time labels
 * and a folder's play queue. Without GStreamer in the build (HAVE_GST)
 * lp_media_open fails with -ENOTSUP. Linux only (PARITY D15). */
#ifndef MARYUI_LP_MEDIA_H
#define MARYUI_LP_MEDIA_H

#include <cairo.h>
#include <stddef.h>

#include "maryui/lp_files.h"

typedef struct lp_media lp_media;

enum lp_media_state { LP_MEDIA_LOADING, LP_MEDIA_PAUSED, LP_MEDIA_PLAYING, LP_MEDIA_ENDED, LP_MEDIA_FAILED };

typedef struct lp_media_info {
    char title[128], artist[128], album[128];   /* from the tags; the title falls back to the file's name */
    int has_video, has_audio;
    int width, height;                          /* the video's, once a frame has arrived */
    double duration;                            /* seconds; 0 until known */
    char error[256];                            /* why, when FAILED */
} lp_media_info;

#define LP_MEDIA_SILENT 1                       /* lp_media_open: audio to a fake sink (tests, previews) */

enum { LP_MEDIA_CHANGED_FRAME = 1, LP_MEDIA_CHANGED_STATE = 2, LP_MEDIA_CHANGED_INFO = 4 };

int lp_media_available(void);
/* Opens path, prerolling to its first frame. 0 or -errno (-ENOTSUP without GStreamer). */
int lp_media_open(lp_media **out, const char *path, int flags);
void lp_media_close(lp_media *m);
/* Readable when a frame or a message waits: call lp_media_update then. */
int lp_media_wake_fd(const lp_media *m);
/* Takes what arrived; returns LP_MEDIA_CHANGED_* for what changed. */
int lp_media_update(lp_media *m);
enum lp_media_state lp_media_state(const lp_media *m);
const lp_media_info *lp_media_info_of(const lp_media *m);
void lp_media_play(lp_media *m);          /* from the start again when it had ended */
void lp_media_pause(lp_media *m);
double lp_media_position(lp_media *m);     /* seconds */
void lp_media_seek(lp_media *m, double seconds);
void lp_media_set_volume(lp_media *m, double volume);   /* 0 … 1 */
double lp_media_volume(const lp_media *m);
/* The newest frame, owned by m and valid until the next update; NULL for audio or before the first. */
cairo_surface_t *lp_media_frame(lp_media *m);
/* The cover art from the tags, owned by m; NULL when there is none. */
cairo_surface_t *lp_media_cover(lp_media *m);

/* 0:07, 3:25, 1:02:03 */
void lp_media_format_time(double seconds, char *out, size_t n);
/* 1 for the kinds the player opens: audio and video. */
int lp_media_kind_supported(enum lp_file_kind kind);
/* The audio and video files beside path, in Finder order; *at is path's index (-1 when absent).
 * Returns the count; free with lp_media_queue_free. */
int lp_media_queue(const char *path, char ***out, int *at);
void lp_media_queue_free(char **queue, int count);

#endif
