CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -D_GNU_SOURCE -O2 -g
NCURSES_CFLAGS := $(shell pkg-config --cflags ncursesw 2>/dev/null)
NCURSES_LIBS   := $(shell pkg-config --libs   ncursesw 2>/dev/null || echo "-lncursesw")
CFLAGS  += $(NCURSES_CFLAGS)
LDFLAGS := $(NCURSES_LIBS)
LDFLAGS_STATIC := -static $(NCURSES_LIBS) -ltinfo

SRCDIR  := src
OBJDIR  := obj
TARGET  := visormd

SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

.PHONY: all static clean install test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Portable static binary — compatible with older distros (GLIBC 2.35+).
# Requires: sudo apt install libncursesw5-dev (on dev machine).
static: $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $^ $(LDFLAGS_STATIC)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)

test: $(TARGET)
	@passed=0; failed=0; \
	for f in test/test.md test/test_emoji.md test/test_table.md test/test_table2.md test/test_user.md test/test_utf8.md test/test_wide.md test/test_lista.md test/test_underscore.md test/test_blockquote.md test/test_highlight_c.md test/test_highlight_cpp.md test/test_highlight_java.md test/test_highlight_js.md; do \
		base=$${f%.md}; \
		if TERM=xterm-256color LANG=C.UTF-8 ./$(TARGET) --cat "$$f" | diff - "$${base}_expected.txt" > /dev/null 2>&1; then \
			echo "OK: $$f"; \
			passed=$$((passed + 1)); \
		else \
			echo "FAIL: $$f"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo "------------------------------"; \
	echo "Pasaron: $$passed | Fallaron: $$failed"; \
	if [ $$failed -gt 0 ]; then exit 1; fi
