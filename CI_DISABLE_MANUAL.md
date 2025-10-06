# Manual CI Workflow Disable Instructions

Due to OAuth scope limitations, the workflow files cannot be modified directly. Here are the manual steps to disable the non-essential CI workflows:

## Files to Modify

### 1. `.github/workflows/linux.yml`
Add these lines after line 3 (`on: [push, pull_request]`):
```yaml
# DISABLED: Only Arch Linux + Hyprland tests are required
# To re-enable: remove the condition below
if: false
```

### 2. `.github/workflows/freebsd.yml`
Add these lines after line 3 (`on: [push, pull_request]`):
```yaml
# DISABLED: Only Arch Linux + Hyprland tests are required
# To re-enable: remove the condition below
if: false
```

### 3. `.github/workflows/nix-tests.yml`
Add these lines after line 5 (`push:`):
```yaml
# DISABLED: Only Arch Linux + Hyprland tests are required
# To re-enable: remove the condition below
if: false
```

## What This Does

- **Disables**: Linux multi-distro tests (Alpine, Debian, Fedora, OpenSUSE, Gentoo)
- **Disables**: FreeBSD tests
- **Disables**: Nix flake tests
- **Keeps**: Arch Linux + Hyprland tests (ci-arch.yml)
- **Keeps**: clang-format and labeler workflows

## Result

Only the essential tests will run:
- ✅ Arch Linux + Hyprland tests
- ✅ clang-format linting
- ✅ labeler
- ❌ All other OS/distro tests

## To Re-enable

Simply remove the `if: false` condition from any workflow file you want to re-enable.
