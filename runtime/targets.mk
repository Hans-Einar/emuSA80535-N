# Generic local host; CPU and session remain independent of dlopen.
RUNTIME_BIN := emu-runtime
RUNTIME_HOST_SRC := runtime/main.c runtime/loader.c runtime/control.c
$(RUNTIME_BIN): $(RUNTIME_HOST_SRC) runtime/loader.h runtime/control.h $(RUNTIME_LIB) $(CPU_LIB) $(HEADERS) FORCE
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -I. -DEMU_RUNTIME_COMMIT=\"$(DEBUG_COMMIT)\" \
		$(LDFLAGS) -o $@ $(RUNTIME_HOST_SRC) $(RUNTIME_LIB) $(CPU_LIB) -ldl

runtime-host-test: $(RUNTIME_BIN)
	$(PYTHON) tests/test_runtime_host.py

runtime-control-test: $(RUNTIME_BIN)
	$(PYTHON) tests/test_runtime_control.py

install-runtime: install $(RUNTIME_BIN) $(RUNTIME_LIB) $(CPU_LIB)
	$(INSTALL) -d $(DESTDIR)$(bindir) $(DESTDIR)$(libdir) $(DESTDIR)$(docdir)/runtime
	$(INSTALL) -m 755 $(RUNTIME_BIN) $(DESTDIR)$(bindir)/
	$(INSTALL) -m 644 $(RUNTIME_LIB) $(CPU_LIB) $(DESTDIR)$(libdir)/
	$(INSTALL) -m 644 runtime/README.md runtime/script1.schema.json runtime/output1.schema.json runtime/example.ndjson $(DESTDIR)$(docdir)/runtime/

.PHONY: runtime-host-test runtime-control-test install-runtime
