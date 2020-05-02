.DEFAULT_GOAL := app

CC := clang
CXX := clang++
CXXFLAGS := -std=c++14 -DBOOST_ASIO_DISABLE_THREADS
LDFLAGS := -lLLVM -lpthread

OBJECTS := compiler.o server.o
APP_OBJECTS := $(OBJECTS) main.o
TEST_OBJECTS := \
  $(OBJECTS) \
  $(patsubst %.cpp,%.o,$(wildcard *_test.cpp)) \
  doctest.o

compiler.o: compiler.cpp compiler.h
server.o: server.cpp server.h compiler.h
main.o: main.cpp server.h
doctest.o: doctest.cpp doctest.h

.PHONY: app
app: llvm-server

llvm-server: $(APP_OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY: test
test: doctest
	@./doctest

doctest: $(TEST_OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

C_SOURCES := $(wildcard *.c)
CC_SOURCES := $(wildcard *.cc)
CPP_SOURCES := $(wildcard *.cpp)

.PHONY: clean
clean:
	rm -f $(basename $(C_SOURCES) $(CC_SOURCES) $(CPP_SOURCES))
	rm -f *.o

%.ll: %.c
	$(CC) -S -emit-llvm -O0 -o $@ $<
	$(CC) -o $(basename $@) $@

%.ll: %.cc
	$(CXX) -S -emit-llvm -O0 -o $@ $<
	$(CXX) -o $(basename $@) $@
