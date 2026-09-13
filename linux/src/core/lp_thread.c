/* The desktop's client for threadd (lp_thread.h). */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_thread.h"

static const char *const kind_names[LP_THREAD_KIND_COUNT] = {
    "stats", "schemas", "library", "documents", "search", "graph", "graph.mutate", "graph.reextract", "ledger", "parity", "file.record", "policy",
};

const char *lp_thread_kind_name(enum lp_thread_kind kind) { return (unsigned)kind < LP_THREAD_KIND_COUNT ? kind_names[kind] : "?"; }

static const unsigned changed_bits[LP_THREAD_KIND_COUNT] = {
    LP_THREAD_CHANGED_STATS, LP_THREAD_CHANGED_SCHEMAS, LP_THREAD_CHANGED_LIBRARY, LP_THREAD_CHANGED_DOCUMENTS, LP_THREAD_CHANGED_SEARCH,
    LP_THREAD_CHANGED_GRAPH, LP_THREAD_CHANGED_MUTATION, LP_THREAD_CHANGED_MUTATION, LP_THREAD_CHANGED_LEDGER, LP_THREAD_CHANGED_PARITY,
    LP_THREAD_CHANGED_FILE, LP_THREAD_CHANGED_POLICY,
};

static int64_t wall_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void lp_thread_init(lp_thread *t, struct lp_desktop *d) {
    memset(t, 0, sizeof *t);
    t->desk = d;
    t->fd = -1;
    t->retry_ms = LP_THREAD_RETRY_MIN_MS;
}

int lp_thread_connected(const lp_thread *t) { return t->fd >= 0; }

static void changed(lp_thread *t, unsigned what) {
    if (t->desk) lp_desktop_models_changed(t->desk, LP_MODEL_THREAD, what);
}

static void disconnect(lp_thread *t, int retry);

#ifdef HAVE_JSONC
#include <json-c/json.h>

int lp_thread_available(void) { return 1; }

struct json_object *lp_thread_answer(const lp_thread *t, enum lp_thread_kind kind) {
    return (unsigned)kind < LP_THREAD_KIND_COUNT ? t->answers[kind] : NULL;
}

static void drop_answers(lp_thread *t) {
    for (int i = 0; i < LP_THREAD_KIND_COUNT; i++) {
        if (t->answers[i]) json_object_put(t->answers[i]);
        t->answers[i] = NULL;
    }
}
#else
int lp_thread_available(void) { return 0; }
struct json_object *lp_thread_answer(const lp_thread *t, enum lp_thread_kind kind) { return NULL; }
static void drop_answers(lp_thread *t) { (void)t; }
#endif

void lp_thread_free(lp_thread *t) {
    t->started = 0;
    disconnect(t, 0);
    if (t->retry) lp_desktop_remove_source(t->desk, t->retry);
    t->retry = NULL;
    drop_answers(t);
    free(t->in);
    free(t->out);
    t->in = t->out = NULL;
    t->in_cap = t->out_cap = 0;
}

static int try_connect(lp_thread *t);

static int on_retry(int fd, uint32_t mask, void *data) {
    try_connect(data);
    return 0;
}

static void schedule_retry(lp_thread *t) {
    if (!t->started || !t->desk) return;
    if (!t->retry) t->retry = lp_desktop_add_timer(t->desk, 0, on_retry, t);
    if (!t->retry) return;
    lp_desktop_update_timer(t->desk, t->retry, t->retry_ms);
    t->retry_ms = t->retry_ms * 2 > LP_THREAD_RETRY_MAX_MS ? LP_THREAD_RETRY_MAX_MS : t->retry_ms * 2;
}

static void disconnect(lp_thread *t, int retry) {
    if (t->fd < 0) return;
    if (t->source) lp_desktop_remove_source(t->desk, t->source);
    t->source = NULL;
    close(t->fd);
    t->fd = -1;
    t->in_len = 0;
    t->discarding = 0;
    t->out_len = 0;
    t->writable_armed = 0;
    t->pending_count = 0;
    memset(t->asking, 0, sizeof t->asking);
    changed(t, LP_THREAD_CHANGED_CONNECTION);
    if (retry) schedule_retry(t);
}

#ifdef HAVE_JSONC

static const char *str(struct json_object *o, const char *key) {
    struct json_object *v;
    return json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string) ? json_object_get_string(v) : NULL;
}

/* The kind a reply answers: its type when it is one of ours, else the oldest outstanding request. */
static int kind_of(lp_thread *t, const char *type) {
    for (int k = 0; k < LP_THREAD_KIND_COUNT; k++) {
        size_t n = strlen(kind_names[k]);
        if (type && strncmp(type, kind_names[k], n) == 0 && strcmp(type + n, ".result") == 0) return k;
    }
    return t->pending_count ? t->pending[t->pending_head] : -1;
}

static void pop_pending(lp_thread *t) {
    if (!t->pending_count) return;
    t->pending_head = (t->pending_head + 1) % LP_THREAD_PENDING;
    t->pending_count--;
}

static void handle(lp_thread *t, struct json_object *msg) {
    const char *type = str(msg, "type");
    int kind = kind_of(t, type);
    pop_pending(t);
    if (kind < 0) return;
    t->asking[kind] = 0;
    if (type && strcmp(type, "error") == 0) {
        const char *message = str(msg, "message");
        snprintf(t->error, sizeof t->error, "%s", message ? message : "threadd refused the request");
        t->error_kind = (enum lp_thread_kind)kind;
        changed(t, LP_THREAD_CHANGED_ERROR);
        return;
    }
    if (t->answers[kind]) json_object_put(t->answers[kind]);
    t->answers[kind] = json_object_get(msg);
    t->answered_ms[kind] = wall_ms();
    t->error[0] = 0;
    changed(t, changed_bits[kind]);
}

static void line(lp_thread *t, char *text, size_t len) {
    if (len && text[len - 1] == '\r') len--;
    if (!len) return;
    struct json_tokener *tok = json_tokener_new();
    struct json_object *msg = tok ? json_tokener_parse_ex(tok, text, (int)len) : NULL;
    if (msg && (json_tokener_get_error(tok) != json_tokener_success || json_tokener_get_parse_end(tok) != len)) {
        json_object_put(msg);
        msg = NULL;
    }
    if (tok) json_tokener_free(tok);
    if (msg && json_object_is_type(msg, json_type_object)) handle(t, msg);
    if (msg) json_object_put(msg);
}

#else
static void line(lp_thread *t, char *text, size_t len) { (void)t; (void)text; (void)len; }
#endif

void lp_thread_feed(lp_thread *t, const char *bytes, size_t n) {
    while (n) {
        const char *nl = memchr(bytes, '\n', n);
        size_t take = nl ? (size_t)(nl - bytes) : n;
        if (!t->discarding && t->in_len + take > LP_THREAD_LINE_MAX) {
            t->discarding = 1;
            t->in_len = 0;
        }
        if (!t->discarding) {
            if (t->in_len + take + 1 > t->in_cap) {
                size_t cap = t->in_cap ? t->in_cap : 4096;
                while (cap < t->in_len + take + 1) cap *= 2;
                char *grown = realloc(t->in, cap);
                if (!grown) return;
                t->in = grown;
                t->in_cap = cap;
            }
            memcpy(t->in + t->in_len, bytes, take);
            t->in_len += take;
        }
        if (!nl) return;
        if (!t->discarding) {
            t->in[t->in_len] = 0;
            line(t, t->in, t->in_len);
        }
        t->in_len = 0;
        t->discarding = 0;
        bytes = nl + 1;
        n -= take + 1;
    }
}

static int on_socket(int fd, uint32_t mask, void *data);

static void arm(lp_thread *t, int writable) {
    if (t->fd < 0 || t->writable_armed == writable) return;
    if (t->source) lp_desktop_remove_source(t->desk, t->source);
    t->source = lp_desktop_add_fd(t->desk, t->fd, LP_SOURCE_READABLE | (writable ? LP_SOURCE_WRITABLE : 0), on_socket, t);
    t->writable_armed = writable;
}

static int flush(lp_thread *t) {
    while (t->out_len) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(t->fd, t->out, t->out_len, MSG_NOSIGNAL);
#else
        ssize_t n = send(t->fd, t->out, t->out_len, 0);
#endif
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                arm(t, 1);
                return 0;
            }
            disconnect(t, 1);
            return -EPIPE;
        }
        memmove(t->out, t->out + n, t->out_len - (size_t)n);
        t->out_len -= (size_t)n;
    }
    arm(t, 0);
    return 0;
}

static int send_bytes(lp_thread *t, const char *bytes, size_t n) {
    if (t->fd < 0) return -ENOTCONN;
    if (t->out_len + n > t->out_cap) {
        size_t cap = t->out_cap ? t->out_cap : 4096;
        while (cap < t->out_len + n) cap *= 2;
        char *grown = realloc(t->out, cap);
        if (!grown) return -ENOMEM;
        t->out = grown;
        t->out_cap = cap;
    }
    memcpy(t->out + t->out_len, bytes, n);
    t->out_len += n;
    return flush(t);
}

int lp_thread_send_line(lp_thread *t, enum lp_thread_kind kind, const char *json) {
    if (!json || !*json) return -EINVAL;
    if (t->fd < 0) return -ENOTCONN;
    if (t->pending_count == LP_THREAD_PENDING) return -EBUSY;
    t->pending[(t->pending_head + t->pending_count) % LP_THREAD_PENDING] = (unsigned char)kind;
    t->pending_count++;
    t->asking[kind] = 1;
    int rc = send_bytes(t, json, strlen(json));
    if (rc == 0) rc = send_bytes(t, "\n", 1);
    return rc;
}

static int on_socket(int fd, uint32_t mask, void *data) {
    lp_thread *t = data;
    if ((mask & LP_SOURCE_WRITABLE) && flush(t) < 0) return 0;
    if (!(mask & (LP_SOURCE_READABLE | LP_SOURCE_HANGUP | LP_SOURCE_ERROR))) return 0;
    char buf[65536];
    while (t->fd == fd) {
        ssize_t r = read(fd, buf, sizeof buf);
        if (r > 0) {
            lp_thread_feed(t, buf, (size_t)r);
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        disconnect(t, 1);
        break;
    }
    return 0;
}

static int try_connect(lp_thread *t) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        schedule_retry(t);
        return -errno;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    size_t path_len = strlen(t->path);
    if (path_len >= sizeof addr.sun_path) {
        close(fd);
        return -ENAMETOOLONG;
    }
    memcpy(addr.sun_path, t->path, path_len + 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        int e = errno;
        close(fd);
        schedule_retry(t);
        return -e;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    t->source = lp_desktop_add_fd(t->desk, fd, LP_SOURCE_READABLE, on_socket, t);
    if (!t->source) {
        close(fd);
        return -ENOSYS;
    }
    t->fd = fd;
    t->writable_armed = 0;
    t->retry_ms = LP_THREAD_RETRY_MIN_MS;
    changed(t, LP_THREAD_CHANGED_CONNECTION);
    return 0;
}

int lp_thread_start(lp_thread *t, const char *path) {
    const char *explicit_path = getenv("THREAD_LOCAL_SOCKET");
    if (path) snprintf(t->path, sizeof t->path, "%s", path);
    else if (explicit_path && *explicit_path) snprintf(t->path, sizeof t->path, "%s", explicit_path);
    else snprintf(t->path, sizeof t->path, "/run/thread/local.sock");
    if (!t->desk || !t->desk->add_fd || !t->desk->add_timer) return -ENOSYS;
    if (!lp_thread_available()) return -ENOSYS;
    t->started = 1;
    if (t->fd < 0) try_connect(t);
    return 0;
}

/* MARK: - Requests */

#ifdef HAVE_JSONC
static struct json_object *typed(const char *type) {
    struct json_object *o = json_object_new_object();
    json_object_object_add(o, "type", json_object_new_string(type));
    json_object_object_add(o, "source", json_object_new_string("desktop"));
    return o;
}

static struct json_object *strings(const char *const *v, int n) {
    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < n; i++) if (v[i]) json_object_array_add(arr, json_object_new_string(v[i]));
    return arr;
}

static int send_object(lp_thread *t, enum lp_thread_kind kind, struct json_object *o) {
    const char *text = json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE);
    int rc = lp_thread_send_line(t, kind, text);
    json_object_put(o);
    return rc;
}

int lp_thread_stats(lp_thread *t) { return send_object(t, LP_THREAD_STATS, typed("stats")); }
int lp_thread_schemas(lp_thread *t) { return send_object(t, LP_THREAD_SCHEMAS, typed("schemas")); }
int lp_thread_parity(lp_thread *t) { return send_object(t, LP_THREAD_PARITY, typed("parity")); }
int lp_thread_policy(lp_thread *t) { return send_object(t, LP_THREAD_POLICY, typed("policy")); }

int lp_thread_library(lp_thread *t, int limit, const char *after) {
    struct json_object *o = typed("library");
    if (limit > 0) json_object_object_add(o, "limit", json_object_new_int(limit));
    if (after && *after) json_object_object_add(o, "after", json_object_new_string(after));
    return send_object(t, LP_THREAD_LIBRARY, o);
}

int lp_thread_documents(lp_thread *t, const char *const *ids, int n) {
    if (n <= 0) return -EINVAL;
    struct json_object *o = typed("documents");
    json_object_object_add(o, "ids", strings(ids, n));
    return send_object(t, LP_THREAD_DOCUMENTS, o);
}

int lp_thread_search(lp_thread *t, const char *query, const char *const *lanes, int n_lanes) {
    if (!query || !*query) return -EINVAL;
    struct json_object *o = typed("search");
    json_object_object_add(o, "query", json_object_new_string(query));
    if (n_lanes > 0) json_object_object_add(o, "lanes", strings(lanes, n_lanes));
    return send_object(t, LP_THREAD_SEARCH, o);
}

int lp_thread_graph(lp_thread *t, const char *entity, const char *query, const char *const *kinds, int n_kinds, int hops, int limit, int documents) {
    struct json_object *o = typed("graph");
    if (entity && *entity) json_object_object_add(o, "entity", json_object_new_string(entity));
    if (query && *query) json_object_object_add(o, "query", json_object_new_string(query));
    if (n_kinds > 0) json_object_object_add(o, "kinds", strings(kinds, n_kinds));
    json_object_object_add(o, "hops", json_object_new_int(hops < 1 ? 1 : hops > 3 ? 3 : hops));
    json_object_object_add(o, "limit", json_object_new_int(limit > 0 ? limit : 20));
    json_object_object_add(o, "documents", json_object_new_boolean(documents));
    return send_object(t, LP_THREAD_GRAPH, o);
}

int lp_thread_mutate(lp_thread *t, const char *op, const char *id, const char *name, const char *into, const char *kind) {
    if (!op || !id) return -EINVAL;
    struct json_object *o = typed("graph.mutate");
    json_object_object_add(o, "op", json_object_new_string(op));
    json_object_object_add(o, "id", json_object_new_string(id));
    if (name) json_object_object_add(o, "name", json_object_new_string(name));
    if (into) json_object_object_add(o, "into", json_object_new_string(into));
    if (kind) json_object_object_add(o, "kind", json_object_new_string(kind));
    return send_object(t, LP_THREAD_MUTATE, o);
}

int lp_thread_reextract(lp_thread *t, const char *document_id) {
    if (!document_id) return -EINVAL;
    struct json_object *o = typed("graph.reextract");
    json_object_object_add(o, "document_id", json_object_new_string(document_id));
    return send_object(t, LP_THREAD_REEXTRACT, o);
}

int lp_thread_ledger(lp_thread *t, const char *kind, int limit) {
    struct json_object *o = typed("ledger");
    if (kind && *kind) json_object_object_add(o, "kind", json_object_new_string(kind));
    json_object_object_add(o, "limit", json_object_new_int(limit > 0 ? limit : 100));
    return send_object(t, LP_THREAD_LEDGER, o);
}

int lp_thread_file_record(lp_thread *t, const char *path) {
    if (!path || !*path) return -EINVAL;
    snprintf(t->file_path, sizeof t->file_path, "%s", path);
    struct json_object *o = typed("file.record");
    json_object_object_add(o, path[0] == '/' ? "path" : "id", json_object_new_string(path));
    return send_object(t, LP_THREAD_FILE_RECORD, o);
}
#else
int lp_thread_stats(lp_thread *t) { return -ENOSYS; }
int lp_thread_schemas(lp_thread *t) { return -ENOSYS; }
int lp_thread_parity(lp_thread *t) { return -ENOSYS; }
int lp_thread_policy(lp_thread *t) { return -ENOSYS; }
int lp_thread_library(lp_thread *t, int limit, const char *after) { return -ENOSYS; }
int lp_thread_documents(lp_thread *t, const char *const *ids, int n) { return -ENOSYS; }
int lp_thread_search(lp_thread *t, const char *query, const char *const *lanes, int n_lanes) { return -ENOSYS; }
int lp_thread_graph(lp_thread *t, const char *entity, const char *query, const char *const *kinds, int n_kinds, int hops, int limit, int documents) { return -ENOSYS; }
int lp_thread_mutate(lp_thread *t, const char *op, const char *id, const char *name, const char *into, const char *kind) { return -ENOSYS; }
int lp_thread_reextract(lp_thread *t, const char *document_id) { return -ENOSYS; }
int lp_thread_ledger(lp_thread *t, const char *kind, int limit) { return -ENOSYS; }
int lp_thread_file_record(lp_thread *t, const char *path) { return -ENOSYS; }
#endif
