# Makefile for AppClip Manager (Parts 1 & 2)
# Modules: Installed Applications Manager & Clipboard History Manager
# Created by: Jeyaul Hoque
# Website: https://jeyaulhoque.pages.dev/

CC ?= gcc
PKG_CONFIG ?= pkg-config

# Required dependencies: GTK+-3.0 and SQLite3
PKGS = gtk+-3.0 sqlite3

# Optional AppIndicator3 tray dependency:
# Compile with: make WITH_TRAY=1 (requires libappindicator3-dev)
ifeq ($(WITH_TRAY), 1)
  PKGS += appindicator3-0.1
  CFLAGS += -DWITH_TRAY_ICON=1
endif

# Compiler and Linker flags
CFLAGS += -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags $(PKGS))
LIBS += $(shell $(PKG_CONFIG) --libs $(PKGS)) -lm

# Targets
UNIFIED_TARGET = appclip-manager
STANDALONE_CLIP_TARGET = appclip-clipboard

# Common clipboard sources
CLIP_SRCS = db.c \
            clip_model.c \
            clipboard_monitor.c \
            ui_cliplist.c \
            settings.c

# Common app manager sources
APP_SRCS = app_model.c \
           app_scanner.c \
           app_uninstaller.c \
           ui_applist.c

# Unified app sources
UNIFIED_SRCS = main.c $(APP_SRCS) $(CLIP_SRCS)
UNIFIED_OBJS = $(UNIFIED_SRCS:.c=.o)

# Standalone clipboard sources
STANDALONE_SRCS = clipboard_main.c $(CLIP_SRCS)
STANDALONE_OBJS = $(STANDALONE_SRCS:.c=.o)

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/appclip-manager

.PHONY: all clean install uninstall run

all: $(UNIFIED_TARGET) $(STANDALONE_CLIP_TARGET)

$(UNIFIED_TARGET): $(UNIFIED_OBJS)
	$(CC) $(UNIFIED_OBJS) -o $@ $(LIBS)

$(STANDALONE_CLIP_TARGET): $(STANDALONE_OBJS)
	$(CC) $(STANDALONE_OBJS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(UNIFIED_OBJS) $(STANDALONE_OBJS) $(UNIFIED_TARGET) $(STANDALONE_CLIP_TARGET)

install: $(UNIFIED_TARGET) $(STANDALONE_CLIP_TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(UNIFIED_TARGET) $(DESTDIR)$(BINDIR)/$(UNIFIED_TARGET)
	install -m 755 $(STANDALONE_CLIP_TARGET) $(DESTDIR)$(BINDIR)/$(STANDALONE_CLIP_TARGET)
	install -d $(DESTDIR)$(DATADIR)
	if [ -f assets/logo.png ]; then \
		install -m 644 assets/logo.png $(DESTDIR)$(DATADIR)/logo.png; \
	fi
	install -d $(DESTDIR)$(PREFIX)/share/applications
	if [ -f appclip-manager.desktop ]; then \
		install -m 644 appclip-manager.desktop $(DESTDIR)$(PREFIX)/share/applications/appclip-manager.desktop; \
	fi
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
	if [ -f assets/icons/appclip-manager.svg ]; then \
		install -m 644 assets/icons/appclip-manager.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/appclip-manager.svg; \
	fi
	for sz in 16 32 48 64 128 256 512; do \
		install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${sz}x$${sz}/apps; \
		if [ -f assets/icons/appclip-manager-$${sz}x$${sz}.png ]; then \
			install -m 644 assets/icons/appclip-manager-$${sz}x$${sz}.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${sz}x$${sz}/apps/appclip-manager.png; \
		elif [ -f assets/logo.png ]; then \
			install -m 644 assets/logo.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${sz}x$${sz}/apps/appclip-manager.png; \
		fi; \
	done

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(UNIFIED_TARGET)
	rm -f $(DESTDIR)$(BINDIR)/$(STANDALONE_CLIP_TARGET)
	rm -f $(DESTDIR)$(PREFIX)/share/applications/appclip-manager.desktop
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/appclip-manager.svg
	for sz in 16 32 48 64 128 256 512; do \
		rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${sz}x$${sz}/apps/appclip-manager.png; \
	done
	rm -rf $(DESTDIR)$(DATADIR)

run: $(UNIFIED_TARGET)
	./$(UNIFIED_TARGET)
