# Makefile for MemoryManager-VM-OSPager
# Automatically builds pager and test binaries

TEST_DIR := tests
TEST_SOURCES := $(wildcard $(TEST_DIR)/test*.cpp)
TESTS := $(patsubst $(TEST_DIR)/%.cpp,%,$(TEST_SOURCES))

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
	CC := clang++
	LIBVMAPP := libvm_app_macos.o
	LIBVMPAGER := libvm_pager_macos.o
else
	CC := g++
	LIBVMAPP := libvm_app.o
	LIBVMPAGER := libvm_pager.o
endif

CFLAGS := -g -Wall -fno-builtin -std=c++17

# Pager source files
PAGER_SOURCES := pager_internal.cpp vm_pager.cpp
PAGER_OBJS := $(PAGER_SOURCES:.cpp=.o)

.PHONY: all clean tests_only

all: pager $(TESTS)

pager: $(PAGER_OBJS) $(LIBVMPAGER)
	$(CC) $(CFLAGS) -o $@ $^

$(TESTS): %: $(TEST_DIR)/%.cpp $(LIBVMAPP)
	$(CC) $(CFLAGS) -I$(TEST_DIR) -I. -o $@ $^ -ldl

tests_only: $(TESTS)
	@echo "Compiled tests: $(TESTS)"

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f pager $(PAGER_OBJS)
	rm -f test*
	rm -f $(TEST_DIR)/output/*.out
	rm -rf $(TEST_DIR)/output/dysm/
	rm -rf *.dSYM
	@echo "Cleaned build files"
