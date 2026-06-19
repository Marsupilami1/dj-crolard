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

# Link the main executable
main: main.o message.o
	$(CXX) $(CXXFLAGS) main.o message.o -o $@ $(LDFLAGS)

# Build each .cc file
%.o : %.cc $(PCH_OUT)
	$(CXX) $(CXXFLAGS) -include-pch $(PCH_OUT) -c $< -o $@

clean:
	rm -f main $(PCH_OUT)
