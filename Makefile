# Makefile for OpenGL Gravity Simulator on macOS

CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O1 -DGL_SILENCE_DEPRECATION \
           -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
# Homebrew paths
BREW_PREFIX = /opt/homebrew
INCLUDE_DIRS = -I$(BREW_PREFIX)/include \
               -I$(BREW_PREFIX)/opt/glfw/include \
               -I$(BREW_PREFIX)/opt/glew/include \
               -I$(BREW_PREFIX)/opt/glm/include

LIBRARY_DIRS = -L$(BREW_PREFIX)/lib \
               -L$(BREW_PREFIX)/opt/glfw/lib \
               -L$(BREW_PREFIX)/opt/glew/lib

# Libraries to link
LIBS = -lglfw -lglew -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo \
       -fsanitize=address -fsanitize=undefined
# Source files
SOURCES_GRAVITY = gravity_sim.cpp
SOURCES_3DGRID = gravity_sim_3Dgrid.cpp  
SOURCES_3DTEST = 3D_test.cpp

# Output executables
TARGET_GRAVITY = gravity_sim
TARGET_3DGRID = gravity_sim_3Dgrid
TARGET_3DTEST = 3D_test

# Default target
all: $(TARGET_GRAVITY) $(TARGET_3DGRID) $(TARGET_3DTEST)

# Build gravity simulator
$(TARGET_GRAVITY): $(SOURCES_GRAVITY)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIBRARY_DIRS) -o $@ $< $(LIBS)

# Build 3D grid gravity simulator
$(TARGET_3DGRID): $(SOURCES_3DGRID)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIBRARY_DIRS) -o $@ $< $(LIBS)

# Build 3D test
$(TARGET_3DTEST): $(SOURCES_3DTEST)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIBRARY_DIRS) -o $@ $< $(LIBS)

# Clean build artifacts
clean:
	rm -f $(TARGET_GRAVITY) $(TARGET_3DGRID) $(TARGET_3DTEST)
	rm -rf *.dSYM

# Run the main gravity simulator
run: $(TARGET_GRAVITY)
	./$(TARGET_GRAVITY)

# Run the 3D grid version
run-3dgrid: $(TARGET_3DGRID)
	./$(TARGET_3DGRID)

# Run the 3D test
run-3dtest: $(TARGET_3DTEST)
	./$(TARGET_3DTEST)

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: all

.PHONY: all clean run run-3dgrid run-3dtest debug
