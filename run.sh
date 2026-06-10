#!/bin/bash

# Configuration
BUILD_DIR="build/native"
EXECUTABLE="$BUILD_DIR/fractory"

# Check if the build exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable not found at $EXECUTABLE. Running build.sh first..."
    ./build.sh
fi

# Run the program
echo "Running fractory..."
"$EXECUTABLE"
