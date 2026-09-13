/* Mary in the desktop: the conversation Spotlight shows, and the client that keeps it
 * current. maryd (MaryPi's linux/mary/runtime) serves the session on
 * $XDG_RUNTIME_DIR/mary/mary.sock with newline-delimited JSON:
 *
 *   maryd → desktop   hello{state, key_present, wake, voice, tail}, wake, state{state}, level{rms},
 *                     transcript{text, final}, reply.delta{text}, reply.end{cancelled},
 *                     key.status{present, verified_at, ok?, message?}, error{stage, message},
 *                     skill.invoke{call_id, app, skill, args}, voices{ok, voices | message},
 *                     voice.sample{voice_id, state, message?}
 *   desktop → maryd   ask{text}, listen, stop, dismiss, key.set{key}, key.verify,
 *                     config{wake?, voice?}, voices.list, voice.sample{voice_id, text?},
 *                     skills{apps}, skill.result{call_id, ok, result | error}
 *
 * An error whose stage is speech or speaker says why the last reply was not heard: it becomes that reply's
 * note, and Mary's state stays what maryd says it is.
 *
 * The client rides the host's event sources (lp_desktop_add_fd/add_timer) and
 * reconnects with backoff when maryd goes away; the conversation is kept. Every change
 * reaches the host through d->on_mary with LP_MARY_CHANGED_* bits. Linux only (PARITY
 * D18). Without json-c the client compiles out: lp_mary_available() is 0 and Spotlight
 * shows no Ask Mary button. */
#ifndef MARYUI_LP_MARY_H
#define MARYUI_LP_MARY_H

#include <stddef.h>
#include <stdint.h>

struct lp_desktop;
struct lp_source;

#define LP_MARY_MESSAGES 24                 /* the exchanges Spotlight keeps */
#define LP_MARY_LINE_MAX (64u << 10)        /* maryd's line cap */
#define LP_MARY_RETRY_MIN_MS 250
#define LP_MARY_RETRY_MAX_MS 5000
#define LP_MARY_VOICES 48                   /* the voices a list keeps */

typedef enum lp_mary_state {
    LP_MARY_IDLE,
    LP_MARY_LISTENING,
    LP_MARY_HEARING,
    LP_MARY_TRANSCRIBING,
    LP_MARY_THINKING,
    LP_MARY_SPEAKING,
    LP_MARY_ERROR,
} lp_mary_state;

typedef enum lp_mary_role { LP_MARY_USER, LP_MARY_REPLY } lp_mary_role;

typedef struct lp_mary_message {
    lp_mary_role role;
    char *text;             /* heap, NUL-terminated */
    size_t len;
    int streaming;          /* a reply still arriving */
    char *note;             /* heap: why this reply was not heard; NULL when it was */
} lp_mary_message;

/* One of Mistral's voices, as maryd lists them. */
typedef struct lp_mary_voice {
    char id[64];            /* what to speak by: fr_marie_neutral, or an id */
    char name[64];
    char language[16];      /* its first language: fr, en … */
    int custom;             /* the account's own */
} lp_mary_voice;

enum lp_mary_voices_state { LP_MARY_VOICES_UNKNOWN, LP_MARY_VOICES_ASKING, LP_MARY_VOICES_LISTED, LP_MARY_VOICES_FAILED };
enum lp_mary_sample_state { LP_MARY_SAMPLE_NONE, LP_MARY_SAMPLE_ASKING, LP_MARY_SAMPLE_PLAYING, LP_MARY_SAMPLE_DONE,
                            LP_MARY_SAMPLE_FAILED };

enum {
    LP_MARY_CHANGED_STATE = 1,
    LP_MARY_CHANGED_MESSAGES = 2,       /* a message, or the live transcript */
    LP_MARY_CHANGED_LEVEL = 4,
    LP_MARY_CHANGED_CONNECTION = 8,
    LP_MARY_CHANGED_KEY = 16,
    LP_MARY_CHANGED_ERROR = 32,
    LP_MARY_WAKE = 64,                  /* "Hey Mary": open Spotlight, listening */
    LP_MARY_CHANGED_VOICES = 128,       /* the list of voices, or asking for it */
    LP_MARY_CHANGED_SAMPLE = 256,       /* a sample's progress */
};

/* A skill call from maryd (U4 answers it); args_json is "null" when there are none. */
typedef void (*lp_mary_skill_fn)(struct lp_desktop *d, const char *call_id, const char *app, const char *skill,
                                 const char *args_json);

typedef struct lp_mary {
    struct lp_desktop *desk;
    int fd;                             /* -1 while disconnected */
    struct lp_source *source, *retry;
    int writable_armed;
    int retry_ms;
    int started;
    char path[256];

    lp_mary_state state;
    lp_mary_message messages[LP_MARY_MESSAGES];
    int message_count;
    char partial[1024];                 /* what maryd has heard so far */
    float level;                        /* the microphone's RMS, 0…1 */
    int key_present;
    int64_t key_verified_at;            /* ms since the epoch; 0: never */
    int key_check;                      /* the last verify: 1 accepted, 0 refused, -1 none */
    char key_message[160];
    int wake;                           /* maryd listens for "Hey Mary" */
    char error_stage[32];
    char error[240];
    char voice[64];                     /* what maryd speaks in (hello) */
    lp_mary_voice voices[LP_MARY_VOICES];
    int voice_count;
    enum lp_mary_voices_state voices_state;
    char voices_message[160];           /* why the list failed */
    enum lp_mary_sample_state sample_state;
    char sample_message[200];           /* why a sample failed */

    char *in;                           /* a partial line */
    size_t in_len, in_cap;
    int discarding;                     /* inside a line past LP_MARY_LINE_MAX */
    char *out;                          /* unsent bytes; zeroed as they go (key.set) */
    size_t out_len, out_cap;

    lp_mary_skill_fn on_skill_invoke;
} lp_mary;

/* 1 when the client is compiled in. */
int lp_mary_available(void);

void lp_mary_init(lp_mary *m, struct lp_desktop *d);
void lp_mary_free(lp_mary *m);
/* Connects (path NULL: $MARY_SOCKET, else $XDG_RUNTIME_DIR/mary/mary.sock) and keeps
 * reconnecting. 0, or -errno when there is no path or no event loop to live on. */
int lp_mary_start(lp_mary *m, const char *path);
int lp_mary_connected(const lp_mary *m);
/* Listening, hearing, transcribing, thinking or speaking. */
int lp_mary_active(const lp_mary *m);
const char *lp_mary_state_name(lp_mary_state s);

/* Requests. 0; -ENOTCONN while maryd is away; -EINVAL. */
int lp_mary_ask(lp_mary *m, const char *text);
int lp_mary_listen(lp_mary *m);
int lp_mary_stop(lp_mary *m);
int lp_mary_dismiss(lp_mary *m);
int lp_mary_verify_key(lp_mary *m);
int lp_mary_set_wake(lp_mary *m, int on);
/* config: `wake` 0 or 1 (-1 leaves it out), and a voice id (NULL leaves it out). */
int lp_mary_send_config(lp_mary *m, int wake, const char *voice);
/* voices.list; the answer fills voices[] (LP_MARY_CHANGED_VOICES). */
int lp_mary_list_voices(lp_mary *m);
/* voice.sample: one text spoken in a voice (text NULL: maryd's own sentence); its progress is sample_state. */
int lp_mary_sample_voice(lp_mary *m, const char *voice_id, const char *text);
/* A voice id as a character and a mood, as Mistral spells its presets <language>_<name>_<mood>: fr_marie_happy is
 * "fr_marie" and "happy", en_paul_cheerful "en_paul" and "cheerful". 1 when the id has that shape; 0 (the whole id,
 * no mood) otherwise, as an id of a voice of the account's own. */
int lp_mary_voice_split(const char *voice_id, char *character, size_t cn, char *mood, size_t mn);
/* Sends key.set and zeroes `key` and every copy the client made, sent or not. Mistral
 * keys are letters and digits; anything else is refused with -EINVAL (and zeroed too). */
int lp_mary_set_key(lp_mary *m, char *key, size_t len);
/* One message as a JSON object's text, sent as a line: skills{apps}, skill.result. */
int lp_mary_send_line(lp_mary *m, const char *json);

/* Bytes from maryd, as the client's reader hands them over (and a test). */
void lp_mary_feed(lp_mary *m, const char *bytes, size_t n);

#endif
