/* The desktop's view of PipeWire's speakers and microphones (lp_audio.h). */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "maryui/lp_audio.h"
#include "maryui/lp_desktop.h"

void lp_audio_init(lp_audio *a, struct lp_desktop *d) {
    memset(a, 0, sizeof *a);
    a->desk = d;
    a->retry_ms = LP_AUDIO_RETRY_MIN_MS;
}

const lp_audio_device *lp_audio_default(const lp_audio *a, enum lp_audio_direction dir) {
    for (int i = 0; a->default_name[dir][0] && i < a->count[dir]; i++)
        if (strcmp(a->devices[dir][i].name, a->default_name[dir]) == 0) return &a->devices[dir][i];
    return NULL;
}

int lp_audio_is_default(const lp_audio *a, enum lp_audio_direction dir, const lp_audio_device *device) {
    return device && a->default_name[dir][0] && strcmp(device->name, a->default_name[dir]) == 0;
}

/* MARK: - Reading what PipeWire says */

static const char *skip_space(const char *c) {
    while (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r') c++;
    return c;
}

int lp_audio_parse_default(const char *json, char *name, size_t n) {
    const char *key = json ? strstr(json, "\"name\"") : NULL;
    if (!key || !name || !n) return -EINVAL;
    const char *c = skip_space(key + 6);
    if (*c != ':') return -EINVAL;
    c = skip_space(c + 1);
    if (*c++ != '"') return -EINVAL;
    size_t len = 0;
    for (; *c && *c != '"'; c++) {
        if (*c == '\\' && !*++c) return -EINVAL;
        if (len + 1 >= n) return -ENAMETOOLONG;
        name[len++] = *c;
    }
    if (*c != '"' || !len) return -EINVAL;
    name[len] = 0;
    return 0;
}

static int mentions(const char *text, const char *word) {
    size_t n = strlen(word);
    for (const char *t = text; t && *t; t++) if (strncasecmp(t, word, n) == 0) return 1;
    return 0;
}

const char *lp_audio_kind_of(const char *node_name, const char *description) {
    if (mentions(node_name, "bluez") || mentions(description, "bluetooth")) return "Bluetooth";
    if (mentions(node_name, "usb") || mentions(description, "usb")) return "USB";
    if (mentions(node_name, "hdmi") || mentions(description, "hdmi") || mentions(node_name, "displayport")) return "HDMI";
    if (mentions(node_name, "virtio") || mentions(description, "virtio") || mentions(node_name, "virtual")) return "Virtual";
    return "Built-in";
}

float lp_audio_meter_value(float peak) {
    float magnitude = fabsf(peak);
    if (!(magnitude > 0.001f)) return 0;
    float value = (20.0f * log10f(magnitude) + 60.0f) / 60.0f;
    return value > 1 ? 1 : value;
}

float lp_audio_release(float shown, float peak, float seconds) {
    float value = lp_audio_meter_value(peak), fall = shown - seconds / 0.3f;
    return value > fall ? value : fall > 0 ? fall : 0;
}

/* MARK: - The lists */

static int same_device(const lp_audio_device *x, const lp_audio_device *y) {
    return x->id == y->id && x->serial == y->serial && strcmp(x->name, y->name) == 0 &&
           strcmp(x->description, y->description) == 0 && strcmp(x->kind, y->kind) == 0;
}

/* One direction's devices and default as they are now; what changed. */
static unsigned take(lp_audio *a, enum lp_audio_direction dir, const lp_audio_device *devices, int n, const char *default_name) {
    unsigned what = 0;
    n = n < 0 ? 0 : n > LP_AUDIO_MAX_DEVICES ? LP_AUDIO_MAX_DEVICES : n;
    int differs = n != a->count[dir];
    for (int i = 0; !differs && i < n; i++) differs = !same_device(&a->devices[dir][i], &devices[i]);
    if (differs) {
        for (int i = 0; i < n; i++) a->devices[dir][i] = devices[i];
        a->count[dir] = n;
        what |= LP_AUDIO_CHANGED_DEVICES;
    }
    const char *want = default_name ? default_name : "";
    if (strcmp(a->default_name[dir], want) != 0) {
        snprintf(a->default_name[dir], sizeof a->default_name[dir], "%s", want);
        what |= LP_AUDIO_CHANGED_DEFAULTS;
    }
    return what;
}

static void changed(lp_audio *a, unsigned what) {
    if (what && a->desk) lp_desktop_models_changed(a->desk, LP_MODEL_AUDIO, what);
}

static void sync_meter(lp_audio *a);

void lp_audio_set_devices(lp_audio *a, enum lp_audio_direction dir, const lp_audio_device *devices, int n, const char *default_name) {
    unsigned what = take(a, dir, devices, n, default_name);
    if (what) sync_meter(a);
    changed(a, what);
}

void lp_audio_set_level(lp_audio *a, float level) { a->level = level < 0 ? 0 : level > 1 ? 1 : level; }

void lp_audio_meter(lp_audio *a, int on) {
    a->leases += on ? 1 : -1;
    if (a->leases <= 0) {
        a->leases = 0;
        a->level = 0;
    }
    sync_meter(a);
}

#ifdef HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>   /* after pipewire.h, which declares the spa_hook it takes */
#include <spa/param/audio/format-utils.h>
#include <spa/utils/dict.h>
#include <stdatomic.h>

#define NODES_MAX 64
#define METER_RATE 48000

struct node {
    enum lp_audio_direction dir;
    lp_audio_device device;
};

struct lp_audio_backend {
    struct pw_thread_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_registry *registry;
    struct pw_metadata *metadata;
    uint32_t metadata_id;
    struct spa_hook core_listener, registry_listener, metadata_listener, meter_listener;
    int core_hooked, registry_hooked;
    int wake[2];                    /* PipeWire's thread writes a byte; the desktop watches wake[0] */
    /* Under the loop's lock: */
    struct node nodes[NODES_MAX];
    int node_count;
    char defaults[2][128];
    int synced, broken, sync_seq;
    struct pw_stream *meter;        /* made and ended on the desktop's thread, under the lock */
    char meter_target[128];
    atomic_uint shown;              /* the meter, as a float's bits: its process callback takes no lock */
};

int lp_audio_available(void) { return 1; }

static void poke(struct lp_audio_backend *b) {
    char c = 1;
    ssize_t ignored = write(b->wake[1], &c, 1);   /* a full pipe already means "come and look" */
    (void)ignored;
}

/* MARK: PipeWire's thread */

static const char *lookup(const struct spa_dict *props, const char *key) {
    const char *value = spa_dict_lookup(props, key);
    return value ? value : "";
}

static int on_property(void *data, uint32_t subject, const char *key, const char *type, const char *value) {
    struct lp_audio_backend *b = data;
    if (subject != PW_ID_CORE) return 0;
    if (!key) {                                     /* everything was cleared */
        b->defaults[LP_AUDIO_OUTPUT][0] = b->defaults[LP_AUDIO_INPUT][0] = 0;
        poke(b);
        return 0;
    }
    int dir = strcmp(key, "default.audio.sink") == 0 ? LP_AUDIO_OUTPUT : strcmp(key, "default.audio.source") == 0 ? LP_AUDIO_INPUT : -1;
    if (dir < 0) return 0;
    if (!value || lp_audio_parse_default(value, b->defaults[dir], sizeof b->defaults[dir]) < 0) b->defaults[dir][0] = 0;
    poke(b);
    return 0;
}

static const struct pw_metadata_events metadata_events = { PW_VERSION_METADATA_EVENTS, .property = on_property };

static void on_global(void *data, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props) {
    struct lp_audio_backend *b = data;
    if (!props) return;
    if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char *media_class = lookup(props, PW_KEY_MEDIA_CLASS);
        int dir = strcmp(media_class, "Audio/Sink") == 0 ? LP_AUDIO_OUTPUT
                : strcmp(media_class, "Audio/Source") == 0 || strcmp(media_class, "Audio/Source/Virtual") == 0 ? LP_AUDIO_INPUT : -1;
        if (dir < 0 || b->node_count == NODES_MAX) return;
        struct node *n = &b->nodes[b->node_count++];
        memset(n, 0, sizeof *n);
        n->dir = (enum lp_audio_direction)dir;
        n->device.id = id;
        n->device.serial = strtoull(lookup(props, PW_KEY_OBJECT_SERIAL), NULL, 10);
        snprintf(n->device.name, sizeof n->device.name, "%s", lookup(props, PW_KEY_NODE_NAME));
        const char *description = lookup(props, PW_KEY_NODE_DESCRIPTION);
        if (!*description) description = lookup(props, PW_KEY_NODE_NICK);
        if (!*description) description = n->device.name;
        snprintf(n->device.description, sizeof n->device.description, "%s", description);
        snprintf(n->device.kind, sizeof n->device.kind, "%s", lp_audio_kind_of(n->device.name, n->device.description));
        poke(b);
    } else if (strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0 && !b->metadata && strcmp(lookup(props, "metadata.name"), "default") == 0) {
        b->metadata = pw_registry_bind(b->registry, id, type, PW_VERSION_METADATA, 0);
        if (!b->metadata) return;
        b->metadata_id = id;
        pw_metadata_add_listener(b->metadata, &b->metadata_listener, &metadata_events, b);
    }
}

static void on_global_remove(void *data, uint32_t id) {
    struct lp_audio_backend *b = data;
    for (int i = 0; i < b->node_count; i++) {
        if (b->nodes[i].device.id != id) continue;
        memmove(&b->nodes[i], &b->nodes[i + 1], (size_t)(b->node_count - i - 1) * sizeof *b->nodes);
        b->node_count--;
        poke(b);
        return;
    }
    if (b->metadata && id == b->metadata_id) {
        spa_hook_remove(&b->metadata_listener);
        pw_proxy_destroy((struct pw_proxy *)b->metadata);
        b->metadata = NULL;
        b->defaults[LP_AUDIO_OUTPUT][0] = b->defaults[LP_AUDIO_INPUT][0] = 0;
        poke(b);
    }
}

static const struct pw_registry_events registry_events = { PW_VERSION_REGISTRY_EVENTS, .global = on_global, .global_remove = on_global_remove };

static void on_core_done(void *data, uint32_t id, int seq) {
    struct lp_audio_backend *b = data;
    if (id != PW_ID_CORE || seq != b->sync_seq) return;
    b->synced = 1;                                  /* every device PipeWire had is in: no half list is shown */
    poke(b);
}

static void on_core_error(void *data, uint32_t id, int seq, int res, const char *message) {
    struct lp_audio_backend *b = data;
    if (id != PW_ID_CORE || res != -EPIPE) return;
    b->broken = 1;                                  /* PipeWire went away */
    poke(b);
}

static const struct pw_core_events core_events = { PW_VERSION_CORE_EVENTS, .done = on_core_done, .error = on_core_error };

static void on_meter(void *data) {
    struct lp_audio_backend *b = data;
    struct pw_buffer *buffer = pw_stream_dequeue_buffer(b->meter);
    if (!buffer) return;
    struct spa_data *d = &buffer->buffer->datas[0];
    if (d->data && d->chunk) {
        uint32_t offset = d->chunk->offset < d->maxsize ? d->chunk->offset : d->maxsize;
        uint32_t size = d->chunk->size <= d->maxsize - offset ? d->chunk->size : d->maxsize - offset;
        const float *samples = (const float *)((uint8_t *)d->data + offset);
        uint32_t n = size / sizeof(float);
        float peak = 0, shown;
        for (uint32_t i = 0; i < n; i++) if (fabsf(samples[i]) > peak) peak = fabsf(samples[i]);
        unsigned bits = atomic_load(&b->shown);
        memcpy(&shown, &bits, sizeof shown);
        shown = lp_audio_release(shown, peak, (float)n / METER_RATE);
        memcpy(&bits, &shown, sizeof bits);
        atomic_store(&b->shown, bits);
    }
    pw_stream_queue_buffer(b->meter, buffer);
}

static const struct pw_stream_events meter_events = { PW_VERSION_STREAM_EVENTS, .process = on_meter };

/* MARK: The desktop's thread */

static void backend_close(struct lp_audio_backend *b) {
    if (b->loop) pw_thread_loop_stop(b->loop);
    if (b->meter) pw_stream_destroy(b->meter);
    if (b->metadata) {
        spa_hook_remove(&b->metadata_listener);
        pw_proxy_destroy((struct pw_proxy *)b->metadata);
    }
    if (b->registry_hooked) spa_hook_remove(&b->registry_listener);
    if (b->registry) pw_proxy_destroy((struct pw_proxy *)b->registry);
    if (b->core_hooked) spa_hook_remove(&b->core_listener);
    if (b->core) pw_core_disconnect(b->core);
    if (b->context) pw_context_destroy(b->context);
    if (b->loop) pw_thread_loop_destroy(b->loop);
    for (int i = 0; i < 2; i++) if (b->wake[i] >= 0) close(b->wake[i]);
    free(b);
}

static struct lp_audio_backend *backend_open(int *error) {
    static int initialised;
    if (!initialised) {
        pw_init(NULL, NULL);
        initialised = 1;
    }
    struct lp_audio_backend *b = calloc(1, sizeof *b);
    if (!b) {
        *error = -ENOMEM;
        return NULL;
    }
    b->wake[0] = b->wake[1] = -1;
    if (pipe(b->wake) != 0) {
        *error = -errno;
        b->wake[0] = b->wake[1] = -1;
        backend_close(b);
        return NULL;
    }
    for (int i = 0; i < 2; i++) {
        fcntl(b->wake[i], F_SETFL, fcntl(b->wake[i], F_GETFL) | O_NONBLOCK);
        fcntl(b->wake[i], F_SETFD, FD_CLOEXEC);
    }
    int failed = 0;
    if (!(b->loop = pw_thread_loop_new("maryui-audio", NULL)) || !(b->context = pw_context_new(pw_thread_loop_get_loop(b->loop), NULL, 0)))
        failed = errno ? -errno : -ENOMEM;
    if (!failed) {
        pw_thread_loop_lock(b->loop);
        b->core = pw_context_connect(b->context, NULL, 0);
        if (!b->core) failed = errno ? -errno : -EHOSTDOWN;
        if (b->core) {
            pw_core_add_listener(b->core, &b->core_listener, &core_events, b);
            b->core_hooked = 1;
            b->registry = pw_core_get_registry(b->core, PW_VERSION_REGISTRY, 0);
            if (!b->registry) failed = -ENOMEM;
        }
        if (b->registry) {
            pw_registry_add_listener(b->registry, &b->registry_listener, &registry_events, b);
            b->registry_hooked = 1;
            b->sync_seq = pw_core_sync(b->core, PW_ID_CORE, 0);
        }
        pw_thread_loop_unlock(b->loop);
    }
    if (!failed && pw_thread_loop_start(b->loop) < 0) failed = -EIO;
    if (failed) {
        *error = failed;
        backend_close(b);
        return NULL;
    }
    return b;
}

static void meter_stop(struct lp_audio_backend *b) {
    if (b->meter) pw_stream_destroy(b->meter);
    b->meter = NULL;
    b->meter_target[0] = 0;
    atomic_store(&b->shown, 0);
}

static void meter_start(struct lp_audio_backend *b, const char *target) {
    struct pw_properties *props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
                                                    PW_KEY_APP_NAME, "System Settings", PW_KEY_NODE_NAME, "maryui-input-level",
                                                    PW_KEY_TARGET_OBJECT, target, NULL);
    if (!props || !(b->meter = pw_stream_new(b->core, "maryui-input-level", props))) return;
    pw_stream_add_listener(b->meter, &b->meter_listener, &meter_events, b);
    uint8_t buffer[1024];
    struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);
    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32, .channels = 1, .rate = METER_RATE, .position = { SPA_AUDIO_CHANNEL_MONO }));
    enum pw_stream_flags flags = PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS;
    if (pw_stream_connect(b->meter, PW_DIRECTION_INPUT, PW_ID_ANY, flags, params, 1) < 0) {
        meter_stop(b);
        return;
    }
    snprintf(b->meter_target, sizeof b->meter_target, "%s", target);
}

/* A meter while anyone watches and there is a microphone to watch; always on the default one. */
static void sync_meter(lp_audio *a) {
    struct lp_audio_backend *b = a->backend;
    if (!b) return;
    const char *want = a->default_name[LP_AUDIO_INPUT];
    pw_thread_loop_lock(b->loop);
    if (b->meter && (!a->leases || strcmp(b->meter_target, want) != 0)) meter_stop(b);
    if (a->leases && !b->meter && *want) meter_start(b, want);
    pw_thread_loop_unlock(b->loop);
}

float lp_audio_level(const lp_audio *a) {
    if (!a->backend) return a->level;
    unsigned bits = atomic_load(&a->backend->shown);
    float shown;
    memcpy(&shown, &bits, sizeof shown);
    return shown;
}

static int try_connect(lp_audio *a);

static int on_retry(int fd, uint32_t mask, void *data) {
    lp_audio *a = data;
    if (!a->backend) try_connect(a);
    return 0;
}

static void schedule_retry(lp_audio *a) {
    if (!a->started || !a->desk) return;
    if (!a->retry) a->retry = lp_desktop_add_timer(a->desk, 0, on_retry, a);
    if (!a->retry) return;
    lp_desktop_update_timer(a->desk, a->retry, a->retry_ms);
    a->retry_ms = a->retry_ms * 2 > LP_AUDIO_RETRY_MAX_MS ? LP_AUDIO_RETRY_MAX_MS : a->retry_ms * 2;
}

static void drop_backend(lp_audio *a) {
    if (a->wake_source) lp_desktop_remove_source(a->desk, a->wake_source);
    a->wake_source = NULL;
    if (a->backend) backend_close(a->backend);
    a->backend = NULL;
}

/* PipeWire went away: nothing is known until it is back. */
static void lost(lp_audio *a) {
    drop_backend(a);
    unsigned what = a->connected ? LP_AUDIO_CHANGED_CONNECTION : 0;
    a->connected = 0;
    for (int dir = 0; dir < 2; dir++) what |= take(a, (enum lp_audio_direction)dir, NULL, 0, "");
    changed(a, what);
    schedule_retry(a);
}

static int on_wake(int fd, uint32_t mask, void *data) {
    lp_audio *a = data;
    char drain[64];
    while (read(fd, drain, sizeof drain) > 0) {}
    struct lp_audio_backend *b = a->backend;
    if (!b) return 0;
    lp_audio_device devices[2][LP_AUDIO_MAX_DEVICES];
    int count[2] = { 0, 0 };
    char defaults[2][128];
    pw_thread_loop_lock(b->loop);
    int broken = b->broken, synced = b->synced;
    for (int i = 0; i < b->node_count; i++) {
        const struct node *n = &b->nodes[i];
        if (count[n->dir] < LP_AUDIO_MAX_DEVICES) devices[n->dir][count[n->dir]++] = n->device;
    }
    memcpy(defaults, b->defaults, sizeof defaults);
    pw_thread_loop_unlock(b->loop);
    if (broken) {
        lost(a);
        return 0;
    }
    if (!synced) return 0;
    unsigned what = a->connected ? 0 : LP_AUDIO_CHANGED_CONNECTION;
    a->connected = 1;
    for (int dir = 0; dir < 2; dir++) what |= take(a, (enum lp_audio_direction)dir, devices[dir], count[dir], defaults[dir]);
    if (what) sync_meter(a);
    changed(a, what);
    return 0;
}

static int try_connect(lp_audio *a) {
    int error = 0;
    struct lp_audio_backend *b = backend_open(&error);
    if (!b) {
        schedule_retry(a);
        return error;
    }
    a->wake_source = lp_desktop_add_fd(a->desk, b->wake[0], LP_SOURCE_READABLE, on_wake, a);
    if (!a->wake_source) {
        backend_close(b);
        return -ENOSYS;
    }
    a->backend = b;
    a->retry_ms = LP_AUDIO_RETRY_MIN_MS;
    return 0;
}

int lp_audio_start(lp_audio *a) {
    if (!a->desk || !a->desk->add_fd || !a->desk->add_timer) return -ENOSYS;
    a->started = 1;
    if (!a->backend) try_connect(a);
    return 0;
}

void lp_audio_free(lp_audio *a) {
    a->started = 0;
    drop_backend(a);
    if (a->retry) lp_desktop_remove_source(a->desk, a->retry);
    a->retry = NULL;
    a->connected = a->leases = 0;
}

#else

int lp_audio_available(void) { return 0; }
int lp_audio_start(lp_audio *a) { return -ENOSYS; }
static void sync_meter(lp_audio *a) {}
float lp_audio_level(const lp_audio *a) { return a->level; }
void lp_audio_free(lp_audio *a) {
    if (a->retry && a->desk) lp_desktop_remove_source(a->desk, a->retry);
    a->retry = NULL;
    a->started = a->connected = a->leases = 0;
}

#endif
