# Convenience wrapper around the CMake build, aimed at local macOS/Homebrew dev.
# Everything is overridable, e.g. `make BUILD_TYPE=Release run`.

BUILD_DIR  ?= _build
BUILD_TYPE ?= Debug
JOBS       ?= $(shell sysctl -n hw.ncpu 2>/dev/null || echo 4)
QT_PREFIX  ?= $(shell brew --prefix qt 2>/dev/null)

APP  := $(BUILD_DIR)/bin/chatterino.app
BIN  := $(APP)/Contents/MacOS/chatterino

# Ninja is a lot faster here; fall back to make if it isn't installed.
GENERATOR ?= $(shell command -v ninja >/dev/null 2>&1 && echo Ninja || echo Unix Makefiles)

# Debug builds skip native-messaging host registration unless this is on
# (see src/singletons/NativeMessaging.cpp and Application::initNm).
CMAKE_ARGS ?= -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
              -DCMAKE_PREFIX_PATH=$(QT_PREFIX) \
              -DCHATTERINO_DEBUG_NATIVE_MESSAGES=ON

.DEFAULT_GOAL := build
.PHONY: deps check-qt configure build run test format clean help

## deps: install Homebrew dependencies and check out submodules
deps:
	brew install boost openssl@3 rapidjson cmake ninja qt
	git submodule update --init --recursive

# brew --prefix prints a path even for uninstalled formulae, so check the dir.
check-qt:
	@test -d "$(QT_PREFIX)" || { echo "qt not installed at '$(QT_PREFIX)' - run 'make deps'"; exit 1; }

$(BUILD_DIR)/CMakeCache.txt:
	@$(MAKE) check-qt
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" $(CMAKE_ARGS)

## configure: re-run cmake from scratch (after changing CMake options)
configure:
	rm -f $(BUILD_DIR)/CMakeCache.txt
	@$(MAKE) $(BUILD_DIR)/CMakeCache.txt

## build: compile the app (default target)
build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) -j $(JOBS)

## run: build, then launch with stdout/stderr attached to this terminal
run: build
	$(BIN)

## test: configure with tests enabled, build, run ctest
test:
	@$(MAKE) check-qt
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" $(CMAKE_ARGS) -DBUILD_TESTS=ON
	cmake --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

## format: run clang-format over the tree
format:
	./scripts/clang-format-all.sh

## clean: delete the build directory
clean:
	rm -rf $(BUILD_DIR)

## help: list targets
help:
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/^## /  /'
