#!/bin/bash

echo "=== Waybar Compile Script ==="

# Configure with meson
echo "Configuring with meson..."
if [ ! -d "build" ]; then
    meson setup build
else
    echo "Directory already configured."
    echo "Just run your build command (e.g. ninja) and Meson will regenerate as necessary."
    echo "Run \"meson setup --reconfigure\" to force Meson to regenerate."
    echo "WARNING: Running the setup command as \`meson [options]\` instead of \`meson setup [options]\` is ambiguous and deprecated."
fi

# Build with ninja
echo "Building with ninja..."
cd build || exit 1
ninja || exit 1

if [ $? -eq 0 ]; then
    echo "✅ Build successful! waybar executable created."
    echo "   Location: build/waybar"
    echo "=== Done! ==="
else
    echo "❌ Build failed!"
    exit 1
fi
