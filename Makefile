# Makefile for grids-jack
# This is a convenience wrapper around CMake

# Build directory
BUILD_DIR := build

# Binary name
BINARY := grids-jack

# CMake generator (can be overridden)
CMAKE_GENERATOR ?= Unix Makefiles

# Default target
.PHONY: all
all: check-deps build

# Check for required dependencies
.PHONY: check-deps
check-deps:
	@echo "Checking dependencies..."
	@command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake is not installed. Please install cmake."; exit 1; }
	@command -v pkg-config >/dev/null 2>&1 || { echo "ERROR: pkg-config is not installed. Please install pkg-config."; exit 1; }
	@pkg-config --exists jack || { echo "ERROR: JACK library (libjack) not found. Please install libjack-dev or jack-audio-connection-kit."; exit 1; }
	@pkg-config --exists sndfile || { echo "ERROR: libsndfile not found. Please install libsndfile1-dev or libsndfile."; exit 1; }
	@echo "All dependencies found."

# Configure CMake (only if needed)
.PHONY: configure
configure: check-deps
	@if [ ! -d "$(BUILD_DIR)" ]; then \
		echo "Configuring CMake..."; \
		cmake -S . -B $(BUILD_DIR) -G "$(CMAKE_GENERATOR)"; \
	fi

# Build the project
.PHONY: build
build: configure
	@echo "Building project..."
	@cmake --build $(BUILD_DIR)

# Run the binary
.PHONY: run
run: build
	@echo "Running $(BINARY)..."
	@./$(BUILD_DIR)/$(BINARY)

# Run tests using ctest
.PHONY: test
test: build
	@echo "Running tests with ctest..."
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Deep clean (removes all generated files)
.PHONY: distclean
distclean: clean
	@echo "Removing all generated files..."
	@find . -maxdepth 2 -name "CMakeCache.txt" -delete
	@find . -maxdepth 2 -type d -name "CMakeFiles" -prune -exec rm -rf {} + 2>/dev/null || true
	@find . -maxdepth 2 -name "cmake_install.cmake" -delete
	@echo "Deep clean complete."

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make          - Check dependencies and build the project (default)"
	@echo "  make build    - Build the project"
	@echo "  make run      - Build and run the project"
	@echo "  make test     - Run tests using ctest"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make distclean - Remove all generated files"
	@echo "  make check-deps - Check for required dependencies"
	@echo "  make help     - Show this help message"
