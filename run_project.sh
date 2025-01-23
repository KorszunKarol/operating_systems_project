#!/bin/bash

# Create directories
mkdir -p files/test files/enigma files/harley

# Create test files if they don't exist
echo "Test file" > files/test/test.txt
touch files/test/media1.jpg
touch files/test/media1.wav

# Build project
make clean
make

# Run processes in separate terminals
gnome-terminal -- bash -c "./bin/Gotham config/gotham_config.txt; exec bash"
sleep 1
gnome-terminal -- bash -c "./bin/Enigma config/enigma_config.txt; exec bash"
sleep 1
gnome-terminal -- bash -c "./bin/Harley config/harley_config.txt; exec bash"
sleep 1
gnome-terminal -- bash -c "./bin/Fleck config/fleck_config.txt; exec bash"