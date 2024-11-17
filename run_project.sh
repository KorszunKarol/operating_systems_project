#!/bin/bash

# Create necessary directories with proper permissions
mkdir -p files/test files/enigma files/harley
chmod 755 files/test files/enigma files/harley

# Create test files
echo "Example text content" > files/test/test1.txt
echo "Another text file" > files/test/test2.txt
touch files/test/media1.wav files/test/media2.jpg files/test/media3.png
touch files/test/sample.jpg files/test/audio.wav
chmod 644 files/test/*

# Kill any existing processes
pkill -f "bin/Gotham"
pkill -f "bin/Enigma"
pkill -f "bin/Harley"
pkill -f "bin/Fleck"

make clean
make

# Start processes in order
./bin/Gotham config/gotham_config.txt &
sleep 2

./bin/Enigma config/enigma_config.txt &
sleep 1

./bin/Harley config/harley_config.txt &
sleep 1

./bin/Fleck config/fleck_config.txt