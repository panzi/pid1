CC=gcc
CFLAGS=-Wall -std=c11 -Werror
BUILDDIR=build
BIN=$(BUILDDIR)/bin/pid1
OBJ=$(BUILDDIR)/obj/pid1.o
RELEASE=OFF
PREFIX=/usr/local/bin

ifeq ($(RELEASE),ON)
    CFLAGS += -O2
else
    CFLAGS += -g
endif

.PHONY: all clean install uninstall

all: $(BIN)

install: $(BIN)
	@mkdir -p $(PREFIX)
	cp $(BIN) $(PREFIX)

uninstall:
	rm $(PREFIX)/pid1

$(BIN): $(OBJ)
	@mkdir -p $(BUILDDIR)/bin
	$(CC) $(CFLAGS) $< -o $@

$(BUILDDIR)/obj/%.o: src/%.c
	@mkdir -p $(BUILDDIR)/obj
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -r $(BIN) $(OBJ)
