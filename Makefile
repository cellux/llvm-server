CXX = g++
CXXFLAGS = -std=c++14

llvm-tests: llvm-tests.cc
	$(CXX) $(CXXFLAGS) -o $@ -lLLVM $<
