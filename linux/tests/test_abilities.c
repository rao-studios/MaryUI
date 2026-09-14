/* The Abilities app (PARITY D30): the packages are the apps that declare skills and the disciplines they
 * realize; the panes open by name; a rehearsal reads maryd's triage answer. */
#include <string.h>

#include "lp_test.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_mary.h"

static lp_desktop d;

static void setup(void) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
}

LP_TEST(the_rail_lists_every_app_with_skills_and_the_disciplines) {
    setup();
    char id[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "abilities", NULL, NULL, id));
    void *state = lp_desktop_instance(&d, id)->state;
    /* finder, textedit, calculator, media, calendar, settings, desktop; then the disciplines as the apps introduce them:
     * awareness, writing, multimedia, system-control, window-management */
    LP_ASSERT_EQ(lp_abilities_app_packages(state), 12);
    LP_ASSERT_STR(lp_abilities_app_package_title(state, 0), "Finder");
    LP_ASSERT_STR(lp_abilities_app_package_title(state, 1), "TextEdit");
    LP_ASSERT_STR(lp_abilities_app_package_title(state, 6), "Desktop");
    LP_ASSERT_STR(lp_abilities_app_package_title(state, 7), "Awareness");
    LP_ASSERT_STR(lp_abilities_app_package_title(state, 8), "Writing");
    LP_ASSERT_STR(lp_abilities_app_package_title(state, 11), "Window management");
    LP_ASSERT_EQ(lp_abilities_app_selected(state), 0);
    LP_ASSERT_EQ(lp_abilities_app_skill_count(state), 2);          /* the Finder: open, reveal */
    lp_abilities_app_select(state, 1);
    LP_ASSERT_EQ(lp_abilities_app_skill_count(state), 5);          /* TextEdit: read, insert, replace, a new note, save */
    lp_abilities_app_select(state, 7);
    LP_ASSERT_EQ(lp_abilities_app_skill_count(state), 3);          /* awareness: the Finder's two and Calendar's one */
}

LP_TEST(the_panes_open_by_name_and_by_command) {
    setup();
    char id[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "abilities", "textedit:skills", NULL, id));
    lp_app_instance *inst = lp_desktop_instance(&d, id);
    void *state = inst->state;
    LP_ASSERT_EQ(lp_abilities_app_selected(state), 1);
    LP_ASSERT_EQ(lp_abilities_app_pane(state), LP_ABILITIES_PANE_SKILLS);
    inst->app->open(state, &d, "tune");
    LP_ASSERT_EQ(lp_abilities_app_pane(state), LP_ABILITIES_PANE_TUNE);
    inst->app->open(state, &d, "discipline:writing");
    LP_ASSERT_EQ(lp_abilities_app_selected(state), 8);
    inst->app->command(state, &d, LP_ABILITIES_PANE_SURFACE);
    LP_ASSERT_EQ(lp_abilities_app_pane(state), LP_ABILITIES_PANE_SURFACE);
    lp_menu_model m;
    memset(&m, 0, sizeof m);
    inst->app->menu_entries(state, &d, LP_MENU_VIEW, &m);
    LP_ASSERT_EQ(m.count, 5);           /* three panes, a separator, Rehearse */
    LP_ASSERT(m.entries[0].checked);
}

LP_TEST(a_rehearsal_reads_who_would_answer) {
    setup();
    if (!lp_mary_available()) return;
    char id[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "abilities", "media", NULL, id));
    lp_app_instance *inst = lp_desktop_instance(&d, id);
    inst->app->command(inst->state, &d, LP_ABILITIES_REHEARSE);        /* nothing typed, maryd away: nothing asked */
    LP_ASSERT_STR(lp_abilities_app_rehearsal(inst->state), "");
    static const char ANSWER[] = "{\"type\":\"triage.result\",\"ok\":true,\"text\":\"play the music\",\"winner\":{\"app\":\"media\",\"skill\":\"play_pause\","
                                 "\"invocation\":\"media__play_pause\",\"title\":\"Play or pause\",\"score\":0.71,\"shape\":\"noRequiredArguments\",\"singleClause\":true,"
                                 "\"decision\":\"allowed\",\"dispatchable\":true},\"affinities\":[{\"invocation\":\"media__play_pause\",\"score\":0.71}]}\n";
    lp_mary_feed(&d.mary, ANSWER, strlen(ANSWER));
    LP_ASSERT(d.mary.triage != NULL);
    /* not asked for: the answer is not read as this window's rehearsal */
    inst->app->model_changed(inst->state, &d, LP_MODEL_MARY, LP_MARY_CHANGED_TRIAGE);
    LP_ASSERT_STR(lp_abilities_app_rehearsal(inst->state), "");
}

int main(void) {
    LP_RUN(the_rail_lists_every_app_with_skills_and_the_disciplines);
    LP_RUN(the_panes_open_by_name_and_by_command);
    LP_RUN(a_rehearsal_reads_who_would_answer);
    LP_TEST_MAIN_END();
}
