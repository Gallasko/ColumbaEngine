#!/bin/bash

# Fix WSL Build Environment for PgEngine
# Fixes missing build tools and repository issues

set -e

echo "=== Fixing WSL Build Environment ==="

# Fix repository issues
echo "Step 1: Fixing repository configuration..."
sudo sed -i '/bullseye-backports/s/^/#/' /etc/apt/sources.list
sudo rm -f /etc/apt/sources.list.d/*backports* 2>/dev/null || true

# Update package lists
echo "Step 2: Updating package lists..."
sudo apt update

# Install build dependencies
echo "Step 3: Installing build tools..."
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    gcc \
    g++ \
    git \
    pkg-config \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libx11-dev \
    libxext-dev \
    libasound2-dev \
    libpulse-dev \
    libudev-dev

echo "Step 4: Verifying installation..."
echo "CMake version: $(cmake --version | head -n1)"
echo "GCC version: $(gcc --version | head -n1)"
echo "Ninja version: $(ninja --version)"

echo "=== WSL Build Environment Fixed! ==="
echo "You can now run the PgEngine installation script."
