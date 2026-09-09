/* MaryUI: the Liquid Platinum design system in C. Include this for everything.
 * The web app in ../web is the reference; tokens.json is the contract. */
#ifndef MARYUI_H
#define MARYUI_H

#include "maryui/lp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "0.1.0" — tracks web/package.json. */
const char *lp_version(void);

#ifdef __cplusplus
}
#endif

#endif
