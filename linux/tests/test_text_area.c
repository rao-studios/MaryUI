/* The TextArea's document, clipboard and layout. Case names mirror
 * web/src/lib/textStats.test.ts where the web has a counterpart. */
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_text.h"

static void type(lp_text_doc *d, const char *s) {
    for (; *s; s++) {
        char utf8[2] = { *s, 0 };
        lp_text_doc_key(d, 0, 0, utf8, NULL);
    }
}

LP_TEST(counts_words_characters_lines_and_columns) {
    lp_text_doc d;
    lp_text_doc_init(&d);
    lp_text_doc_set(&d, "Hello world\nsecond  line — café");
    LP_ASSERT_EQ(lp_text_doc_word_count(&d), 6);
    LP_ASSERT_EQ(lp_text_doc_char_count(&d), 31);
    int line, col;
    lp_text_doc_line_col(&d, d.len, &line, &col);
    LP_ASSERT_EQ(line, 2);
    LP_ASSERT_EQ(col, 20);
    lp_text_doc_line_col(&d, 0, &line, &col);
    LP_ASSERT_EQ(line, 1);
    LP_ASSERT_EQ(col, 1);
    lp_text_doc_set(&d, "");
    LP_ASSERT_EQ(lp_text_doc_word_count(&d), 0);
    LP_ASSERT_EQ(lp_text_doc_char_count(&d), 0);
    lp_text_doc_free(&d);
}

LP_TEST(inserts_at_the_caret_and_replaces_the_selection) {
    lp_text_doc d;
    lp_text_doc_init(&d);
    type(&d, "helo");
    d.cursor = d.anchor = 3;
    type(&d, "l");
    LP_ASSERT_STR(d.text, "hello");
    LP_ASSERT_EQ(d.cursor, 4);
    d.anchor = 0; d.cursor = 5;
    type(&d, "H");
    LP_ASSERT_STR(d.text, "H");
    lp_text_doc_key(&d, XKB_KEY_Return, 0, "\r", NULL);
    lp_text_doc_key(&d, XKB_KEY_Tab, 0, "\t", NULL);
    LP_ASSERT_STR(d.text, "H\n\t");
    LP_ASSERT_EQ(lp_text_doc_key(&d, XKB_KEY_x, LP_MOD_CTRL, "\x18", NULL), 0); /* a shortcut, not text */
    LP_ASSERT_STR(d.text, "H\n\t");
    lp_text_doc_free(&d);
}

LP_TEST(backspace_deletes_the_selection_or_the_previous_grapheme) {
    lp_text_doc d;
    lp_text_doc_init(&d);
    lp_text_doc_set(&d, "café!");
    LP_ASSERT_EQ(lp_text_doc_key(&d, XKB_KEY_BackSpace, 0, "", NULL), 1);
    LP_ASSERT_STR(d.text, "café");
    lp_text_doc_key(&d, XKB_KEY_BackSpace, 0, "", NULL);
    LP_ASSERT_STR(d.text, "caf");
    d.anchor = 0;
    lp_text_doc_key(&d, XKB_KEY_Left, LP_MOD_SHIFT, "", NULL);
    LP_ASSERT_EQ(d.cursor, 2);
    LP_ASSERT_EQ(d.anchor, 0);
    lp_text_doc_key(&d, XKB_KEY_BackSpace, 0, "", NULL);
    LP_ASSERT_STR(d.text, "f");
    LP_ASSERT_EQ(d.cursor, 0); /* the caret lands where the selection began */
    LP_ASSERT_EQ(lp_text_doc_key(&d, XKB_KEY_BackSpace, 0, "", NULL), 0);
    LP_ASSERT_EQ(lp_text_doc_key(&d, XKB_KEY_Delete, 0, "", NULL), 1);
    LP_ASSERT_EQ(d.len, 0);
    lp_text_doc_set(&d, "ab");
    d.cursor = d.anchor = 0;
    lp_text_doc_key(&d, XKB_KEY_Delete, 0, "", NULL);
    LP_ASSERT_STR(d.text, "b");
    lp_text_doc_free(&d);
}

LP_TEST(select_all_then_cut_empties_the_document_and_fills_the_clipboard) {
    lp_text_doc d;
    lp_text_clipboard clip = { 0 };
    lp_text_doc_init(&d);
    lp_text_doc_set(&d, "one two");
    lp_text_doc_key(&d, XKB_KEY_a, LP_MOD_LOGO, "a", &clip);
    LP_ASSERT(lp_text_doc_has_selection(&d));
    LP_ASSERT_EQ(lp_text_doc_key(&d, XKB_KEY_x, LP_MOD_LOGO, "x", &clip), 1);
    LP_ASSERT_STR(d.text, "");
    LP_ASSERT_EQ(d.len, 0);
    LP_ASSERT_STR(clip.text, "one two");
    LP_ASSERT_EQ(clip.len, 7);
    free(clip.text);
    lp_text_doc_free(&d);
}

LP_TEST(paste_inserts_the_clipboard_at_the_caret) {
    lp_text_doc d;
    lp_text_clipboard clip = { 0 };
    lp_text_clipboard_set(&clip, "-mid-", 5);
    lp_text_doc_init(&d);
    lp_text_doc_set(&d, "ab");
    d.cursor = d.anchor = 1;
    LP_ASSERT_EQ(lp_text_doc_key(&d, XKB_KEY_v, LP_MOD_CTRL, "v", &clip), 1);
    LP_ASSERT_STR(d.text, "a-mid-b");
    LP_ASSERT_EQ(d.cursor, 6);
    /* copy leaves the text alone */
    d.anchor = 0;
    LP_ASSERT_EQ(lp_text_doc_key(&d, XKB_KEY_c, LP_MOD_CTRL, "c", &clip), 0);
    LP_ASSERT_STR(clip.text, "a-mid-");
    free(clip.text);
    lp_text_doc_free(&d);
}

LP_TEST(wrapped_layout_maps_index_to_position_and_back) {
    lp_text_style st = lp_text_style_default();
    const char *text = "The quick brown fox jumps over the lazy dog, again and again and again.";
    lp_text_layout *l = lp_text_layout_new(NULL, text, -1, &st, 160);
    LP_ASSERT(lp_text_layout_line_count(l) >= 3);
    lp_size size = lp_text_layout_size(l);
    LP_ASSERT(size.w <= 160.5f);
    for (int i = 0; i <= (int)strlen(text); i += 7) {
        lp_rect pos = lp_text_layout_index_to_pos(l, i);
        int back = lp_text_layout_xy_to_index(l, pos.x + 0.5f, pos.y + pos.h / 2);
        LP_ASSERT_EQ(back, i);
    }
    int s, e;
    lp_text_layout_line_bounds(l, 0, &s, &e);
    LP_ASSERT_EQ(s, 0);
    LP_ASSERT(e > 0 && e < (int)strlen(text));
    lp_rect rects[16];
    int n = lp_text_layout_range_rects(l, 0, (int)strlen(text), rects, 16);
    LP_ASSERT_EQ(n, lp_text_layout_line_count(l));
    lp_text_layout_free(l);
}

LP_TEST(moves_the_caret_a_visual_line_up_and_down_keeping_x) {
    lp_text_style st = lp_text_style_default();
    const char *text = "first line here\nsecond line here\nthird";
    lp_text_layout *l = lp_text_layout_new(NULL, text, -1, &st, 400);
    LP_ASSERT_EQ(lp_text_layout_line_count(l), 3);
    int at = 6; /* "first |line" */
    float x = lp_text_layout_index_to_pos(l, at).x;
    int down = lp_text_layout_move_line(l, at, 1, x);
    LP_ASSERT(down > 16 && down <= 33);
    LP_ASSERT_NEAR(lp_text_layout_index_to_pos(l, down).x, x, 6);
    int up = lp_text_layout_move_line(l, down, -1, x);
    LP_ASSERT_EQ(up, at);
    LP_ASSERT_EQ(lp_text_layout_move_line(l, at, -1, x), -1);
    LP_ASSERT_EQ(lp_text_layout_move_line(l, (int)strlen(text), 1, x), -1);
    lp_text_layout_free(l);
}

int main(void) {
    LP_RUN(counts_words_characters_lines_and_columns);
    LP_RUN(inserts_at_the_caret_and_replaces_the_selection);
    LP_RUN(backspace_deletes_the_selection_or_the_previous_grapheme);
    LP_RUN(select_all_then_cut_empties_the_document_and_fills_the_clipboard);
    LP_RUN(paste_inserts_the_clipboard_at_the_caret);
    LP_RUN(wrapped_layout_maps_index_to_position_and_back);
    LP_RUN(moves_the_caret_a_visual_line_up_and_down_keeping_x);
    LP_TEST_MAIN_END();
}
