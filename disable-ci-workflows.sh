#!/bin/bash

# Script to disable non-essential CI workflows
# Run this script to disable Linux multi-distro, FreeBSD, and Nix tests
# Keep only Arch Linux + Hyprland tests (ci-arch.yml)

echo "🔧 Disabling non-essential CI workflows..."

# Disable Linux multi-distro tests
echo "Disabling Linux multi-distro tests..."
sed -i '/^on: \[push, pull_request\]$/a\\n# DISABLED: Only Arch Linux + Hyprland tests are required\n# To re-enable: remove the condition below\nif: false' .github/workflows/linux.yml

# Disable FreeBSD tests
echo "Disabling FreeBSD tests..."
sed -i '/^on: \[push, pull_request\]$/a\\n# DISABLED: Only Arch Linux + Hyprland tests are required\n# To re-enable: remove the condition below\nif: false' .github/workflows/freebsd.yml

# Disable Nix tests
echo "Disabling Nix tests..."
sed -i '/^  push:$/a\\n# DISABLED: Only Arch Linux + Hyprland tests are required\n# To re-enable: remove the condition below\nif: false' .github/workflows/nix-tests.yml

echo "✅ CI workflows disabled!"
echo ""
echo "Disabled workflows:"
echo "  - Linux multi-distro tests (Alpine, Debian, Fedora, OpenSUSE, Gentoo)"
echo "  - FreeBSD tests"
echo "  - Nix flake tests"
echo ""
echo "Active workflows:"
echo "  - Arch Linux + Hyprland tests (ci-arch.yml)"
echo "  - clang-format linting"
echo "  - labeler"
echo ""
echo "To re-enable any workflow, remove the 'if: false' condition from the file."