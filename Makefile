# SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
#
# SPDX-License-Identifier: MIT

# macOS convenience wrapper around the CMake build.
# Written for the GNU Make 3.81 that ships with Xcode's command line tools.
#
#   make              configure (once) and build a Release chatterino.app
#   make install      bundle Qt into the app and copy it to /Applications
#   make run          open the freshly built app without installing it
#   make help         list every target and option

CONFIG      ?= Release
BUILD_DIR   ?= build/$(shell echo $(CONFIG) | tr A-Z a-z)
PREFIX      ?= /Applications
JOBS        ?= $(shell sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

# Feature toggles, passed straight through to CMake
PLUGINS     ?= ON
SPELLCHECK  ?= OFF
LTO         ?= OFF
UPDATER     ?= OFF

BREW        := $(shell command -v brew 2>/dev/null)
QT_DIR      ?= $(if $(BREW),$(shell $(BREW) --prefix qt 2>/dev/null))
OPENSSL_DIR ?= $(if $(BREW),$(shell $(BREW) --prefix openssl@3 2>/dev/null))
MACDEPLOYQT ?= $(if $(QT_DIR),$(QT_DIR)/bin/macdeployqt,macdeployqt)

# Ninja and ccache are optional; both make rebuilds noticeably faster
GENERATOR   := $(if $(shell command -v ninja 2>/dev/null),Ninja,Unix Makefiles)
CCACHE      := $(shell command -v ccache 2>/dev/null)

APP         := $(BUILD_DIR)/bin/chatterino.app
STAGED_APP  := $(BUILD_DIR)/install/chatterino.app

CMAKE_FLAGS := \
	-G "$(GENERATOR)" \
	-DCMAKE_BUILD_TYPE=$(CONFIG) \
	-DUSE_PRECOMPILED_HEADERS=OFF \
	-DCHATTERINO_PLUGINS=$(PLUGINS) \
	-DCHATTERINO_SPELLCHECK=$(SPELLCHECK) \
	-DCHATTERINO_LTO=$(LTO) \
	-DCHATTERINO_UPDATER=$(UPDATER) \
	$(if $(QT_DIR),-DCMAKE_PREFIX_PATH="$(QT_DIR)") \
	$(if $(OPENSSL_DIR),-DOPENSSL_ROOT_DIR="$(OPENSSL_DIR)") \
	$(if $(CCACHE),-DCMAKE_C_COMPILER_LAUNCHER="$(CCACHE)" -DCMAKE_CXX_COMPILER_LAUNCHER="$(CCACHE)") \
	$(CMAKE_ARGS)

.DEFAULT_GOAL := build
.PHONY: build configure reconfigure deps run deploy install uninstall clean distclean help

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build "$(BUILD_DIR)" --parallel $(JOBS)

# Configure only when the build dir is new; CMake re-runs itself after that
$(BUILD_DIR)/CMakeCache.txt:
	@test -f lib/settings/CMakeLists.txt || git submodule update --init --recursive
	cmake -S . -B "$(BUILD_DIR)" $(CMAKE_FLAGS)

configure: $(BUILD_DIR)/CMakeCache.txt

reconfigure:
	cmake -S . -B "$(BUILD_DIR)" $(CMAKE_FLAGS)

deps:
	brew install boost openssl@3 rapidjson cmake qt ninja ccache $(if $(filter ON On on,$(SPELLCHECK)),hunspell)
	git submodule update --init --recursive

run: build
	# -n: the browser may already be running this bundle as its native
	# messaging host, which would make a plain open fail with error -600
	open -n "$(APP)"

# Bundle Qt into a staged copy so the build tree stays incrementally buildable
deploy: build
	rm -rf "$(STAGED_APP)"
	mkdir -p "$(dir $(STAGED_APP))"
	ditto "$(APP)" "$(STAGED_APP)"
	"$(MACDEPLOYQT)" "$(STAGED_APP)"
	codesign --force --deep --sign - "$(STAGED_APP)"

install: deploy
	./scripts/install-macos.sh "$(STAGED_APP)" "$(PREFIX)"

uninstall:
	./scripts/install-macos.sh --uninstall "$(PREFIX)"

clean:
	@test ! -f "$(BUILD_DIR)/CMakeCache.txt" || cmake --build "$(BUILD_DIR)" --target clean
	rm -rf "$(dir $(STAGED_APP))"

distclean:
	rm -rf "$(BUILD_DIR)"

help:
	@echo "Targets:"
	@echo "  build (default)  configure if needed, then build $(APP)"
	@echo "  configure        run CMake once for $(BUILD_DIR)"
	@echo "  reconfigure      re-run CMake, e.g. after changing an option below"
	@echo "  deps             brew install the build dependencies and fetch submodules"
	@echo "  run              build, then open the app from the build tree"
	@echo "  deploy           bundle Qt into a self-contained, ad-hoc signed copy"
	@echo "  install          deploy, then replace $(PREFIX)/chatterino.app"
	@echo "  uninstall        remove $(PREFIX)/chatterino.app (settings are kept)"
	@echo "  clean            clean build outputs but keep the CMake cache"
	@echo "  distclean        delete $(BUILD_DIR) entirely"
	@echo ""
	@echo "Options (current value):"
	@echo "  CONFIG=$(CONFIG)  BUILD_DIR=$(BUILD_DIR)  PREFIX=$(PREFIX)  JOBS=$(JOBS)"
	@echo "  PLUGINS=$(PLUGINS)  SPELLCHECK=$(SPELLCHECK)  LTO=$(LTO)  UPDATER=$(UPDATER)"
	@echo "  QT_DIR=$(QT_DIR)"
	@echo "  CMAKE_ARGS='$(CMAKE_ARGS)'  extra flags appended to the CMake command"
