# Makefile untuk Arion Lexer
# IF2224 - Teori Bahasa Formal dan Automata
#
# Usage:
#   make          → build lexer
#   make run      → build dan jalankan dengan test input
#   make clean    → hapus file hasil build

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
SRCDIR   = src
BUILDDIR = build
BINDIR   = bin
TARGET   = $(BINDIR)/lexer

# Daftar source files
SOURCES  = $(SRCDIR)/main.cpp \
           $(SRCDIR)/dfa.cpp \
           $(SRCDIR)/lexer.cpp \
           $(SRCDIR)/utils.cpp

# Object files
OBJECTS  = $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# Default target: build lexer
all: $(TARGET)

# Link object files jadi executable
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile tiap .cpp ke .o
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Buat build directory jika belum ada
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Jalankan dengan test input
run: $(TARGET)
	./$(TARGET) test/milestone-1/input-1.txt test/milestone-1/output-1.txt

# Hapus file hasil build
clean:
	rm -rf $(BUILDDIR) $(TARGET)

.PHONY: all run clean
