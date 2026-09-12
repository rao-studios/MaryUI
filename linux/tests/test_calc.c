/* Calculator's model: precedence, repeated equals, percent, sign, memory, the
 * display's formatting and the keyboard. Linux only (PARITY D15). */
#include <xkbcommon/xkbcommon-keysyms.h>
#include "lp_test.h"
#include "maryui/lp_calc.h"

static lp_calc c;
static char shown[80], expr[200];

/* Presses each character's key: digits . + - * / = % s(ign) c(lear) b(ackspace) and M m R Z (M+ M− MR MC). */
static void type(const char *keys) {
    for (const char *p = keys; *p; p++) {
        enum lp_calc_key k;
        switch (*p) {
        case 's': k = LP_CALC_SIGN; break;
        case 'c': k = LP_CALC_CLEAR; break;
        case 'b': k = LP_CALC_BACKSPACE; break;
        case 'M': k = LP_CALC_MPLUS; break;
        case 'm': k = LP_CALC_MMINUS; break;
        case 'R': k = LP_CALC_MR; break;
        case 'Z': k = LP_CALC_MC; break;
        default: { char ch[2] = { *p, 0 }; k = lp_calc_key_for(0, ch); }
        }
        lp_calc_press(&c, k);
    }
    lp_calc_display(&c, shown, sizeof shown);
    lp_calc_expression(&c, expr, sizeof expr);
}

LP_TEST(multiplies_and_divides_before_adding) {
    lp_calc_init(&c);
    type("2+3*4=");
    LP_ASSERT_STR(shown, "14");
    LP_ASSERT_STR(expr, "2 + 3 × 4 =");
    lp_calc_init(&c);
    type("8-2-1=");
    LP_ASSERT_STR(shown, "5");
    lp_calc_init(&c);
    type("8/4*2=");
    LP_ASSERT_STR(shown, "4");
    lp_calc_init(&c);
    type("1+2*3-4/2=");
    LP_ASSERT_STR(shown, "5");
}

LP_TEST(shows_the_expression_while_it_is_typed) {
    lp_calc_init(&c);
    type("2+3*");
    LP_ASSERT_STR(expr, "2 + 3 ×");
    LP_ASSERT_STR(shown, "3");
    type("45");
    LP_ASSERT_STR(expr, "2 + 3 × 45");
    LP_ASSERT_STR(shown, "45");
}

LP_TEST(repeats_the_last_operation_on_equals) {
    lp_calc_init(&c);
    type("2+3==");
    LP_ASSERT_STR(shown, "8");
    LP_ASSERT_STR(expr, "5 + 3 =");
    lp_calc_init(&c);
    type("3*4==");
    LP_ASSERT_STR(shown, "48");
    type("7=");                  /* a new number takes the operation too */
    LP_ASSERT_STR(shown, "28");
}

LP_TEST(an_operator_after_another_replaces_it) {
    lp_calc_init(&c);
    type("2+*3=");
    LP_ASSERT_STR(shown, "6");
    lp_calc_init(&c);
    type("2+=");                 /* the missing operand is the last one */
    LP_ASSERT_STR(shown, "4");
}

LP_TEST(continues_from_a_result_and_starts_over_on_a_number) {
    lp_calc_init(&c);
    type("2+3=+1=");
    LP_ASSERT_STR(shown, "6");
    lp_calc_init(&c);
    type("2+3=7");
    LP_ASSERT_STR(shown, "7");
    LP_ASSERT_STR(expr, "7");
}

LP_TEST(divide_by_zero_is_an_error_until_a_number_or_clear) {
    lp_calc_init(&c);
    type("5M");                  /* memory survives the error */
    type("1/0=");
    LP_ASSERT_STR(shown, "Error");
    LP_ASSERT_STR(expr, "");
    LP_ASSERT(c.error);
    type("+");
    LP_ASSERT_STR(shown, "Error");
    type("7");
    LP_ASSERT_STR(shown, "7");
    type("cR");
    LP_ASSERT_STR(shown, "5");
    lp_calc_init(&c);
    type("4/0=c");
    LP_ASSERT_STR(shown, "0");
}

LP_TEST(percent_of_what_it_is_added_to) {
    lp_calc_init(&c);
    type("50+10%");
    LP_ASSERT_STR(shown, "5");
    type("=");
    LP_ASSERT_STR(shown, "55");
    lp_calc_init(&c);
    type("200*10%=");
    LP_ASSERT_STR(shown, "20");
    lp_calc_init(&c);
    type("50%");
    LP_ASSERT_STR(shown, "0.5");
}

LP_TEST(sign_point_and_backspace_edit_the_entry) {
    lp_calc_init(&c);
    type("12.5s");
    LP_ASSERT_STR(shown, "-12.5");
    type("s");
    LP_ASSERT_STR(shown, "12.5");
    type(".");
    LP_ASSERT_STR(shown, "12.5");   /* one point only */
    type("bbb");
    LP_ASSERT_STR(shown, "1");
    type("b");
    LP_ASSERT_STR(shown, "0");
    lp_calc_init(&c);
    type(".5");
    LP_ASSERT_STR(shown, "0.5");
    lp_calc_init(&c);
    type("s7");
    LP_ASSERT_STR(shown, "-7");
    lp_calc_init(&c);
    type("0007");
    LP_ASSERT_STR(shown, "7");
}

LP_TEST(clear_is_c_while_typing_and_ac_otherwise) {
    lp_calc_init(&c);
    LP_ASSERT(lp_calc_clear_is_all(&c));
    type("2+3");
    LP_ASSERT(!lp_calc_clear_is_all(&c));
    type("c");
    LP_ASSERT(lp_calc_clear_is_all(&c));
    LP_ASSERT_STR(expr, "2 +");
    type("4=");
    LP_ASSERT_STR(shown, "6");
    type("c");
    LP_ASSERT_STR(shown, "0");
    LP_ASSERT_STR(expr, "");
}

LP_TEST(memory_adds_subtracts_recalls_and_clears) {
    lp_calc_init(&c);
    type("5M3M");
    LP_ASSERT_STR(shown, "3");
    type("R");
    LP_ASSERT_STR(shown, "8");
    type("2");                   /* a recalled number is replaced, not appended to */
    LP_ASSERT_STR(shown, "2");
    type("m");
    type("R");
    LP_ASSERT_STR(shown, "6");
    type("+R=");
    LP_ASSERT_STR(shown, "12");
    type("Z");
    LP_ASSERT(!c.has_memory);
}

LP_TEST(caps_a_typed_number_at_sixteen_digits) {
    lp_calc_init(&c);
    type("12345678901234567890");
    LP_ASSERT_EQ(c.entry[16], 0);
    LP_ASSERT_STR(shown, "1,234,567,890,123,456");
}

LP_TEST(formats_values_for_the_display) {
    lp_calc_init(&c);
    type(".1+.2=");
    LP_ASSERT_STR(shown, "0.3");
    char s[64];
    lp_calc_format(1234567, s, sizeof s);
    LP_ASSERT_STR(s, "1,234,567");
    lp_calc_format(-0.0, s, sizeof s);
    LP_ASSERT_STR(s, "0");
    lp_calc_format(1e20, s, sizeof s);
    LP_ASSERT_STR(s, "1e20");
    lp_calc_format(-2.5e-7, s, sizeof s);
    LP_ASSERT_STR(s, "-2.5e-7");
    lp_calc_init(&c);
    type("1234.50");
    LP_ASSERT_STR(shown, "1,234.50");  /* a typed number keeps its trailing zeros */
}

LP_TEST(maps_the_keyboard) {
    LP_ASSERT_EQ(lp_calc_key_for(0, "5"), LP_CALC_5);
    LP_ASSERT_EQ(lp_calc_key_for(0, "*"), LP_CALC_MUL);
    LP_ASSERT_EQ(lp_calc_key_for(0, "x"), LP_CALC_MUL);
    LP_ASSERT_EQ(lp_calc_key_for(0, ","), LP_CALC_POINT);
    LP_ASSERT_EQ(lp_calc_key_for(XKB_KEY_Return, "\r"), LP_CALC_EQUALS);
    LP_ASSERT_EQ(lp_calc_key_for(XKB_KEY_KP_Enter, ""), LP_CALC_EQUALS);
    LP_ASSERT_EQ(lp_calc_key_for(XKB_KEY_BackSpace, "\b"), LP_CALC_BACKSPACE);
    LP_ASSERT_EQ(lp_calc_key_for(XKB_KEY_Escape, "\x1b"), LP_CALC_CLEAR);
    LP_ASSERT_EQ(lp_calc_key_for(0, "a"), LP_CALC_KEY_COUNT);
    LP_ASSERT_EQ(lp_calc_key_for(0, ""), LP_CALC_KEY_COUNT);
    LP_ASSERT_EQ(lp_calc_key_for(0, "×"), LP_CALC_KEY_COUNT);
}

LP_TEST(copies_and_pastes_plain_numbers) {
    lp_calc_init(&c);
    type("1234.5s");
    char text[64];
    lp_calc_copy_text(&c, text, sizeof text);
    LP_ASSERT_STR(text, "-1234.5");
    lp_calc_init(&c);
    lp_calc_paste(&c, "1,000 * 3\n", -1);
    type("=");
    LP_ASSERT_STR(shown, "3,000");
    lp_calc_init(&c);
    lp_calc_paste(&c, "12abc", 3);       /* only len bytes; the "c" is never a clear */
    type("");
    LP_ASSERT_STR(shown, "12");
    type("/0=");
    lp_calc_copy_text(&c, text, sizeof text);
    LP_ASSERT_STR(text, "");
}

int main(void) {
    LP_RUN(multiplies_and_divides_before_adding);
    LP_RUN(shows_the_expression_while_it_is_typed);
    LP_RUN(repeats_the_last_operation_on_equals);
    LP_RUN(an_operator_after_another_replaces_it);
    LP_RUN(continues_from_a_result_and_starts_over_on_a_number);
    LP_RUN(divide_by_zero_is_an_error_until_a_number_or_clear);
    LP_RUN(percent_of_what_it_is_added_to);
    LP_RUN(sign_point_and_backspace_edit_the_entry);
    LP_RUN(clear_is_c_while_typing_and_ac_otherwise);
    LP_RUN(memory_adds_subtracts_recalls_and_clears);
    LP_RUN(caps_a_typed_number_at_sixteen_digits);
    LP_RUN(formats_values_for_the_display);
    LP_RUN(maps_the_keyboard);
    LP_RUN(copies_and_pastes_plain_numbers);
    LP_TEST_MAIN_END();
}
