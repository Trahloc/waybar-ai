#!/bin/bash

# Script to disable non-essential CI workflows
# Run this script to disable all non-essential workflows

echo "🔧 Disabling non-essential CI workflows..."

# Function to add disable condition to workflow file
disable_workflow() {
    local file="$1"
    local after_line="$2"
    local search_pattern="$3"
    
    if [ -f "$file" ]; then
        echo "Disabling $file..."
        # Add the disable condition after the specified line
        sed -i "${after_line}a\\n# DISABLED: Only Arch Linux + Hyprland tests are required\n# To re-enable: remove the condition below\nif: false" "$file"
    else
        echo "Warning: $file not found"
    fi
}

# Disable Linux multi-distro tests
disable_workflow ".github/workflows/linux.yml" 3 "on: \[push, pull_request\]"

# Disable FreeBSD tests
disable_workflow ".github/workflows/freebsd.yml" 3 "on: \[push, pull_request\]"

# Disable Nix tests
disable_workflow ".github/workflows/nix-tests.yml" 5 "push:"

# Disable Docker builds
disable_workflow ".github/workflows/docker.yml" 8 "cron: '0 0 1 \* \*'"

# Disable Nix flake lock updates
disable_workflow ".github/workflows/nix-update-flake-lock.yml" 9 "flake.nix"

echo "✅ All non-essential CI workflows disabled!"
echo ""
echo "Disabled workflows:"
echo "  - Linux multi-distro tests (Alpine, Debian, Fedora, OpenSUSE, Gentoo)"
echo "  - FreeBSD tests"
echo "  - Nix flake tests and flake lock updates"
echo "  - Docker image builds"
echo ""
echo "Active workflows:"
echo "  - Arch Linux + Hyprland tests (ci-arch.yml)"
echo "  - clang-format linting"
echo "  - labeler"
echo ""
echo "To re-enable any workflow, remove the 'if: false' condition from the file."
