#!/bin/bash

make clean
make

if [ $? -ne 0 ]; then
    echo "Build failed. Exiting."
    exit 1
fi

./bin/Gotham config/gotham_config.txt &

./bin/Enigma config/enigma_config.txt &

./bin/Harley config/harley_config.txt &

./bin/Fleck config/fleck_config.txt