# Git Hooks for waybar-ai

This directory contains git hooks to enforce code quality standards in the waybar-ai project.

## Quick Setup

Run the setup script to configure git hooks:

```bash
./setup-hooks.sh
```

## Available Hooks

### Pre-commit Hook

The pre-commit hook runs before each commit and performs the following checks:

- **clang-format formatting check** - Ensures C++ code follows the project's formatting standards
- **Basic C++ code quality checks** - Warns about TODO/FIXME comments and debug prints
- **File size warnings** - Alerts when files exceed 1000 lines

### Hook Modes

#### 1. Strict Mode (Default)
- Fails the commit if formatting issues are found
- Requires manual fixing of formatting issues
- Use: `cp .githooks/pre-commit .githooks/pre-commit-active`

#### 2. Auto-fix Mode
- Automatically fixes formatting issues
- Re-stages fixed files automatically
- Use: `cp .githooks/pre-commit-auto-fix .githooks/pre-commit-active`

## Manual Commands

### Format all C++ files
```bash
find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format --style=file -i
```

### Check formatting without fixing
```bash
find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format --style=file --dry-run --Werror
```

### Disable hooks temporarily
```bash
git config core.hooksPath .git/hooks
```

### Re-enable hooks
```bash
git config core.hooksPath .githooks
```

## Requirements

- `clang-format` (version 19+ recommended for consistency with CI)
- Bash shell

## Installation

The hooks are automatically installed when you run `./setup-hooks.sh`. The script will:

1. Create the `.githooks` directory
2. Configure git to use the custom hooks directory
3. Set up the appropriate pre-commit hook based on your preference

## Troubleshooting

### clang-format not found
Install clang-format on your system:
- **Arch Linux**: `sudo pacman -S clang-tools`
- **Ubuntu/Debian**: `sudo apt install clang-format`
- **macOS**: `brew install clang-format`

### Hook not running
Make sure the hook is executable:
```bash
chmod +x .githooks/pre-commit-active
```

### Bypass hooks (emergency only)
```bash
git commit --no-verify -m "emergency commit"
```

## Contributing

When contributing to this project, please ensure your code passes all pre-commit checks. The hooks help maintain code quality and consistency across the project.
