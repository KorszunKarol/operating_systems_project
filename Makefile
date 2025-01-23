CC = gcc
CFLAGS = -Wall -Wextra -pedantic -D_GNU_SOURCE -I$(INC_DIR)
LDFLAGS = -pthread -lm

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

$(shell mkdir -p $(BIN_DIR) $(OBJ_DIR))

# Common objects for all executables
COMMON_OBJS = $(OBJ_DIR)/utils.o $(OBJ_DIR)/network.o $(OBJ_DIR)/config.o \
              $(OBJ_DIR)/string_utils.o $(OBJ_DIR)/shared.o

# Worker objects (needed by Enigma and Harley)
WORKER_OBJS = $(COMMON_OBJS) $(OBJ_DIR)/worker.o

EXECUTABLES = Fleck Gotham Enigma Harley

all: $(EXECUTABLES)

# Special rules for worker-based executables
Enigma Harley: %: $(OBJ_DIR)/%.o $(WORKER_OBJS)
	$(CC) $^ $(LDFLAGS) -o $(BIN_DIR)/$@

# Rule for non-worker executables
Fleck Gotham: %: $(OBJ_DIR)/%.o $(COMMON_OBJS)
	$(CC) $^ $(LDFLAGS) -o $(BIN_DIR)/$@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR) project.tar.gz project/

.PHONY: all clean