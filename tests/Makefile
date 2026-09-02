ifeq ($(origin CC),default)
CC := gcc
endif
CFLAGS ?= -O2 -g
CFLAGS += -std=c99 -Wall -Wextra -Wno-unused-parameter -Wshadow

ifeq ($(OS),Windows_NT)
STAGE0_BIN := stage0_tests.exe
STAGE1_BIN := stage1_irq_tests.exe
TIMER_BIN := stage1_timer_tests.exe
UART_BIN := stage1_uart_tests.exe
PORT_BIN := stage2_port_tests.exe
DEBUG_BIN_TEST := debug_facade_tests.exe
DEBUG_EVENT_BIN := debug_event_tests.exe
DEBUG_TRACE_BIN := debug_trace_tests.exe
DEBUG_RUNTIME_BIN := debug_runtime_tests.exe
CLEAN_CMD = powershell -NoProfile -Command "Remove-Item -Force -ErrorAction Ignore 'stage0_tests.exe','stage0_tests','stage1_irq_tests.exe','stage1_irq_tests','stage1_timer_tests.exe','stage1_timer_tests','stage1_uart_tests.exe','stage1_uart_tests','stage2_port_tests.exe','stage2_port_tests','debug_facade_tests.exe','debug_facade_tests','debug_event_tests.exe','debug_event_tests','debug_trace_tests.exe','debug_trace_tests','debug_runtime_tests.exe','debug_runtime_tests','stage0_tests.ilk','stage0_tests.pdb','stage1_irq_tests.ilk','stage1_irq_tests.pdb','stage1_timer_tests.ilk','stage1_timer_tests.pdb','stage1_uart_tests.ilk','stage1_uart_tests.pdb','stage2_port_tests.ilk','stage2_port_tests.pdb','debug_facade_tests.ilk','debug_facade_tests.pdb','debug_event_tests.ilk','debug_event_tests.pdb','debug_trace_tests.ilk','debug_trace_tests.pdb','debug_runtime_tests.ilk','debug_runtime_tests.pdb','stage0-short.bin','stage0-exact.bin','stage0-long.bin','debug-image.bin','debug-short.bin'; exit 0"
else
STAGE0_BIN := stage0_tests
STAGE1_BIN := stage1_irq_tests
TIMER_BIN := stage1_timer_tests
UART_BIN := stage1_uart_tests
PORT_BIN := stage2_port_tests
DEBUG_BIN_TEST := debug_facade_tests
DEBUG_EVENT_BIN := debug_event_tests
DEBUG_TRACE_BIN := debug_trace_tests
DEBUG_RUNTIME_BIN := debug_runtime_tests
CLEAN_CMD = rm -f stage0_tests stage0_tests.exe stage1_irq_tests stage1_irq_tests.exe stage1_timer_tests stage1_timer_tests.exe stage1_uart_tests stage1_uart_tests.exe stage2_port_tests stage2_port_tests.exe debug_facade_tests debug_facade_tests.exe debug_event_tests debug_event_tests.exe debug_trace_tests debug_trace_tests.exe debug_runtime_tests debug_runtime_tests.exe stage0-short.bin stage0-exact.bin stage0-long.bin debug-image.bin debug-short.bin
endif
CORE_SRC := ../core.c ../opcodes.c ../disasm.c ../binary_loader.c
DEBUG_SRC := ../emu_debug.c $(CORE_SRC)
PYTHON ?= python
NODE ?= node
DEBUG_BIN ?= ../emu-debug
DAP_ROOT ?= ../../emuSA80535-DAP

all: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(DEBUG_EVENT_BIN) $(DEBUG_TRACE_BIN) $(DEBUG_RUNTIME_BIN)

$(STAGE0_BIN): test_stage0.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage0.c $(CORE_SRC)

$(STAGE1_BIN): test_stage1_irq.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage1_irq.c $(CORE_SRC)

$(TIMER_BIN): test_stage1_timers.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage1_timers.c $(CORE_SRC)

$(UART_BIN): test_stage1_uart.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage1_uart.c $(CORE_SRC)

$(PORT_BIN): test_stage2_ports.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage2_ports.c $(CORE_SRC)

$(DEBUG_BIN_TEST): test_debug_facade.c $(DEBUG_SRC) ../emu_debug.h ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_debug_facade.c $(DEBUG_SRC)

$(DEBUG_EVENT_BIN): test_debug_event.c ../emu_debug_event.c ../emu_debug_event.h
	$(CC) $(CFLAGS) -Werror -o $@ test_debug_event.c ../emu_debug_event.c

$(DEBUG_TRACE_BIN): test_debug_trace.c ../emu_debug_trace.c ../emu_debug_trace.h ../emu_debug_event.h
	$(CC) $(CFLAGS) -Werror -o $@ test_debug_trace.c ../emu_debug_trace.c

$(DEBUG_RUNTIME_BIN): test_debug_runtime.c ../emu_debug_runtime.c ../emu_debug_runtime.h ../emu_debug_trace.c ../emu_debug_trace.h ../emu_debug_event.c ../emu_debug_event.h
	$(CC) $(CFLAGS) -Werror -o $@ test_debug_runtime.c ../emu_debug_runtime.c ../emu_debug_trace.c ../emu_debug_event.c

test: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(DEBUG_EVENT_BIN) $(DEBUG_TRACE_BIN) $(DEBUG_RUNTIME_BIN)
	./$(STAGE0_BIN)
	./$(STAGE1_BIN)
	./$(TIMER_BIN)
	./$(UART_BIN)
	./$(PORT_BIN)
	./$(DEBUG_EVENT_BIN)
	./$(DEBUG_TRACE_BIN)
	./$(DEBUG_RUNTIME_BIN)

debug-test: $(DEBUG_BIN_TEST)
	./$(DEBUG_BIN_TEST)
	$(PYTHON) test_emu_debug_process.py $(DEBUG_BIN)

debug-event-test: $(DEBUG_EVENT_BIN)
	./$(DEBUG_EVENT_BIN)

debug-trace-test: $(DEBUG_TRACE_BIN)
	./$(DEBUG_TRACE_BIN)

debug-runtime-test: $(DEBUG_RUNTIME_BIN)
	./$(DEBUG_RUNTIME_BIN)

dap-integration-test:
	$(NODE) test_dap_real.mjs $(DAP_ROOT) $(DEBUG_BIN)

clean:
	$(CLEAN_CMD)

.PHONY: all test debug-test debug-event-test debug-trace-test debug-runtime-test dap-integration-test clean
