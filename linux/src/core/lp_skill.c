/* What Mary can do with each application (lp_skill.h). */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "maryui/lp_desktop.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_skill.h"

static int valid_id(const char *id) {
    if (!id || !*id || strlen(id) > 40) return 0;
    for (const char *c = id; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) return 0;
    return 1;
}

void lp_skill_policy_init(lp_skill_policy *p) { p->count = 0; }

static const char *lookup(const lp_skill_policy *p, const char *key) {
    for (int i = 0; i < p->count; i++)
        if (strcmp(p->entries[i].key, key) == 0) return p->entries[i].value;
    return NULL;
}

static void put(lp_skill_policy *p, const char *key, const char *value) {
    for (int i = 0; i < p->count; i++) {
        if (strcmp(p->entries[i].key, key) != 0) continue;
        snprintf(p->entries[i].value, sizeof p->entries[i].value, "%s", value);
        return;
    }
    if (p->count == LP_SKILL_POLICY_MAX) return;
    snprintf(p->entries[p->count].key, sizeof p->entries[p->count].key, "%s", key);
    snprintf(p->entries[p->count].value, sizeof p->entries[p->count].value, "%s", value);
    p->count++;
}

static int is_off(const char *v) { return v && (strcmp(v, "off") == 0 || strcmp(v, "0") == 0); }

int lp_skill_policy_load(lp_skill_policy *p) {
    lp_skill_policy_init(p);
    char path[1100];
    lp_config_path("skills.conf", path, sizeof path, 0);
    FILE *f = fopen(path, "r");
    if (!f) return errno == ENOENT ? 0 : -errno;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char key[96], value[16];
        if (sscanf(line, " %95[A-Za-z0-9_.-] = %15[A-Za-z0-9]", key, value) == 2) put(p, key, value);
    }
    fclose(f);
    return 0;
}

int lp_skill_policy_save(const lp_skill_policy *p) {
    char path[1100], tmp[1200];
    lp_config_path("skills.conf", path, sizeof path, 1);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -errno;
    fprintf(f, "# What Mary may do with each application (System Settings › Mary)\n");
    for (int i = 0; i < p->count; i++) fprintf(f, "%s=%s\n", p->entries[i].key, p->entries[i].value);
    int failed = fflush(f) != 0 || fsync(fileno(f)) != 0;
    if (fclose(f) != 0) failed = 1;
    if (failed || rename(tmp, path) != 0) {
        int e = errno ? errno : EIO;
        unlink(tmp);
        return -e;
    }
    return 0;
}

int lp_skill_app_enabled(const lp_skill_policy *p, const char *app) { return !is_off(lookup(p, app)); }

lp_skill_ask lp_skill_app_ask(const lp_skill_policy *p, const char *app) {
    char key[96];
    snprintf(key, sizeof key, "%s.ask", app);
    const char *v = lookup(p, key);
    return v && strcmp(v, "never") == 0 ? LP_SKILL_ASK_NEVER : v && strcmp(v, "always") == 0 ? LP_SKILL_ASK_ALWAYS : LP_SKILL_ASK_CHANGES;
}

int lp_skill_enabled(const lp_skill_policy *p, const char *app, const char *skill) {
    char key[96];
    snprintf(key, sizeof key, "%s.%s", app, skill);
    return !is_off(lookup(p, key));
}

void lp_skill_set_app_enabled(lp_skill_policy *p, const char *app, int on) {
    if (valid_id(app)) put(p, app, on ? "on" : "off");
}

void lp_skill_set_app_ask(lp_skill_policy *p, const char *app, lp_skill_ask ask) {
    char key[96];
    if (!valid_id(app)) return;
    snprintf(key, sizeof key, "%s.ask", app);
    put(p, key, lp_skill_ask_name(ask));
}

void lp_skill_set_enabled(lp_skill_policy *p, const char *app, const char *skill, int on) {
    char key[96];
    if (!valid_id(app) || !valid_id(skill) || strcmp(skill, "ask") == 0) return;
    snprintf(key, sizeof key, "%s.%s", app, skill);
    put(p, key, on ? "on" : "off");
}

const char *lp_skill_decision_name(lp_skill_decision d) {
    static const char *const names[] = { "allowed", "unknown", "denied", "needs_confirmation" };
    return (unsigned)d < 4 ? names[d] : "unknown";
}

const char *lp_skill_effect_name(lp_skill_effect e) {
    static const char *const names[] = { "read", "act", "destructive" };
    return (unsigned)e < 3 ? names[e] : "act";
}

const char *lp_skill_ask_name(lp_skill_ask a) {
    static const char *const names[] = { "never", "changes", "always" };
    return (unsigned)a < 3 ? names[a] : "changes";
}

void *lp_desktop_app_state(lp_desktop *d, const char *app_id) {
    for (int i = 0; i < d->instance_count; i++)
        if (d->instances[i].app && strcmp(d->instances[i].app->id, app_id) == 0) return d->instances[i].state;
    return NULL;
}

static const lp_skill *find_skill(const lp_desktop *d, const char *app_id, const char *skill_id, const lp_app **app) {
    *app = app_id ? lp_desktop_find_app(d, app_id) : NULL;
    for (int i = 0; *app && skill_id && i < (*app)->skill_count; i++)
        if (strcmp((*app)->skills[i].id, skill_id) == 0) return &(*app)->skills[i];
    return NULL;
}

lp_skill_decision lp_desktop_skill_decide(const lp_desktop *d, const char *app_id, const char *skill_id) {
    const lp_app *app;
    const lp_skill *skill = find_skill(d, app_id, skill_id, &app);
    if (!skill || !app->perform) return LP_SKILL_UNKNOWN;
    if (!lp_skill_app_enabled(&d->skill_policy, app->id) || !lp_skill_enabled(&d->skill_policy, app->id, skill->id)) return LP_SKILL_DENIED;
    lp_skill_ask ask = lp_skill_app_ask(&d->skill_policy, app->id);
    if (skill->effect == LP_SKILL_DESTRUCTIVE || ask == LP_SKILL_ASK_ALWAYS || (ask == LP_SKILL_ASK_CHANGES && skill->effect != LP_SKILL_READ))
        return LP_SKILL_NEEDS_CONFIRMATION;
    return LP_SKILL_ALLOWED;
}

lp_skill_decision lp_desktop_perform_skill(lp_desktop *d, const char *app_id, const char *skill_id, const char *args_json,
                                           char *result, size_t n, int *status) {
    lp_skill_decision decision = lp_desktop_skill_decide(d, app_id, skill_id);
    if (decision != LP_SKILL_ALLOWED) return decision;
    const lp_app *app = lp_desktop_find_app(d, app_id);
    if (n) result[0] = 0;
    int rc = app->perform(lp_desktop_app_state(d, app_id), d, skill_id, args_json && strcmp(args_json, "null") != 0 ? args_json : NULL, result, n);
    if (status) *status = rc;
    return decision;
}

#ifdef HAVE_JSONC
#include <json-c/json.h>

#define PLAIN (JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE)

static struct json_object *str_or_null(const char *s) { return s ? json_object_new_string(s) : NULL; }

char *lp_desktop_skills_json(const lp_desktop *d) {
    struct json_object *msg = json_object_new_object(), *apps = json_object_new_array();
    json_object_object_add(msg, "type", json_object_new_string("skills"));
    for (int a = 0; a < d->app_count; a++) {
        const lp_app *app = d->apps[a];
        if (!app->skill_count || !app->perform) continue;
        struct json_object *o = json_object_new_object(), *skills = json_object_new_array();
        json_object_object_add(o, "id", json_object_new_string(app->id));
        json_object_object_add(o, "name", json_object_new_string(app->name ? app->name : app->title));
        json_object_object_add(o, "enabled", json_object_new_boolean(lp_skill_app_enabled(&d->skill_policy, app->id)));
        json_object_object_add(o, "ask", json_object_new_string(lp_skill_ask_name(lp_skill_app_ask(&d->skill_policy, app->id))));
        for (int s = 0; s < app->skill_count; s++) {
            const lp_skill *sk = &app->skills[s];
            struct json_object *k = json_object_new_object();
            json_object_object_add(k, "id", json_object_new_string(sk->id));
            json_object_object_add(k, "title", str_or_null(sk->title));
            json_object_object_add(k, "summary", str_or_null(sk->summary));
            json_object_object_add(k, "params", sk->params ? json_tokener_parse(sk->params) : NULL);
            json_object_object_add(k, "effect", json_object_new_string(lp_skill_effect_name(sk->effect)));
            json_object_object_add(k, "enabled", json_object_new_boolean(lp_skill_enabled(&d->skill_policy, app->id, sk->id)));
            json_object_array_add(skills, k);
        }
        json_object_object_add(o, "skills", skills);
        json_object_array_add(apps, o);
    }
    json_object_object_add(msg, "apps", apps);
    char *line = strdup(json_object_to_json_string_ext(msg, PLAIN));
    json_object_put(msg);
    return line;
}

void lp_desktop_on_skill_invoke(lp_desktop *d, const char *call_id, const char *app, const char *skill, const char *args_json) {
    char result[LP_SKILL_RESULT_MAX];
    int status = 0;
    lp_skill_decision decision = lp_desktop_perform_skill(d, app, skill, args_json, result, sizeof result, &status);
    struct json_object *msg = json_object_new_object();
    json_object_object_add(msg, "type", json_object_new_string("skill.result"));
    json_object_object_add(msg, "call_id", json_object_new_string(call_id));
    int ok = decision == LP_SKILL_ALLOWED && status == 0;
    json_object_object_add(msg, "ok", json_object_new_boolean(ok));
    if (ok) {
        struct json_object *value = result[0] ? json_tokener_parse(result) : NULL;
        json_object_object_add(msg, "result", value || !result[0] ? value : json_object_new_string(result));
    } else {
        json_object_object_add(msg, "error", json_object_new_string(decision == LP_SKILL_ALLOWED ? "failed" : lp_skill_decision_name(decision)));
        if (decision == LP_SKILL_ALLOWED && result[0]) json_object_object_add(msg, "message", json_object_new_string(result));
    }
    lp_mary_send_line(&d->mary, json_object_to_json_string_ext(msg, PLAIN));
    json_object_put(msg);
}

#else

char *lp_desktop_skills_json(const lp_desktop *d) { return NULL; }
void lp_desktop_on_skill_invoke(lp_desktop *d, const char *call_id, const char *app, const char *skill, const char *args_json) {}

#endif

int lp_desktop_publish_skills(lp_desktop *d) {
    if (!lp_mary_connected(&d->mary)) return -ENOTCONN;
    char *line = lp_desktop_skills_json(d);
    if (!line) return -ENOSYS;
    int rc = lp_mary_send_line(&d->mary, line);
    free(line);
    return rc;
}

int lp_desktop_skill_policy_changed(lp_desktop *d) {
    int rc = lp_skill_policy_save(&d->skill_policy);
    lp_desktop_publish_skills(d);
    return rc;
}

#ifdef HAVE_JSONC
#include <json-c/json.h>
#endif

int lp_skill_arg_string(const char *args_json, const char *key, char *out, size_t n) {
    if (n) out[0] = 0;
#ifdef HAVE_JSONC
    struct json_object *o = args_json ? json_tokener_parse(args_json) : NULL, *v = NULL;
    int found = o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string);
    if (found) snprintf(out, n, "%s", json_object_get_string(v));
    if (o) json_object_put(o);
    return found;
#else
    return 0;
#endif
}

size_t lp_skill_json_escape(const char *text, char *out, size_t n) {
    size_t j = 0;
    for (; text && *text && j + 7 < n; text++) {
        unsigned char c = (unsigned char)*text;
        if (c == '"' || c == '\\') { out[j++] = '\\'; out[j++] = (char)c; }
        else if (c < 0x20) j += (size_t)snprintf(out + j, n - j, "\\u%04x", c);
        else out[j++] = (char)c;
    }
    if (n) out[j < n ? j : n - 1] = 0;
    return j;
}
