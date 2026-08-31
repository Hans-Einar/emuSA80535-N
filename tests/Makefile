ifeq ($(origin CC),default)
CC := gcc
endif
CFLAGS ?= -O2 -g
CFLAGS += -std=c99 -Wall -Wextra -Wno-unused-parameter -Wshadow

ifeq ($(OS),Windows_NT)
TEST_BIN := stage0_tests.exe
CLEAN_CMD = cmd /C "if exist stage0_tests.exe del /Q stage0_tests.exe & if exist stage0_tests del /Q stage0_tests & if exist stage0-short.bin del /Q stage0-short.bin & if exist stage0-exact.bin del /Q stage0-exact.bin & if exist stage0-long.bin del /Q stage0-long.bin"
else
TEST_BIN := stage0_tests
CLEAN_CMD = rm -f stage0_tests stage0_tests.exe stage0-short.bin stage0-exact.bin stage0-long.bin
endif
CORE_SRC := ../core.c ../opcodes.c ../disasm.c ../binary_loader.c

all: $(TEST_BIN)

$(TEST_BIN): test_stage0.c $(CORE_SRC) ../emu8051.h
	$(CC) $(CFLAGS) -o $@ test_stage0.c $(CORE_SRC)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	$(CLEAN_CMD)

.PHONY: all test clean
