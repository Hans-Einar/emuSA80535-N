#####################################################################
# Config
#####################################################################
BIN := emu
.DEFAULT_GOAL := all

ifeq ($(origin CC),default)
CC := gcc
endif

ifeq ($(OS),Windows_NT)
DEBUG_BIN := emu-debug.exe
CLEAN_CMD = powershell -NoProfile -Command "Remove-Item -Force -ErrorAction Ignore 'emu','emu.exe','emu-debug','emu-debug.exe','emu-debug.ilk','emu-debug.pdb','*.o','libemusa80535.a','emusa80535.pc'; exit 0"
else
DEBUG_BIN := emu-debug
CLEAN_CMD = rm -f $(BIN) $(DEBUG_BIN) $(OBJ) $(SDK_OBJ) $(SDK_LIB) emusa80535.pc
endif

DEBUG_COMMIT := $(shell git rev-parse --verify HEAD 2>/dev/null || echo unknown)
DEBUG_SRC := emu_debug_server.c emu_debug.c core.c opcodes.c disasm.c binary_loader.c
DAP_ROOT ?= ../emuSA80535-DAP

SDK_LIB := libemusa80535.a
SDK_SRC := core.c opcodes.c disasm.c binary_loader.c emu_debug.c \
	emu_debug_event.c emu_debug_trace.c emu_debug_runtime.c
SDK_OBJ := $(SDK_SRC:.c=.sdk.o)
SDK_HEADERS := $(wildcard lib/include/emusa80535/*.h lib/include/emusa80535/contracts/*.h)
CPPFLAGS += -Ilib/include
PYTHON ?= python3
CONTRACT_VERSION := 0.1.0
VERSION := 1.0.0
prefix ?= /usr/local
bindir ?= $(prefix)/bin
libdir ?= $(prefix)/lib
includedir ?= $(prefix)/include
datadir ?= $(prefix)/share
docdir ?= $(datadir)/doc/emusa80535
licensedir ?= $(datadir)/licenses/emusa80535
INSTALL ?= install

CFLAGS += -O2
CFLAGS += -pipe
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -Wshadow

# Uncomment to activate LTO
#CFLAGS += -flto

LDLIBS += -lcurses

#####################################################################
# Rules
#####################################################################
HEADERS := $(wildcard *.h) $(SDK_HEADERS)
SRC := $(filter-out emu_debug_server.c emu_debug.c,$(wildcard *.c))
OBJ := $(SRC:.c=.o)

%.o: %.c $(HEADERS)
	 $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(DEBUG_BIN): $(DEBUG_SRC) $(SDK_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -DEMU_DEBUG_BUILD_COMMIT=\"$(DEBUG_COMMIT)\" \
		$(LDFLAGS) -o $@ $(DEBUG_SRC)

%.sdk.o: %.c $(SDK_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -c -o $@ $<

$(SDK_LIB): $(SDK_OBJ)
	$(AR) rcs $@ $^

sdk: $(SDK_LIB)

# Generated on every install so changing prefix/libdir never reuses old paths.
emusa80535.pc: packaging/emusa80535.pc.in FORCE
	sed -e 's|@PREFIX@|$(prefix)|g' -e 's|@LIBDIR@|$(libdir)|g' \
		-e 's|@INCLUDEDIR@|$(includedir)|g' -e 's|@VERSION@|$(VERSION)|g' $< > $@

install: $(DEBUG_BIN) $(SDK_LIB) emusa80535.pc
	$(INSTALL) -d $(DESTDIR)$(bindir) $(DESTDIR)$(libdir)/pkgconfig \
		$(DESTDIR)$(includedir)/emusa80535 $(DESTDIR)$(docdir) $(DESTDIR)$(licensedir)
	$(INSTALL) -m 755 $(DEBUG_BIN) $(DESTDIR)$(bindir)/emu-debug
	$(INSTALL) -m 644 $(SDK_LIB) $(DESTDIR)$(libdir)/
	$(INSTALL) -m 644 emusa80535.pc $(DESTDIR)$(libdir)/pkgconfig/
	$(INSTALL) -d $(DESTDIR)$(includedir)/emusa80535/contracts
	$(INSTALL) -m 644 lib/include/emusa80535/*.h $(DESTDIR)$(includedir)/emusa80535/
	$(INSTALL) -m 644 lib/include/emusa80535/contracts/*.h $(DESTDIR)$(includedir)/emusa80535/contracts/
	$(PYTHON) interfaces/tools/bundle.py install --destination $(DESTDIR)$(docdir)/interface-bundles/$(CONTRACT_VERSION) --source-commit $(DEBUG_COMMIT)
	$(INSTALL) -m 644 LICENSE $(DESTDIR)$(licensedir)/
	$(INSTALL) -m 644 README.md $(DESTDIR)$(docdir)/
	$(INSTALL) -m 644 packaging/README.md $(DESTDIR)$(docdir)/PACKAGING.md
	printf '%s\n' '$(DEBUG_COMMIT)' > $(DESTDIR)$(docdir)/source-commit
	$(INSTALL) -m 644 packaging/sdk_smoke.c $(DESTDIR)$(docdir)/

FORCE:

.PHONY: sdk install FORCE

ifeq ($(OS),Windows_NT)
emu-debug: $(DEBUG_BIN)
.PHONY: emu-debug
endif

clean:
	-$(CLEAN_CMD)

.PHONY: clean all

all: $(BIN)

core-test:
	$(MAKE) -C tests clean
	$(MAKE) -C tests test

debug-test: $(DEBUG_BIN)
	$(MAKE) -C tests debug-test DEBUG_BIN=../$(DEBUG_BIN)

debug-event-test:
	$(MAKE) -C tests debug-event-test

debug-trace-test:
	$(MAKE) -C tests debug-trace-test

dap-integration-test: $(DEBUG_BIN)
	$(MAKE) -C tests dap-integration-test DEBUG_BIN=../$(DEBUG_BIN) \
		DAP_ROOT=$(abspath $(DAP_ROOT))

.PHONY: core-test debug-test debug-event-test debug-trace-test dap-integration-test

interface-check:
	$(PYTHON) interfaces/tools/check_contracts.py

interface-export:
	$(PYTHON) interfaces/tools/bundle.py export --output $(INTERFACE_ARCHIVE)

.PHONY: interface-check interface-export
