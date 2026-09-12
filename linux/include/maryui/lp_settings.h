/* Desktop preferences (web/src/desktop/settings.ts): accent, folder
 * appearance, goo, wallpaper source, reduced motion. Persisted as KEY=VALUE in
 * $XDG_CONFIG_HOME/maryui/settings.conf. The accent and folder mappings are
 * base.css's `:root[data-accent]` and `[data-folders]` blocks. Branding comes
 * from /etc/os-release. */
#ifndef MARYUI_LP_SETTINGS_H
#define MARYUI_LP_SETTINGS_H

#include "maryui/lp_molten.h"
#include "maryui/lp_types.h"

enum lp_accent_kind { LP_ACCENT_BLUE, LP_ACCENT_GRAPHITE };
/* Warm card stock, or the cool blue-grey Mac OS 8/9 drew folders in. */
enum lp_folder_appearance { LP_FOLDER_MANILA, LP_FOLDER_SLATE };
enum lp_wallpaper_mode { LP_WALLPAPER_MOLTEN, LP_WALLPAPER_PROCEDURAL, LP_WALLPAPER_RASTER };

typedef struct lp_settings {
    enum lp_accent_kind accent;
    enum lp_folder_appearance folders;
    int goo;
    enum lp_wallpaper_mode wallpaper;
    enum lp_molten_tone molten_tone;
    int reduced_motion;
} lp_settings;

typedef struct lp_accent {
    lp_color base, deep, light, soft, focus_ring;
} lp_accent;

/* A material's three ramp stops, as object.material.<name>.{hi,mid,lo}. */
typedef struct lp_ramp {
    lp_color hi, mid, lo;
} lp_ramp;

lp_settings lp_settings_defaults(void);
/* Reads the file when it exists; defaults otherwise. */
lp_settings lp_settings_load(void);
int lp_settings_save(const lp_settings *s);
lp_accent lp_settings_accent(const lp_settings *s);
/* The folder material's ramp for the chosen appearance. object.material.folder
 * aliases manila in tokens.json, so the setting picks the ramp, not that alias. */
lp_ramp lp_settings_folders(const lp_settings *s);

typedef struct lp_branding {
    char name[64];         /* NAME, e.g. "MaryOS" */
    char pretty_name[128]; /* PRETTY_NAME, e.g. "MaryOS 0.0 (Liquid Platinum)" */
    char version[32];      /* VERSION_ID */
} lp_branding;

/* /etc/os-release, falling back to "Liquid Platinum". */
lp_branding lp_branding_load(void);

#endif
