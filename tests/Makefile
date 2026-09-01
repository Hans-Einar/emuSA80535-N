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
CLEAN_CMD = powershell -NoProfile -Command "Remove-Item -Force -ErrorAction Ignore 'stage0_tests.exe','stage0_tests','stage1_irq_tests.exe','stage1_irq_tests','stage1_timer_tests.exe','stage1_timer_tests','stage1_uart_tests.exe','stage1_uart_tests','stage0_tests.ilk','stage0_tests.pdb','stage1_irq_tests.ilk','stage1_irq_tests.pdb','stage1_timer_tests.ilk','stage1_timer_tests.pdb','stage1_uart_tests.ilk','stage1_uart_tests.pdb','stage0-short.bin','stage0-exact.bin','stage0-long.bin'; exit 0"
else
STAGE0_BIN := stage0_tests
STAGE1_BIN := stage1_irq_tests
TIMER_BIN := stage1_timer_tests
UART_BIN := stage1_uart_tests
CLEAN_CMD = rm -f stage0_tests stage0_tests.exe stage1_irq_tests stage1_irq_tests.exe stage1_timer_tests stage1_timer_tests.exe stage1_uart_tests stage1_uart_tests.exe stage0-short.bin stage0-exact.bin stage0-long.bin
endif
CORE_SRC := ../core.c ../opcodes.c ../disasm.c ../binary_loader.c

all: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN)

$(STAGE0_BIN): test_stage0.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage0.c $(CORE_SRC)

$(STAGE1_BIN): test_stage1_irq.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage1_irq.c $(CORE_SRC)

$(TIMER_BIN): test_stage1_timers.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage1_timers.c $(CORE_SRC)

$(UART_BIN): test_stage1_uart.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage1_uart.c $(CORE_SRC)

test: $(STAGE0_BIN) $(STAGE1_BIN) $(TIMER_BIN) $(UART_BIN)
	./$(STAGE0_BIN)
	./$(STAGE1_BIN)
	./$(TIMER_BIN)
	./$(UART_BIN)

clean:
	$(CLEAN_CMD)

.PHONY: all test clean
