# Compiler and compiler flags
CXX = gcc
CXXFLAGS = -g -O3 -lm -std=c99 #-Wall -Wextra
SDIR = sources
ODIR = objects
SOURCES = $(wildcard $(SDIR)/*.c)
OBJECTS = $(patsubst $(SDIR)/%.c, $(ODIR)/%.o, $(SOURCES))
TARGET = sine

# Targets and rules
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(CXXFLAGS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(CXX) -c -o $@ $< $(CXXFLAGS)

clean:
	rm -rf $(ODIR)/* $(TARGET)
