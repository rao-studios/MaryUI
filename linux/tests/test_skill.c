/* What Mary may do with each app (PARITY D20): skills.conf kept and read back, the
 * desktop's rule for every call, perform run only when allowed, the skills message
 * maryd learns them from, and skill.invoke answered over a socket. A Notes app stands
 * in for the system apps. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_skill.h"
#include "maryui/lp_calendar.h"
#include <sys/stat.h>

static lp_desktop d;
static char home[64];
static char performed[256];

static const lp_skill notes_skills[] = {
    { "list", "List notes", "Reads every note's title.", NULL, LP_SKILL_READ },
    { "add", "Add a note", "Adds a note with the given text.",
      "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}", LP_SKILL_ACT },
    { "erase", "Erase every note", "Deletes all of them for good.", NULL, LP_SKILL_DESTRUCTIVE },
};

static int notes_perform(void *state, lp_desktop *desk, const char *skill, const char *args, char *result, size_t n) {
    snprintf(performed, sizeof performed, "%s %s", skill, args ? args : "null");
    if (strcmp(skill, "add") == 0 && (!args || !strstr(args, "\"text\""))) {
        snprintf(result, n, "A note needs its text.");
        return -EINVAL;
    }
    snprintf(result, n, "{\"count\":3}");
    return 0;
}

static const lp_app notes = {
    .id = "notes", .title = "Notes", .skills = notes_skills, .skill_count = 3, .perform = notes_perform,
};

static void setup(void) {
    if (!home[0]) {
        snprintf(home, sizeof home, "/tmp/lp-skill-XXXXXX");
        if (!mkdtemp(home)) LP_FAIL("mkdtemp");
    }
    setenv("XDG_CONFIG_HOME", home, 1);
    char path[256];
    snprintf(path, sizeof path, "%s/maryui/skills.conf", home);
    unlink(path);
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_app(&d, &notes);
    performed[0] = 0;
}

LP_TEST(the_policy_starts_open_and_survives_a_save) {
    setup();
    lp_skill_policy *p = &d.skill_policy;
    LP_ASSERT(lp_skill_app_enabled(p, "notes"));
    LP_ASSERT_EQ(lp_skill_app_ask(p, "notes"), LP_SKILL_ASK_CHANGES);
    LP_ASSERT(lp_skill_enabled(p, "notes", "add"));
    lp_skill_set_app_enabled(p, "notes", 0);
    lp_skill_set_app_ask(p, "notes", LP_SKILL_ASK_ALWAYS);
    lp_skill_set_enabled(p, "notes", "add", 0);
    lp_skill_set_enabled(p, "notes", "not a skill", 0);      /* ignored */
    lp_skill_set_app_enabled(p, "../etc", 0);                /* ignored */
    LP_ASSERT_EQ(p->count, 3);
    LP_ASSERT_EQ(lp_skill_policy_save(p), 0);

    lp_skill_policy back;
    LP_ASSERT_EQ(lp_skill_policy_load(&back), 0);
    LP_ASSERT(!lp_skill_app_enabled(&back, "notes"));
    LP_ASSERT_EQ(lp_skill_app_ask(&back, "notes"), LP_SKILL_ASK_ALWAYS);
    LP_ASSERT(!lp_skill_enabled(&back, "notes", "add"));
    LP_ASSERT(lp_skill_enabled(&back, "notes", "list"));
    LP_ASSERT(lp_skill_app_enabled(&back, "calendar"));
}

LP_TEST(every_call_is_decided_by_the_desktops_rule) {
    setup();
    lp_skill_policy *p = &d.skill_policy;
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "mail", "send"), LP_SKILL_UNKNOWN);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "print"), LP_SKILL_UNKNOWN);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "list"), LP_SKILL_ALLOWED);              /* reading never asks under "changes" */
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "add"), LP_SKILL_NEEDS_CONFIRMATION);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "erase"), LP_SKILL_NEEDS_CONFIRMATION);
    lp_skill_set_app_ask(p, "notes", LP_SKILL_ASK_NEVER);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "add"), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "erase"), LP_SKILL_NEEDS_CONFIRMATION);  /* destructive always asks */
    lp_skill_set_app_ask(p, "notes", LP_SKILL_ASK_ALWAYS);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "list"), LP_SKILL_NEEDS_CONFIRMATION);
    lp_skill_set_enabled(p, "notes", "list", 0);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "list"), LP_SKILL_DENIED);
    lp_skill_set_enabled(p, "notes", "list", 1);
    lp_skill_set_app_enabled(p, "notes", 0);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "list"), LP_SKILL_DENIED);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "notes", "print"), LP_SKILL_UNKNOWN);             /* unknown comes first */
}

LP_TEST(perform_runs_only_what_is_allowed) {
    setup();
    char result[256] = "untouched";
    int status = 99;
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "notes", "add", "{\"text\":\"milk\"}", result, sizeof result, &status), LP_SKILL_NEEDS_CONFIRMATION);
    LP_ASSERT_STR(performed, "");
    LP_ASSERT_STR(result, "untouched");
    LP_ASSERT_EQ(status, 99);
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "notes", "list", "null", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_STR(performed, "list null");
    LP_ASSERT_EQ(status, 0);
    LP_ASSERT_STR(result, "{\"count\":3}");
    lp_skill_set_app_ask(&d.skill_policy, "notes", LP_SKILL_ASK_NEVER);
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "notes", "add", "{}", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(status, -EINVAL);
    LP_ASSERT_STR(result, "A note needs its text.");
    LP_ASSERT(lp_desktop_app_state(&d, "notes") == NULL);
}

#ifdef HAVE_JSONC
#include <json-c/json.h>

static struct json_object *field(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    return o && json_object_object_get_ex(o, key, &v) ? v : NULL;
}

LP_TEST(the_skills_message_carries_each_app_and_its_policy) {
    setup();
    lp_skill_set_enabled(&d.skill_policy, "notes", "erase", 0);
    char *line = lp_desktop_skills_json(&d);
    LP_ASSERT(line && !strchr(line, '\n'));
    struct json_object *msg = line ? json_tokener_parse(line) : NULL;
    free(line);
    LP_ASSERT_STR(json_object_get_string(field(msg, "type")), "skills");
    struct json_object *apps = field(msg, "apps");
    LP_ASSERT_EQ(json_object_array_length(apps), 1);                /* only apps that declare skills */
    struct json_object *app = json_object_array_get_idx(apps, 0);
    LP_ASSERT_STR(json_object_get_string(field(app, "id")), "notes");
    LP_ASSERT_STR(json_object_get_string(field(app, "ask")), "changes");
    LP_ASSERT(json_object_get_boolean(field(app, "enabled")));
    struct json_object *skills = field(app, "skills");
    LP_ASSERT_EQ(json_object_array_length(skills), 3);
    struct json_object *add = json_object_array_get_idx(skills, 1), *erase = json_object_array_get_idx(skills, 2);
    LP_ASSERT_STR(json_object_get_string(field(add, "effect")), "act");
    LP_ASSERT(json_object_is_type(field(field(add, "params"), "properties"), json_type_object));
    LP_ASSERT(field(erase, "params") == NULL);
    LP_ASSERT(!json_object_get_boolean(field(erase, "enabled")));
    json_object_put(msg);
}

/* One line maryd received, parsed. */
static struct json_object *received(int fd) {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof buf - 1, MSG_DONTWAIT);
    if (n <= 0) return NULL;
    buf[n] = 0;
    char *nl = strchr(buf, '\n');
    if (nl) *nl = 0;
    return json_tokener_parse(buf);
}

LP_TEST(skill_invoke_is_answered_with_skill_result) {
    setup();
    int sv[2];
    LP_ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    d.mary.fd = sv[0];
    LP_ASSERT(d.mary.on_skill_invoke == lp_desktop_on_skill_invoke);

    lp_desktop_on_skill_invoke(&d, "c1", "notes", "list", "null");
    struct json_object *r = received(sv[1]);
    LP_ASSERT_STR(json_object_get_string(field(r, "type")), "skill.result");
    LP_ASSERT_STR(json_object_get_string(field(r, "call_id")), "c1");
    LP_ASSERT(json_object_get_boolean(field(r, "ok")));
    LP_ASSERT_EQ(json_object_get_int(field(field(r, "result"), "count")), 3);
    json_object_put(r);

    lp_desktop_on_skill_invoke(&d, "c2", "notes", "add", "{\"text\":\"milk\"}");
    r = received(sv[1]);
    LP_ASSERT(!json_object_get_boolean(field(r, "ok")));
    LP_ASSERT_STR(json_object_get_string(field(r, "error")), "needs_confirmation");
    json_object_put(r);
    LP_ASSERT_STR(performed, "list null");

    lp_skill_set_app_ask(&d.skill_policy, "notes", LP_SKILL_ASK_NEVER);
    lp_desktop_on_skill_invoke(&d, "c3", "notes", "add", "{}");
    r = received(sv[1]);
    LP_ASSERT_STR(json_object_get_string(field(r, "error")), "failed");
    LP_ASSERT_STR(json_object_get_string(field(r, "message")), "A note needs its text.");
    json_object_put(r);

    lp_desktop_on_skill_invoke(&d, "c4", "mail", "send", "null");
    r = received(sv[1]);
    LP_ASSERT_STR(json_object_get_string(field(r, "error")), "unknown");
    json_object_put(r);

    LP_ASSERT_EQ(lp_desktop_skill_policy_changed(&d), 0);          /* saved, and maryd hears the new policy */
    r = received(sv[1]);
    LP_ASSERT_STR(json_object_get_string(field(r, "type")), "skills");
    json_object_put(r);
    lp_skill_policy back;
    lp_skill_policy_load(&back);
    LP_ASSERT_EQ(lp_skill_app_ask(&back, "notes"), LP_SKILL_ASK_NEVER);

    lp_mary_free(&d.mary);
    close(sv[1]);
}
#endif

LP_TEST(settings_media_and_calendar_carry_the_first_skills) {
    setup();
    lp_desktop_register_builtin_apps(&d);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "settings", "open_pane"), LP_SKILL_ALLOWED);           /* showing a pane changes nothing */
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "media", "play_pause"), LP_SKILL_NEEDS_CONFIRMATION);    /* it acts: asked first by default */
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "calendar", "events_today"), LP_SKILL_ALLOWED);
    char result[512];
    int status = 1;
    lp_skill_set_app_ask(&d.skill_policy, "media", LP_SKILL_ASK_NEVER);
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "media", "play_pause", NULL, result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(status, -ENOENT);
    LP_ASSERT_STR(result, "Nothing is open in the Media Player.");
    lp_skill_set_app_enabled(&d.skill_policy, "media", 0);
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "media", "play_pause"), LP_SKILL_DENIED);
#ifdef HAVE_JSONC
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "settings", "open_pane", "{\"pane\":\"sound\"}", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(status, 0);
    LP_ASSERT_STR(result, "{\"pane\":\"sound\"}");
    void *prefs = lp_desktop_app_state(&d, "settings");
    LP_ASSERT(prefs && lp_prefs_pane(prefs) == LP_PREFS_SOUND);
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "settings", "open_pane", "{\"pane\":\"Keyboard & Mouse\"}", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT(prefs && lp_prefs_pane(prefs) == LP_PREFS_KEYBOARD);
    lp_desktop_perform_skill(&d, "settings", "open_pane", "{\"pane\":\"attic\"}", result, sizeof result, &status);
    LP_ASSERT_EQ(status, -EINVAL);
    char *line = lp_desktop_skills_json(&d);
    LP_ASSERT(line && strstr(line, "\"id\":\"settings\"") && strstr(line, "\"id\":\"media\"") && strstr(line, "\"id\":\"calendar\""));
    free(line);
#endif
}

#ifdef HAVE_JSONC
LP_TEST(textedit_the_finder_the_calculator_and_the_desktop_carry_their_skills) {
    setup();
    lp_desktop_register_builtin_apps(&d);
    char result[LP_SKILL_RESULT_MAX];
    int status = 1;
    /* the Calculator reads without a window */
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "calculator", "calculate", "{\"expression\":\"12*3\"}", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(status, 0);
    LP_ASSERT(strstr(result, "\"value\":\"36\"") != NULL);
    /* the desktop's own: nothing open yet */
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "desktop", "list_windows", NULL, result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_STR(result, "{\"count\":0,\"windows\":[]}");
    lp_skill_set_app_ask(&d.skill_policy, "desktop", LP_SKILL_ASK_NEVER);
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "desktop", "close_front_window", NULL, result, sizeof result, &status), LP_SKILL_NEEDS_CONFIRMATION);   /* destructive: always asked */
    LP_ASSERT_EQ(lp_desktop_skill_decide(&d, "desktop", "shade_front_window"), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "desktop", "bring_forward", "{\"app\":\"Calculator\"}", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(status, 0);
    LP_ASSERT(lp_desktop_app_state(&d, "calculator") != NULL);
    lp_desktop_perform_skill(&d, "desktop", "list_windows", NULL, result, sizeof result, &status);
    LP_ASSERT(strstr(result, "\"count\":1") && strstr(result, "\"app\":\"calculator\"") && strstr(result, "\"front\":true"));
    /* TextEdit: read what is open, insert at the end, and the read shows it */
    lp_skill_set_app_ask(&d.skill_policy, "textedit", LP_SKILL_ASK_NEVER);
    char id[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "textedit", NULL, NULL, id));
    lp_textedit_set_text(lp_desktop_instance(&d, id)->state, "Tides", "The tide comes in.");
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "textedit", "insert_text", "{\"text\":\" Twice a day.\",\"where\":\"end\"}", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(status, 0);
    LP_ASSERT(strstr(result, "\"landed\":true") != NULL);
    LP_ASSERT_STR(lp_textedit_text(lp_desktop_instance(&d, id)->state), "The tide comes in. Twice a day.");
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "textedit", "read", NULL, result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT(strstr(result, "\"name\":\"Tides\"") && strstr(result, "\"text\":\"The tide comes in. Twice a day.\""));
    lp_desktop_perform_skill(&d, "textedit", "replace_selection", "{\"text\":\"x\"}", result, sizeof result, &status);
    LP_ASSERT_EQ(status, -EINVAL);                                  /* nothing selected */
    lp_textedit_select(lp_desktop_instance(&d, id)->state, 0, 8);
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "textedit", "replace_selection", "{\"text\":\"A wave\"}", result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_STR(lp_textedit_text(lp_desktop_instance(&d, id)->state), "A wave comes in. Twice a day.");
    /* the Finder refuses what is not there */
    lp_desktop_perform_skill(&d, "finder", "reveal", "{\"path\":\"~/no-such-file-here\"}", result, sizeof result, &status);
    LP_ASSERT_EQ(status, -ENOENT);
    /* the message carries the schema words */
    char *line = lp_desktop_skills_json(&d);
    LP_ASSERT(line != NULL);
    if (line) {
        struct json_object *msg = json_tokener_parse(line);
        struct json_object *apps = field(msg, "apps");
        int found = 0;
        for (size_t i = 0; i < json_object_array_length(apps); i++) {
            struct json_object *app = json_object_array_get_idx(apps, i);
            if (strcmp(json_object_get_string(field(app, "id")), "textedit") != 0) continue;
            found = 1;
            LP_ASSERT_STR(json_object_get_string(field(app, "discipline")), "writing");
            LP_ASSERT_STR(json_object_get_string(field(app, "paradigm")), "applicationExpertise");
            struct json_object *skills = field(app, "skills"), *insert = json_object_array_get_idx(skills, 1);
            LP_ASSERT_EQ(json_object_array_length(skills), 4);
            LP_ASSERT_STR(json_object_get_string(field(insert, "kind")), "effectful");
            LP_ASSERT_STR(json_object_get_string(field(insert, "access")), "reversible");
            LP_ASSERT_EQ(json_object_array_length(field(field(insert, "triggers"), "tokens")), 4);
            LP_ASSERT(json_object_is_type(field(field(insert, "spoken"), "where"), json_type_object));
        }
        LP_ASSERT(found);
        json_object_put(msg);
        free(line);
    }
}
#endif

LP_TEST(events_today_reads_the_calendar_without_opening_it) {
    if (!lp_calendar_available()) return;
    setup();
    lp_desktop_register_builtin_apps(&d);
    char data[128], path[256], dir[512];
    snprintf(data, sizeof data, "%s/data", home);
    setenv("XDG_DATA_HOME", data, 1);
    mkdir(data, 0700);
    snprintf(path, sizeof path, "%s/maryui", data);
    mkdir(path, 0700);
    lp_calendar_default_dir(dir, sizeof dir);
    mkdir(dir, 0700);
    lp_calendar cal;
    memset(&cal, 0, sizeof cal);
    lp_calendar_load(&cal, dir);
    lp_event e;
    memset(&e, 0, sizeof e);
    snprintf(e.title, sizeof e.title, "Standup \"daily\"");
    snprintf(e.location, sizeof e.location, "Kitchen");
    lp_date today = lp_date_today();
    e.start = lp_date_at(today, 9 * 60);
    e.end = lp_date_at(today, 9 * 60 + 15);
    LP_ASSERT_EQ(lp_calendar_save(&cal, &e), 0);
    lp_calendar_free(&cal);
    char result[2048];
    int status = 1;
    LP_ASSERT_EQ(lp_desktop_perform_skill(&d, "calendar", "events_today", NULL, result, sizeof result, &status), LP_SKILL_ALLOWED);
    LP_ASSERT_EQ(status, 0);
    if (!strstr(result, "\"title\":\"Standup \\\"daily\\\"\"") || !strstr(result, "\"start\":\"09:00\"") || !strstr(result, "\"end\":\"09:15\"") || !strstr(result, "\"location\":\"Kitchen\""))
        LP_FAIL("events_today said %s", result);
    LP_ASSERT(lp_desktop_app_state(&d, "calendar") == NULL);
}

int main(void) {
    LP_RUN(the_policy_starts_open_and_survives_a_save);
    LP_RUN(every_call_is_decided_by_the_desktops_rule);
    LP_RUN(perform_runs_only_what_is_allowed);
    LP_RUN(settings_media_and_calendar_carry_the_first_skills);
    LP_RUN(events_today_reads_the_calendar_without_opening_it);
#ifdef HAVE_JSONC
    LP_RUN(textedit_the_finder_the_calculator_and_the_desktop_carry_their_skills);
    LP_RUN(the_skills_message_carries_each_app_and_its_policy);
    LP_RUN(skill_invoke_is_answered_with_skill_result);
#endif
    LP_TEST_MAIN_END();
}
