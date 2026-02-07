# Makefile for grids-jack
# Provides convenient build, clean, test, and run targets

# Configuration
BUILD_DIR = build
EXECUTABLE = $(BUILD_DIR)/grids-jack

# Default target: build the project
.PHONY: all
all: check-deps
	@echo "Building grids-jack..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && $(MAKE)
	@echo "Build complete! Executable: $(EXECUTABLE)"

# Check for required dependencies
.PHONY: check-deps
check-deps:
	@echo "Checking for required dependencies..."
	@command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake is not installed. Please install cmake (version 3.10 or higher)."; exit 1; }
	@command -v pkg-config >/dev/null 2>&1 || { echo "ERROR: pkg-config is not installed. Please install pkg-config."; exit 1; }
	@pkg-config --exists jack 2>/dev/null || { echo "ERROR: JACK library not found. Please install libjack-jackd2-dev (Ubuntu/Debian) or equivalent."; exit 1; }
	@pkg-config --exists sndfile 2>/dev/null || { echo "ERROR: libsndfile not found. Please install libsndfile1-dev (Ubuntu/Debian) or equivalent."; exit 1; }
	@echo "All dependencies found!"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete!"

# Run tests using ctest
.PHONY: test
test: all
	@echo "Running tests with ctest..."
	@cd $(BUILD_DIR) && ctest --output-on-failure
	@echo "Tests complete!"

# Build and run the executable
.PHONY: run
run: all
	@echo "Running grids-jack..."
	@$(EXECUTABLE)

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make          - Build the project (default)"
	@echo "  make clean    - Remove all build artifacts"
	@echo "  make test     - Build and run tests using ctest"
	@echo "  make run      - Build and run grids-jack"
	@echo "  make check-deps - Check for required dependencies"
	@echo "  make help     - Show this help message"
