# Compiler to use
CXX = g++-14

# Compiler flags
CXXFLAGS = -Wall -Wextra -std=c++17 -g -I./include

# Target executable name
TARGET = udp_comms

# Source files
SRCS = receiver.cpp sender.cpp transmission.cpp utils.cpp entry.cpp

# Build directory for intermediate files
BUILD_DIR = build

# Object files (generated in the build directory)
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))

# Libraries to link (none explicitly needed for sys/socket.h)
# LIBS = 

# Default target to build the program
all: $(BUILD_DIR) $(TARGET)

# Ensure the build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile the source files into object files in the build directory
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Phony targets
.PHONY: all clean