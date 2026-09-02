#####################################################################
# Config
#####################################################################
BIN := emu

ifeq ($(origin CC),default)
CC := gcc
endif

ifeq ($(OS),Windows_NT)
DEBUG_BIN := emu-debug.exe
CLEAN_CMD = powershell -NoProfile -Command "Remove-Item -Force -ErrorAction Ignore 'emu','emu.exe','emu-debug','emu-debug.exe','emu-debug.ilk','emu-debug.pdb','*.o'; exit 0"
else
DEBUG_BIN := emu-debug
CLEAN_CMD = rm -f $(BIN) $(DEBUG_BIN) $(OBJ)
endif

DEBUG_COMMIT := $(shell git rev-parse --verify HEAD 2>/dev/null || echo unknown)
DEBUG_SRC := emu_debug_server.c emu_debug.c core.c opcodes.c disasm.c binary_loader.c
DAP_ROOT ?= ../emuSA80535-DAP

CFLAGS += -O2
CFLAGS += -pipe
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -Wshadow

# Uncomment to activate LTO
#CFLAGS += -flto

LDLIBS += -lcurses

#####################################################################
# Rules
#####################################################################
HEADERS := $(wildcard *.h)
SRC := $(filter-out emu_debug_server.c emu_debug.c,$(wildcard *.c))
OBJ := $(SRC:.c=.o)

%.o: %.c $(HEADERS)
	 $(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(DEBUG_BIN): $(DEBUG_SRC) emu_debug.h emu8051.h
	$(CC) $(CFLAGS) -std=c99 -DEMU_DEBUG_BUILD_COMMIT=\"$(DEBUG_COMMIT)\" \
		$(LDFLAGS) -o $@ $(DEBUG_SRC)

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

dap-integration-test: $(DEBUG_BIN)
	$(MAKE) -C tests dap-integration-test DEBUG_BIN=../$(DEBUG_BIN) \
		DAP_ROOT=$(abspath $(DAP_ROOT))

.PHONY: core-test debug-test debug-event-test dap-integration-test
