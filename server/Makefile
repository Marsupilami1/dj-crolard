CXX = clang++
CXXFLAGS = -O3 -march=native -std=c++23 -Wall -DCROW_ENABLE_DEBUG -DCROW_ENABLE_COMPRESSION
LDFLAGS = -lpthread -lz -lcpr

# Precompiled header stuff
PCH_SRC = crow_all.h
PCH_OUT = crow_all.h.pch

# Targets
all: main

# Build the Precompiled Header
$(PCH_OUT): $(PCH_SRC)
	$(CXX) $(CXXFLAGS) -x c++-header $(PCH_SRC) -o $(PCH_OUT)

# Build the main executable
main: main.cc $(PCH_OUT)
	$(CXX) $(CXXFLAGS) -include-pch $(PCH_OUT) main.cc -o $@ $(LDFLAGS)

clean:
	rm -f main $(PCH_OUT)
