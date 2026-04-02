CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
SRCDIR   = src
BUILDDIR = build
BINDIR   = bin

# Detect OS
ifeq ($(OS),Windows_NT)
    SHELL  = cmd.exe
    TARGET = $(BINDIR)/lexer.exe
    MKDIR  = if not exist $(BUILDDIR) mkdir $(BUILDDIR)
    MKDIR_BIN = if not exist $(BINDIR) mkdir $(BINDIR)
    RM     = rmdir /s /q
    RUN    = $(TARGET)
else
    TARGET = $(BINDIR)/lexer
    MKDIR  = mkdir -p $(BUILDDIR)
    MKDIR_BIN = mkdir -p $(BINDIR)
    RM     = rm -rf
    RUN    = ./$(TARGET)
endif

# Sources
SOURCES  = $(SRCDIR)/main.cpp \
           $(SRCDIR)/dfa.cpp \
           $(SRCDIR)/lexer.cpp \
           $(SRCDIR)/utils.cpp

OBJECTS  = $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# Default
all: $(TARGET)

# Link
$(TARGET): $(OBJECTS)
	@$(MKDIR_BIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@$(MKDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Run
run: $(TARGET)
	$(RUN) milestone-1/input-1.txt milestone-1/output-1.txt

# Clean
clean:
	-$(RM) $(BUILDDIR)
	-$(RM) $(BINDIR)

.PHONY: all run clean