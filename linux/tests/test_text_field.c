/* TextField editing (PARITY D21): ⌘/Ctrl + A, C, X and V through the desktop's text clipboard, a
 * paste kept to one line and to the field's size, a secure field that pastes but never gives its
 * text away, and the right-click that asks the host for the editing menu, whose entries reach the
 * window as LP_CMD_EDIT. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "lp_test.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_ui.h"

#define FIELD_ID 7

static lp_ctx ctx;

static int run(lp_text_buffer *b, lp_input in, int secure) {
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 400, 100), 1000);
    int changed = lp_text_field(&ctx, FIELD_ID, LP_RECT(20, 20, 300, 28), b, (lp_text_field_opts){ .icon = LP_ICON_COUNT, .secure = secure });
    lp_ctx_end(&ctx);
    return changed;
}

static int key(lp_text_buffer *b, uint32_t keysym, uint32_t mods, int secure) {
    ctx.focus = FIELD_ID;
    return run(b, (lp_input){ .mx = NAN, .my = NAN, .keysym = keysym, .mods = mods, .key_pressed = 1 }, secure);
}

static void clipboard(const char *text) { lp_text_clipboard_set(lp_text_clipboard_shared(), text, (int)strlen(text)); }

static int clipboard_is(const char *want) {
    const lp_text_clipboard *c = lp_text_clipboard_shared();
    return c->text && c->len == (int)strlen(want) && memcmp(c->text, want, (size_t)c->len) == 0;
}

LP_TEST(ctrl_or_cmd_v_pastes_at_the_cursor) {
    lp_text_buffer b;
    lp_text_buffer_set(&b, "ab");
    b.cursor = 1;
    clipboard("XY");
    LP_ASSERT_EQ(key(&b, XKB_KEY_v, LP_MOD_CTRL, 0), 1);
    LP_ASSERT_STR(b.text, "aXYb");
    LP_ASSERT_EQ(b.cursor, 3);
    LP_ASSERT_EQ(key(&b, XKB_KEY_v, LP_MOD_LOGO, 0), 1);
    LP_ASSERT_STR(b.text, "aXYXYb");
}

LP_TEST(a_paste_keeps_to_one_line_and_to_the_field) {
    lp_text_buffer b;
    lp_text_buffer_set(&b, "");
    clipboard("sk-123\r\n\tend\x01");
    key(&b, XKB_KEY_v, LP_MOD_CTRL, 0);
    LP_ASSERT_STR(b.text, "sk-123 end");
    char big[1024] = "";
    for (int i = 0; i < 400; i++) strcat(big, "\xC3\xA9");      /* 800 bytes of é */
    lp_text_buffer_set(&b, "");
    clipboard(big);
    key(&b, XKB_KEY_v, LP_MOD_CTRL, 0);
    LP_ASSERT(b.len > 0 && b.len < (int)sizeof b.text);
    LP_ASSERT_EQ(b.len % 2, 0);                                 /* whole characters only */
    LP_ASSERT_EQ((int)strlen(b.text), b.len);
}

LP_TEST(select_all_then_copy_cut_and_paste_over_it) {
    lp_text_buffer b;
    lp_text_buffer_set(&b, "hello");
    clipboard("before");
    LP_ASSERT_EQ(key(&b, XKB_KEY_c, LP_MOD_CTRL, 0), 0);
    LP_ASSERT(clipboard_is("before"));                          /* nothing selected, nothing copied */
    key(&b, XKB_KEY_a, LP_MOD_CTRL, 0);
    LP_ASSERT(b.all_selected);
    key(&b, XKB_KEY_c, LP_MOD_LOGO, 0);
    LP_ASSERT(clipboard_is("hello"));
    LP_ASSERT_EQ(key(&b, XKB_KEY_x, LP_MOD_CTRL, 0), 1);
    LP_ASSERT_STR(b.text, "");
    LP_ASSERT(clipboard_is("hello"));
    clipboard("new");
    lp_text_buffer_set_selected(&b, "old");
    LP_ASSERT_EQ(key(&b, XKB_KEY_v, LP_MOD_CTRL, 0), 1);
    LP_ASSERT_STR(b.text, "new");                               /* a paste replaces the selection */
}

LP_TEST(a_secure_field_pastes_but_never_gives_its_text_away) {
    lp_text_buffer b;
    lp_text_buffer_set(&b, "");
    clipboard("abcDEF1234567890");
    LP_ASSERT_EQ(key(&b, XKB_KEY_v, LP_MOD_CTRL, 1), 1);
    LP_ASSERT_STR(b.text, "abcDEF1234567890");
    clipboard("mine");
    key(&b, XKB_KEY_a, LP_MOD_CTRL, 1);
    key(&b, XKB_KEY_c, LP_MOD_CTRL, 1);
    LP_ASSERT(clipboard_is("mine"));
    LP_ASSERT_EQ(key(&b, XKB_KEY_x, LP_MOD_LOGO, 1), 0);
    LP_ASSERT_STR(b.text, "abcDEF1234567890");
    LP_ASSERT(clipboard_is("mine"));
}

static int right_click(lp_text_buffer *b, int secure) {
    ctx.focus = 0;
    run(b, (lp_input){ .mx = 60, .my = 30, .pressed = LP_BUTTON_RIGHT, .buttons = LP_BUTTON_RIGHT }, secure);
    return ctx.text_menu.requested;
}

LP_TEST(a_right_click_asks_for_the_editing_menu) {
    lp_text_buffer b;
    lp_text_buffer_set(&b, "hello");
    clipboard("key");
    LP_ASSERT(right_click(&b, 0));
    LP_ASSERT_EQ(ctx.focus, FIELD_ID);
    LP_ASSERT(ctx.text_menu.can_paste && ctx.text_menu.can_select_all);
    LP_ASSERT(!ctx.text_menu.can_copy && !ctx.text_menu.can_cut);       /* nothing selected */
    LP_ASSERT_NEAR(ctx.text_menu.x, 60, 0.01);
    b.all_selected = 1;
    right_click(&b, 0);
    LP_ASSERT(ctx.text_menu.can_copy && ctx.text_menu.can_cut);
    right_click(&b, 1);
    LP_ASSERT(!ctx.text_menu.can_copy && !ctx.text_menu.can_cut && ctx.text_menu.can_paste);
    run(&b, (lp_input){ .mx = 60, .my = 30 }, 0);
    LP_ASSERT(!ctx.text_menu.requested);                                 /* each pass starts without one */
    ctx.focus = 0;
    run(&b, (lp_input){ .mx = 380, .my = 90, .pressed = LP_BUTTON_RIGHT }, 0);
    LP_ASSERT(!ctx.text_menu.requested);                                 /* outside the field: nothing */
}

static lp_desktop desk;
static char edited_window[16];
static int edited_action = -1;

static void on_edit(lp_desktop *d, const char *window_id, int action) {
    snprintf(edited_window, sizeof edited_window, "%s", window_id);
    edited_action = action;
}

LP_TEST(the_menu_hands_its_edit_to_that_window) {
    lp_desktop_init(&desk, LP_RECT(0, 0, 1280, 800), NULL);
    desk.edit_text = on_edit;
    lp_text_menu_request r = { .requested = 1, .x = 60, .y = 30, .can_paste = 1, .can_select_all = 1 };
    lp_desktop_open_text_menu(&desk, "w3", &r);
    LP_ASSERT_EQ(desk.open_menu, LP_DESKTOP_MENU_POPUP);
    LP_ASSERT_STR(desk.popup_window, "w3");
    const lp_menu_model *m = &desk.menus[LP_DESKTOP_MENU_POPUP];
    LP_ASSERT_EQ(m->count, 5);
    LP_ASSERT_STR(m->entries[0].label, "Cut");
    LP_ASSERT(m->entries[0].disabled && m->entries[1].disabled && !m->entries[2].disabled);
    LP_ASSERT(m->entries[3].separator);
    LP_ASSERT_STR(m->entries[4].label, "Select All");
    lp_desktop_select_menu_entry(&desk, 2);
    LP_ASSERT_STR(edited_window, "w3");
    LP_ASSERT_EQ(edited_action, LP_EDIT_PASTE);
    LP_ASSERT(desk.open_menu < 0);
    edited_action = -1;
    lp_desktop_open_text_menu(&desk, "w3", &r);
    lp_desktop_select_menu_entry(&desk, 0);                             /* a disabled entry does nothing */
    LP_ASSERT_EQ(edited_action, -1);
    desk.edit_text = NULL;
    LP_ASSERT_EQ(lp_desktop_run_command(&desk, LP_CMD_EDIT, LP_EDIT_PASTE), 0);   /* a host without it */
}

int main(void) {
    LP_RUN(ctrl_or_cmd_v_pastes_at_the_cursor);
    LP_RUN(a_paste_keeps_to_one_line_and_to_the_field);
    LP_RUN(select_all_then_copy_cut_and_paste_over_it);
    LP_RUN(a_secure_field_pastes_but_never_gives_its_text_away);
    LP_RUN(a_right_click_asks_for_the_editing_menu);
    LP_RUN(the_menu_hands_its_edit_to_that_window);
    LP_TEST_MAIN_END();
}
