#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "maryui/lp_settings.h"
#include "maryui/lp_tokens.h"

lp_settings lp_settings_defaults(void) {
    /* Molten is the default, as it is on the web; it falls back on its own
     * when the image has no EGL (lp_wallpaper_cached). */
    /* Slate folders, as object.folder-appearance says and base.css's bare :root maps. */
    /* The clock shows until someone hides it, so a file written before the key existed keeps it. */
    return (lp_settings){ .accent = LP_ACCENT_BLUE, .folders = LP_FOLDER_SLATE, .goo = 1,
                          .wallpaper = LP_WALLPAPER_MOLTEN, .molten_tone = LP_MOLTEN_PLATINUM, .reduced_motion = 0,
                          .clock = 1, .key_repeat_rate = 25, .key_repeat_delay = 600 };
}

static void config_path(char *out, size_t n, int mkdirs) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char dir[1024];
    if (xdg && *xdg) snprintf(dir, sizeof dir, "%s/maryui", xdg);
    else snprintf(dir, sizeof dir, "%s/.config/maryui", getenv("HOME") ? getenv("HOME") : "/tmp");
    if (mkdirs) {
        char parent[1024];
        snprintf(parent, sizeof parent, "%s", dir);
        char *slash = strrchr(parent, '/');
        if (slash) { *slash = 0; mkdir(parent, 0755); }
        mkdir(dir, 0755);
    }
    snprintf(out, n, "%s/settings.conf", dir);
}

lp_settings lp_settings_load(void) {
    lp_settings s = lp_settings_defaults();
    char path[1100];
    config_path(path, sizeof path, 0);
    FILE *f = fopen(path, "r");
    if (!f) return s;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char key[64], value[128];
        if (sscanf(line, " %63[A-Za-z0-9_] = %127s", key, value) != 2) continue;
        if (strcmp(key, "accent") == 0) s.accent = strcmp(value, "graphite") == 0 ? LP_ACCENT_GRAPHITE : LP_ACCENT_BLUE;
        else if (strcmp(key, "folders") == 0) s.folders = strcmp(value, "manila") == 0 ? LP_FOLDER_MANILA : LP_FOLDER_SLATE;
        else if (strcmp(key, "goo") == 0) s.goo = strcmp(value, "off") != 0 && strcmp(value, "0") != 0;
        else if (strcmp(key, "wallpaper") == 0)
            s.wallpaper = strcmp(value, "raster") == 0 ? LP_WALLPAPER_RASTER
                        : strcmp(value, "procedural") == 0 ? LP_WALLPAPER_PROCEDURAL
                                                           : LP_WALLPAPER_MOLTEN;
        else if (strcmp(key, "molten_tone") == 0)
            s.molten_tone = strcmp(value, "faithful") == 0 ? LP_MOLTEN_FAITHFUL : LP_MOLTEN_PLATINUM;
        else if (strcmp(key, "reduced_motion") == 0) s.reduced_motion = strcmp(value, "on") == 0 || strcmp(value, "1") == 0;
        else if (strcmp(key, "clock") == 0) s.clock = strcmp(value, "off") != 0 && strcmp(value, "0") != 0;
        else if (strcmp(key, "key_repeat_rate") == 0) s.key_repeat_rate = atoi(value);
        else if (strcmp(key, "key_repeat_delay") == 0) s.key_repeat_delay = atoi(value);
        else if (strcmp(key, "keyboard_layout") == 0) snprintf(s.keyboard_layout, sizeof s.keyboard_layout, "%s", strcmp(value, "default") == 0 ? "" : value);
        else if (strcmp(key, "pointer_speed") == 0) s.pointer_speed = (float)atof(value);
        else if (strcmp(key, "natural_scroll") == 0) s.natural_scroll = strcmp(value, "on") == 0 || strcmp(value, "1") == 0;
        else if (strcmp(key, "clock_24h") == 0) s.clock_24h = strcmp(value, "on") == 0 || strcmp(value, "1") == 0;
        else if (strcmp(key, "dock") == 0) snprintf(s.dock, sizeof s.dock, "%s", value);
    }
    fclose(f);
    /* a hand-edited file stays inside what the compositor can use (a rate of 0 would divide by zero) */
    if (s.key_repeat_rate < 1) s.key_repeat_rate = 1;
    if (s.key_repeat_rate > 100) s.key_repeat_rate = 100;
    if (s.key_repeat_delay < 100) s.key_repeat_delay = 100;
    if (s.key_repeat_delay > 2000) s.key_repeat_delay = 2000;
    if (!(s.pointer_speed >= -1)) s.pointer_speed = -1;
    if (s.pointer_speed > 1) s.pointer_speed = 1;
    return s;
}

int lp_settings_save(const lp_settings *s) {
    char path[1100];
    config_path(path, sizeof path, 1);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# Liquid Platinum desktop settings (View menu). Mirrors the web's localStorage['lp-settings'];\n"
               "# clock is the one key the web does not have.\n");
    fprintf(f, "accent=%s\nfolders=%s\ngoo=%s\nwallpaper=%s\nmolten_tone=%s\nreduced_motion=%s\nclock=%s\n",
        s->accent == LP_ACCENT_GRAPHITE ? "graphite" : "blue",
        s->folders == LP_FOLDER_MANILA ? "manila" : "slate", s->goo ? "on" : "off",
        s->wallpaper == LP_WALLPAPER_RASTER ? "raster" : s->wallpaper == LP_WALLPAPER_PROCEDURAL ? "procedural" : "molten",
        s->molten_tone == LP_MOLTEN_FAITHFUL ? "faithful" : "platinum", s->reduced_motion ? "on" : "off",
        s->clock ? "on" : "off");
    fprintf(f, "# System Settings\nkey_repeat_rate=%d\nkey_repeat_delay=%d\nkeyboard_layout=%s\npointer_speed=%.2f\nnatural_scroll=%s\nclock_24h=%s\n",
        s->key_repeat_rate, s->key_repeat_delay, s->keyboard_layout[0] ? s->keyboard_layout : "default", s->pointer_speed,
        s->natural_scroll ? "on" : "off", s->clock_24h ? "on" : "off");
    if (s->dock[0]) fprintf(f, "dock=%s\n", s->dock);
    fclose(f);
    return 0;
}

lp_ramp lp_settings_folders(const lp_settings *s) {
    if (s && s->folders == LP_FOLDER_MANILA)
        return (lp_ramp){ LP_OBJECT_MATERIAL_MANILA_HI, LP_OBJECT_MATERIAL_MANILA_MID, LP_OBJECT_MATERIAL_MANILA_LO };
    return (lp_ramp){ LP_OBJECT_MATERIAL_SLATE_HI, LP_OBJECT_MATERIAL_SLATE_MID, LP_OBJECT_MATERIAL_SLATE_LO };
}

lp_accent lp_settings_accent(const lp_settings *s) {
    if (s && s->accent == LP_ACCENT_GRAPHITE) {
        return (lp_accent){ LP_ACCENT_GRAPHITE_BASE, LP_ACCENT_GRAPHITE_DEEP, LP_ACCENT_GRAPHITE_LIGHT, LP_ACCENT_GRAPHITE_SOFT, LP_ACCENT_GRAPHITE_FOCUS_RING };
    }
    return (lp_accent){ LP_ACCENT_BLUE_BASE, LP_ACCENT_BLUE_DEEP, LP_ACCENT_BLUE_LIGHT, LP_ACCENT_BLUE_SOFT, LP_ACCENT_BLUE_FOCUS_RING };
}

static void unquote(char *v) {
    size_t n = strlen(v);
    while (n && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ')) v[--n] = 0;
    if (n >= 2 && (v[0] == '"' || v[0] == '\'') && v[n - 1] == v[0]) { memmove(v, v + 1, n - 2); v[n - 2] = 0; }
}

lp_branding lp_branding_load(void) {
    lp_branding b;
    snprintf(b.name, sizeof b.name, "Liquid Platinum");
    snprintf(b.pretty_name, sizeof b.pretty_name, "Liquid Platinum");
    snprintf(b.version, sizeof b.version, "%s", "");
    FILE *f = fopen("/etc/os-release", "r");
    if (!f) return b;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *value = eq + 1;
        unquote(value);
        if (strcmp(line, "NAME") == 0) snprintf(b.name, sizeof b.name, "%s", value);
        else if (strcmp(line, "PRETTY_NAME") == 0) snprintf(b.pretty_name, sizeof b.pretty_name, "%s", value);
        else if (strcmp(line, "VERSION_ID") == 0) snprintf(b.version, sizeof b.version, "%s", value);
    }
    fclose(f);
    return b;
}
