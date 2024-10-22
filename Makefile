# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -D_GNU_SOURCE -I$(INC_DIR)
LDFLAGS = -pthread -lm

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Create directories if they don't exist
$(shell mkdir -p $(BIN_DIR) $(OBJ_DIR))

# Source files and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)
EXECUTABLES = Fleck Gotham Enigma Harley

# Default target
all: $(EXECUTABLES)

# Pattern rule for executables
$(EXECUTABLES): %: $(OBJ_DIR)/%.o $(OBJ_DIR)/utils.o
	$(CC) $^ $(LDFLAGS) -o $(BIN_DIR)/$@

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# Include dependencies
-include $(DEPS)

# Clean build files
clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

# Create tar file for submission
tar: clean
	tar -czf project.tar.gz src include Makefile README.md config/*.txt

.PHONY: all clean tar
