# Branch Organization Guide

## 🌿 Overview

This guide explains the branch organization strategy for the autonomy project, where we separate infrastructure files from project code across two main branches.

## 📋 Branch Structure

### **Main Branch** (Infrastructure & Documentation)
Contains all infrastructure, CI/CD, and documentation files:

```
main/
├── .github/workflows/     # All GitHub Actions workflows
├── scripts/              # Build and deployment scripts
├── docs/                 # Documentation
├── configs/              # Configuration examples
├── README.md             # Main documentation
├── ARCHITECTURE.md       # System architecture
├── ROADMAP.md           # Project roadmap
├── STATUS.md            # Project status
├── PROJECT_INSTRUCTION.md # Project instructions
├── AUTONOMOUS_WORKFLOWS_*.md # Workflow documentation
├── github-todo.md       # GitHub tasks
├── azure/               # Azure deployment files
├── luci/                # LuCI interface files
├── package/             # Package definitions
├── uci-schema/          # UCI schema files
├── vuci-app-autonomy/   # VUCI app files
├── .gitignore           # Git ignore rules
├── .cursorinstructions  # Cursor instructions
└── .cursorrules         # Cursor rules
```

### **Main-Dev Branch** (Project Code)
Contains all project source code and development files:

```
main-dev/
├── pkg/                 # Go packages
├── cmd/                 # Application commands
├── test/                # Test files
├── go.mod               # Go module file
├── go.sum               # Go checksums
├── Makefile             # Build system
├── TODO.md              # Development tasks
└── etc/                 # Configuration files
```

## 🔄 Workflow

### **Development Workflow**

1. **Infrastructure Changes** (on `main` branch):
   - Update workflows, documentation, scripts
   - Changes automatically sync to `main-dev` via PR

2. **Project Code Changes** (on `main-dev` branch):
   - Update Go code, tests, dependencies
   - Changes automatically sync to `main` via PR

3. **Automatic Synchronization**:
   - GitHub Actions workflow monitors changes
   - Creates PRs for cross-branch synchronization
   - Auto-merges when appropriate

### **File Organization Rules**

#### **Main Branch Files** (Infrastructure)
- `.github/workflows/` - All GitHub Actions workflows
- `scripts/` - Build, deployment, and utility scripts
- `docs/` - All documentation files
- `configs/` - Configuration examples and templates
- `README.md` - Main project documentation
- `ARCHITECTURE.md` - System architecture documentation
- `ROADMAP.md` - Project roadmap and milestones
- `STATUS.md` - Current project status
- `PROJECT_INSTRUCTION.md` - Project setup instructions
- `AUTONOMOUS_WORKFLOWS_*.md` - Workflow documentation
- `github-todo.md` - GitHub tasks and TODO items
- `azure/` - Azure deployment configurations
- `luci/` - LuCI web interface files
- `package/` - Package definitions and build files
- `uci-schema/` - UCI configuration schemas
- `vuci-app-autonomy/` - VUCI application files
- `.gitignore` - Git ignore patterns
- `.cursorinstructions` - Cursor IDE instructions
- `.cursorrules` - Cursor IDE rules

#### **Main-Dev Branch Files** (Project Code)
- `pkg/` - Go packages and libraries
- `cmd/` - Application entry points
- `test/` - Test files and test infrastructure
- `go.mod` - Go module definition
- `go.sum` - Go dependency checksums
- `Makefile` - Build system and development tasks
- `TODO.md` - Development tasks and TODO items
- `etc/` - Configuration files

## 🚀 Setup Instructions

### **1. Run the Organization Script**

```bash
# Make the script executable
chmod +x scripts/organize-branches.sh

# Run the organization script
./scripts/organize-branches.sh
```

### **2. Push Both Branches**

```bash
# Push main branch (infrastructure)
git checkout main
git push origin main

# Push main-dev branch (project code)
git checkout main-dev
git push origin main-dev
```

### **3. Configure GitHub Settings**

#### **Branch Protection Rules**

1. Go to **Settings** → **Branches**
2. Add rule for `main` branch:
   - ✅ Require pull request reviews
   - ✅ Require status checks to pass
   - ✅ Restrict pushes to matching branches
   - ✅ Include administrators
3. Add rule for `main-dev` branch:
   - ✅ Require pull request reviews
   - ✅ Allow force pushes (for development)
   - ✅ Include administrators

#### **Default Branch Settings**

1. Go to **Settings** → **General** → **Default branch**
2. Set default branch to `main` (infrastructure)
3. This ensures new contributors see infrastructure first

## 🔄 Synchronization Workflow

### **Automatic Sync Process**

1. **Infrastructure Changes** (main-dev → main):
   - When infrastructure files change on `main-dev`
   - Workflow creates PR to `main` branch
   - Auto-merge when appropriate

2. **Project Changes** (main → main-dev):
   - When project files change on `main`
   - Workflow creates PR to `main-dev` branch
   - Auto-merge when appropriate

### **Manual Sync Commands**

```bash
# Sync infrastructure changes to main
gh workflow run sync-branches.yml --field sync_direction=dev-to-main

# Sync project changes to main-dev
gh workflow run sync-branches.yml --field sync_direction=main-to-dev

# Auto sync (detects direction)
gh workflow run sync-branches.yml --field sync_direction=auto
```

## 📊 Benefits

### **For Infrastructure Management**
- ✅ Clean separation of concerns
- ✅ Infrastructure changes don't affect development
- ✅ Easier to manage CI/CD and documentation
- ✅ Better version control for infrastructure

### **For Development**
- ✅ Focused development environment
- ✅ Cleaner git history for code changes
- ✅ Easier to manage dependencies
- ✅ Better isolation of project changes

### **For Collaboration**
- ✅ Clear ownership of different file types
- ✅ Easier code reviews
- ✅ Better project organization
- ✅ Reduced merge conflicts

## 🛠️ Development Workflow

### **Making Infrastructure Changes**

1. **Switch to main branch**:
   ```bash
   git checkout main
   ```

2. **Make your changes**:
   - Update workflows in `.github/workflows/`
   - Update documentation in `docs/`
   - Update scripts in `scripts/`

3. **Commit and push**:
   ```bash
   git add .
   git commit -m "🏗️ Update infrastructure: [description]"
   git push origin main
   ```

4. **Changes automatically sync** to `main-dev` via PR

### **Making Project Code Changes**

1. **Switch to main-dev branch**:
   ```bash
   git checkout main-dev
   ```

2. **Make your changes**:
   - Update Go code in `pkg/` and `cmd/`
   - Update tests in `test/`
   - Update dependencies in `go.mod`

3. **Commit and push**:
   ```bash
   git add .
   git commit -m "🚀 Update project code: [description]"
   git push origin main-dev
   ```

4. **Changes automatically sync** to `main` via PR

## 🔍 Monitoring and Maintenance

### **Check Sync Status**

```bash
# Check for open sync PRs
gh pr list --label "sync"

# Check workflow runs
gh run list --workflow=sync-branches.yml

# View sync workflow logs
gh run view --log [run-id]
```

### **Troubleshooting**

#### **Sync PR Not Created**
- Check if files changed in the correct categories
- Verify workflow permissions
- Check workflow logs for errors

#### **Sync PR Not Auto-Merging**
- Check branch protection rules
- Verify PR has required labels
- Check for merge conflicts

#### **Files in Wrong Branch**
- Use the organization script to move files
- Manually create PRs to move files
- Update the file lists in the workflow

## 📋 Best Practices

### **File Organization**
- ✅ Keep infrastructure and project files separate
- ✅ Use clear naming conventions
- ✅ Document file organization rules
- ✅ Regular cleanup and organization

### **Workflow Management**
- ✅ Test workflows before pushing
- ✅ Monitor sync process regularly
- ✅ Review auto-generated PRs
- ✅ Maintain workflow documentation

### **Collaboration**
- ✅ Communicate branch organization to team
- ✅ Use appropriate commit messages
- ✅ Review changes in both branches
- ✅ Keep documentation updated

## 🎯 Success Metrics

### **Organization Metrics**
- [ ] All files properly organized by branch
- [ ] No orphaned files in wrong branches
- [ ] Clear separation of concerns
- [ ] Easy navigation between branches

### **Workflow Metrics**
- [ ] Sync PRs created automatically
- [ ] Auto-merge working correctly
- [ ] No sync conflicts
- [ ] Fast sync process

### **Development Metrics**
- [ ] Faster development cycles
- [ ] Cleaner git history
- [ ] Easier code reviews
- [ ] Better project organization

---

**🌿 This branch organization strategy provides a clean separation between infrastructure and project code, making the autonomy project easier to manage and develop.**
