CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -MMD -MP -Isrc
LDFLAGS ?=

BIN := truco
TEST_BIN := build/tests/runner

CORE_SRC := $(wildcard src/core/*.cpp)
AI_SRC := $(wildcard src/ai/*.cpp)
UI_SRC := $(wildcard src/ui/*.cpp)
APP_SRC := $(wildcard src/app/*.cpp)
NET_SRC := $(wildcard src/net/*.cpp)
COMMON_SRC := $(CORE_SRC) $(AI_SRC) $(UI_SRC) $(APP_SRC) $(NET_SRC)
MAIN_SRC := src/main.cpp
TEST_SRC := $(wildcard tests/*.cpp)

COMMON_OBJ := $(patsubst src/%.cpp,build/obj/%.o,$(COMMON_SRC))
MAIN_OBJ := build/obj/main.o
TEST_OBJ := $(patsubst tests/%.cpp,build/test/%.o,$(TEST_SRC))

all: $(BIN)

$(BIN): $(COMMON_OBJ) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_BIN): $(COMMON_OBJ) $(TEST_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/test/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build $(BIN)

.PHONY: all test run clean

-include $(shell find build -name '*.d' 2>/dev/null)
