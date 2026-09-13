/* The Thread in the desktop: the hard drive's own memory (MaryPi's linux/mary/thread,
 * threadd), read over its local socket at /run/thread/local.sock with newline-delimited
 * JSON — one request line, one {"type": "<op>.result", …} or {"type": "error", …} line
 * back, in order, on one connection:
 *
 *   desktop → threadd   stats, schemas, library{limit, after}, documents{ids}, search{query, lanes},
 *                       graph{entity, query, kinds, hops, limit, documents}, graph.mutate{op, id, name,
 *                       into, kind}, graph.reextract{document_id}, ledger{kind, limit}, parity,
 *                       file.record{path}, policy
 *
 * The client keeps the last answer of each kind as a json-c object the Thread app, Disk
 * Utility, the Finder and Get Info read, and tells every window through
 * lp_desktop_models_changed(LP_MODEL_THREAD, LP_THREAD_CHANGED_*). It rides the host's event
 * sources and reconnects with backoff. Linux only (PARITY D25). Without json-c it compiles
 * out: lp_thread_available() is 0. */
#ifndef MARYUI_LP_THREAD_H
#define MARYUI_LP_THREAD_H

#include <stddef.h>
#include <stdint.h>

struct lp_desktop;
struct lp_source;
struct json_object;

#define LP_THREAD_LINE_MAX (16u << 20)
#define LP_THREAD_RETRY_MIN_MS 250
#define LP_THREAD_RETRY_MAX_MS 5000
#define LP_THREAD_PENDING 64

enum lp_thread_kind {
    LP_THREAD_STATS, LP_THREAD_SCHEMAS, LP_THREAD_LIBRARY, LP_THREAD_DOCUMENTS, LP_THREAD_SEARCH, LP_THREAD_GRAPH,
    LP_THREAD_MUTATE, LP_THREAD_REEXTRACT, LP_THREAD_LEDGER, LP_THREAD_PARITY, LP_THREAD_FILE_RECORD, LP_THREAD_POLICY,
    LP_THREAD_KIND_COUNT,
};

enum {
    LP_THREAD_CHANGED_CONNECTION = 1,
    LP_THREAD_CHANGED_STATS = 2,
    LP_THREAD_CHANGED_SCHEMAS = 4,
    LP_THREAD_CHANGED_LIBRARY = 8,
    LP_THREAD_CHANGED_DOCUMENTS = 16,
    LP_THREAD_CHANGED_SEARCH = 32,
    LP_THREAD_CHANGED_GRAPH = 64,
    LP_THREAD_CHANGED_LEDGER = 128,
    LP_THREAD_CHANGED_PARITY = 256,
    LP_THREAD_CHANGED_FILE = 512,
    LP_THREAD_CHANGED_POLICY = 1024,
    LP_THREAD_CHANGED_ERROR = 2048,     /* the last request failed: lp_thread.error says how */
    LP_THREAD_CHANGED_MUTATION = 4096,  /* a repair or re-extraction was accepted: ask for the graph again */
};

typedef struct lp_thread {
    struct lp_desktop *desk;
    int fd;                             /* -1 while disconnected */
    struct lp_source *source, *retry;
    int writable_armed;
    int retry_ms;
    int started;
    char path[256];

    /* the last answer of each kind (json-c objects the client owns; NULL until one came) */
    struct json_object *answers[LP_THREAD_KIND_COUNT];
    int64_t answered_ms[LP_THREAD_KIND_COUNT];  /* wall clock */
    int asking[LP_THREAD_KIND_COUNT];           /* a request of that kind is out */
    char error[240];
    enum lp_thread_kind error_kind;
    char file_path[1024];               /* the path the last file.record asked about */

    /* what is out, in order (threadd answers in order) */
    unsigned char pending[LP_THREAD_PENDING];
    int pending_head, pending_count;

    char *in;
    size_t in_len, in_cap;
    int discarding;
    char *out;
    size_t out_len, out_cap;
} lp_thread;

int lp_thread_available(void);
void lp_thread_init(lp_thread *t, struct lp_desktop *d);
void lp_thread_free(lp_thread *t);
/* Connects (path NULL: $THREAD_LOCAL_SOCKET, else /run/thread/local.sock) and keeps reconnecting.
 * 0, or -errno when there is no event loop to live on. */
int lp_thread_start(lp_thread *t, const char *path);
int lp_thread_connected(const lp_thread *t);
/* The last answer of a kind, or NULL. Borrowed: valid until the next answer of that kind. */
struct json_object *lp_thread_answer(const lp_thread *t, enum lp_thread_kind kind);
const char *lp_thread_kind_name(enum lp_thread_kind kind);

/* Requests. 0; -ENOTCONN while threadd is away; -EINVAL. */
int lp_thread_stats(lp_thread *t);
int lp_thread_schemas(lp_thread *t);
int lp_thread_library(lp_thread *t, int limit, const char *after);
int lp_thread_documents(lp_thread *t, const char *const *ids, int n);
int lp_thread_search(lp_thread *t, const char *query, const char *const *lanes, int n_lanes);
/* entity or query may be NULL; kinds may be NULL; hops 1…3; documents: include the documents behind the nodes. */
int lp_thread_graph(lp_thread *t, const char *entity, const char *query, const char *const *kinds, int n_kinds, int hops, int limit, int documents);
/* op: rename{id, name} | merge{id, into} | set_kind{id, kind} | delete_entity{id} | delete_relationship{id}. */
int lp_thread_mutate(lp_thread *t, const char *op, const char *id, const char *name, const char *into, const char *kind);
int lp_thread_reextract(lp_thread *t, const char *document_id);
int lp_thread_ledger(lp_thread *t, const char *kind, int limit);
int lp_thread_parity(lp_thread *t);
int lp_thread_file_record(lp_thread *t, const char *path);
int lp_thread_policy(lp_thread *t);
/* One request as a JSON object's text, of a known kind. */
int lp_thread_send_line(lp_thread *t, enum lp_thread_kind kind, const char *json);

/* Bytes from threadd, as the client's reader hands them over (and a test). */
void lp_thread_feed(lp_thread *t, const char *bytes, size_t n);

#endif
