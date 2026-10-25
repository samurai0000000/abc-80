#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

SHELL := /bin/bash
BUILD_DIR := build
ROM_BIN := build/rom.bin
NUM_PROCS := $(shell nproc 2>/dev/null || echo 4)
CMAKE := $(shell if [ -x /usr/bin/cmake ]; then echo /usr/bin/cmake; else echo cmake; fi)
TAPE_ASMS := $(sort $(wildcard tapes/[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F].asm))
TAPE_FILES := $(patsubst tapes/%.asm,tapes/%.tape,$(TAPE_ASMS))
TAPE_BINS := $(patsubst tapes/%.asm,build/tapes/%.bin,$(TAPE_ASMS))

SANITIZE ?= 0
ifneq (,$(findstring -fsanitize,$(CFLAGS) $(CXXFLAGS)))
SANITIZE := 1
endif
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF
ifeq ($(SANITIZE),1)
BUILD_DIR := build/asan
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
endif

all: $(ROM_BIN)
	@if [ -f .gitmodules ] && { [ ! -f third_party/cpputest/CMakeLists.txt ] || [ ! -f third_party/cpp-httplib/httplib.h ] || [ ! -f third_party/json/include/nlohmann/json.hpp ]; }; then \
		git submodule update --init --recursive; \
	fi
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && (test -f Makefile || $(CMAKE) $(CURDIR) $(CMAKE_FLAGS))
ifeq ($(SANITIZE),1)
	@cd $(BUILD_DIR) && $(CMAKE) $(CURDIR) $(CMAKE_FLAGS) > /dev/null
endif
	@$(MAKE) -C $(BUILD_DIR) -j$(NUM_PROCS)
	@$(MAKE) --no-print-directory tapes

deb:
	@rm -f build/abc-80_*.deb
	@if [ "$(SANITIZE)" = "1" ]; then \
		$(MAKE) SANITIZE=0 all; \
	else \
		$(MAKE) all; \
	fi

tapes: $(TAPE_BINS) $(TAPE_FILES)

build/tapes/%.bin: tapes/%.asm
	@command -v pasmo >/dev/null 2>&1 || { echo "error: pasmo not found; it is needed to assemble $<" >&2; exit 1; }
	@mkdir -p build/tapes
	@pasmo --bin $< $@

tapes/%.tape: tapes/%.asm build/tapes/%.bin $(BUILD_DIR)/abc80_cli
	@./$(BUILD_DIR)/abc80_cli -c "tape encode $< build/tapes/$*.bin $@" >/dev/null

# The book monitor ROM: src/rom.asm assembled with pasmo (needed by the "monitor" ROM profile).
$(ROM_BIN): src/rom.asm
	@command -v pasmo >/dev/null 2>&1 || { echo "error: pasmo not found; it is needed to assemble src/rom.asm" >&2; exit 1; }
	@mkdir -p build
	@pasmo --bin src/rom.asm $(ROM_BIN)

test: all
ifeq ($(SANITIZE),1)
	@if ! ls build/abc-80_*.deb 1>/dev/null 2>&1; then \
		$(MAKE) SANITIZE=0 deb; \
	fi
endif
	@./$(BUILD_DIR)/abc80_tests

# Browser tests of the web front panel: real abc80_server, real Chromium (needs: pip install -r tests/web/requirements.txt).
test-web: all
	python3 -m pytest tests/web -v

clean:
	@if [ -d $(BUILD_DIR) ] && [ -f $(BUILD_DIR)/Makefile ]; then \
		$(MAKE) -C $(BUILD_DIR) clean; \
	fi

distclean:
	rm -rf build

.PHONY: all deb tapes test test-web clean distclean
