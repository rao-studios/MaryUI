#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "maryui/lp_settings.h"
#include "maryui/lp_tokens.h"

lp_settings lp_settings_defaults(void) {
    /* Molten is the default, as it is on the web; it falls back on its own
     * when the image has no EGL (lp_wallpaper_cached). */
    return (lp_settings){ .accent = LP_ACCENT_BLUE, .goo = 1, .wallpaper = LP_WALLPAPER_MOLTEN,
                          .molten_tone = LP_MOLTEN_PLATINUM, .reduced_motion = 0 };
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
        if (sscanf(line, " %63[A-Za-z_] = %127s", key, value) != 2) continue;
        if (strcmp(key, "accent") == 0) s.accent = strcmp(value, "graphite") == 0 ? LP_ACCENT_GRAPHITE : LP_ACCENT_BLUE;
        else if (strcmp(key, "goo") == 0) s.goo = strcmp(value, "off") != 0 && strcmp(value, "0") != 0;
        else if (strcmp(key, "wallpaper") == 0)
            s.wallpaper = strcmp(value, "raster") == 0 ? LP_WALLPAPER_RASTER
                        : strcmp(value, "procedural") == 0 ? LP_WALLPAPER_PROCEDURAL
                                                           : LP_WALLPAPER_MOLTEN;
        else if (strcmp(key, "molten_tone") == 0)
            s.molten_tone = strcmp(value, "faithful") == 0 ? LP_MOLTEN_FAITHFUL : LP_MOLTEN_PLATINUM;
        else if (strcmp(key, "reduced_motion") == 0) s.reduced_motion = strcmp(value, "on") == 0 || strcmp(value, "1") == 0;
    }
    fclose(f);
    return s;
}

int lp_settings_save(const lp_settings *s) {
    char path[1100];
    config_path(path, sizeof path, 1);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# Liquid Platinum desktop settings (View menu). Mirrors the web's localStorage['lp-settings'].\n");
    fprintf(f, "accent=%s\ngoo=%s\nwallpaper=%s\nmolten_tone=%s\nreduced_motion=%s\n",
        s->accent == LP_ACCENT_GRAPHITE ? "graphite" : "blue", s->goo ? "on" : "off",
        s->wallpaper == LP_WALLPAPER_RASTER ? "raster" : s->wallpaper == LP_WALLPAPER_PROCEDURAL ? "procedural" : "molten",
        s->molten_tone == LP_MOLTEN_FAITHFUL ? "faithful" : "platinum", s->reduced_motion ? "on" : "off");
    fclose(f);
    return 0;
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
