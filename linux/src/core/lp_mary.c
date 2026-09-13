/* The desktop's client for maryd (lp_mary.h). */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_mary.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

static const char *const state_names[] = { "idle", "listening", "hearing", "transcribing", "thinking", "speaking", "error" };

const char *lp_mary_state_name(lp_mary_state s) {
    return (unsigned)s < sizeof state_names / sizeof *state_names ? state_names[s] : "idle";
}

static void wipe(void *p, size_t n) {
    volatile unsigned char *b = p;
    while (n--) *b++ = 0;
}

void lp_mary_init(lp_mary *m, struct lp_desktop *d) {
    memset(m, 0, sizeof *m);
    m->desk = d;
    m->fd = -1;
    m->key_check = -1;
    m->retry_ms = LP_MARY_RETRY_MIN_MS;
}

int lp_mary_connected(const lp_mary *m) { return m->fd >= 0; }

int lp_mary_active(const lp_mary *m) { return m->state >= LP_MARY_LISTENING && m->state <= LP_MARY_SPEAKING; }

int lp_mary_voice_split(const char *voice_id, char *character, size_t cn, char *mood, size_t mn) {
    const char *id = voice_id ? voice_id : "", *first = strchr(id, '_'), *second = first ? strchr(first + 1, '_') : NULL;
    int split = first && first > id && second && second > first + 1 && second[1];
    if (cn) snprintf(character, cn, "%.*s", split ? (int)(second - id) : (int)strlen(id), id);
    if (mn) snprintf(mood, mn, "%s", split ? second + 1 : "");
    return split;
}

static void changed(lp_mary *m, unsigned what) {
    if (m->desk && m->desk->on_mary) m->desk->on_mary(m->desk, what);
    /* and any window showing Mary (Settings › Mary); the microphone's level only moves Spotlight's meter */
    if (m->desk && (what & ~(unsigned)LP_MARY_CHANGED_LEVEL)) lp_desktop_models_changed(m->desk, LP_MODEL_MARY, what);
}

static void free_credits(lp_mary_message *msg);

static void clear_messages(lp_mary *m) {
    for (int i = 0; i < m->message_count; i++) {
        free(m->messages[i].text);
        free(m->messages[i].note);
        free_credits(&m->messages[i]);
    }
    m->message_count = 0;
}

int lp_mary_span_owner_at(const lp_mary_message *msg, int index) {
    for (int i = 0; i < msg->span_count; i++)
        if (index >= msg->spans[i].lower && index < msg->spans[i].upper) return msg->spans[i].owner;
    return -1;
}

static void disconnect(lp_mary *m, int retry);

void lp_mary_free(lp_mary *m) {
    m->started = 0;
    disconnect(m, 0);
    if (m->retry) lp_desktop_remove_source(m->desk, m->retry);
    m->retry = NULL;
    clear_messages(m);
#ifdef HAVE_JSONC
    if (m->ambient) json_object_put(m->ambient);
    if (m->trace) json_object_put(m->trace);
#endif
    m->ambient = m->trace = NULL;
    free(m->trace_report);
    m->trace_report = NULL;
    free(m->in);
    if (m->out) wipe(m->out, m->out_cap);
    free(m->out);
    m->in = m->out = NULL;
    m->in_cap = m->out_cap = 0;
}

static int try_connect(lp_mary *m);

static int on_retry(int fd, uint32_t mask, void *data) {
    try_connect(data);
    return 0;
}

static void schedule_retry(lp_mary *m) {
    if (!m->started || !m->desk) return;
    if (!m->retry) m->retry = lp_desktop_add_timer(m->desk, 0, on_retry, m);
    if (!m->retry) return;
    lp_desktop_update_timer(m->desk, m->retry, m->retry_ms);
    m->retry_ms = m->retry_ms * 2 > LP_MARY_RETRY_MAX_MS ? LP_MARY_RETRY_MAX_MS : m->retry_ms * 2;
}

static void disconnect(lp_mary *m, int retry) {
    if (m->fd < 0) return;
    if (m->source) lp_desktop_remove_source(m->desk, m->source);
    m->source = NULL;
    close(m->fd);
    m->fd = -1;
    m->in_len = 0;
    m->discarding = 0;
    if (m->out) wipe(m->out, m->out_cap);
    m->out_len = 0;
    m->writable_armed = 0;
    m->state = LP_MARY_IDLE;
    m->partial[0] = 0;
    m->level = 0;
    if (m->sample_state == LP_MARY_SAMPLE_ASKING || m->sample_state == LP_MARY_SAMPLE_PLAYING) m->sample_state = LP_MARY_SAMPLE_NONE;
    for (int i = 0; i < m->message_count; i++) m->messages[i].streaming = 0;
    changed(m, LP_MARY_CHANGED_CONNECTION | LP_MARY_CHANGED_STATE);
    if (retry) schedule_retry(m);
}

#ifdef HAVE_JSONC
#include <json-c/json.h>

int lp_mary_available(void) { return 1; }

static const char *str(struct json_object *o, const char *key) {
    struct json_object *v;
    return json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string) ? json_object_get_string(v) : NULL;
}

static int has(struct json_object *o, const char *key, json_type type, struct json_object **out) {
    return json_object_object_get_ex(o, key, out) && json_object_is_type(*out, type);
}

static void free_credits(lp_mary_message *msg) {
    free(msg->spans);
    if (msg->contribution) json_object_put(msg->contribution);
    if (msg->retrieved) json_object_put(msg->retrieved);
    msg->spans = NULL;
    msg->span_count = msg->owner_count = 0;
    msg->contribution = msg->retrieved = NULL;
    msg->highlighted_ms = 0;
}

void lp_mary_message_credit(lp_mary_message *msg, void *contribution_v, void *retrieved_v) {
    struct json_object *contribution = contribution_v, *retrieved = retrieved_v, *owners, *v;
    free_credits(msg);
    if (contribution) msg->contribution = json_object_get(contribution);
    if (retrieved) msg->retrieved = json_object_get(retrieved);
    if (!has(contribution, "owners", json_type_array, &owners)) return;
    size_t n = json_object_array_length(owners);
    int cap = 0;
    for (size_t i = 0; i < n && msg->owner_count < LP_MARY_OWNERS; i++) {
        struct json_object *o = json_object_array_get_idx(owners, i), *spans;
        lp_mary_owner *owner = &msg->owners[msg->owner_count];
        memset(owner, 0, sizeof *owner);
        snprintf(owner->thread_id, sizeof owner->thread_id, "%s", str(o, "thread_id") ? str(o, "thread_id") : "");
        snprintf(owner->owner_id, sizeof owner->owner_id, "%s", str(o, "owner_id") ? str(o, "owner_id") : "");
        snprintf(owner->id, sizeof owner->id, "%s|%s", owner->thread_id, owner->owner_id);
        owner->royalty = json_object_object_get_ex(o, "royalty", &v) ? (float)json_object_get_double(v) : 0;
        owner->documents = has(o, "document_ids", json_type_array, &v) ? (int)json_object_array_length(v) : 0;
        if (has(o, "spans", json_type_array, &spans)) {
            for (size_t k = 0; k < json_object_array_length(spans); k++) {
                struct json_object *span = json_object_array_get_idx(spans, k), *lo, *hi;
                if (!json_object_object_get_ex(span, "lower", &lo) || !json_object_object_get_ex(span, "upper", &hi)) continue;
                if (msg->span_count == cap) {
                    cap = cap ? cap * 2 : 8;
                    lp_mary_span *grown = realloc(msg->spans, (size_t)cap * sizeof *grown);
                    if (!grown) break;
                    msg->spans = grown;
                }
                msg->spans[msg->span_count++] = (lp_mary_span){ msg->owner_count, json_object_get_int(lo), json_object_get_int(hi) };
            }
        }
        msg->owner_count++;
    }
    /* reading order, so the stagger sweeps down the reply the same way every time */
    for (int i = 1; i < msg->span_count; i++)
        for (int k = i; k > 0 && msg->spans[k].lower < msg->spans[k - 1].lower; k--) {
            lp_mary_span t = msg->spans[k];
            msg->spans[k] = msg->spans[k - 1];
            msg->spans[k - 1] = t;
        }
}

static lp_mary_message *push(lp_mary *m, lp_mary_role role, const char *text, int streaming) {
    if (m->message_count == LP_MARY_MESSAGES) {
        free(m->messages[0].text);
        free(m->messages[0].note);
        free_credits(&m->messages[0]);
        memmove(m->messages, m->messages + 1, (LP_MARY_MESSAGES - 1) * sizeof *m->messages);
        m->message_count--;
    }
    lp_mary_message *msg = &m->messages[m->message_count];
    memset(msg, 0, sizeof *msg);
    msg->text = strdup(text ? text : "");
    if (!msg->text) return NULL;
    msg->len = strlen(msg->text);
    msg->role = role;
    msg->streaming = streaming;
    msg->note = NULL;
    m->message_count++;
    return msg;
}

static void append(lp_mary_message *msg, const char *text) {
    size_t n = strlen(text);
    char *grown = realloc(msg->text, msg->len + n + 1);
    if (!grown) return;
    memcpy(grown + msg->len, text, n + 1);
    msg->text = grown;
    msg->len += n;
}

static int send_bytes(lp_mary *m, const char *bytes, size_t n);

static int send_object(lp_mary *m, struct json_object *o) {
    size_t len = 0;
    const char *text = json_object_to_json_string_length(o, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE, &len);
    int rc = text ? send_bytes(m, text, len) : -ENOMEM;
    if (rc == 0) rc = send_bytes(m, "\n", 1);
    json_object_put(o);
    return rc;
}

static struct json_object *typed(const char *type) {
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "type", json_object_new_string(type));
    return o;
}

static void skill_invoke(lp_mary *m, struct json_object *msg) {
    const char *call_id = str(msg, "call_id"), *app = str(msg, "app"), *skill = str(msg, "skill");
    if (!call_id) return;
    if (m->on_skill_invoke && app && skill) {
        struct json_object *args;
        const char *json = json_object_object_get_ex(msg, "args", &args) && args
                               ? json_object_to_json_string_ext(args, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE)
                               : "null";
        m->on_skill_invoke(m->desk, call_id, app, skill, json);
        return;
    }
    struct json_object *r = typed("skill.result");
    json_object_object_add(r, "call_id", json_object_new_string(call_id));
    json_object_object_add(r, "ok", json_object_new_boolean(0));
    json_object_object_add(r, "error", json_object_new_string("unknown"));
    send_object(m, r);
}

static void handle(lp_mary *m, struct json_object *msg) {
    const char *type = str(msg, "type");
    struct json_object *v;
    if (!type) return;
    if (strcmp(type, "hello") == 0) {
        const char *state = str(msg, "state");
        for (int i = 0; state && i < (int)(sizeof state_names / sizeof *state_names); i++)
            if (strcmp(state, state_names[i]) == 0) m->state = (lp_mary_state)i;
        if (has(msg, "key_present", json_type_boolean, &v)) m->key_present = json_object_get_boolean(v);
        if (has(msg, "wake", json_type_boolean, &v)) m->wake = json_object_get_boolean(v);
        snprintf(m->voice, sizeof m->voice, "%s", str(msg, "voice") ? str(msg, "voice") : "");
        if (has(msg, "tail", json_type_array, &v)) {
            clear_messages(m);
            size_t n = json_object_array_length(v), first = n > LP_MARY_MESSAGES ? n - LP_MARY_MESSAGES : 0;
            for (size_t i = first; i < n; i++) {
                struct json_object *item = json_object_array_get_idx(v, i);
                const char *role = str(item, "role");
                push(m, role && strcmp(role, "user") == 0 ? LP_MARY_USER : LP_MARY_REPLY, str(item, "content"), 0);
            }
        }
        changed(m, LP_MARY_CHANGED_STATE | LP_MARY_CHANGED_MESSAGES | LP_MARY_CHANGED_KEY);
    } else if (strcmp(type, "state") == 0) {
        const char *state = str(msg, "state");
        for (int i = 0; state && i < (int)(sizeof state_names / sizeof *state_names); i++) {
            if (strcmp(state, state_names[i]) != 0) continue;
            m->state = (lp_mary_state)i;
            if (m->state == LP_MARY_LISTENING || m->state == LP_MARY_IDLE) m->level = 0;
            if (m->state == LP_MARY_LISTENING) m->partial[0] = 0;
            changed(m, LP_MARY_CHANGED_STATE);
        }
    } else if (strcmp(type, "wake") == 0) {
        changed(m, LP_MARY_WAKE);
    } else if (strcmp(type, "level") == 0) {
        if (!json_object_object_get_ex(msg, "rms", &v)) return;
        double rms = json_object_get_double(v);
        m->level = rms < 0 ? 0 : rms > 1 ? 1 : (float)rms;
        changed(m, LP_MARY_CHANGED_LEVEL);
    } else if (strcmp(type, "transcript") == 0) {
        const char *text = str(msg, "text");
        if (!text) return;
        if (has(msg, "final", json_type_boolean, &v) && json_object_get_boolean(v)) {
            push(m, LP_MARY_USER, text, 0);
            m->partial[0] = 0;
        } else {
            snprintf(m->partial, sizeof m->partial, "%s", text);
        }
        changed(m, LP_MARY_CHANGED_MESSAGES);
    } else if (strcmp(type, "reply.delta") == 0) {
        const char *text = str(msg, "text");
        if (!text) return;
        lp_mary_message *last = m->message_count ? &m->messages[m->message_count - 1] : NULL;
        if (last && last->role == LP_MARY_REPLY && last->streaming) append(last, text);
        else push(m, LP_MARY_REPLY, text, 1);
        changed(m, LP_MARY_CHANGED_MESSAGES);
    } else if (strcmp(type, "reply.end") == 0) {
        for (int i = 0; i < m->message_count; i++) m->messages[i].streaming = 0;
        lp_mary_message *last = m->message_count ? &m->messages[m->message_count - 1] : NULL;
        struct json_object *contribution = NULL, *retrieved = NULL;
        if (has(msg, "contribution", json_type_object, &contribution) && last && last->role == LP_MARY_REPLY) {
            has(msg, "retrieved", json_type_array, &retrieved);
            lp_mary_message_credit(last, contribution, retrieved);
        }
        changed(m, LP_MARY_CHANGED_MESSAGES);
    } else if (strcmp(type, "key.status") == 0) {
        if (has(msg, "present", json_type_boolean, &v)) m->key_present = json_object_get_boolean(v);
        m->key_verified_at = json_object_object_get_ex(msg, "verified_at", &v) ? json_object_get_int64(v) : 0;
        m->key_check = has(msg, "ok", json_type_boolean, &v) ? json_object_get_boolean(v) : -1;
        snprintf(m->key_message, sizeof m->key_message, "%s", str(msg, "message") ? str(msg, "message") : "");
        changed(m, LP_MARY_CHANGED_KEY);
    } else if (strcmp(type, "error") == 0) {
        snprintf(m->error_stage, sizeof m->error_stage, "%s", str(msg, "stage") ? str(msg, "stage") : "error");
        snprintf(m->error, sizeof m->error, "%s", str(msg, "message") ? str(msg, "message") : "");
        lp_mary_message *last = m->message_count ? &m->messages[m->message_count - 1] : NULL;
        if ((strcmp(m->error_stage, "speech") == 0 || strcmp(m->error_stage, "speaker") == 0) && last && last->role == LP_MARY_REPLY) {
            free(last->note);                   /* the words arrived; this says why they were not heard */
            last->note = strdup(m->error);
            changed(m, LP_MARY_CHANGED_ERROR | LP_MARY_CHANGED_MESSAGES);
        } else {
            changed(m, LP_MARY_CHANGED_ERROR);
        }
    } else if (strcmp(type, "voices") == 0) {
        struct json_object *list;
        int ok = has(msg, "ok", json_type_boolean, &v) && json_object_get_boolean(v);
        if (ok && has(msg, "voices", json_type_array, &list)) {
            m->voice_count = 0;
            size_t n = json_object_array_length(list);
            for (size_t i = 0; i < n && m->voice_count < LP_MARY_VOICES; i++) {
                struct json_object *item = json_object_array_get_idx(list, i), *languages;
                const char *id = str(item, "voice_id"), *name = str(item, "name");
                if (!id || !*id) continue;
                lp_mary_voice *voice = &m->voices[m->voice_count++];
                memset(voice, 0, sizeof *voice);
                snprintf(voice->id, sizeof voice->id, "%s", id);
                snprintf(voice->name, sizeof voice->name, "%s", name && *name ? name : id);
                if (has(item, "languages", json_type_array, &languages) && json_object_array_length(languages)) {
                    const char *language = json_object_get_string(json_object_array_get_idx(languages, 0));
                    snprintf(voice->language, sizeof voice->language, "%s", language ? language : "");
                }
                voice->custom = has(item, "custom", json_type_boolean, &v) && json_object_get_boolean(v);
            }
            m->voices_state = LP_MARY_VOICES_LISTED;
            m->voices_message[0] = 0;
        } else {
            m->voices_state = LP_MARY_VOICES_FAILED;
            snprintf(m->voices_message, sizeof m->voices_message, "%s", str(msg, "message") ? str(msg, "message") : "Mistral’s voices could not be listed.");
        }
        changed(m, LP_MARY_CHANGED_VOICES);
    } else if (strcmp(type, "voice.sample") == 0) {
        const char *state = str(msg, "state");
        m->sample_state = !state ? LP_MARY_SAMPLE_NONE
                        : strcmp(state, "asking") == 0 ? LP_MARY_SAMPLE_ASKING
                        : strcmp(state, "playing") == 0 ? LP_MARY_SAMPLE_PLAYING
                        : strcmp(state, "done") == 0 ? LP_MARY_SAMPLE_DONE : LP_MARY_SAMPLE_FAILED;
        snprintf(m->sample_message, sizeof m->sample_message, "%s", str(msg, "message") ? str(msg, "message") : "");
        changed(m, LP_MARY_CHANGED_SAMPLE);
    } else if (strcmp(type, "skill.invoke") == 0) {
        skill_invoke(m, msg);
    } else if (strcmp(type, "world.request") == 0) {
        if (m->on_world_request) m->on_world_request(m->desk);
    } else if (strcmp(type, "app.state") == 0) {
        const char *call_id = str(msg, "call_id"), *app = str(msg, "app");
        if (m->on_app_state && call_id) m->on_app_state(m->desk, call_id, app);
        else if (call_id) {
            struct json_object *r = typed("app.state.result");
            json_object_object_add(r, "call_id", json_object_new_string(call_id));
            json_object_object_add(r, "ok", json_object_new_boolean(0));
            json_object_object_add(r, "error", json_object_new_string("unknown"));
            send_object(m, r);
        }
    } else if (strcmp(type, "ambient") == 0) {
        if (has(msg, "state", json_type_object, &v)) {
            if (m->ambient) json_object_put(m->ambient);
            m->ambient = json_object_get(v);
            changed(m, LP_MARY_CHANGED_AMBIENT);
        }
    } else if (strcmp(type, "trace") == 0) {
        if (has(msg, "records", json_type_array, &v)) {
            if (m->trace) json_object_put(m->trace);
            m->trace = json_object_get(v);
            changed(m, LP_MARY_CHANGED_TRACE);
        }
    } else if (strcmp(type, "trace.report") == 0) {
        free(m->trace_report);
        m->trace_report = strdup(str(msg, "text") ? str(msg, "text") : "");
        changed(m, LP_MARY_CHANGED_TRACE);
    }
}

static void line(lp_mary *m, char *text, size_t len) {
    if (len && text[len - 1] == '\r') len--;
    if (!len) return;
    struct json_tokener *tok = json_tokener_new();
    struct json_object *msg = tok ? json_tokener_parse_ex(tok, text, (int)len) : NULL;
    if (msg && (json_tokener_get_error(tok) != json_tokener_success || json_tokener_get_parse_end(tok) != len)) {
        json_object_put(msg);
        msg = NULL;
    }
    if (tok) json_tokener_free(tok);
    if (msg && json_object_is_type(msg, json_type_object)) handle(m, msg);
    if (msg) json_object_put(msg);
}

void lp_mary_feed(lp_mary *m, const char *bytes, size_t n) {
    while (n) {
        const char *nl = memchr(bytes, '\n', n);
        size_t take = nl ? (size_t)(nl - bytes) : n;
        if (!m->discarding && m->in_len + take > LP_MARY_LINE_MAX) {
            m->discarding = 1;      /* a line past maryd's cap: skip to its end */
            m->in_len = 0;
        }
        if (!m->discarding) {
            if (m->in_len + take + 1 > m->in_cap) {
                size_t cap = m->in_cap ? m->in_cap : 4096;
                while (cap < m->in_len + take + 1) cap *= 2;
                char *grown = realloc(m->in, cap);
                if (!grown) return;
                m->in = grown;
                m->in_cap = cap;
            }
            memcpy(m->in + m->in_len, bytes, take);
            m->in_len += take;
        }
        if (!nl) return;
        if (!m->discarding) {
            m->in[m->in_len] = 0;
            line(m, m->in, m->in_len);
        }
        m->in_len = 0;
        m->discarding = 0;
        bytes = nl + 1;
        n -= take + 1;
    }
}

static int on_socket(int fd, uint32_t mask, void *data);

static void arm(lp_mary *m, int writable) {
    if (m->fd < 0 || m->writable_armed == writable) return;
    if (m->source) lp_desktop_remove_source(m->desk, m->source);
    m->source = lp_desktop_add_fd(m->desk, m->fd, LP_SOURCE_READABLE | (writable ? LP_SOURCE_WRITABLE : 0), on_socket, m);
    m->writable_armed = writable;
}

static int flush(lp_mary *m) {
    while (m->out_len) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(m->fd, m->out, m->out_len, MSG_NOSIGNAL);
#else
        ssize_t n = send(m->fd, m->out, m->out_len, 0);
#endif
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                arm(m, 1);
                return 0;
            }
            disconnect(m, 1);
            return -EPIPE;
        }
        memmove(m->out, m->out + n, m->out_len - (size_t)n);
        m->out_len -= (size_t)n;
        wipe(m->out + m->out_len, (size_t)n);
    }
    arm(m, 0);
    return 0;
}

static int send_bytes(lp_mary *m, const char *bytes, size_t n) {
    if (m->fd < 0) return -ENOTCONN;
    if (m->out_len + n > m->out_cap) {
        size_t cap = m->out_cap ? m->out_cap : 4096;
        while (cap < m->out_len + n) cap *= 2;
        char *grown = calloc(1, cap);      /* past out_len the buffer is always zero */
        if (!grown) return -ENOMEM;
        if (m->out) {
            memcpy(grown, m->out, m->out_len);
            wipe(m->out, m->out_cap);
            free(m->out);
        }
        m->out = grown;
        m->out_cap = cap;
    }
    memcpy(m->out + m->out_len, bytes, n);
    m->out_len += n;
    return flush(m);
}

static int on_socket(int fd, uint32_t mask, void *data) {
    lp_mary *m = data;
    if ((mask & LP_SOURCE_WRITABLE) && flush(m) < 0) return 0;
    if (!(mask & (LP_SOURCE_READABLE | LP_SOURCE_HANGUP | LP_SOURCE_ERROR))) return 0;
    char buf[16384];
    while (m->fd == fd) {
        ssize_t r = read(fd, buf, sizeof buf);
        if (r > 0) {
            lp_mary_feed(m, buf, (size_t)r);
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        disconnect(m, 1);
        break;
    }
    return 0;
}

static int try_connect(lp_mary *m) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        schedule_retry(m);
        return -errno;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    size_t path_len = strlen(m->path);
    if (path_len >= sizeof addr.sun_path) {
        close(fd);
        return -ENAMETOOLONG;
    }
    memcpy(addr.sun_path, m->path, path_len + 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        int e = errno;
        close(fd);
        schedule_retry(m);
        return -e;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    m->source = lp_desktop_add_fd(m->desk, fd, LP_SOURCE_READABLE, on_socket, m);
    if (!m->source) {
        close(fd);
        return -ENOSYS;
    }
    m->fd = fd;
    m->writable_armed = 0;
    m->retry_ms = LP_MARY_RETRY_MIN_MS;
    changed(m, LP_MARY_CHANGED_CONNECTION);
    return 0;
}

int lp_mary_start(lp_mary *m, const char *path) {
    const char *explicit_path = getenv("MARY_SOCKET"), *runtime = getenv("XDG_RUNTIME_DIR");
    if (path) snprintf(m->path, sizeof m->path, "%s", path);
    else if (explicit_path && *explicit_path) snprintf(m->path, sizeof m->path, "%s", explicit_path);
    else if (runtime && *runtime) snprintf(m->path, sizeof m->path, "%s/mary/mary.sock", runtime);
    else return -ENOENT;
    if (!m->desk || !m->desk->add_fd || !m->desk->add_timer) return -ENOSYS;
    m->started = 1;
    if (m->fd < 0) try_connect(m);
    return 0;
}

int lp_mary_ask(lp_mary *m, const char *text) {
    if (!text || !*text) return -EINVAL;
    if (m->fd < 0) return -ENOTCONN;
    struct json_object *o = typed("ask");
    json_object_object_add(o, "text", json_object_new_string(text));
    return send_object(m, o);
}

static int simple(lp_mary *m, const char *type) {
    if (m->fd < 0) return -ENOTCONN;
    return send_object(m, typed(type));
}

int lp_mary_listen(lp_mary *m) { return simple(m, "listen"); }
int lp_mary_ambient_state(lp_mary *m) { return simple(m, "ambient.state"); }
int lp_mary_list_trace(lp_mary *m) { return simple(m, "trace.list"); }
int lp_mary_trace_report(lp_mary *m) { return simple(m, "trace.report"); }
int lp_mary_stop(lp_mary *m) { return simple(m, "stop"); }
int lp_mary_dismiss(lp_mary *m) { return simple(m, "dismiss"); }
int lp_mary_verify_key(lp_mary *m) { return simple(m, "key.verify"); }

int lp_mary_send_config(lp_mary *m, int wake, const char *voice) {
    if (m->fd < 0) return -ENOTCONN;
    struct json_object *o = typed("config");
    if (wake >= 0) json_object_object_add(o, "wake", json_object_new_boolean(wake != 0));
    if (voice && *voice) json_object_object_add(o, "voice", json_object_new_string(voice));
    return send_object(m, o);
}

int lp_mary_set_wake(lp_mary *m, int on) { return lp_mary_send_config(m, on != 0, NULL); }

int lp_mary_list_voices(lp_mary *m) {
    int rc = simple(m, "voices.list");
    if (rc == 0) {
        m->voices_state = LP_MARY_VOICES_ASKING;
        changed(m, LP_MARY_CHANGED_VOICES);
    }
    return rc;
}

int lp_mary_sample_voice(lp_mary *m, const char *voice_id, const char *text) {
    if (!voice_id || !*voice_id) return -EINVAL;
    if (m->fd < 0) return -ENOTCONN;
    struct json_object *o = typed("voice.sample");
    json_object_object_add(o, "voice_id", json_object_new_string(voice_id));
    if (text && *text) json_object_object_add(o, "text", json_object_new_string(text));
    int rc = send_object(m, o);
    if (rc == 0) {
        m->sample_state = LP_MARY_SAMPLE_ASKING;
        m->sample_message[0] = 0;
        changed(m, LP_MARY_CHANGED_SAMPLE);
    }
    return rc;
}

int lp_mary_set_key(lp_mary *m, char *key, size_t len) {
    static const char head[] = "{\"type\":\"key.set\",\"key\":\"", tail[] = "\"}\n";
    int ok = key && len > 0 && len <= 256;
    for (size_t i = 0; ok && i < len; i++) {
        char c = key[i];
        ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    }
    int rc = !ok ? -EINVAL : m->fd < 0 ? -ENOTCONN : 0;
    if (rc == 0) {
        char line[sizeof head + 256 + sizeof tail];
        size_t n = 0;
        memcpy(line, head, sizeof head - 1);
        n += sizeof head - 1;
        memcpy(line + n, key, len);
        n += len;
        memcpy(line + n, tail, sizeof tail - 1);
        n += sizeof tail - 1;
        rc = send_bytes(m, line, n);
        wipe(line, sizeof line);
    }
    if (key) wipe(key, len);
    return rc;
}

int lp_mary_send_line(lp_mary *m, const char *json) {
    if (!json || !*json || strchr(json, '\n')) return -EINVAL;
    if (m->fd < 0) return -ENOTCONN;
    int rc = send_bytes(m, json, strlen(json));
    return rc == 0 ? send_bytes(m, "\n", 1) : rc;
}

#else

int lp_mary_available(void) { return 0; }
static void free_credits(lp_mary_message *msg) {
    free(msg->spans);
    msg->spans = NULL;
    msg->span_count = msg->owner_count = 0;
    msg->contribution = msg->retrieved = NULL;
}
void lp_mary_message_credit(lp_mary_message *msg, void *contribution, void *retrieved) { (void)contribution; (void)retrieved; free_credits(msg); }
int lp_mary_start(lp_mary *m, const char *path) { return -ENOSYS; }
int lp_mary_ask(lp_mary *m, const char *text) { return -ENOTCONN; }
int lp_mary_listen(lp_mary *m) { return -ENOTCONN; }
int lp_mary_ambient_state(lp_mary *m) { return -ENOTCONN; }
int lp_mary_list_trace(lp_mary *m) { return -ENOTCONN; }
int lp_mary_trace_report(lp_mary *m) { return -ENOTCONN; }
int lp_mary_stop(lp_mary *m) { return -ENOTCONN; }
int lp_mary_dismiss(lp_mary *m) { return -ENOTCONN; }
int lp_mary_verify_key(lp_mary *m) { return -ENOTCONN; }
int lp_mary_set_wake(lp_mary *m, int on) { return -ENOTCONN; }
int lp_mary_send_config(lp_mary *m, int wake, const char *voice) { return -ENOTCONN; }
int lp_mary_list_voices(lp_mary *m) { return -ENOTCONN; }
int lp_mary_sample_voice(lp_mary *m, const char *voice_id, const char *text) { return -ENOTCONN; }
int lp_mary_set_key(lp_mary *m, char *key, size_t len) {
    if (key) wipe(key, len);
    return -ENOTCONN;
}
int lp_mary_send_line(lp_mary *m, const char *json) { return -ENOTCONN; }
void lp_mary_feed(lp_mary *m, const char *bytes, size_t n) {}
static int try_connect(lp_mary *m) { return -ENOSYS; }
#endif
