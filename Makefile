#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

BUILD_DIR ?= build
ROM_BIN := $(BUILD_DIR)/rom.bin
TAPE_ASMS := $(sort $(wildcard tapes/[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F].asm))
TAPE_FILES := $(patsubst tapes/%.asm,tapes/%.tape,$(TAPE_ASMS))

all: $(ROM_BIN)
	@if [ -f .gitmodules ] && { [ ! -f third_party/cpputest/CMakeLists.txt ] || [ ! -f third_party/cpp-httplib/httplib.h ] || [ ! -f third_party/json/include/nlohmann/json.hpp ]; }; then \
		git submodule update --init --recursive; \
	fi
	@mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON && $(MAKE) -j$$(nproc)
	@$(MAKE) --no-print-directory tapes

tapes: $(TAPE_FILES)

tapes/%.tape: tapes/%.asm $(BUILD_DIR)/abc80_cli
	@command -v pasmo >/dev/null 2>&1 || { echo "error: pasmo not found; it is needed to assemble $<" >&2; exit 1; }
	@mkdir -p $(BUILD_DIR)/tapes
	@pasmo --bin $< $(BUILD_DIR)/tapes/$*.bin
	@./$(BUILD_DIR)/abc80_cli -c "tape encode $< $(BUILD_DIR)/tapes/$*.bin $@" >/dev/null

# The book monitor ROM: src/rom.asm assembled with pasmo (needed by the "monitor" ROM profile).
$(ROM_BIN): src/rom.asm
	@command -v pasmo >/dev/null 2>&1 || { echo "error: pasmo not found; it is needed to assemble src/rom.asm" >&2; exit 1; }
	@mkdir -p $(BUILD_DIR)
	@pasmo --bin src/rom.asm $(ROM_BIN)

test: all
	@./$(BUILD_DIR)/abc80_tests

# Browser tests of the web front panel: real abc80_server, real Chromium (needs: pip install -r tests/web/requirements.txt).
test-web: all
	python3 -m pytest tests/web -v

clean:
	@if [ -d $(BUILD_DIR) ]; then $(MAKE) -C $(BUILD_DIR) clean; fi

distclean:
	rm -rf $(BUILD_DIR)

.PHONY: all tapes test test-web clean distclean
