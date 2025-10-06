# CI Workflow Disable Instructions

This repository currently runs CI tests for multiple operating systems, but you only need Arch Linux + Hyprland tests.

## To Disable Non-Essential CI Tests

Add `if: false` condition to these workflow files:

### 1. Disable Linux Multi-Distro Tests
File: `.github/workflows/linux.yml`
```yaml
name: linux

on: [push, pull_request]

# DISABLED: Only Arch Linux + Hyprland tests are required
# To re-enable: remove the condition below
if: false
```

### 2. Disable FreeBSD Tests
File: `.github/workflows/freebsd.yml`
```yaml
name: freebsd

on: [push, pull_request]

# DISABLED: Only Arch Linux + Hyprland tests are required
# To re-enable: remove the condition below
if: false
```

### 3. Disable Nix Tests
File: `.github/workflows/nix-tests.yml`
```yaml
name: "Nix-Tests"
on:
  pull_request:
  push:

# DISABLED: Only Arch Linux + Hyprland tests are required
# To re-enable: remove the condition below
if: false
```

## Workflows to Keep Active

- ✅ `.github/workflows/ci-arch.yml` - Arch Linux + Hyprland tests
- ✅ `.github/workflows/clang-format.yml` - Code formatting checks
- ✅ `.github/workflows/labeler.yml` - Issue labeling

## Benefits

- Reduces CI noise and build failures
- Focuses on your primary use case (Arch + Hyprland)
- Faster PR processing
- Easily re-enableable by removing the `if: false` conditions

## Note

The OAuth token used by this AI assistant doesn't have the `workflow` scope required to modify GitHub workflow files. You'll need to make these changes manually or with a token that has the appropriate permissions.
