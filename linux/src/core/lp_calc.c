/* Calculator's arithmetic (lp_calc.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/lp_calc.h"

static const char *const OP_TEXT[] = { "+", "−", "×", "÷" };

static int is_op(int k) { return k >= LP_CALC_ADD && k <= LP_CALC_DIV; }

void lp_calc_init(lp_calc *c) {
    memset(c, 0, sizeof *c);
    c->last_op = -1;
}

static void all_clear(lp_calc *c) {
    double memory = c->memory;
    int has_memory = c->has_memory;
    lp_calc_init(c);
    c->memory = memory;
    c->has_memory = has_memory;
}

static void fail(lp_calc *c) {
    all_clear(c);
    c->error = 1;
}

/* "-1234.5" → "-1,234.5"; "1e+20" → "1e20". */
static void group(const char *num, char *out, size_t n) {
    char buf[80];
    size_t o = 0;
    const char *p = num;
    if (*p == '-') buf[o++] = *p++;
    size_t int_len = strspn(p, "0123456789");
    for (size_t i = 0; i < int_len && o < 40; i++) {
        if (i > 0 && (int_len - i) % 3 == 0) buf[o++] = ',';
        buf[o++] = p[i];
    }
    p += int_len;
    if (*p == '.') {
        buf[o++] = *p++;
        size_t frac = strspn(p, "0123456789");
        if (frac > 20) frac = 20;
        memcpy(buf + o, p, frac);
        o += frac;
        p += frac;
    }
    if (*p == 'e' || *p == 'E') {
        buf[o++] = 'e';
        p++;
        if (*p == '-') buf[o++] = *p++;
        else if (*p == '+') p++;
        while (*p == '0' && p[1]) p++;
        while (*p >= '0' && *p <= '9' && o < sizeof buf - 1) buf[o++] = *p++;
    }
    buf[o] = 0;
    snprintf(out, n, "%s", buf);
}

void lp_calc_format(double v, char *out, size_t n) {
    if (!isfinite(v)) { snprintf(out, n, "Error"); return; }
    if (v == 0) v = 0; /* no "-0" */
    char raw[40];
    snprintf(raw, sizeof raw, "%.12g", v);
    group(raw, out, n);
}

static int digits_in(const char *s) {
    int n = 0;
    for (; *s; s++) n += *s >= '0' && *s <= '9';
    return n;
}

/* The number on the display. */
static double shown_value(const lp_calc *c) {
    if (c->entry[0]) return strtod(c->entry, NULL);
    if (c->has_result) return c->result;
    if (c->count > 0) return c->values[c->count - 1];
    return 0;
}

static void set_entry(lp_calc *c, double v) {
    if (v == 0) v = 0;
    snprintf(c->entry, sizeof c->entry, "%.12g", v);
    c->fresh = 1;
}

static int apply(double a, int op, double b, double *out) {
    switch (op) {
    case LP_CALC_ADD: *out = a + b; break;
    case LP_CALC_SUB: *out = a - b; break;
    case LP_CALC_MUL: *out = a * b; break;
    case LP_CALC_DIV: if (b == 0) return -1; *out = a / b; break;
    default: return -1;
    }
    return isfinite(*out) ? 0 : -1;
}

/* values/ops with `last` as the final operand: × and ÷ bind first, then + and −, each left to right. */
static int evaluate(const lp_calc *c, double last, double *out) {
    double sum = 0, term = c->count > 0 ? c->values[0] : last;
    int sign = 1;
    for (int i = 0; i < c->count; i++) {
        double next = i + 1 < c->count ? c->values[i + 1] : last;
        switch (c->ops[i]) {
        case LP_CALC_MUL: term *= next; break;
        case LP_CALC_DIV: if (next == 0) return -1; term /= next; break;
        default: sum += sign * term; sign = c->ops[i] == LP_CALC_ADD ? 1 : -1; term = next; break;
        }
    }
    sum += sign * term;
    if (!isfinite(sum)) return -1;
    *out = sum;
    return 0;
}

static void append_word(char *out, size_t n, const char *word) {
    size_t len = strlen(out);
    if (len + 1 < n) snprintf(out + len, n - len, "%s%s", len ? " " : "", word);
}

static void append_value(char *out, size_t n, double v) {
    char s[64];
    lp_calc_format(v, s, sizeof s);
    append_word(out, n, s);
}

static void terms_text(const lp_calc *c, char *out, size_t n) {
    out[0] = 0;
    for (int i = 0; i < c->count; i++) {
        append_value(out, n, c->values[i]);
        append_word(out, n, OP_TEXT[c->ops[i] - LP_CALC_ADD]);
    }
}

static void type_digit(lp_calc *c, int key) {
    if (c->has_result) { c->has_result = 0; c->tape[0] = 0; }   /* a number after = starts over */
    if (c->fresh) { c->entry[0] = 0; c->fresh = 0; }
    size_t len = strlen(c->entry);
    if (key == LP_CALC_POINT) {
        if (strchr(c->entry, '.') || strchr(c->entry, 'e')) return;
        if (len == 0) snprintf(c->entry, sizeof c->entry, "0.");
        else if (len + 1 < sizeof c->entry) { c->entry[len] = '.'; c->entry[len + 1] = 0; }
        return;
    }
    if (strchr(c->entry, 'e') || digits_in(c->entry) >= LP_CALC_DIGITS) return;
    char digit = (char)('0' + key);
    if (strcmp(c->entry, "0") == 0) { c->entry[0] = digit; return; }
    if (strcmp(c->entry, "-0") == 0) { c->entry[1] = digit; return; }
    if (len + 1 < sizeof c->entry) { c->entry[len] = digit; c->entry[len + 1] = 0; }
}

static void equals(lp_calc *c) {
    double out;
    char tape[sizeof c->tape];
    if (c->count > 0) {
        double operand = c->entry[0] ? strtod(c->entry, NULL) : c->values[c->count - 1]; /* "2 + =" is 2 + 2 */
        terms_text(c, tape, sizeof tape);
        append_value(tape, sizeof tape, operand);
        if (evaluate(c, operand, &out)) { fail(c); return; }
        c->last_op = c->ops[c->count - 1];
        c->last_operand = operand;
    } else if (c->last_op >= 0 && (c->entry[0] || c->has_result)) {
        double from = shown_value(c);   /* = again, or a new number and =: the last operation repeats */
        tape[0] = 0;
        append_value(tape, sizeof tape, from);
        append_word(tape, sizeof tape, OP_TEXT[c->last_op - LP_CALC_ADD]);
        append_value(tape, sizeof tape, c->last_operand);
        if (apply(from, c->last_op, c->last_operand, &out)) { fail(c); return; }
    } else if (c->entry[0]) {
        out = strtod(c->entry, NULL);
        tape[0] = 0;
        append_value(tape, sizeof tape, out);
    } else {
        return;
    }
    append_word(tape, sizeof tape, "=");
    snprintf(c->tape, sizeof c->tape, "%s", tape);
    c->result = out == 0 ? 0 : out;
    c->has_result = 1;
    c->count = 0;
    c->entry[0] = 0;
    c->fresh = 0;
}

void lp_calc_press(lp_calc *c, enum lp_calc_key key) {
    if (key >= LP_CALC_KEY_COUNT) return;
    if (c->error) {
        /* only a fresh start leaves the error: clear, or a number */
        if (key != LP_CALC_CLEAR && key > LP_CALC_POINT) return;
        all_clear(c);
        if (key == LP_CALC_CLEAR) return;
    }
    if (key <= LP_CALC_POINT) { type_digit(c, key); return; }
    if (is_op(key)) {
        double v;
        if (c->entry[0]) v = strtod(c->entry, NULL);
        else if (c->has_result) v = c->result;
        else if (c->count > 0) { c->ops[c->count - 1] = key; return; }   /* "2 + ×" changes its mind */
        else v = 0;
        if (c->count >= LP_CALC_MAX_TERMS) return;
        c->values[c->count] = v;
        c->ops[c->count] = key;
        c->count++;
        c->entry[0] = 0;
        c->fresh = 0;
        c->has_result = 0;
        c->tape[0] = 0;
        return;
    }
    switch (key) {
    case LP_CALC_EQUALS: equals(c); return;
    case LP_CALC_PERCENT: {
        double x = shown_value(c), v = x / 100;
        if (c->count > 0 && c->entry[0] && (c->ops[c->count - 1] == LP_CALC_ADD || c->ops[c->count - 1] == LP_CALC_SUB)) {
            /* "50 + 10 %" is ten percent of what it is added to */
            lp_calc head = *c;
            head.count = c->count - 1;
            double base;
            if (evaluate(&head, c->values[c->count - 1], &base)) { fail(c); return; }
            v = base * x / 100;
        }
        if (!c->entry[0] && c->has_result) c->result = v;
        else set_entry(c, v);
        return;
    }
    case LP_CALC_SIGN:
        if (c->entry[0] == '-') memmove(c->entry, c->entry + 1, strlen(c->entry));
        else if (c->entry[0]) {
            size_t len = strlen(c->entry);
            if (len + 1 < sizeof c->entry) { memmove(c->entry + 1, c->entry, len + 1); c->entry[0] = '-'; }
        } else if (c->has_result) c->result = -c->result;
        else snprintf(c->entry, sizeof c->entry, "-0");
        return;
    case LP_CALC_CLEAR:
        if (c->entry[0]) { c->entry[0] = 0; c->fresh = 0; }
        else all_clear(c);
        return;
    case LP_CALC_BACKSPACE: {
        size_t len = strlen(c->entry);
        if (c->fresh || len == 0) return;
        c->entry[len - 1] = 0;
        if (strcmp(c->entry, "-") == 0) c->entry[0] = 0;
        return;
    }
    case LP_CALC_MC: c->memory = 0; c->has_memory = 0; return;
    case LP_CALC_MR:
        if (!c->has_memory) return;
        if (c->has_result) { c->has_result = 0; c->tape[0] = 0; }
        set_entry(c, c->memory);
        return;
    case LP_CALC_MPLUS:
    case LP_CALC_MMINUS:
        c->memory += key == LP_CALC_MPLUS ? shown_value(c) : -shown_value(c);
        c->has_memory = 1;
        if (c->entry[0]) c->fresh = 1;
        return;
    default: return;
    }
}

void lp_calc_display(const lp_calc *c, char *out, size_t n) {
    if (c->error) { snprintf(out, n, "Error"); return; }
    if (c->entry[0]) { group(c->entry, out, n); return; }
    lp_calc_format(shown_value(c), out, n);
}

void lp_calc_expression(const lp_calc *c, char *out, size_t n) {
    if (n) out[0] = 0;
    if (c->error) return;
    if (c->count == 0 && c->has_result) { snprintf(out, n, "%s", c->tape); return; }
    terms_text(c, out, n);
    if (c->entry[0]) {
        char s[80];
        group(c->entry, s, sizeof s);
        append_word(out, n, s);
    }
}

int lp_calc_clear_is_all(const lp_calc *c) { return !c->entry[0]; }

enum lp_calc_key lp_calc_key_for(uint32_t keysym, const char *utf8) {
    switch (keysym) {
    case XKB_KEY_Return: case XKB_KEY_KP_Enter: return LP_CALC_EQUALS;
    case XKB_KEY_BackSpace: case XKB_KEY_Delete: return LP_CALC_BACKSPACE;
    case XKB_KEY_Escape: return LP_CALC_CLEAR;
    default: break;
    }
    if (!utf8 || !utf8[0] || utf8[1]) return LP_CALC_KEY_COUNT;
    char ch = utf8[0];
    if (ch >= '0' && ch <= '9') return (enum lp_calc_key)(LP_CALC_0 + (ch - '0'));
    switch (ch) {
    case '.': case ',': return LP_CALC_POINT;
    case '+': return LP_CALC_ADD;
    case '-': return LP_CALC_SUB;
    case '*': case 'x': case 'X': return LP_CALC_MUL;
    case '/': return LP_CALC_DIV;
    case '=': return LP_CALC_EQUALS;
    case '%': return LP_CALC_PERCENT;
    case 'c': case 'C': return LP_CALC_CLEAR;
    default: return LP_CALC_KEY_COUNT;
    }
}

void lp_calc_copy_text(const lp_calc *c, char *out, size_t n) {
    if (n) out[0] = 0;
    if (c->error) return;
    char shown[80];
    lp_calc_display(c, shown, sizeof shown);
    size_t o = 0;
    for (const char *p = shown; *p && o + 1 < n; p++) if (*p != ',') out[o++] = *p;
    if (n) out[o] = 0;
}

void lp_calc_paste(lp_calc *c, const char *text, int len) {
    if (!text) return;
    size_t n = len < 0 ? strlen(text) : (size_t)len;
    for (size_t i = 0; i < n; i++) {
        char ch[2] = { text[i], 0 };
        if (ch[0] == ',' || ch[0] == ' ' || ch[0] == '\t' || ch[0] == '\n' || ch[0] == '\r') continue;
        enum lp_calc_key key = lp_calc_key_for(0, ch);
        if (key != LP_CALC_CLEAR) lp_calc_press(c, key);   /* a "c" in pasted text is not a command */
    }
}
