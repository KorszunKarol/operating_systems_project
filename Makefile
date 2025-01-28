CC = gcc
CFLAGS = -Wall -Wextra -pedantic -D_GNU_SOURCE -Iinclude -MMD
LDFLAGS = -pthread -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Source files (excluding protocol.c since it's not needed)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

# Executables
EXECS = $(BIN_DIR)/Gotham $(BIN_DIR)/Fleck $(BIN_DIR)/Enigma $(BIN_DIR)/Harley

# Default target
all: directories $(EXECS)

# Create necessary directories
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link executables (without protocol.o dependency)
$(BIN_DIR)/Gotham: $(OBJ_DIR)/Gotham.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/network.o $(OBJ_DIR)/config.o $(OBJ_DIR)/string_utils.o $(OBJ_DIR)/shared.o $(OBJ_DIR)/logging.o
	$(CC) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/Fleck: $(OBJ_DIR)/Fleck.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/network.o $(OBJ_DIR)/config.o $(OBJ_DIR)/string_utils.o $(OBJ_DIR)/shared.o $(OBJ_DIR)/logging.o
	$(CC) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/Enigma: $(OBJ_DIR)/Enigma.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/network.o $(OBJ_DIR)/config.o $(OBJ_DIR)/string_utils.o $(OBJ_DIR)/shared.o $(OBJ_DIR)/worker.o $(OBJ_DIR)/logging.o
	$(CC) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/Harley: $(OBJ_DIR)/Harley.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/network.o $(OBJ_DIR)/config.o $(OBJ_DIR)/string_utils.o $(OBJ_DIR)/shared.o $(OBJ_DIR)/worker.o
	$(CC) $^ $(LDFLAGS) -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Include dependencies
-include $(DEPS)

.PHONY: all clean directories