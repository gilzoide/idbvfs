EMAR ?= emar
EMCC ?= emcc
EMCXX ?= em++

AR = $(EMAR)
CC = $(EMCC)
CXX = $(EMCXX)
CXXFLAGS += -std=c++11 -Ilibs/sqlite-amalgamation -Ilibs/sqlite-vfs-cpp
BUILD_DIRS = build

$(BUILD_DIRS):
	mkdir -p $@

build/idbvfs.o: idbvfs.cpp | build
	$(CXX) -c -o $@ $< $(CXXFLAGS)

%/idbvfs.a: %/idbvfs.o | %
	$(AR) rc $@ $<

%/idbvfs.bc: idbvfs.cpp | %
	$(CXX) -c -emit-llvm -o $@ $< $(CXXFLAGS)

.PRECIOUS: build/idbvfs.o

static-lib: build/idbvfs.a
llvm-bitcode: build/idbvfs.bc
all: static-lib llvm-bitcode
