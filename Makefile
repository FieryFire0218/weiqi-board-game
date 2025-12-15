CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -O2 -Wall -Wextra -pedantic -MMD -MP
INCLUDES  := -Isrc

SRC       := src/main.cpp src/board.cpp src/stone.cpp
OUTDIR    := bin
OBJDIR    := $(OUTDIR)/obj
TARGET    := $(OUTDIR)/sfml-app

# Try pkg-config first, fall back to explicit libs
SFML_FLAGS := $(shell pkg-config --cflags --libs sfml-graphics sfml-window sfml-system 2>/dev/null)
ifeq ($(strip $(SFML_FLAGS)),)
  SFML_FLAGS := -lsfml-graphics -lsfml-window -lsfml-system
endif

OBJS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRC))
DEPS := $(OBJS:.o=.d)

.PHONY: all run clean debug

all: $(TARGET)

$(TARGET): $(OBJS) | $(OUTDIR)
	$(CXX) $(OBJS) -o $@ $(SFML_FLAGS)

$(OBJDIR)/%.o: src/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OUTDIR) $(OBJDIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS += -g -O0
debug: clean all

clean:
	rm -rf $(OBJDIR) $(TARGET)

-include $(DEPS)