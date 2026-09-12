# MaryUI for Linux: libmaryui (Liquid Platinum in C) and maryui-desktop (a
# wlroots Wayland compositor that is the desktop). Plain GNU make + pkg-config.
#
#   make               libmaryui.a (+ .so on Linux), lp-render, maryui-desktop (when wlroots is installed)
#   make test          build and run tests/test_*.c
#   make install       DESTDIR=… PREFIX=/usr
#   make gen           regenerate lp_tokens.h / lp_icons.h / lp_objects.h from ../web (needs node)
#   make gen-check     fail when the committed generated headers are stale
#   make parity        tools/parity-check.sh
#   make check-deps    pkg-config for every library used
#
# Every output goes under $(O) (default build/), so the source tree may be a
# read-only mount (the MaryPi builder mounts it that way).
O ?= build
PREFIX ?= /usr
DESTDIR ?=
CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
VERSION := 0.1.0
UNAME := $(shell uname -s)

WAYLAND_SCANNER ?= $(shell $(PKG_CONFIG) --variable=wayland_scanner wayland-scanner 2>/dev/null || echo wayland-scanner)
WAYLAND_PROTOCOLS ?= $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols 2>/dev/null)

LIB_PKGS := cairo pangocairo fontconfig pixman-1 xkbcommon
DESKTOP_PKGS := wlroots wayland-server
HAVE_WLROOTS := $(shell $(PKG_CONFIG) --exists $(DESKTOP_PKGS) 2>/dev/null && echo 1)
# The molten wallpaper renders offscreen through EGL + GLES2. Optional: without
# it lp_molten.c compiles to stubs and the procedural wallpaper stands in.
MOLTEN_PKGS := egl glesv2
HAVE_EGL := $(shell $(PKG_CONFIG) --exists $(MOLTEN_PKGS) 2>/dev/null && echo 1)

# -O3 for the drawing loops (blur, noise, per-pixel compositing), which is what
# this library spends its time in. No -mcpu=native: one ui build is installed
# into every target (MaryPi's build.sh runs the stage once), so the code has to
# run on both a Pi 5's Cortex-A76 and Apple silicon. A76 is the floor of the two.
CFLAGS ?= -O3 -g -DNDEBUG
CFLAGS += -std=c11 -Wall -Wextra -Wno-unused-parameter -Werror=implicit-function-declaration -fPIC
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -DMARYUI_VERSION=\"$(VERSION)\"
LIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(LIB_PKGS))
LIB_LIBS := $(shell $(PKG_CONFIG) --libs $(LIB_PKGS)) -lm
ifeq ($(HAVE_EGL),1)
LIB_CFLAGS += $(shell $(PKG_CONFIG) --cflags $(MOLTEN_PKGS)) -DHAVE_EGL
LIB_LIBS += $(shell $(PKG_CONFIG) --libs $(MOLTEN_PKGS))
endif
DESKTOP_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(DESKTOP_PKGS) 2>/dev/null) -DWLR_USE_UNSTABLE -I$(O)/protocols
DESKTOP_LIBS := $(shell $(PKG_CONFIG) --libs $(DESKTOP_PKGS) 2>/dev/null)

LIB_SRCS := $(wildcard src/core/*.c src/draw/*.c src/ui/*.c src/components/*/*.c src/apps/*.c)
LIB_OBJS := $(LIB_SRCS:%.c=$(O)/obj/%.o)
DESKTOP_SRCS := $(wildcard src/compositor/*.c)
DESKTOP_OBJS := $(DESKTOP_SRCS:%.c=$(O)/obj/%.o)
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/%.c=$(O)/tests/%)
PROTOCOLS := $(O)/protocols/xdg-shell-protocol.h $(O)/protocols/xdg-decoration-unstable-v1-protocol.h

STATIC := $(O)/libmaryui.a
SHARED := $(O)/libmaryui.so.$(firstword $(subst ., ,$(VERSION)))
DESKTOP := $(O)/maryui-desktop
RENDER := $(O)/lp-render
INPUT := $(O)/lp-input

ifeq ($(UNAME),Linux)
LIB_TARGETS := $(STATIC) $(SHARED)
else
LIB_TARGETS := $(STATIC)
endif

.PHONY: all lib desktop tools test install gen gen-check parity check-deps clean
ifeq ($(HAVE_EGL),1)
MOLTEN_NOTE := true
else
MOLTEN_NOTE := echo "make: egl/glesv2 not found; the molten wallpaper falls back to the procedural one"
endif
ifeq ($(HAVE_WLROOTS),1)
all: lib tools desktop
	@$(MOLTEN_NOTE)
else
all: lib tools
	@echo "make: wlroots not found; maryui-desktop skipped (libmaryui and lp-render built)"
	@$(MOLTEN_NOTE)
endif
lib: $(LIB_TARGETS)
desktop: $(DESKTOP)
ifeq ($(UNAME),Linux)
tools: $(RENDER) $(INPUT)
else
tools: $(RENDER)
endif

$(O)/obj/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIB_CFLAGS) $(if $(findstring src/compositor/,$<),$(DESKTOP_CFLAGS)) -MMD -MP -c $< -o $@

$(DESKTOP_OBJS): $(PROTOCOLS)

$(O)/protocols/xdg-shell-protocol.h:
	@mkdir -p $(@D)
	$(WAYLAND_SCANNER) server-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@
$(O)/protocols/xdg-decoration-unstable-v1-protocol.h:
	@mkdir -p $(@D)
	$(WAYLAND_SCANNER) server-header $(WAYLAND_PROTOCOLS)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml $@

$(STATIC): $(LIB_OBJS)
	@mkdir -p $(@D)
	rm -f $@
	$(AR) rcs $@ $^

$(SHARED): $(LIB_OBJS)
	$(CC) -shared -Wl,-soname,$(notdir $@) -o $@ $^ $(LIB_LIBS)
	ln -sf $(notdir $@) $(O)/libmaryui.so

$(DESKTOP): $(DESKTOP_OBJS) $(STATIC)
	$(CC) -o $@ $(DESKTOP_OBJS) $(STATIC) $(DESKTOP_LIBS) $(LIB_LIBS)

$(RENDER): $(O)/obj/tools/lp-render.o $(STATIC)
	$(CC) -o $@ $< $(STATIC) $(LIB_LIBS)

$(INPUT): $(O)/obj/tools/lp-input.o
	$(CC) -o $@ $<

$(O)/tests/%: tests/%.c $(STATIC) tests/lp_test.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIB_CFLAGS) -Itests -o $@ $< $(STATIC) $(LIB_LIBS)

test: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do echo "== $$t"; $$t || fail=1; done; [ $$fail = 0 ]

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib/pkgconfig $(DESTDIR)$(PREFIX)/include/maryui $(DESTDIR)$(PREFIX)/share/maryui
	install -m 755 $(RENDER) $(DESTDIR)$(PREFIX)/bin/
ifeq ($(UNAME),Linux)
	install -m 755 $(INPUT) $(DESTDIR)$(PREFIX)/bin/
endif
ifeq ($(HAVE_WLROOTS),1)
	install -m 755 $(DESKTOP) $(DESTDIR)$(PREFIX)/bin/
endif
	install -m 644 $(STATIC) $(DESTDIR)$(PREFIX)/lib/
ifeq ($(UNAME),Linux)
	install -m 755 $(SHARED) $(DESTDIR)$(PREFIX)/lib/
	ln -sf $(notdir $(SHARED)) $(DESTDIR)$(PREFIX)/lib/libmaryui.so
endif
	cp -R include/maryui/. $(DESTDIR)$(PREFIX)/include/maryui/
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|$(VERSION)|g' -e 's|@LIBS@|$(LIB_LIBS)|g' maryui.pc.in > $(DESTDIR)$(PREFIX)/lib/pkgconfig/maryui.pc
	install -m 644 README.md LICENSE NOTICE $(wildcard PARITY.md) $(DESTDIR)$(PREFIX)/share/maryui/

gen:
	node ../web/scripts/build-tokens.mjs

gen-check:
	@tmp=$$(mktemp -d); node ../web/scripts/build-tokens.mjs --out "$$tmp" > /dev/null \
	  && for f in include/maryui/lp_tokens.h include/maryui/lp_icons.h include/maryui/lp_objects.h; do \
	       [ -f "$$tmp/$$(basename $$f)" ] && ! cmp -s "$$tmp/$$(basename $$f)" "$$f" && { echo "gen-check: $$f is stale (run make gen)"; exit 1; }; done; \
	  rm -rf "$$tmp"; echo "gen-check: generated headers are current"

parity:
	sh tools/parity-check.sh

check-deps:
	$(PKG_CONFIG) --print-errors --exists $(LIB_PKGS) && echo "libmaryui deps ok"
	$(PKG_CONFIG) --print-errors --exists $(DESKTOP_PKGS) wayland-protocols && echo "maryui-desktop deps ok"
	@$(PKG_CONFIG) --exists $(MOLTEN_PKGS) && echo "molten wallpaper deps ok" \
	  || echo "molten wallpaper deps missing (egl glesv2); the procedural wallpaper stands in"

clean:
	rm -rf $(O)

-include $(LIB_OBJS:.o=.d) $(DESKTOP_OBJS:.o=.d) $(O)/obj/tools/lp-render.d $(O)/obj/tools/lp-input.d
