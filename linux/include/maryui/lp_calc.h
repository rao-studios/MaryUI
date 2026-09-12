/* Calculator's model: the keys of a four-function calculator, entered the way
 * people type them — 2 + 3 × 4 = is 14, precedence included — with percent,
 * sign change, a memory register and repeated equals. Pure and host-agnostic:
 * the app (src/apps/calculator.c) is its keypad and display. Linux only
 * (PARITY D15). */
#ifndef MARYUI_LP_CALC_H
#define MARYUI_LP_CALC_H

#include <stddef.h>
#include <stdint.h>

enum lp_calc_key {
    LP_CALC_0, LP_CALC_1, LP_CALC_2, LP_CALC_3, LP_CALC_4,
    LP_CALC_5, LP_CALC_6, LP_CALC_7, LP_CALC_8, LP_CALC_9,
    LP_CALC_POINT,
    LP_CALC_ADD, LP_CALC_SUB, LP_CALC_MUL, LP_CALC_DIV,
    LP_CALC_EQUALS,
    LP_CALC_PERCENT,
    LP_CALC_SIGN,
    LP_CALC_CLEAR,       /* C while a number is being typed, AC otherwise (memory survives both) */
    LP_CALC_BACKSPACE,
    LP_CALC_MC, LP_CALC_MR, LP_CALC_MPLUS, LP_CALC_MMINUS,
    LP_CALC_KEY_COUNT,   /* also "no key" from lp_calc_key_for */
};

#define LP_CALC_MAX_TERMS 32
#define LP_CALC_DIGITS 16    /* digits a typed number may have */

typedef struct lp_calc {
    double values[LP_CALC_MAX_TERMS];   /* the committed terms: values[i] ops[i] values[i+1] … */
    int ops[LP_CALC_MAX_TERMS];         /* the operator after values[i] (LP_CALC_ADD…DIV) */
    int count;
    char entry[32];      /* the number being typed, as typed; "" when none */
    int fresh;           /* the entry was put there (MR, %, M±): the next digit replaces it */
    double result;       /* after = */
    int has_result;
    int last_op;         /* the operator and operand = repeats; last_op < 0: none */
    double last_operand;
    double memory;
    int has_memory;
    int error;           /* a division by zero or an overflow: "Error" until a digit or clear */
    char tape[160];      /* "2 + 3 × 4 =", the expression the result came from */
} lp_calc;

void lp_calc_init(lp_calc *c);
void lp_calc_press(lp_calc *c, enum lp_calc_key key);
/* What the display shows: the number being typed (digits grouped), else the running value, or "Error". */
void lp_calc_display(const lp_calc *c, char *out, size_t n);
/* The line above it: "2 + 3 ×", then "2 + 3 × 4 =" once evaluated; "" after an error. */
void lp_calc_expression(const lp_calc *c, char *out, size_t n);
/* 1 when the clear key means AC (nothing is being typed), 0 when it means C. */
int lp_calc_clear_is_all(const lp_calc *c);
/* A value as the display shows it: 12 significant digits, grouped, "1e20" beyond. */
void lp_calc_format(double v, char *out, size_t n);
/* The keyboard: digits . , + - * x / = % c, Return, Backspace, Delete and Escape.
 * LP_CALC_KEY_COUNT for anything else. */
enum lp_calc_key lp_calc_key_for(uint32_t keysym, const char *utf8);
/* The display without grouping, for the clipboard; "" after an error. */
void lp_calc_copy_text(const lp_calc *c, char *out, size_t n);
/* Types text in as keys, skipping grouping commas and whitespace; len < 0 for NUL-terminated. */
void lp_calc_paste(lp_calc *c, const char *text, int len);

#endif
