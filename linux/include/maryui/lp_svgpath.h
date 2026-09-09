/* A parser for SVG path data (the `d` attribute) that emits cairo path
 * segments: M/m L/l H/h V/v C/c S/s Q/q T/t A/a Z/z with implicit repeats.
 * Enough for icons.json and for traced marks. */
#ifndef MARYUI_LP_SVGPATH_H
#define MARYUI_LP_SVGPATH_H

#include <cairo.h>

/* Appends the path to cr's current path. Returns the number of commands, or -1 on a syntax error. */
int lp_svgpath_apply(cairo_t *cr, const char *d);

#endif
