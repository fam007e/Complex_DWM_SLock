# Complex DWM Slock
#
#   make                       build ./slock
#   sudo make install          install setuid root to $(PREFIX)/bin
#   make test                  build and run `./slock -t`
#   make PREFIX=/usr DATADIR=/usr/share/slock    (what the AUR package uses)
#
# DATADIR is compiled into the binary, so run `make clean` after changing it.

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/slock

CC      ?= cc
CFLAGS  ?= -O2 -pipe

VERSION := $(shell date +%Y.%m.%d).$(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)

PKGS    := x11 xext xrandr xrender xft fontconfig imlib2
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(PKGS))

override CFLAGS += -std=gnu11 -Wall -Wextra -fstack-protector-strong \
                   -DBUILD_VERSION=$(VERSION) -DDATADIR='"$(DATADIR)"' \
                   $(PKG_CFLAGS) -MMD -MP
override LDLIBS += $(PKG_LIBS) -lpam -lm

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=obj/%.o)

all: slock

slock: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c -o $@ $<

obj:
	mkdir -p obj

install: slock
	install -Dm4755 slock $(DESTDIR)$(BINDIR)/slock
	install -Dm644 assets/images/avatar.png $(DESTDIR)$(DATADIR)/avatar.png
	install -Dm644 assets/fonts/font.ttf    $(DESTDIR)$(DATADIR)/font.ttf

uninstall:
	rm -f  $(DESTDIR)$(BINDIR)/slock
	rm -rf $(DESTDIR)$(DATADIR)

test: slock
	./slock -t

clean:
	rm -rf obj slock

-include $(OBJ:.o=.d)

.PHONY: all install uninstall test clean
