CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_DEFAULT_SOURCE

# Source files and object files
FLECK_SRC = Fleck.c utils.c config.c
GOTHAM_SRC = Gotham.c utils.c config.c
ENIGMA_SRC = Enigma.c utils.c config.c
HARLEY_SRC = Harley.c utils.c config.c

FLECK_OBJ = $(FLECK_SRC:.c=.o)
GOTHAM_OBJ = $(GOTHAM_SRC:.c=.o)
ENIGMA_OBJ = $(ENIGMA_SRC:.c=.o)
HARLEY_OBJ = $(HARLEY_SRC:.c=.o)

# Executables
TARGETS = Fleck Gotham Enigma Harley

all: $(TARGETS)

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Linking rules for each executable
Fleck: $(FLECK_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

Gotham: $(GOTHAM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

Enigma: $(ENIGMA_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

Harley: $(HARLEY_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
