#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_molten.h"
#include "maryui/lp_tokens.h"

lp_molten_grade lp_molten_grade_for(enum lp_molten_tone tone) {
    if (tone == LP_MOLTEN_FAITHFUL) {
        return (lp_molten_grade){ LP_MOLTEN_FAITHFUL_BASE, LP_MOLTEN_FAITHFUL_LIFT, LP_MOLTEN_FAITHFUL_GAIN,
                                  LP_MOLTEN_FAITHFUL_SATURATION };
    }
    return (lp_molten_grade){ LP_MOLTEN_PLATINUM_BASE, LP_MOLTEN_PLATINUM_LIFT, LP_MOLTEN_PLATINUM_GAIN,
                              LP_MOLTEN_PLATINUM_SATURATION };
}

#ifndef HAVE_EGL

int lp_molten_available(void) { return 0; }
int lp_molten_is_software(void) { return 1; }
cairo_surface_t *lp_molten_render(int w, int h, float time, float zoom, enum lp_molten_tone tone) {
    (void)w; (void)h; (void)time; (void)zoom; (void)tone;
    return NULL;
}
void lp_molten_shutdown(void) {}

#else

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

/* Kept character-for-character alongside web/src/lib/moltenShader.ts, so the
 * two can be diffed. Do not reformat. */
static const char *MOLTEN_VERTEX =
    "attribute vec2 a_position;\n"
    "void main() {\n"
    "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char *MOLTEN_FRAGMENT =
    "precision highp float;\n"
    "\n"
    "uniform vec2 u_resolution;\n"
    "uniform float u_time;\n"
    "uniform float u_zoom;\n"
    "uniform vec3 u_base;\n"
    "uniform float u_lift;\n"
    "uniform float u_gain;\n"
    "uniform float u_saturation;\n"
    "\n"
    "#define PI  3.141592654\n"
    "#define TAU (2.0*PI)\n"
    "\n"
    "void rot(inout vec2 p, float a) {\n"
    "  float c = cos(a);\n"
    "  float s = sin(a);\n"
    "  p = vec2(c*p.x + s*p.y, -s*p.x + c*p.y);\n"
    "}\n"
    "\n"
    "float hash(in vec2 co) {\n"
    "  return fract(sin(dot(co.xy, vec2(12.9898, 58.233))) * 13758.5453);\n"
    "}\n"
    "\n"
    "float psin(float a) { return 0.5 + 0.5*sin(a); }\n"
    "\n"
    "float tanh_approx(float x) {\n"
    "  float x2 = x*x;\n"
    "  return clamp(x*(27.0 + x2)/(27.0 + 9.0*x2), -1.0, 1.0);\n"
    "}\n"
    "\n"
    "float onoise(vec2 x) {\n"
    "  x *= 0.5;\n"
    "  float a = sin(x.x);\n"
    "  float b = sin(x.y);\n"
    "  return mix(a, b, psin(TAU*tanh_approx(a*b + a + b)));\n"
    "}\n"
    "\n"
    "float vnoise(vec2 x) {\n"
    "  vec2 i = floor(x);\n"
    "  vec2 w = fract(x);\n"
    "  vec2 u = w*w*w*(w*(w*6.0 - 15.0) + 10.0);\n"
    "  float a = hash(i + vec2(0.0, 0.0));\n"
    "  float b = hash(i + vec2(1.0, 0.0));\n"
    "  float c = hash(i + vec2(0.0, 1.0));\n"
    "  float d = hash(i + vec2(1.0, 1.0));\n"
    "  return a + (b - a)*u.x + (c - a)*u.y + (d - c + a - b)*u.x*u.y;\n"
    "}\n"
    "\n"
    "float fbm(vec2 p, const int octaves) {\n"
    "  vec2 op = p;\n"
    "  const float aa = 0.45;\n"
    "  const float pp = 2.03;\n"
    "  const vec2 oo = -vec2(1.23, 1.5);\n"
    "  const float rr = 1.2;\n"
    "  float h = 0.0;\n"
    "  float d = 0.0;\n"
    "  float a = 1.0;\n"
    "  for (int i = 0; i < 7; ++i) {\n"
    "    if (i >= octaves) break;\n"
    "    h += a*onoise(p);\n"
    "    d += a;\n"
    "    a *= aa;\n"
    "    p += oo;\n"
    "    p *= pp;\n"
    "    rot(p, rr);\n"
    "  }\n"
    "  return mix((h/d), -0.5*(h/d), pow(vnoise(0.9*op), 0.25));\n"
    "}\n"
    "\n"
    "float warp(vec2 p) {\n"
    "  vec2 v = vec2(fbm(p, 5), fbm(p + 0.7*vec2(1.0, 1.0), 5));\n"
    "  rot(v, 1.0 + u_time*0.1);\n"
    "  vec2 vv = vec2(fbm(p + 3.7*v, 7), fbm(p - 2.7*v.yx + 0.7*vec2(1.0, 1.0), 7));\n"
    "  rot(vv, -1.0 + u_time*0.21315);\n"
    "  return fbm(p + 1.4*vv, 3);\n"
    "}\n"
    "\n"
    "float height(vec2 p) {\n"
    "  float a = 0.005*u_time;\n"
    "  p += 5.0*vec2(cos(a), sin(a));\n"
    "  p *= 2.0;\n"
    "  p += 13.0;\n"
    "  float rs = 3.0;\n"
    "  return 0.35*tanh_approx(rs*warp(p))/rs;\n"
    "}\n"
    "\n"
    "vec3 normal(vec2 p) {\n"
    "  vec2 eps = -vec2(1.0/u_resolution.y, 0.0);\n"
    "  vec3 n;\n"
    "  n.x = height(p + eps.xy) - height(p - eps.xy);\n"
    "  n.y = 2.0*eps.x;\n"
    "  n.z = height(p + eps.yx) - height(p - eps.yx);\n"
    "  return normalize(n);\n"
    "}\n"
    "\n"
    "vec3 postProcess(vec3 col, vec2 q) {\n"
    "  col = pow(clamp(col, 0.0, 1.0), vec3(0.75));\n"
    "  col = col*0.6 + 0.4*col*col*(3.0 - 2.0*col);\n"
    "  col = mix(col, vec3(dot(col, vec3(0.13))), u_saturation);\n"
    "  col = clamp(u_lift + u_gain*col, 0.0, 1.0);\n"
    "  col *= 0.5 + 0.5*pow(19.0*q.x*q.y*(1.0 - q.x)*(1.0 - q.y), 0.1);\n"
    "  return col;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "  vec2 q = gl_FragCoord.xy/u_resolution.xy;\n"
    "  vec2 p = -1.0 + 2.0*q;\n"
    "  p.x *= u_resolution.x/u_resolution.y;\n"
    "  p /= u_zoom;\n"
    "\n"
    "  const vec3 lp1 = vec3(0.4, -0.5, 0.5);\n"
    "  const vec3 lp2 = vec3(-0.1, -0.5, 0.5);\n"
    "\n"
    "  float h = height(p);\n"
    "  vec3 pp = vec3(p.x, h, p.y);\n"
    "  float ll1 = length(lp1.xz - pp.xz);\n"
    "  vec3 ld1 = normalize(lp1 - pp);\n"
    "  vec3 ld2 = normalize(lp2 - pp);\n"
    "\n"
    "  vec3 n = normal(p);\n"
    "  float diff1 = max(dot(ld1, n), 0.0);\n"
    "  float diff2 = max(dot(ld2, n), 0.0);\n"
    "\n"
    "  vec3 baseCol = u_base;\n"
    "\n"
    "  float oh = height(p + ll1*0.05*normalize(ld1.xz));\n"
    "  vec3 scol = baseCol*(smoothstep(0.0, 0.15, h) - smoothstep(0.0, 0.15, oh));\n"
    "\n"
    "  vec3 col = vec3(0.0);\n"
    "  col += baseCol*pow(diff1, 1.5);\n"
    "  col += 0.5*baseCol*pow(diff1, 0.5);\n"
    "  col += 0.5*baseCol.zyx*pow(diff2, 7.0);\n"
    "  col += 0.015*baseCol.zyx*pow(diff2, 2.0);\n"
    "  col += scol*0.7;\n"
    "\n"
    "  gl_FragColor = vec4(postProcess(col, q), 1.0);\n"
    "}\n";

static struct {
    int probed, ok, software;
    EGLDisplay dpy;
    EGLContext ctx;
    GLuint program, vbo;
    GLint u_resolution, u_time, u_zoom, u_base, u_lift, u_gain, u_saturation;
} G;

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetShaderInfoLog(s, sizeof log - 1, NULL, log);
        fprintf(stderr, "lp_molten: shader compile failed: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

/* A surfaceless display: no window system, no GBM device needed. Mesa serves
 * this with llvmpipe when there is no GPU, which is what a VM gets. */
static EGLDisplay open_display(void) {
    PFNEGLGETPLATFORMDISPLAYEXTPROC get =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (get) {
        EGLDisplay d = get(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
        if (d != EGL_NO_DISPLAY) return d;
    }
    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

static int init(void) {
    if (G.probed) return G.ok;
    G.probed = 1;
    G.dpy = open_display();
    if (G.dpy == EGL_NO_DISPLAY || !eglInitialize(G.dpy, NULL, NULL)) {
        fprintf(stderr, "lp_molten: no EGL display; the procedural wallpaper stands in\n");
        return 0;
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return 0;
    static const EGLint cfg_attrs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE };
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(G.dpy, cfg_attrs, &cfg, 1, &n) || n < 1) {
        fprintf(stderr, "lp_molten: no suitable EGL config\n");
        return 0;
    }
    static const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    G.ctx = eglCreateContext(G.dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
    if (G.ctx == EGL_NO_CONTEXT) return 0;
    /* Surfaceless: everything is rendered to a framebuffer object. */
    if (!eglMakeCurrent(G.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, G.ctx)) {
        fprintf(stderr, "lp_molten: eglMakeCurrent failed\n");
        return 0;
    }

    GLuint vs = compile(GL_VERTEX_SHADER, MOLTEN_VERTEX), fs = compile(GL_FRAGMENT_SHADER, MOLTEN_FRAGMENT);
    if (!vs || !fs) return 0;
    G.program = glCreateProgram();
    glAttachShader(G.program, vs);
    glAttachShader(G.program, fs);
    glBindAttribLocation(G.program, 0, "a_position");
    glLinkProgram(G.program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(G.program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024] = { 0 };
        glGetProgramInfoLog(G.program, sizeof log - 1, NULL, log);
        fprintf(stderr, "lp_molten: link failed: %s\n", log);
        return 0;
    }
    G.u_resolution = glGetUniformLocation(G.program, "u_resolution");
    G.u_time = glGetUniformLocation(G.program, "u_time");
    G.u_zoom = glGetUniformLocation(G.program, "u_zoom");
    G.u_base = glGetUniformLocation(G.program, "u_base");
    G.u_lift = glGetUniformLocation(G.program, "u_lift");
    G.u_gain = glGetUniformLocation(G.program, "u_gain");
    G.u_saturation = glGetUniformLocation(G.program, "u_saturation");

    /* One triangle that covers the clip box; no index buffer, no quad seam. */
    static const GLfloat tri[] = { -1, -1, 3, -1, -1, 3 };
    glGenBuffers(1, &G.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, G.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);

    const char *renderer = (const char *)glGetString(GL_RENDERER);
    G.software = renderer && (strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe") ||
                              strstr(renderer, "swrast") || strstr(renderer, "SWR"));
    fprintf(stderr, "lp_molten: GL renderer \"%s\"%s\n", renderer ? renderer : "?",
            G.software ? " (software: the wallpaper is baked once, never animated)" : "");
    G.ok = 1;
    return 1;
}

int lp_molten_available(void) { return init(); }
int lp_molten_is_software(void) { return G.software; }

cairo_surface_t *lp_molten_render(int w, int h, float time, float zoom, enum lp_molten_tone tone) {
    if (w <= 0 || h <= 0 || !init()) return NULL;
    GLint max_tex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex);
    if (max_tex > 0 && (w > max_tex || h > max_tex)) {
        fprintf(stderr, "lp_molten: %dx%d exceeds GL_MAX_TEXTURE_SIZE %d\n", w, h, max_tex);
        return NULL;
    }
    eglMakeCurrent(G.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, G.ctx);

    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "lp_molten: incomplete framebuffer at %dx%d\n", w, h);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        return NULL;
    }

    lp_molten_grade g = lp_molten_grade_for(tone);
    glViewport(0, 0, w, h);
    glUseProgram(G.program);
    glUniform2f(G.u_resolution, (GLfloat)w, (GLfloat)h);
    glUniform1f(G.u_time, time);
    glUniform1f(G.u_zoom, zoom > 0 ? zoom : 1);
    glUniform3f(G.u_base, g.base.r, g.base.g, g.base.b);
    glUniform1f(G.u_lift, g.lift);
    glUniform1f(G.u_gain, g.gain);
    glUniform1f(G.u_saturation, g.saturation);
    glBindBuffer(GL_ARRAY_BUFFER, G.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    cairo_surface_t *out = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(out) != CAIRO_STATUS_SUCCESS) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        return NULL;
    }
    unsigned char *rgba = malloc((size_t)w * h * 4);
    if (!rgba) {
        cairo_surface_destroy(out);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        return NULL;
    }
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    /* GL's origin is bottom-left and its bytes are RGBA; Cairo wants top-left
     * and native-endian ARGB (BGRA on little-endian). The shader is opaque. */
    cairo_surface_flush(out);
    unsigned char *data = cairo_image_surface_get_data(out);
    int stride = cairo_image_surface_get_stride(out);
    for (int y = 0; y < h; y++) {
        const unsigned char *src = rgba + (size_t)(h - 1 - y) * w * 4;
        unsigned char *dst = data + (size_t)y * stride;
        for (int x = 0; x < w; x++) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = 0xff;
        }
    }
    cairo_surface_mark_dirty(out);
    free(rgba);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return out;
}

void lp_molten_shutdown(void) {
    if (!G.ok) return;
    eglMakeCurrent(G.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (G.ctx != EGL_NO_CONTEXT) eglDestroyContext(G.dpy, G.ctx);
    eglTerminate(G.dpy);
    memset(&G, 0, sizeof G);
}

#endif
