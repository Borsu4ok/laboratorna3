CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BISON    ?= bison
FLEX     ?= flex

SRC   := src
BUILD := build
INC   := -I$(SRC) -I$(BUILD)

HEADERS := $(wildcard $(SRC)/*.hpp)
OBJS := $(BUILD)/parser.tab.o $(BUILD)/lexer.o $(BUILD)/ast.o $(BUILD)/semantic.o \
        $(BUILD)/interpreter.o $(BUILD)/codegen_js.o $(BUILD)/main.o

all: $(BUILD)/cparser

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/parser.tab.cpp $(BUILD)/parser.tab.hpp: $(SRC)/parser.y | $(BUILD)
	$(BISON) -d -Wall --report=state,itemset,lookahead,solved -o $(BUILD)/parser.tab.cpp $<

$(BUILD)/lexer.cpp: $(SRC)/lexer.l $(BUILD)/parser.tab.hpp | $(BUILD)
	$(FLEX) -o $@ $<

$(BUILD)/parser.tab.o: $(BUILD)/parser.tab.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -w $(INC) -c $< -o $@

$(BUILD)/lexer.o: $(BUILD)/lexer.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -w $(INC) -c $< -o $@

$(BUILD)/%.o: $(SRC)/%.cpp $(HEADERS) $(BUILD)/parser.tab.hpp
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(BUILD)/cparser: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: all
	./tests/run_tests.sh

theory: all
	./theory/run_theory.sh

serve: all
	python3 web/server.py

clean:
	rm -rf $(BUILD)

.PHONY: all test theory serve clean
