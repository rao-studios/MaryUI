/* MenuItem rows are painted by lp_menu (Menu.c); this unit exists so the
 * component directory mirrors the web's. */
#include "maryui/components/lp_menu.h"
#include "maryui/lp_tokens.h"

int lp_menu_item_height(void) { return (int)LP_SIZE_CONTROL_HEIGHT; }
