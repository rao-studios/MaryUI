/* The desktop's view of PipeWire's speakers and microphones (Linux, PARITY D23): the output and input devices
 * the session offers, which of each WirePlumber has made the default, and, while someone watches, how loud the
 * default microphone is. A thread of PipeWire's follows devices as they come and go and the defaults as they
 * move; the desktop takes a copy on its own thread whenever the host's event loop says something changed, and
 * each change reaches the windows that show it through lp_desktop_models_changed(d, LP_MODEL_AUDIO, what).
 * Choosing a device, volume and mute stay wpctl's, run by System Settings: WirePlumber keeps the routes and
 * remembers the choice, and maryd, whose streams follow the defaults, listens and speaks through whatever is
 * chosen. Without libpipewire (a Mac build) lp_audio_start answers -ENOSYS; the lists can still be set by hand
 * for tests and renders. */
#ifndef MARYUI_LP_AUDIO_H
#define MARYUI_LP_AUDIO_H

#include <stddef.h>
#include <stdint.h>

struct lp_desktop;
struct lp_source;

#define LP_AUDIO_MAX_DEVICES 16
#define LP_AUDIO_RETRY_MIN_MS 250
#define LP_AUDIO_RETRY_MAX_MS 5000

enum lp_audio_direction { LP_AUDIO_OUTPUT, LP_AUDIO_INPUT };

enum {
    LP_AUDIO_CHANGED_DEVICES = 1,
    LP_AUDIO_CHANGED_DEFAULTS = 2,
    LP_AUDIO_CHANGED_CONNECTION = 4,
};

typedef struct lp_audio_device {
    uint32_t id;                    /* PipeWire's global id: what wpctl takes */
    uint64_t serial;                /* object.serial: an id PipeWire reuses for another device is not this one */
    char name[128];                 /* node.name: stable, and what WirePlumber records the default by */
    char description[128];          /* node.description: what a person reads */
    char kind[16];                  /* Built-in, USB, Bluetooth, HDMI or Virtual */
} lp_audio_device;

typedef struct lp_audio_backend lp_audio_backend;

typedef struct lp_audio {
    struct lp_desktop *desk;
    int connected;                  /* PipeWire answered, and its first lists are in */
    lp_audio_device devices[2][LP_AUDIO_MAX_DEVICES];   /* [LP_AUDIO_OUTPUT] and [LP_AUDIO_INPUT] */
    int count[2];
    char default_name[2][128];      /* the default sink's and source's node.name; "" when there is none */
    float level;                    /* the meter when there is no PipeWire to ask: tests and renders */
    int leases;                     /* how many watch the level (lp_audio_meter) */
    int started, retry_ms;
    struct lp_source *wake_source, *retry;
    lp_audio_backend *backend;      /* NULL without PipeWire, and while it is away */
} lp_audio;

/* 1 when PipeWire support is compiled in. */
int lp_audio_available(void);
void lp_audio_init(lp_audio *a, struct lp_desktop *d);
/* Follows the session's PipeWire on the host's event loop, reconnecting while it is away. 0; -ENOSYS without
 * PipeWire or without an event loop to live on. */
int lp_audio_start(lp_audio *a);
void lp_audio_free(lp_audio *a);

/* The default output or input, or NULL (no microphone, say). */
const lp_audio_device *lp_audio_default(const lp_audio *a, enum lp_audio_direction dir);
int lp_audio_is_default(const lp_audio *a, enum lp_audio_direction dir, const lp_audio_device *device);

/* Watching the default microphone's level: on 1 takes a lease, 0 gives one back; a capture stream runs while
 * anyone holds one. The level is 0…1 on a -60…0 dBFS scale, rising at once and falling back over 300 ms. */
void lp_audio_meter(lp_audio *a, int on);
float lp_audio_level(const lp_audio *a);

/* Tests and renders: the lists, the default and the level, as PipeWire would report them. */
void lp_audio_set_devices(lp_audio *a, enum lp_audio_direction dir, const lp_audio_device *devices, int n,
                          const char *default_name);
void lp_audio_set_level(lp_audio *a, float level);

/* The node named by the default metadata's value, {"name":"alsa_output…"}. 0; -EINVAL; -ENAMETOOLONG. */
int lp_audio_parse_default(const char *json, char *name, size_t n);
/* What kind of device a node is, from its name and description. */
const char *lp_audio_kind_of(const char *node_name, const char *description);
/* A sample peak on the meter's scale: 0 at -60 dBFS and below, 1 at full scale. */
float lp_audio_meter_value(float peak);
/* The meter after `seconds` more audio peaking at `peak`: up at once, and down by the whole scale in 300 ms. */
float lp_audio_release(float shown, float peak, float seconds);

#endif
