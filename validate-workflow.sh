#!/bin/bash

# Waybar Fork Workflow Validation Script
# Run this before creating PRs to ensure proper workflow

set -e

echo "🔍 Validating Waybar Fork Workflow..."

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "❌ Not in a git repository"
    exit 1
fi

# Check current branch
CURRENT_BRANCH=$(git branch --show-current)
echo "📍 Current branch: $CURRENT_BRANCH"

# Check if we're on a feature branch
if [[ $CURRENT_BRANCH != feature/* ]]; then
    echo "⚠️  Warning: Not on a feature branch. Consider using feature/name format"
fi

# Check if working tree is clean
if [ -n "$(git status --porcelain)" ]; then
    echo "❌ Working tree is not clean. Please commit or stash changes"
    git status --short
    exit 1
fi

# Check if branch is up to date with master
if [ "$CURRENT_BRANCH" != "master" ]; then
    echo "🔄 Checking if feature branch is up to date with master..."
    git fetch origin master
    BEHIND=$(git rev-list --count master..$CURRENT_BRANCH)
    AHEAD=$(git rev-list --count $CURRENT_BRANCH..master)
    
    if [ $AHEAD -gt 0 ]; then
        echo "⚠️  Warning: Feature branch is $AHEAD commits behind master"
        echo "   Consider: git checkout master && git merge upstream/master && git checkout $CURRENT_BRANCH && git rebase master"
    fi
fi

# Check if code compiles
echo "🔨 Checking if code compiles..."
if ! ninja -C build waybar > /dev/null 2>&1; then
    echo "❌ Code does not compile. Please fix compilation errors"
    exit 1
fi

# Check for common issues
echo "🔍 Checking for common issues..."

# Check for hardcoded values in autohide module
if grep -r "TODO\|FIXME\|XXX" src/modules/autohide.cpp > /dev/null 2>&1; then
    echo "⚠️  Warning: Found TODO/FIXME/XXX in autohide module"
fi

# Check for proper error handling
if ! grep -q "if (!bar_)" src/modules/autohide.cpp; then
    echo "⚠️  Warning: Missing null pointer check in autohide module"
fi

# Check for proper logging levels
if grep -q "spdlog::debug.*mouse" src/modules/autohide.cpp; then
    echo "⚠️  Warning: Mouse position logging should use trace level"
fi

# Check commit message format
echo "📝 Checking last commit message..."
LAST_COMMIT=$(git log -1 --pretty=%s)
if [[ $LAST_COMMIT =~ ^(feat|fix|refactor|docs|test|chore): ]]; then
    echo "✅ Commit message follows conventional format"
else
    echo "⚠️  Warning: Commit message should start with feat:, fix:, refactor:, docs:, test:, or chore:"
fi

# Check if branch is pushed
if [ "$CURRENT_BRANCH" != "master" ]; then
    echo "🌐 Checking if branch is pushed to remote..."
    if git ls-remote --heads origin $CURRENT_BRANCH | grep -q $CURRENT_BRANCH; then
        echo "✅ Branch is pushed to remote"
    else
        echo "❌ Branch is not pushed to remote. Run: git push origin $CURRENT_BRANCH"
        exit 1
    fi
fi

echo "✅ Workflow validation passed!"
echo ""
echo "🚀 Ready to create PR:"
echo "   gh pr create --title \"Your Title\" --body \"Your Description\""
echo ""
echo "📋 Or review manually at:"
echo "   https://github.com/Trahloc/waybar-ai/pull/new/$CURRENT_BRANCH"
