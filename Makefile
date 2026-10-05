#
# Makefile
#
# Copyright (C) 2026, Charles Chiou
#

BUILD_DIR ?= build

all:
	@if [ -f .gitmodules ] && [ ! -f third_party/cpputest/CMakeLists.txt ]; then \
		git submodule update --init --recursive; \
	fi
	@mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON && $(MAKE) -j$$(nproc)

test: all
	@./$(BUILD_DIR)/abc80_tests

clean:
	@if [ -d $(BUILD_DIR) ]; then $(MAKE) -C $(BUILD_DIR) clean; fi

distclean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean distclean
