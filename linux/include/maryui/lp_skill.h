/* What Mary can do with each application, and what a person lets her (Linux, PARITY D20).
 * On macOS Mary learns an app from recorded abilities and drives it through the
 * accessibility tree; on MaryOS the apps are the desktop's own, so each declares its
 * skills in code (lp_app.skills) and performs one through the same code its menus run
 * (lp_app.perform). Nothing reads the screen and nothing synthesises input.
 *
 * System Settings keeps the policy, per application, in $XDG_CONFIG_HOME/maryui/skills.conf:
 *
 *   <app>=on|off                        Mary may use the app at all (on)
 *   <app>.ask=never|changes|always      when she must ask first (changes: before anything that acts)
 *   <app>.<skill>=on|off                one skill (on)
 *
 * maryd learns the skills and the policy from `skills{apps}`, sent whenever it connects
 * and whenever the policy changes, and calls one with `skill.invoke`; the desktop
 * decides every call again here before anything runs. */
#ifndef MARYUI_LP_SKILL_H
#define MARYUI_LP_SKILL_H

#include <stddef.h>

struct lp_desktop;

typedef enum lp_skill_effect {
    LP_SKILL_READ,              /* looks, changes nothing */
    LP_SKILL_ACT,               /* changes something that can be changed back */
    LP_SKILL_DESTRUCTIVE,       /* cannot be undone: always asks */
} lp_skill_effect;

typedef enum lp_skill_ask { LP_SKILL_ASK_NEVER, LP_SKILL_ASK_CHANGES, LP_SKILL_ASK_ALWAYS } lp_skill_ask;

typedef struct lp_skill {
    const char *id;             /* [A-Za-z0-9_-]: "open_pane" */
    const char *title;          /* "Open a pane" */
    const char *summary;        /* one sentence, for Mary and for Settings */
    const char *params;         /* a JSON Schema for the arguments, or NULL */
    lp_skill_effect effect;
} lp_skill;

#define LP_SKILL_POLICY_MAX 128
#define LP_SKILL_RESULT_MAX 8192

typedef struct lp_skill_policy {
    struct { char key[96]; char value[16]; } entries[LP_SKILL_POLICY_MAX];
    int count;
} lp_skill_policy;

void lp_skill_policy_init(lp_skill_policy *p);
/* Replaces p with skills.conf; a missing file is the default policy. 0, or -errno. */
int lp_skill_policy_load(lp_skill_policy *p);
/* Writes skills.conf (atomically). 0, or -errno. */
int lp_skill_policy_save(const lp_skill_policy *p);
int lp_skill_app_enabled(const lp_skill_policy *p, const char *app);
lp_skill_ask lp_skill_app_ask(const lp_skill_policy *p, const char *app);
int lp_skill_enabled(const lp_skill_policy *p, const char *app, const char *skill);
/* Setters ignore ids outside [A-Za-z0-9_-]. */
void lp_skill_set_app_enabled(lp_skill_policy *p, const char *app, int on);
void lp_skill_set_app_ask(lp_skill_policy *p, const char *app, lp_skill_ask ask);
void lp_skill_set_enabled(lp_skill_policy *p, const char *app, const char *skill, int on);

typedef enum lp_skill_decision {
    LP_SKILL_ALLOWED,
    LP_SKILL_UNKNOWN,                   /* no such app, or it has no such skill */
    LP_SKILL_DENIED,                    /* the app or the skill is turned off */
    LP_SKILL_NEEDS_CONFIRMATION,        /* destructive, or the app asks first */
} lp_skill_decision;

/* The wire's words: "allowed", "unknown", "denied", "needs_confirmation"; "read", "act", "destructive"; "never", "changes", "always". */
const char *lp_skill_decision_name(lp_skill_decision d);
const char *lp_skill_effect_name(lp_skill_effect e);
const char *lp_skill_ask_name(lp_skill_ask a);

/* The first window's state for an app, or NULL when it has none open. */
void *lp_desktop_app_state(struct lp_desktop *d, const char *app_id);
/* The rule (maryd's registry mirrors it): unknown, then denied, then needs confirmation — destructive, the app
 * asks always, or it asks before changes and the skill acts — else allowed. */
lp_skill_decision lp_desktop_skill_decide(const struct lp_desktop *d, const char *app, const char *skill);
/* Decides, and runs the app's perform only when allowed. *status is perform's (0, or -errno) and result holds
 * its JSON or its sentence; both are untouched unless the call was allowed. */
lp_skill_decision lp_desktop_perform_skill(struct lp_desktop *d, const char *app, const char *skill, const char *args_json,
                                           char *result, size_t n, int *status);
/* {"type":"skills","apps":[{id, name, enabled, ask, skills:[{id, title, summary, params, effect, enabled}]}]}
 * for every app that declares skills, as one line (heap; NULL without json-c). */
char *lp_desktop_skills_json(const struct lp_desktop *d);
/* Sends that to maryd. 0, or -errno (-ENOTCONN while maryd is away: it is sent again on connect). */
int lp_desktop_publish_skills(struct lp_desktop *d);
/* Settings changed the policy: save it and tell maryd. 0, or the save's -errno. */
int lp_desktop_skill_policy_changed(struct lp_desktop *d);
/* lp_mary's skill.invoke handler (lp_desktop_init installs it): decides, performs and answers skill.result. */
void lp_desktop_on_skill_invoke(struct lp_desktop *d, const char *call_id, const char *app, const char *skill,
                                const char *args_json);

/* For an app's perform: a string argument (1 and a copy in out, else 0; always 0 without json-c), and text escaped
 * for a JSON string (quotes and backslashes and control characters; NUL-terminated, truncated to fit). */
int lp_skill_arg_string(const char *args_json, const char *key, char *out, size_t n);
size_t lp_skill_json_escape(const char *text, char *out, size_t n);

#endif
