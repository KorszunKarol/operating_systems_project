CC = gcc
CFLAGS = -Wall -Wextra -pedantic -D_GNU_SOURCE -I$(INC_DIR)
LDFLAGS = -pthread -lm

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

$(shell mkdir -p $(BIN_DIR) $(OBJ_DIR))

# Add shared.o to common objects
COMMON_OBJS = $(OBJ_DIR)/utils.o $(OBJ_DIR)/network.o $(OBJ_DIR)/config.o \
              $(OBJ_DIR)/string_utils.o $(OBJ_DIR)/shared.o

EXECUTABLES = Fleck Gotham Enigma Harley

all: $(EXECUTABLES)

$(EXECUTABLES): %: $(OBJ_DIR)/%.o $(COMMON_OBJS)
	$(CC) $^ $(LDFLAGS) -o $(BIN_DIR)/$@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR) project.tar.gz project/

tar: clean
	mkdir -p project/files/test project/files/enigma project/files/harley
	cp -r src include config Makefile README.md run_project.sh project/
	tar -czf project.tar.gz project
	rm -rf project

.PHONY: all clean tar