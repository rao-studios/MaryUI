/* lp-render: headless renders of Liquid Platinum (the brush tile, the
 * wallpaper, and — as milestones land — components and apps) to PNG, for
 * parity checks against the web app and for the assets the MaryPi builder
 * ships. Links libmaryui only, so it also builds on macOS with Homebrew cairo. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <cairo.h>

#include "maryui/maryui.h"
#include "maryui/components/lp_menu_bar.h"
#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/components/lp_surface.h"
#include "maryui/components/lp_window.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"
#include "maryui/lp_ui.h"
#include "maryui/lp_molten.h"
#include "maryui/lp_wallpaper.h"

static int usage(int status) {
    fprintf(status ? stderr : stdout,
        "usage: lp-render --brush <out.png>              the 512px brushed-platinum tile\n"
        "       lp-render --wallpaper WxH <out.png>      the wallpaper cover-fitted to WxH\n"
        "       lp-render --molten WxH [TONE] <out.png>  the molten shader at WxH (TONE platinum|faithful); needs EGL\n"
        "       lp-render --menubar W <out.png>          the menu bar, W px wide, over the wallpaper\n"
        "       lp-render --window <out.png>             a focused About window and an inactive one, over the wallpaper\n"
        "       lp-render --finder <out.png>             the Finder window (icon view)\n"
        "       lp-render --gallery N <out.png>          the Gallery window on tab N (0 controls … 4 motion)\n"
        "       lp-render --textedit <out.png>           the TextEdit window with a sample document\n"
        "       lp-render --spotlight [QUERY] <out.png>  Spotlight over the desktop: the dock, or the results for QUERY\n"
        "       lp-render --all <dir>                    every preview into <dir>\n"
        "       lp-render --version\n");
    return status;
}

static int ensure_dir(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    if (mkdir(dir, 0755) != 0) { perror(dir); return -1; }
    return 0;
}

static int write_png(cairo_surface_t *s, const char *path) {
    cairo_status_t st = cairo_surface_write_to_png(s, path);
    if (st != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "lp-render: %s: %s\n", path, cairo_status_to_string(st));
        return 1;
    }
    printf("lp-render: wrote %s\n", path);
    return 0;
}

static int render_brush(const char *path) {
    lp_brush_params p = lp_brush_params_from_tokens();
    cairo_surface_t *tile = lp_brush_tile_render(&p);
    int rc = write_png(tile, path);
    cairo_surface_destroy(tile);
    return rc;
}

static int render_wallpaper(int w, int h, const char *path) {
    cairo_surface_t *s = lp_wallpaper_render(w, h);
    cairo_t *cr = cairo_create(s);
    lp_wallpaper_vignette(cr, w, h);
    cairo_destroy(cr);
    int rc = write_png(s, path);
    cairo_surface_destroy(s);
    return rc;
}

static int render_molten(int w, int h, const char *tone_name, const char *path) {
    if (!lp_molten_available()) {
        fprintf(stderr, "lp-render: no EGL/GLES2 context; the molten wallpaper is unavailable here\n");
        return 1;
    }
    enum lp_molten_tone tone = tone_name && strcmp(tone_name, "faithful") == 0 ? LP_MOLTEN_FAITHFUL : LP_MOLTEN_PLATINUM;
    cairo_surface_t *s = lp_molten_render(w, h, 0.0f, LP_MOLTEN_ZOOM, tone);
    if (!s) {
        fprintf(stderr, "lp-render: the molten render failed\n");
        return 1;
    }
    cairo_t *cr = cairo_create(s);
    lp_wallpaper_vignette_at(cr, w, h, 0.35f);
    cairo_destroy(cr);
    int rc = write_png(s, path);
    cairo_surface_destroy(s);
    return rc;
}

static int render_menubar(int w, const char *path) {
    int h = (int)LP_SIZE_MENUBAR_HEIGHT + LP_MENU_BAR_SHADOW_EXTENT + 40;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(s);
    cairo_surface_t *wp = lp_wallpaper_render(w, h + 200);
    cairo_set_source_surface(cr, wp, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(wp);
    lp_settings settings = lp_settings_defaults();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    ctx.active_window = 1;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, w, h), 0);
    lp_menu_bar_model model = { .labels = { "", "File", "Edit", "View", "Go", "Window", "Help" }, .count = 7, .open_index = 3, .clock = "Tue 9:41 AM" };
    lp_menu_bar(&ctx, LP_RECT(0, 0, w, LP_SIZE_MENUBAR_HEIGHT), &model, NULL);
    lp_ctx_end(&ctx);
    cairo_destroy(cr);
    int rc = write_png(s, path);
    cairo_surface_destroy(s);
    return rc;
}

static void paint_window_preview(lp_ctx *ctx, lp_desktop *d, lp_rect rect, const char *title, int focused, const lp_app *app) {
    lp_window_view view = { .title = title, .focused = focused, .state = LP_WIN_NORMAL, .resizable = 1, .rect = rect };
    ctx->active_window = focused;
    lp_title_bar_result bar;
    lp_rect body = lp_window_chrome(ctx, &view, &bar);
    cairo_save(ctx->cr);
    cairo_rectangle(ctx->cr, body.x, body.y, body.w, body.h);
    cairo_clip(ctx->cr);
    lp_surface_paint(ctx->cr, body, (lp_surface_opts){ .variant = LP_VARIANT_BODY, .radius = 0 }, 0.5f, 0);
    if (app && app->paint) app->paint(NULL, ctx, body, d);
    cairo_restore(ctx->cr);
}

static int render_window(const char *path) {
    int w = 900, h = 560;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(s);
    cairo_surface_t *wp = lp_wallpaper_render(w, h);
    cairo_set_source_surface(cr, wp, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(wp);
    lp_desktop d;
    lp_desktop_init(&d, LP_RECT(0, 0, w, h), NULL);
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    ctx.sheen_x = 0.5f;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, w, h), 0);
    paint_window_preview(&ctx, &d, LP_RECT(430, 90, 400, 300), "Rao", 0, NULL);
    paint_window_preview(&ctx, &d, LP_RECT(80, 120, 380, 300), "About Liquid Platinum", 1, &lp_app_about);
    lp_ctx_end(&ctx);
    cairo_destroy(cr);
    int rc = write_png(s, path);
    cairo_surface_destroy(s);
    return rc;
}

static int render_app(const lp_app *app, int tab, const char *path) {
    int w = 900, h = 640;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(s);
    cairo_surface_t *wp = lp_wallpaper_render(w, h);
    cairo_set_source_surface(cr, wp, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(wp);
    lp_desktop d;
    lp_desktop_init(&d, LP_RECT(0, 0, w, h), NULL);
    lp_desktop_register_builtin_apps(&d);
    void *state = app->create ? app->create(&d, "w1") : NULL;
    if (tab > 0 && state) *(int *)state = tab; /* the gallery's first field is its tab */
    if (app == &lp_app_finder && state) lp_finder_set_preview(state); /* files.ts, not the build host's home */
    if (app == &lp_app_textedit && state)
        lp_textedit_set_text(state, "Design notes",
            "Liquid Platinum is brushed metal that moves like liquid.\n\n"
            "Every surface is a token; every window is a physics target. The title bar's sheen lags a drag, "
            "the lights tilt and ring down, and the frame stretches like jelly before it settles.\n\n— Rao");
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    ctx.sheen_x = 0.5f;
    ctx.active_window = 1;
    lp_rect rect = LP_RECT(40, 30, w - 80, h - 60);
    lp_window_view view = { .title = app->title, .focused = 1, .state = LP_WIN_NORMAL, .resizable = 1, .rect = rect };
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, w, h), 1234.0);
    lp_title_bar_result bar;
    lp_rect body = lp_window_chrome(&ctx, &view, &bar);
    cairo_save(cr);
    cairo_rectangle(cr, body.x, body.y, body.w, body.h);
    cairo_clip(cr);
    lp_surface_paint(cr, body, (lp_surface_opts){ .variant = LP_VARIANT_BODY, .radius = 0 }, 0.5f, 0);
    app->paint(state, &ctx, body, &d);
    cairo_restore(cr);
    lp_ctx_end(&ctx);
    if (app->destroy) app->destroy(state);
    cairo_destroy(cr);
    int rc = write_png(s, path);
    cairo_surface_destroy(s);
    return rc;
}

/* Spotlight over a 1280×800 desktop with the Finder and the Gallery open: the dock, or the results for a query. */
static int render_spotlight(const char *query, const char *path) {
    int w = 1280, h = 800;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(s);
    cairo_surface_t *wp = lp_wallpaper_render(w, h);
    cairo_set_source_surface(cr, wp, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(wp);
    lp_wallpaper_vignette(cr, w, h);
    lp_desktop d;
    lp_desktop_init(&d, LP_RECT(0, LP_SIZE_MENUBAR_HEIGHT, w, h - LP_SIZE_MENUBAR_HEIGHT), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_desktop_open_app(&d, "finder");
    lp_desktop_open_app(&d, "gallery");
    lp_spotlight_open(&d.spotlight);
    lp_spotlight_set_query(&d.spotlight, query ? query : "");
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    ctx.sheen_x = 0.5f;
    ctx.active_window = 1;
    ctx.focus = LP_SPOTLIGHT_QUERY_ID;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, w, h), 0);
    lp_menu_bar_model model = { .labels = { "", "File", "Edit", "View", "Go", "Window", "Help" }, .count = 7, .open_index = -1, .clock = "Tue 9:41 AM" };
    lp_menu_bar(&ctx, LP_RECT(0, 0, w, LP_SIZE_MENUBAR_HEIGHT), &model, NULL);
    lp_spotlight_item results[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_desktop_spotlight_results(&d, results, LP_SPOTLIGHT_MAX_RESULTS);
    lp_spotlight_view view = { .query = &d.spotlight.query, .items = results, .count = n, .selection = 0 };
    lp_size size = lp_spotlight_measure(&view);
    float x = (w - size.w) / 2, y = h * LP_SPOTLIGHT_Y_FRACTION - LP_SIZE_SPOTLIGHT_BAR_HEIGHT / 2 - LP_SPOTLIGHT_PAD;
    lp_spotlight_panel(&ctx, x, y, &view, NULL);
    lp_ctx_end(&ctx);
    cairo_destroy(cr);
    int rc = write_png(s, path);
    cairo_surface_destroy(s);
    return rc;
}

static int parse_size(const char *text, int *w, int *h) {
    return sscanf(text, "%dx%d", w, h) == 2 && *w > 0 && *h > 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(2);
    if (strcmp(argv[1], "--version") == 0) { printf("lp-render %s\n", lp_version()); return 0; }
    if (strcmp(argv[1], "--brush") == 0 && argc == 3) return render_brush(argv[2]);
    if (strcmp(argv[1], "--wallpaper") == 0 && argc == 4) {
        int w, h;
        if (!parse_size(argv[2], &w, &h)) return usage(2);
        return render_wallpaper(w, h, argv[3]);
    }
    if (strcmp(argv[1], "--molten") == 0 && (argc == 4 || argc == 5)) {
        int w, h;
        if (!parse_size(argv[2], &w, &h)) return usage(2);
        return argc == 5 ? render_molten(w, h, argv[3], argv[4]) : render_molten(w, h, NULL, argv[3]);
    }
    if (strcmp(argv[1], "--menubar") == 0 && argc == 4) return render_menubar(atoi(argv[2]), argv[3]);
    if (strcmp(argv[1], "--window") == 0 && argc == 3) return render_window(argv[2]);
    if (strcmp(argv[1], "--finder") == 0 && argc == 3) return render_app(&lp_app_finder, 0, argv[2]);
    if (strcmp(argv[1], "--gallery") == 0 && argc == 4) return render_app(&lp_app_gallery, atoi(argv[2]), argv[3]);
    if (strcmp(argv[1], "--textedit") == 0 && argc == 3) return render_app(&lp_app_textedit, 0, argv[2]);
    if (strcmp(argv[1], "--spotlight") == 0 && argc == 3) return render_spotlight(NULL, argv[2]);
    if (strcmp(argv[1], "--spotlight") == 0 && argc == 4) return render_spotlight(argv[2], argv[3]);
    if (strcmp(argv[1], "--all") == 0 && argc == 3) {
        if (ensure_dir(argv[2]) != 0) return 1;
        char path[1024];
        int rc = 0;
        snprintf(path, sizeof path, "%s/brush-tile.png", argv[2]);
        rc |= render_brush(path);
        snprintf(path, sizeof path, "%s/wallpaper-1280x800.png", argv[2]);
        rc |= render_wallpaper(1280, 800, path);
        /* The molten still, when this build can reach a GL context. Under a
         * software rasteriser it takes seconds, which is exactly why it is
         * baked here and shipped rather than rendered on a first boot. */
        if (lp_molten_available()) {
            snprintf(path, sizeof path, "%s/molten-platinum-1280x800.png", argv[2]);
            rc |= render_molten(1280, 800, "platinum", path);
        } else {
            fprintf(stderr, "lp-render: no EGL here; the molten wallpaper is not baked\n");
        }
        snprintf(path, sizeof path, "%s/menubar.png", argv[2]);
        rc |= render_menubar(640, path);
        snprintf(path, sizeof path, "%s/window.png", argv[2]);
        rc |= render_window(path);
        snprintf(path, sizeof path, "%s/finder.png", argv[2]);
        rc |= render_app(&lp_app_finder, 0, path);
        static const char *const tabs[5] = { "controls", "surfaces", "bubbles", "tokens", "motion" };
        for (int t = 0; t < 5; t++) {
            snprintf(path, sizeof path, "%s/gallery-%s.png", argv[2], tabs[t]);
            rc |= render_app(&lp_app_gallery, t, path);
        }
        snprintf(path, sizeof path, "%s/textedit.png", argv[2]);
        rc |= render_app(&lp_app_textedit, 0, path);
        snprintf(path, sizeof path, "%s/spotlight.png", argv[2]);
        rc |= render_spotlight(NULL, path);
        snprintf(path, sizeof path, "%s/spotlight-results.png", argv[2]);
        rc |= render_spotlight("te", path);
        return rc;
    }
    return usage(2);
}
