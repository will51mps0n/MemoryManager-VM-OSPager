TEST_DIR = tests
TEST_SOURCES = $(wildcard $(TEST_DIR)/test*.cpp)
TESTS = $(patsubst $(TEST_DIR)/%.cpp,%,$(TEST_SOURCES))

$(info TEST_SOURCES = $(TEST_SOURCES))
$(info TESTS = $(TESTS))

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    CC=clang++
    CC+=-D_XOPEN_SOURCE
    LIBVMAPP=libvm_app_macos.o
    LIBVMPAGER=libvm_pager_macos.o
else
    CC=g++
    LIBVMAPP=libvm_app.o
    LIBVMPAGER=libvm_pager.o
endif

CC+=-g -Wall -fno-builtin -std=c++17

# List of source files for your pager
PAGER_SOURCES=pager_internal.cpp vm_pager.cpp 

# Generate the names of the pager's object files
PAGER_OBJS=${PAGER_SOURCES:.cpp=.o}

all: pager $(TESTS)

# Compile the pager and tag this compilation
pager: ${PAGER_OBJS} ${LIBVMPAGER}
	./autotag.sh push
	${CC} -o $@ $^

# Compile tests
$(TESTS): %: $(TEST_DIR)/%.cpp ${LIBVMAPP}
	${CC} -I$(TEST_DIR) -I. -o $@ $^ -ldl

tests_only: $(TESTS)
    @echo "Compiled tests: ${TESTS}"

# Generic rules for compiling a source file to an object file
%.o: %.cpp
	${CC} -c $<
%.o: %.cc
	${CC} -c $<

clean:
	rm -f pager pager_internal.o vm_pager.o

	rm -f test_*.4

	rm -f tests/output/*.out
	rm -rf tests/output/dysm/*

	rm -rf *.dSYM

	@echo "Cleaned pager and test outputs only"
