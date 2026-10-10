## Makefile for memvar.h GoogleTest suite
## Assumes GoogleTest is installed system-wide (e.g. `apt install libgtest-dev`,
## or `brew install googletest`). No network access required.

CXX       ?= g++
CXXSTD    := -std=c++20
INCLUDES  := -I.
WARNINGS  := -Wall -Wextra
CXXFLAGS  ?= -O0 -g $(CXXSTD) $(WARNINGS) $(INCLUDES)
LDFLAGS   ?=
LDLIBS    := -lgtest -lgtest_main -lpthread

SRC_DIR   := .
BUILD_DIR := build
TARGET    := $(BUILD_DIR)/memvar_tests

SRCS      := $(SRC_DIR)/memvar_test.cpp
OBJS      := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DEPS      := $(OBJS:.o=.d)

.PHONY: all test clean coverage help

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	cp ../../memvar.h .

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

test: $(TARGET)
	./$(TARGET)

## Build with gcov/lcov instrumentation: `make coverage` then inspect
## build/*.gcda / build/*.gcno with lcov/gcovr.
coverage: CXXFLAGS += --coverage -O0
coverage: LDFLAGS  += --coverage
coverage: clean $(TARGET)
	./$(TARGET)
	@echo "Coverage data written to $(BUILD_DIR)/*.gcda / *.gcno"
	@echo "Generate a report with, e.g.: gcovr -r . --object-directory $(BUILD_DIR)"

clean:
	rm -rf $(BUILD_DIR)
	rm -f ./memvar.h

help:
	@echo "Targets:"
	@echo "  all       - build the test binary (default)"
	@echo "  test      - build and run all tests"
	@echo "  coverage  - build with --coverage instrumentation and run tests"
	@echo "  clean     - remove build artifacts"

-include $(DEPS)

