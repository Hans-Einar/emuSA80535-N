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
CLEAN_CMD = powershell -NoProfile -Command "Remove-Item -Force -ErrorAction Ignore 'emu','emu.exe','emu-debug','emu-debug.exe','emu-debug.ilk','emu-debug.pdb','*.o','libemusa80535.a','libemusa80535_cpu.a','emusa80535.pc'; exit 0"
else
DEBUG_BIN := emu-debug
CLEAN_CMD = rm -f $(BIN) $(DEBUG_BIN) $(OBJ) $(SDK_OBJ) $(SDK_LIB) $(CPU_LIB) $(RUNTIME_LIB) $(RUNTIME_OBJ) emu-runtime emusa80535.pc
endif

DEBUG_COMMIT := $(shell if test -f SOURCE_COMMIT; then cat SOURCE_COMMIT; else git rev-parse --verify HEAD 2>/dev/null || echo unknown; fi)
DEBUG_SRC := emu_debug_server.c emu_json_internal.c emu_sha_internal.c emu_debug.c emu_decode_internal.c core.c opcodes.c disasm.c binary_loader.c
DAP_ROOT ?= ../emuSA80535-DAP

RUNTIME_LIB := libemusa80535_runtime.a
SDK_LIB := libemusa80535.a
SDK_SRC := emu_sha_internal.c emu_cpu_facade.c core.c opcodes.c disasm.c binary_loader.c emu_debug.c emu_decode_internal.c \
	emu_debug_event.c emu_debug_trace.c emu_debug_runtime.c
SDK_OBJ := $(SDK_SRC:.c=.sdk.o)
SDK_HEADERS := $(wildcard lib/include/emusa80535/*.h lib/include/emusa80535/contracts/*.h)
override CPPFLAGS += -Ilib/include
PYTHON ?= python3
CONTRACT_VERSION := 0.2.0
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
SRC := $(filter-out emu_debug_server.c emu_debug.c emu_session.c emu_outcome_internal.c emu_json_internal.c,$(wildcard *.c))
OBJ := $(SRC:.c=.o)

%.o: %.c $(HEADERS)
	 $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(DEBUG_BIN): $(DEBUG_SRC) $(HEADERS) FORCE
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -DEMU_DEBUG_BUILD_COMMIT=\"$(DEBUG_COMMIT)\" \
		$(LDFLAGS) -o $@ $(DEBUG_SRC)

%.sdk.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -c -o $@ $<

$(SDK_LIB): $(SDK_OBJ)
	$(AR) rcs $@ $^

CPU_LIB := libemusa80535_cpu.a
CPU_OBJ := $(filter-out emu_debug.sdk.o emu_debug_event.sdk.o emu_debug_trace.sdk.o emu_debug_runtime.sdk.o emu_sha_internal.sdk.o,$(SDK_OBJ))

$(CPU_LIB): $(CPU_OBJ)
	$(AR) rcs $@ $^

cpu-sdk: $(CPU_LIB)

sdk: $(SDK_LIB) $(CPU_LIB) $(RUNTIME_LIB)

# Generated on every install so changing prefix/libdir never reuses old paths.
emusa80535.pc: packaging/emusa80535.pc.in FORCE
	sed -e 's|@PREFIX@|$(prefix)|g' -e 's|@LIBDIR@|$(libdir)|g' \
		-e 's|@INCLUDEDIR@|$(includedir)|g' -e 's|@VERSION@|$(VERSION)|g' $< > $@

install: $(DEBUG_BIN) $(SDK_LIB) $(CPU_LIB) $(RUNTIME_LIB) emu-runtime emusa80535.pc
	$(INSTALL) -d $(DESTDIR)$(bindir) $(DESTDIR)$(libdir)/pkgconfig \
		$(DESTDIR)$(includedir)/emusa80535 $(DESTDIR)$(docdir) $(DESTDIR)$(licensedir)
	$(INSTALL) -m 755 $(DEBUG_BIN) $(DESTDIR)$(bindir)/emu-debug
	$(INSTALL) -m 644 $(SDK_LIB) $(CPU_LIB) $(RUNTIME_LIB) $(DESTDIR)$(libdir)/
	$(INSTALL) -m 755 emu-runtime $(DESTDIR)$(bindir)/
	$(INSTALL) -d $(DESTDIR)$(docdir)/runtime
	$(INSTALL) -m 644 runtime/README.md runtime/script1.schema.json runtime/output1.schema.json runtime/example.ndjson $(DESTDIR)$(docdir)/runtime/
	$(INSTALL) -m 644 emusa80535.pc $(DESTDIR)$(libdir)/pkgconfig/
	$(INSTALL) -d $(DESTDIR)$(includedir)/emusa80535/contracts
	$(INSTALL) -m 644 lib/include/emusa80535/*.h $(DESTDIR)$(includedir)/emusa80535/
	$(INSTALL) -m 644 lib/include/emusa80535/contracts/*.h $(DESTDIR)$(includedir)/emusa80535/contracts/
	$(PYTHON) interfaces/0.4.0/bundle.py install --destination $(DESTDIR)$(docdir)/interface-bundles/0.4.0 $(CONTROL31_PROVENANCE)
	$(PYTHON) interfaces/tools/bundle.py --version 0.3.0 install --destination $(DESTDIR)$(docdir)/interface-bundles/0.3.0 --source-commit $(DEBUG_COMMIT)
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

cpu-facade-test:
	$(MAKE) -C tests cpu-facade-test

.PHONY: cpu-sdk cpu-facade-test

cpu-link-test: cpu-sdk
	$(PYTHON) tests/test_cpu_link.py

.PHONY: cpu-link-test

# Native session/debug controller, separately linkable from the CPU engine.
RUNTIME_OBJ := emu_trace_internal.sdk.o emu_session.sdk.o emu_json_internal.sdk.o emu_sha_internal.sdk.o emu_outcome_internal.sdk.o emu_debug_event.sdk.o emu_debug_trace.sdk.o emu_debug_runtime.sdk.o
CONTRACT_MANIFEST_SHA256 := $(shell sha256sum interfaces/$(CONTRACT_VERSION)/manifest.json | cut -d ' ' -f 1)
CONTROL3_MANIFEST_SHA256 := $(shell sha256sum interfaces/0.3.0/manifest.json | cut -d ' ' -f 1)
CONTROL31_MANIFEST_SHA256 := $(shell sha256sum interfaces/0.4.0/manifest.json | cut -d ' ' -f 1)
RUNTIME_CPPFLAGS := -DEMU_CONTROL31_MANIFEST_SHA256=\"$(CONTROL31_MANIFEST_SHA256)\" -DEMU_CONTROL3_MANIFEST_SHA256=\"$(CONTROL3_MANIFEST_SHA256)\" -DEMU_RUNTIME_COMMIT=\"$(DEBUG_COMMIT)\" -DEMU_CONTRACT_MANIFEST_SHA256=\"$(CONTRACT_MANIFEST_SHA256)\"
emu_session.sdk.o: emu_session.c $(HEADERS) interfaces/0.4.0/manifest.json FORCE
	$(CC) $(CPPFLAGS) $(RUNTIME_CPPFLAGS) $(CFLAGS) -std=c99 -c -o $@ $<
$(RUNTIME_LIB): $(RUNTIME_OBJ)
	$(AR) rcs $@ $^
runtime-sdk: $(RUNTIME_LIB) $(CPU_LIB)
runtime-session-test: runtime-sdk
	$(MAKE) -C tests runtime-session-test
-include runtime/targets.mk
.PHONY: runtime-sdk runtime-session-test

# Contract-only target; no new runtime symbols/capabilities are built.
interface-target-check:
	$(PYTHON) interfaces/tools/check_target_contracts.py
interface-target-export:
	$(PYTHON) interfaces/tools/bundle.py --version 0.3.0 export --output $(INTERFACE_ARCHIVE)
.PHONY: interface-target-check interface-target-export

control3-test: runtime-sdk
	$(MAKE) -C tests control3-test
.PHONY: control3-test

outcome-test: runtime-sdk
	$(MAKE) -C tests outcome-test
.PHONY: outcome-test

cpu-observation-test:
	$(MAKE) -C tests cpu-observation-test
.PHONY: cpu-observation-test

trace-test: emu-runtime
	$(MAKE) -C tests trace-native-test
	$(PYTHON) tests/test_runtime_trace.py
.PHONY: trace-test

# Explicitly negotiated wire 3.1 sidecar; native SDK and trace files remain 0.3.
# The external release lock pins contract provenance independently of runtime HEAD.
CONTROL31_LOCK := $(wildcard interfaces/control31.lock.json)
CONTROL31_PROVENANCE = $(if $(CONTROL31_LOCK),--lock $(CONTROL31_LOCK),--source-commit $(DEBUG_COMMIT))
CONTROL31_ARCHIVE ?= emusa80535-control-0.4.0.tar.gz
interface-control31-check:
	$(PYTHON) interfaces/0.4.0/bundle.py check $(if $(CONTROL31_LOCK),--lock $(CONTROL31_LOCK))
interface-control31-export:
	$(PYTHON) interfaces/0.4.0/bundle.py export --output $(CONTROL31_ARCHIVE) $(CONTROL31_PROVENANCE)
.PHONY: interface-control31-check interface-control31-export
