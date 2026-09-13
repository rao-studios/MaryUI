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

static void changed(lp_mary *m, unsigned what) {
    if (m->desk && m->desk->on_mary) m->desk->on_mary(m->desk, what);
    /* and any window showing Mary (Settings › Mary); the microphone's level only moves Spotlight's meter */
    if (m->desk && (what & ~(unsigned)LP_MARY_CHANGED_LEVEL)) lp_desktop_models_changed(m->desk, LP_MODEL_MARY, what);
}

static void clear_messages(lp_mary *m) {
    for (int i = 0; i < m->message_count; i++) free(m->messages[i].text);
    m->message_count = 0;
}

static void disconnect(lp_mary *m, int retry);

void lp_mary_free(lp_mary *m) {
    m->started = 0;
    disconnect(m, 0);
    if (m->retry) lp_desktop_remove_source(m->desk, m->retry);
    m->retry = NULL;
    clear_messages(m);
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

static lp_mary_message *push(lp_mary *m, lp_mary_role role, const char *text, int streaming) {
    if (m->message_count == LP_MARY_MESSAGES) {
        free(m->messages[0].text);
        memmove(m->messages, m->messages + 1, (LP_MARY_MESSAGES - 1) * sizeof *m->messages);
        m->message_count--;
    }
    lp_mary_message *msg = &m->messages[m->message_count];
    msg->text = strdup(text ? text : "");
    if (!msg->text) return NULL;
    msg->len = strlen(msg->text);
    msg->role = role;
    msg->streaming = streaming;
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
        changed(m, LP_MARY_CHANGED_ERROR);
    } else if (strcmp(type, "skill.invoke") == 0) {
        skill_invoke(m, msg);
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
int lp_mary_stop(lp_mary *m) { return simple(m, "stop"); }
int lp_mary_dismiss(lp_mary *m) { return simple(m, "dismiss"); }
int lp_mary_verify_key(lp_mary *m) { return simple(m, "key.verify"); }

int lp_mary_set_wake(lp_mary *m, int on) {
    if (m->fd < 0) return -ENOTCONN;
    struct json_object *o = typed("config");
    json_object_object_add(o, "wake", json_object_new_boolean(on != 0));
    return send_object(m, o);
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
int lp_mary_start(lp_mary *m, const char *path) { return -ENOSYS; }
int lp_mary_ask(lp_mary *m, const char *text) { return -ENOTCONN; }
int lp_mary_listen(lp_mary *m) { return -ENOTCONN; }
int lp_mary_stop(lp_mary *m) { return -ENOTCONN; }
int lp_mary_dismiss(lp_mary *m) { return -ENOTCONN; }
int lp_mary_verify_key(lp_mary *m) { return -ENOTCONN; }
int lp_mary_set_wake(lp_mary *m, int on) { return -ENOTCONN; }
int lp_mary_set_key(lp_mary *m, char *key, size_t len) {
    if (key) wipe(key, len);
    return -ENOTCONN;
}
int lp_mary_send_line(lp_mary *m, const char *json) { return -ENOTCONN; }
void lp_mary_feed(lp_mary *m, const char *bytes, size_t n) {}
static int try_connect(lp_mary *m) { return -ENOSYS; }
#endif
