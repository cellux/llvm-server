CC := clang
CXX := clang++
CXXFLAGS := -std=c++14
OPT := opt
LDFLAGS := -lLLVM

C_SOURCES := $(wildcard *.c)
CC_SOURCES := $(wildcard *.cc)
CPP_SOURCES := $(wildcard *.cpp)

OBJECTS := compiler.o

.PHONY: test
test: doctest
	@./doctest

doctest: $(OBJECTS) doctest.o
	$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(basename $(C_SOURCES) $(CC_SOURCES) $(CPP_SOURCES))
	rm -f *.o
	rm -f *.ll

%.ll: %.c
	$(CC) -S -emit-llvm -O0 -o $@ $<
	$(CC) -o $(basename $@) $@

%.ll: %.cc
	$(CXX) -S -emit-llvm -O0 -o $@ $<
	$(CXX) -o $(basename $@) $@
