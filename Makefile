CXX = g++
CXXFLAGS = -std=c++14
LDFLAGS := -lLLVM

OBJECTS := \
	CompileContext.o

testrunner: $(OBJECTS) testrunner.o
	$(CXX) -o $@ $^ $(LDFLAGS)

llvm-tests: llvm-tests.cc
	$(CXX) $(CXXFLAGS) -o $@ -lLLVM $<
