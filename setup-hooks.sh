#!/bin/bash

# Setup script for waybar-ai git hooks
# This script sets up pre-commit hooks for code quality enforcement

set -e

echo "🔧 Setting up git hooks for waybar-ai..."

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "❌ Error: Not in a git repository!"
    exit 1
fi

# Create .githooks directory if it doesn't exist
mkdir -p .githooks

echo "Choose your pre-commit hook style:"
echo "1) Strict mode (fails on formatting issues, requires manual fix)"
echo "2) Auto-fix mode (automatically fixes formatting issues)"
echo ""

# Verify required hook files exist
if [ ! -f ".githooks/pre-commit" ] || [ ! -f ".githooks/pre-commit-auto-fix" ]; then
    echo "❌ Error: Required hook files not found in .githooks/"
    exit 1
fi

read -p "Enter choice [1-2]: " choice

case $choice in
    1)
        echo "Setting up strict mode..."
        cp .githooks/pre-commit .githooks/pre-commit-active
        ;;
    2)
        echo "Setting up auto-fix mode..."
        cp .githooks/pre-commit-auto-fix .githooks/pre-commit-active
        ;;
    *)
        echo "Invalid choice. Using strict mode by default."
        cp .githooks/pre-commit .githooks/pre-commit-active
        ;;
esac

# Make sure the active pre-commit hook is executable
chmod +x .githooks/pre-commit-active

# Configure git to use our custom hooks directory
echo "Configuring git to use .githooks directory..."
git config core.hooksPath .githooks

echo "✅ Git hooks configured successfully!"
echo ""
echo "📋 The following checks will run before each commit:"
echo "   • clang-format formatting check"
if [ "$choice" = "2" ]; then
    echo "   • Auto-fix formatting issues"
fi
echo "   • Basic C++ code quality checks"
echo "   • File size warnings"
echo ""
echo "🔧 To manually run formatting on all C++ files:"
echo "   find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format --style=file -i"
echo ""
echo "🔄 To switch hook modes later, run this script again"
echo "🚀 Hooks are now active! Try making a commit to test them."
