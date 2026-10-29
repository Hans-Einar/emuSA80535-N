.DEFAULT_GOAL := all
ifeq ($(origin CC),default)
CC := gcc
endif
override CPPFLAGS += -I../lib/include
CFLAGS ?= -O2 -g
CFLAGS += -std=c99 -Wall -Wextra -Wno-unused-parameter -Wshadow

ifeq ($(OS),Windows_NT)
STAGE0_BIN := stage0_tests.exe
STAGE1_BIN := stage1_irq_tests.exe
TIMER_BIN := stage1_timer_tests.exe
UART_BIN := stage1_uart_tests.exe
PORT_BIN := stage2_port_tests.exe
EDGE_BIN := stage2_edge_tests.exe
ADC_BIN := stage3_adc_tests.exe
TIMER2_BIN := stage4_timer2_tests.exe
DEBUG_BIN_TEST := debug_facade_tests.exe
DEBUG_EVENT_BIN := debug_event_tests.exe
DEBUG_TRACE_BIN := debug_trace_tests.exe
DEBUG_RUNTIME_BIN := debug_runtime_tests.exe
CLEAN_CMD = powershell -NoProfile -Command "Remove-Item -Force -ErrorAction Ignore 'stage0_tests.exe','stage0_tests','stage1_irq_tests.exe','stage1_irq_tests','stage1_timer_tests.exe','stage1_timer_tests','stage1_uart_tests.exe','stage1_uart_tests','stage2_port_tests.exe','stage2_port_tests','stage2_edge_tests.exe','stage2_edge_tests','stage3_adc_tests.exe','stage3_adc_tests','stage4_timer2_tests.exe','stage4_timer2_tests','debug_facade_tests.exe','debug_facade_tests','debug_event_tests.exe','debug_event_tests','debug_trace_tests.exe','debug_trace_tests','debug_runtime_tests.exe','debug_runtime_tests','stage0_tests.ilk','stage0_tests.pdb','stage1_irq_tests.ilk','stage1_irq_tests.pdb','stage1_timer_tests.ilk','stage1_timer_tests.pdb','stage1_uart_tests.ilk','stage1_uart_tests.pdb','stage2_port_tests.ilk','stage2_port_tests.pdb','stage2_edge_tests.ilk','stage2_edge_tests.pdb','stage3_adc_tests.ilk','stage3_adc_tests.pdb','stage4_timer2_tests.ilk','stage4_timer2_tests.pdb','debug_facade_tests.ilk','debug_facade_tests.pdb','debug_event_tests.ilk','debug_event_tests.pdb','debug_trace_tests.ilk','debug_trace_tests.pdb','debug_runtime_tests.ilk','debug_runtime_tests.pdb','cpu_facade_tests','cpu_facade_tests.exe','stage0-short.bin','stage0-exact.bin','stage0-long.bin','debug-image.bin','debug-short.bin'; exit 0"
else
STAGE0_BIN := stage0_tests
STAGE1_BIN := stage1_irq_tests
TIMER_BIN := stage1_timer_tests
UART_BIN := stage1_uart_tests
PORT_BIN := stage2_port_tests
EDGE_BIN := stage2_edge_tests
ADC_BIN := stage3_adc_tests
TIMER2_BIN := stage4_timer2_tests
DEBUG_BIN_TEST := debug_facade_tests
DEBUG_EVENT_BIN := debug_event_tests
DEBUG_TRACE_BIN := debug_trace_tests
DEBUG_RUNTIME_BIN := debug_runtime_tests
CLEAN_CMD = rm -f stage0_tests stage0_tests.exe stage1_irq_tests stage1_irq_tests.exe stage1_timer_tests stage1_timer_tests.exe stage1_uart_tests stage1_uart_tests.exe stage2_port_tests stage2_port_tests.exe stage2_edge_tests stage2_edge_tests.exe stage3_adc_tests stage3_adc_tests.exe stage4_timer2_tests stage4_timer2_tests.exe debug_facade_tests debug_facade_tests.exe debug_event_tests debug_event_tests.exe debug_trace_tests debug_trace_tests.exe debug_runtime_tests debug_runtime_tests.exe cpu_facade_tests cpu_facade_tests.exe runtime_session_tests trace_native_tests control3_tests control31_tests control3_cpp_tests outcome_tests cpu_observation_tests stage0-short.bin stage0-exact.bin stage0-long.bin debug-image.bin debug-short.bin
endif
CORE_SRC := ../core.c ../opcodes.c ../disasm.c ../binary_loader.c
DEBUG_SRC := ../emu_sha_internal.c ../emu_debug.c ../emu_decode_internal.c $(CORE_SRC)
PYTHON ?= python
NODE ?= node
DEBUG_BIN ?= ../emu-debug
DAP_ROOT ?= ../../emuSA80535-DAP

PUBLIC_HEADERS := $(wildcard ../lib/include/emusa80535/*.h ../lib/include/emusa80535/contracts/*.h ../*_internal.h)
$(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(EDGE_BIN) $(ADC_BIN) $(TIMER2_BIN) $(DEBUG_BIN_TEST) $(DEBUG_EVENT_BIN) $(DEBUG_TRACE_BIN) $(DEBUG_RUNTIME_BIN): $(PUBLIC_HEADERS)

all: cpu-facade-test $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(EDGE_BIN) $(ADC_BIN) $(TIMER2_BIN) $(DEBUG_EVENT_BIN) $(DEBUG_TRACE_BIN) $(DEBUG_RUNTIME_BIN)

$(STAGE0_BIN): test_stage0.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage0.c $(CORE_SRC)

$(STAGE1_BIN): test_stage1_irq.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage1_irq.c $(CORE_SRC)

$(TIMER_BIN): test_stage1_timers.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage1_timers.c $(CORE_SRC)

$(UART_BIN): test_stage1_uart.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage1_uart.c $(CORE_SRC)

$(PORT_BIN): test_stage2_ports.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage2_ports.c $(CORE_SRC)

$(EDGE_BIN): test_stage2_edges.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage2_edges.c $(CORE_SRC)

$(ADC_BIN): test_stage3_adc.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage3_adc.c $(CORE_SRC)

$(TIMER2_BIN): test_stage4_timer2.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_stage4_timer2.c $(CORE_SRC)

$(DEBUG_BIN_TEST): ../emu_decode_internal.h test_debug_facade.c $(DEBUG_SRC) ../emu_debug.h ../emu8051.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test_debug_facade.c $(DEBUG_SRC)

$(DEBUG_EVENT_BIN): test_debug_event.c ../emu_debug_event.c ../emu_debug_event.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ test_debug_event.c ../emu_debug_event.c

$(DEBUG_TRACE_BIN): test_debug_trace.c ../emu_debug_trace.c ../emu_debug_trace.h ../emu_debug_event.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ test_debug_trace.c ../emu_debug_trace.c

$(DEBUG_RUNTIME_BIN): test_debug_runtime.c ../emu_debug_runtime.c ../emu_debug_runtime.h ../emu_debug_trace.c ../emu_debug_trace.h ../emu_debug_event.c ../emu_debug_event.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ test_debug_runtime.c ../emu_debug_runtime.c ../emu_debug_trace.c ../emu_debug_event.c

test: trace-native-test cpu-facade-test $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN) $(PORT_BIN) $(EDGE_BIN) $(ADC_BIN) $(TIMER2_BIN) $(DEBUG_EVENT_BIN) $(DEBUG_TRACE_BIN) $(DEBUG_RUNTIME_BIN)
	./$(STAGE0_BIN)
	./$(STAGE1_BIN)
	./$(TIMER_BIN)
	./$(UART_BIN)
	./$(PORT_BIN)
	./$(EDGE_BIN)
	./$(ADC_BIN)
	./$(TIMER2_BIN)
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

CPU_FACADE_BIN := cpu_facade_tests$(if $(filter Windows_NT,$(OS)),.exe,)
$(CPU_FACADE_BIN): test_cpu_facade.c ../emu_cpu_facade.c ../emu_decode_internal.c ../emu_decode_internal.h $(CORE_SRC) $(PUBLIC_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -DEMU_CPU_FACADE_TESTING -o $@ test_cpu_facade.c ../emu_cpu_facade.c ../emu_decode_internal.c $(CORE_SRC)

cpu-facade-test: $(CPU_FACADE_BIN)
	./$(CPU_FACADE_BIN)

.PHONY: cpu-facade-test

SESSION_SRC := ../emu_trace_internal.c ../emu_outcome_internal.c ../emu_debug_event.c ../emu_debug_trace.c ../emu_debug_runtime.c ../emu_session.c ../emu_json_internal.c ../emu_sha_internal.c ../emu_cpu_facade.c ../emu_decode_internal.c $(CORE_SRC)
runtime_session_tests: test_runtime_session.c $(SESSION_SRC) $(PUBLIC_HEADERS) ../emu_json_internal.h ../emu_sha_internal.h ../emu_session_internal.h ../emu_wire_schema_internal.h ../emu_control3_schema_internal.h ../emu_control3_validation_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ test_runtime_session.c $(SESSION_SRC)
runtime-session-test: runtime_session_tests
	$(PYTHON) test_runtime_wire_schema.py
.PHONY: runtime-session-test

control3_tests: test_control3.c test_runtime_session.c $(SESSION_SRC) $(PUBLIC_HEADERS) ../emu_control3_schema_internal.h ../emu_control3_validation_internal.h ../emu_session_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ test_control3.c $(SESSION_SRC)
control3-test: control3_tests control3_cpp_tests
	./control3_cpp_tests
	$(PYTHON) test_control3_schema.py
.PHONY: control3-test

control3_cpp_tests: test_control3_cpp.cpp $(PUBLIC_HEADERS) ../libemusa80535_runtime.a ../libemusa80535_cpu.a
	$(CXX) $(CPPFLAGS) -std=c++11 -Wall -Wextra -Werror -o $@ $< ../libemusa80535_runtime.a ../libemusa80535_cpu.a

CPU_OBSERVATION_BIN := cpu_observation_tests
$(CPU_OBSERVATION_BIN): test_cpu_observation.c ../emu_cpu_facade.c ../emu_cpu_observation_internal.h ../emu_cpu_lifecycle_internal.h ../emu_decode_internal.c $(CORE_SRC) $(PUBLIC_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -DEMU_CPU_FACADE_TESTING -o $@ test_cpu_observation.c ../emu_cpu_facade.c ../emu_decode_internal.c $(CORE_SRC)
cpu-observation-test: $(CPU_OBSERVATION_BIN)
	./$(CPU_OBSERVATION_BIN)
.PHONY: cpu-observation-test

outcome_tests: test_outcome.c ../emu_outcome_internal.c ../emu_outcome_internal.h ../emu_debug_observation_internal.h ../emu_json_internal.c ../emu_debug_event.c ../emu_debug_trace.c ../emu_debug_runtime.c $(PUBLIC_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ test_outcome.c ../emu_outcome_internal.c ../emu_json_internal.c ../emu_debug_event.c ../emu_debug_trace.c ../emu_debug_runtime.c
outcome-test: outcome_tests
	./outcome_tests
.PHONY: outcome-test

trace_native_tests: test_trace_native.c ../emu_trace_internal.c ../emu_debug_runtime.c ../emu_debug_trace.c ../emu_debug_event.c ../emu_json_internal.c ../emu_outcome_internal.c ../emu_sha_internal.c $(PUBLIC_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -DEMU_TRACE_TEST -o $@ test_trace_native.c ../emu_trace_internal.c ../emu_debug_runtime.c ../emu_debug_trace.c ../emu_debug_event.c ../emu_json_internal.c ../emu_outcome_internal.c ../emu_sha_internal.c
trace-native-test: trace_native_tests
	./trace_native_tests
.PHONY: trace-native-test

control31_tests: test_control31.c test_runtime_session.c $(SESSION_SRC) $(PUBLIC_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -o $@ test_control31.c $(SESSION_SRC)
control31-test: control31_tests
	./control31_tests
.PHONY: control31-test
