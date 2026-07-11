.PHONY: all clean debug release install uninstall

CC      ?= gcc
CFLAGS  ?= -std=gnu11 -Wall -Wextra
LDFLAGS ?= -lpthread

SRCDIR   = src
BUILDDIR = build
BINDIR   = bin
TARGET   = $(BINDIR)/tfm

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

# Release
release: CFLAGS += -O2 -s
release: LDFLAGS += -s
release: $(TARGET)

# Debug
debug: CFLAGS += -O0 -g -DDEBUG
debug: $(TARGET)

all: release

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BUILDDIR) $(BINDIR)

install: release
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/tfm
	install -d $(DESTDIR)$(PREFIX)/share/tfm/themes
	install -m 644 themes/default.json $(DESTDIR)$(PREFIX)/share/tfm/themes/default.json

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/tfm
	rm -rf $(DESTDIR)$(PREFIX)/share/tfm

PREFIX ?= /usr/local
DESTDIR ?=
