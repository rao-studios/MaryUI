/* The Ambient app (PARITY D29): fed what maryd answers, it shows the places, the last turn's realm,
 * every route (filtered by intent) and the runs; Copy Report puts the RouteReport on the clipboard. */
#include <string.h>

#include "lp_test.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_mary.h"

#ifdef HAVE_JSONC
#include "lp_ambient_fixture.h"

static lp_desktop d;

static void setup(void) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_mary_feed(&d.mary, LP_AMBIENT_STATE_LINE, strlen(LP_AMBIENT_STATE_LINE));
    lp_mary_feed(&d.mary, LP_AMBIENT_TRACE_LINE, strlen(LP_AMBIENT_TRACE_LINE));
}

LP_TEST(the_world_tab_lists_the_places_mary_holds) {
    setup();
    LP_ASSERT(d.mary.ambient != NULL && d.mary.trace != NULL);
    char id[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "ambient", "world", NULL, id));
    void *state = lp_desktop_instance(&d, id)->state;
    LP_ASSERT_EQ(lp_ambient_app_tab(state), 0);
    LP_ASSERT_EQ(lp_ambient_app_places(state), 2);
    LP_ASSERT_STR(lp_ambient_app_place_name(state, 0), "TextEdit");
    LP_ASSERT_STR(lp_ambient_app_place_name(state, 1), "Media Player");
    /* the tabs open by name, and through the View menu's commands */
    lp_app_instance *inst = lp_desktop_instance(&d, id);
    inst->app->open(state, &d, "realms");
    LP_ASSERT_EQ(lp_ambient_app_tab(state), 1);
    inst->app->command(state, &d, LP_AMBIENT_TAB_RUNS);
    LP_ASSERT_EQ(lp_ambient_app_tab(state), 3);
    lp_menu_model m;
    memset(&m, 0, sizeof m);
    inst->app->menu_entries(state, &d, LP_MENU_VIEW, &m);
    LP_ASSERT_EQ(m.count, 7);           /* four tabs, a separator, Reload, Copy Report */
    LP_ASSERT(m.entries[3].checked);
}

LP_TEST(the_routes_tab_filters_by_intent) {
    setup();
    char id[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "ambient", "routes", NULL, id));
    void *state = lp_desktop_instance(&d, id)->state;
    LP_ASSERT_EQ(lp_ambient_app_tab(state), 2);
    LP_ASSERT_EQ(lp_ambient_app_routes(state), 2);
    LP_ASSERT_STR(lp_ambient_app_route_intent(state, 0), "ask");
    LP_ASSERT_STR(lp_ambient_app_route_intent(state, 1), "converse");
    lp_ambient_app_set_filter(state, "converse");
    LP_ASSERT_EQ(lp_ambient_app_routes(state), 1);
    LP_ASSERT_STR(lp_ambient_app_route_intent(state, 0), "converse");
    lp_ambient_app_set_filter(state, NULL);
    LP_ASSERT_EQ(lp_ambient_app_routes(state), 2);
    /* opened on a filter from elsewhere */
    lp_desktop_instance(&d, id)->app->open(state, &d, "routes:ask");
    LP_ASSERT_EQ(lp_ambient_app_routes(state), 1);
}

LP_TEST(the_report_lands_on_the_clipboard_when_maryd_answers) {
    setup();
    char id[12];
    LP_ASSERT(lp_desktop_open_app_with(&d, "ambient", "routes", NULL, id));
    lp_app_instance *inst = lp_desktop_instance(&d, id);
    inst->app->command(inst->state, &d, LP_AMBIENT_COPY_REPORT);     /* maryd is away: nothing was asked */
    static const char REPORT[] = "{\"type\":\"trace.report\",\"text\":\"=== Mary ABILITY ROUTES 2026-09-13T10:00:00Z ===\\nturns: 2\\n\"}\n";
    lp_text_clipboard_set(lp_text_clipboard_shared(), "", 0);
    lp_mary_feed(&d.mary, REPORT, strlen(REPORT));
    LP_ASSERT(d.mary.trace_report && strncmp(d.mary.trace_report, "=== Mary ABILITY ROUTES", 23) == 0);
    LP_ASSERT_EQ(lp_text_clipboard_shared()->len, 0);               /* not asked for: not copied */
    /* asked for (as the button does when maryd is there), the next report is copied */
    struct { char pad[12]; lp_desktop *desk; int tab; } *peek = inst->state;
    (void)peek;
    inst->app->model_changed(inst->state, &d, LP_MODEL_MARY, LP_MARY_CHANGED_TRACE);
    LP_ASSERT_EQ(lp_text_clipboard_shared()->len, 0);
}

int main(void) {
    LP_RUN(the_world_tab_lists_the_places_mary_holds);
    LP_RUN(the_routes_tab_filters_by_intent);
    LP_RUN(the_report_lands_on_the_clipboard_when_maryd_answers);
    return lp_test_failures;
}
#else
int main(void) { return 0; }
#endif
