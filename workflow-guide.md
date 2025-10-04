# Waybar Fork Development Workflow Guide

## 🚀 Quick Reference Commands

### Starting a New Feature
```bash
# 1. Get latest upstream changes
git fetch upstream
git checkout master
git merge upstream/master

# 2. Create feature branch
git checkout -b feature/your-feature-name

# 3. Work on your feature
# ... make changes ...

# 4. Commit changes
git add .
git commit -m "feat: descriptive commit message"

# 5. Push and create PR
git push origin feature/your-feature-name
gh pr create --title "feat: Your Feature Title" --body "Detailed description"
```

### Upstream Sync (Monthly)
```bash
# 1. Fetch latest upstream
git fetch upstream

# 2. Create sync branch
git checkout -b upstream-sync
git merge upstream/master

# 3. Test everything works
ninja -C build waybar
# Test your custom features

# 4. If good, merge to master
git checkout master
git merge upstream-sync
git push origin master

# 5. Clean up
git branch -d upstream-sync
```

### Emergency Recovery
```bash
# If master gets corrupted
git checkout master
git reset --hard upstream/master
git push origin master --force-with-lease
```

## 📋 Feature Development Checklist

### Before Starting
- [ ] Check if upstream has updates
- [ ] Ensure master is up to date
- [ ] Plan feature architecture

### During Development
- [ ] Follow waybar module patterns
- [ ] Use proper thread safety
- [ ] Add configuration options
- [ ] Implement error handling
- [ ] Add appropriate logging

### Before PR
- [ ] Code compiles without errors
- [ ] No compiler warnings
- [ ] All features tested
- [ ] Commit messages are descriptive
- [ ] Feature branch is up to date

### After PR Merge
- [ ] Delete feature branch
- [ ] Update documentation if needed
- [ ] Test merged code

## 🎯 Autohide Module Specific

### Configuration Options
```json
"autohide": {
  "threshold-hidden-y": 1,      // Mouse Y ≤ this shows waybar
  "threshold-visible-y": 50,    // Mouse Y > this hides waybar
  "delay-show": 0,              // Show delay in ms
  "delay-hide": 1000,           // Hide delay in ms
  "check-interval": 100         // Mouse check interval in ms
}
```

### Key Implementation Points
- Use `waybar::modules::hyprland::IPC` for mouse tracking
- Implement `EventHandler` interface for workspace changes
- Use state machine with `enum class WaybarState`
- Two-consecutive-events requirement for show trigger
- Per-monitor mouse position tracking
- Thread-safe GTK operations via `dp.emit()`

## 🔧 Build Commands

### Standard Build
```bash
ninja -C build waybar
```

### Full Rebuild
```bash
rm -rf build
meson setup build
ninja -C build waybar
```

### Test Build
```bash
./compile.sh
```

## 📚 Useful Git Commands

### Check Status
```bash
git status
git log --oneline -5
git branch -vv
```

### Compare with Upstream
```bash
git log --oneline master..upstream/master
git log --oneline upstream/master..master
git diff --name-only upstream/master master
```

### Clean Up
```bash
git branch -d old-feature-branch
git remote prune origin
```

## 🚨 Troubleshooting

### Build Fails
1. Check for missing dependencies
2. Clean build directory: `rm -rf build`
3. Reconfigure: `meson setup build`
4. Check compiler errors

### PR Won't Create
1. Ensure branch is pushed: `git push origin feature-name`
2. Check authentication: `gh auth status`
3. Verify branch exists on remote

### Merge Conflicts
1. Fetch latest: `git fetch upstream`
2. Rebase feature branch: `git rebase upstream/master`
3. Resolve conflicts
4. Force push: `git push origin feature-name --force-with-lease`

## 🎯 Best Practices

1. **Always work in feature branches**
2. **Keep master clean and up to date**
3. **Test after every upstream merge**
4. **Use descriptive commit messages**
5. **Follow waybar coding standards**
6. **Minimize resource usage**
7. **Implement proper error handling**
8. **Document complex logic**
