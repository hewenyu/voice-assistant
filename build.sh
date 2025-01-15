#!/bin/bash

rm -rf build
# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make -j$(nproc)


# Return to original directory
cd .. 