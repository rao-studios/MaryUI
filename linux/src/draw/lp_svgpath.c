#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "maryui/lp_svgpath.h"

struct parser {
    const char *p;
    double cx, cy;       /* current point */
    double sx, sy;       /* subpath start */
    double lcx, lcy;     /* last control point (for S/T) */
    char last;           /* last command letter */
    int count;
};

static void skip(struct parser *ps) {
    while (*ps->p && (isspace((unsigned char)*ps->p) || *ps->p == ',')) ps->p++;
}

static int number(struct parser *ps, double *out) {
    skip(ps);
    const char *start = ps->p;
    char *end;
    double v = strtod(start, &end);
    if (end == start) return 0;
    ps->p = end;
    *out = v;
    return 1;
}

static int flag(struct parser *ps, int *out) {
    skip(ps);
    if (*ps->p == '0' || *ps->p == '1') { *out = *ps->p - '0'; ps->p++; return 1; }
    return 0;
}

/* SVG implementation notes F.6.5: endpoint to centre parameterisation, then
 * the arc as a series of cairo arcs on a unit circle under a transform. */
static void arc_to(cairo_t *cr, struct parser *ps, double rx, double ry, double phi_deg, int large, int sweep, double x2, double y2) {
    double x1 = ps->cx, y1 = ps->cy;
    if (rx == 0 || ry == 0) { cairo_line_to(cr, x2, y2); return; }
    rx = fabs(rx); ry = fabs(ry);
    double phi = phi_deg * M_PI / 180.0, cp = cos(phi), sp = sin(phi);
    double dx = (x1 - x2) / 2, dy = (y1 - y2) / 2;
    double x1p = cp * dx + sp * dy, y1p = -sp * dx + cp * dy;
    double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1) { rx *= sqrt(lambda); ry *= sqrt(lambda); }
    double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    double coef = den == 0 ? 0 : sqrt(fmax(0, num / den));
    if (large == sweep) coef = -coef;
    double cxp = coef * (rx * y1p / ry), cyp = coef * (-ry * x1p / rx);
    double cx = cp * cxp - sp * cyp + (x1 + x2) / 2, cy = sp * cxp + cp * cyp + (y1 + y2) / 2;
    double ux = (x1p - cxp) / rx, uy = (y1p - cyp) / ry;
    double vx = (-x1p - cxp) / rx, vy = (-y1p - cyp) / ry;
    double theta1 = atan2(uy, ux);
    double dot = ux * vx + uy * vy, len = sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
    double dtheta = acos(fmax(-1, fmin(1, len == 0 ? 1 : dot / len)));
    if (ux * vy - uy * vx < 0) dtheta = -dtheta;
    if (sweep && dtheta < 0) dtheta += 2 * M_PI;
    if (!sweep && dtheta > 0) dtheta -= 2 * M_PI;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, phi);
    cairo_scale(cr, rx, ry);
    if (sweep) cairo_arc(cr, 0, 0, 1, theta1, theta1 + dtheta);
    else cairo_arc_negative(cr, 0, 0, 1, theta1, theta1 + dtheta);
    cairo_restore(cr);
}

int lp_svgpath_apply(cairo_t *cr, const char *d) {
    struct parser ps = { .p = d, .last = 0 };
    char cmd = 0;
    for (;;) {
        skip(&ps);
        if (!*ps.p) break;
        if (isalpha((unsigned char)*ps.p)) {
            cmd = *ps.p++;
        } else if (cmd == 0) {
            return -1;
        } else if (cmd == 'M') cmd = 'L';
        else if (cmd == 'm') cmd = 'l';
        else if (cmd == 'Z' || cmd == 'z') return -1;
        int rel = islower((unsigned char)cmd);
        double ox = rel ? ps.cx : 0, oy = rel ? ps.cy : 0;
        double a, b, c, e, f, g;
        switch (toupper((unsigned char)cmd)) {
        case 'M':
            if (!number(&ps, &a) || !number(&ps, &b)) return -1;
            ps.cx = ox + a; ps.cy = oy + b; ps.sx = ps.cx; ps.sy = ps.cy;
            cairo_move_to(cr, ps.cx, ps.cy);
            ps.lcx = ps.cx; ps.lcy = ps.cy;
            break;
        case 'L':
            if (!number(&ps, &a) || !number(&ps, &b)) return -1;
            ps.cx = ox + a; ps.cy = oy + b;
            cairo_line_to(cr, ps.cx, ps.cy);
            ps.lcx = ps.cx; ps.lcy = ps.cy;
            break;
        case 'H':
            if (!number(&ps, &a)) return -1;
            ps.cx = ox + a;
            cairo_line_to(cr, ps.cx, ps.cy);
            ps.lcx = ps.cx; ps.lcy = ps.cy;
            break;
        case 'V':
            if (!number(&ps, &a)) return -1;
            ps.cy = oy + a;
            cairo_line_to(cr, ps.cx, ps.cy);
            ps.lcx = ps.cx; ps.lcy = ps.cy;
            break;
        case 'C':
            if (!number(&ps, &a) || !number(&ps, &b) || !number(&ps, &c) || !number(&ps, &e) || !number(&ps, &f) || !number(&ps, &g)) return -1;
            cairo_curve_to(cr, ox + a, oy + b, ox + c, oy + e, ox + f, oy + g);
            ps.lcx = ox + c; ps.lcy = oy + e; ps.cx = ox + f; ps.cy = oy + g;
            break;
        case 'S': {
            if (!number(&ps, &c) || !number(&ps, &e) || !number(&ps, &f) || !number(&ps, &g)) return -1;
            double c1x = ps.cx, c1y = ps.cy;
            if (toupper((unsigned char)ps.last) == 'C' || toupper((unsigned char)ps.last) == 'S') { c1x = 2 * ps.cx - ps.lcx; c1y = 2 * ps.cy - ps.lcy; }
            cairo_curve_to(cr, c1x, c1y, ox + c, oy + e, ox + f, oy + g);
            ps.lcx = ox + c; ps.lcy = oy + e; ps.cx = ox + f; ps.cy = oy + g;
            break;
        }
        case 'Q': {
            if (!number(&ps, &a) || !number(&ps, &b) || !number(&ps, &c) || !number(&ps, &e)) return -1;
            double qx = ox + a, qy = oy + b, x = ox + c, y = oy + e;
            cairo_curve_to(cr, ps.cx + 2.0 / 3 * (qx - ps.cx), ps.cy + 2.0 / 3 * (qy - ps.cy), x + 2.0 / 3 * (qx - x), y + 2.0 / 3 * (qy - y), x, y);
            ps.lcx = qx; ps.lcy = qy; ps.cx = x; ps.cy = y;
            break;
        }
        case 'T': {
            if (!number(&ps, &c) || !number(&ps, &e)) return -1;
            double qx = ps.cx, qy = ps.cy;
            if (toupper((unsigned char)ps.last) == 'Q' || toupper((unsigned char)ps.last) == 'T') { qx = 2 * ps.cx - ps.lcx; qy = 2 * ps.cy - ps.lcy; }
            double x = ox + c, y = oy + e;
            cairo_curve_to(cr, ps.cx + 2.0 / 3 * (qx - ps.cx), ps.cy + 2.0 / 3 * (qy - ps.cy), x + 2.0 / 3 * (qx - x), y + 2.0 / 3 * (qy - y), x, y);
            ps.lcx = qx; ps.lcy = qy; ps.cx = x; ps.cy = y;
            break;
        }
        case 'A': {
            int large, sweep;
            if (!number(&ps, &a) || !number(&ps, &b) || !number(&ps, &c) || !flag(&ps, &large) || !flag(&ps, &sweep) || !number(&ps, &f) || !number(&ps, &g)) return -1;
            arc_to(cr, &ps, a, b, c, large, sweep, ox + f, oy + g);
            ps.cx = ox + f; ps.cy = oy + g; ps.lcx = ps.cx; ps.lcy = ps.cy;
            break;
        }
        case 'Z':
            cairo_close_path(cr);
            ps.cx = ps.sx; ps.cy = ps.sy;
            cairo_move_to(cr, ps.cx, ps.cy);
            break;
        default:
            return -1;
        }
        ps.last = cmd;
        ps.count++;
    }
    return ps.count;
}
