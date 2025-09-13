# Branch Setup Completion Guide

## 🎯 **Current Status**

✅ **Completed:**

- Branch organization script has run successfully
- Files have been organized between `main` and `main-dev` branches
- All changes have been committed locally

🔄 **Next Steps Required:**

## 📋 **Step 1: Push Both Branches**

### **Option A: Using Personal Access Token (Recommended)**

1. **Create a Personal Access Token:**
   - Go to GitHub → Settings → Developer settings → Personal access tokens → Tokens (classic)
   - Click "Generate new token (classic)"
   - Select scopes: `repo`, `workflow`, `write:packages`
   - Copy the token (you'll only see it once!)

2. **Push main-dev branch:**

   ```bash
   wsl git push origin main-dev
   # When prompted for username: your-github-username
   # When prompted for password: use your personal access token
   ```

3. **Switch to main branch and push:**

   ```bash
   wsl git checkout main
   wsl git push origin main
   # Use same credentials as above
   ```

### **Option B: Using GitHub CLI (Alternative)**

1. **Install GitHub CLI if not already installed:**

   ```bash
   # On Windows with winget
   winget install GitHub.cli
   
   # Or download from: https://cli.github.com/
   ```

2. **Authenticate with GitHub:**

   ```bash
   gh auth login
   # Follow the interactive prompts
   ```

3. **Push both branches:**

   ```bash
   wsl git push origin main-dev
   wsl git checkout main
   wsl git push origin main
   ```

## 🔧 **Step 2: Configure GitHub Repository Settings**

### **Branch Protection Rules**

1. **Go to your repository on GitHub:**
   - Navigate to `https://github.com/markus-lassfolk/autonomy`

2. **Set up branch protection for `main`:**
   - Go to **Settings** → **Branches**
   - Click **"Add rule"**
   - Branch name pattern: `main`
   - ✅ **Require pull request reviews before merging**
   - ✅ **Require status checks to pass before merging**
   - ✅ **Restrict pushes that create files that use the Git push options**
   - ✅ **Include administrators**
   - Click **"Create"**

3. **Set up branch protection for `main-dev`:**
   - Click **"Add rule"** again
   - Branch name pattern: `main-dev`
   - ✅ **Require pull request reviews before merging**
   - ✅ **Allow force pushes** (for development flexibility)
   - ✅ **Include administrators**
   - Click **"Create"**

### **Default Branch Settings**

1. **Set default branch to `main`:**
   - Go to **Settings** → **General** → **Default branch**
   - Change from current default to `main`
   - Click **"Update"**
   - Confirm the change

### **Repository Variables (Optional but Recommended)**

1. **Go to Settings** → **Secrets and variables** → **Actions** → **Variables**
2. **Add these variables:**
   - `SUPPORTED_VERSIONS`: `RUTX_R_00.07.17,RUTX_R_00.07.18,RUTX_R_00.08.00`
   - `MIN_SEVERITY`: `warn`
   - `COPILOT_ENABLED`: `true`
   - `AUTO_ASSIGN`: `true`
   - `BUILD_PLATFORMS`: `linux/amd64,linux/arm64,linux/arm/v7`
   - `DOCKER_REGISTRY`: `ghcr.io/markus-lassfolk`

## 🤖 **Step 3: Enable GitHub Copilot**

1. **Go to repository settings:**
   - Navigate to your repository settings

2. **Enable Copilot:**
   - Go to **Settings** → **Copilot**
   - Enable GitHub Copilot for this repository
   - Configure any specific rules if needed

## 🔄 **Step 4: Test the Synchronization Workflow**

### **Manual Test**

1. **Make a small change to infrastructure files on main-dev:**

   ```bash
   wsl git checkout main-dev
   echo "# Test infrastructure change" >> docs/BRANCH_SETUP_COMPLETION_GUIDE.md
   wsl git add docs/BRANCH_SETUP_COMPLETION_GUIDE.md
   wsl git commit -m "🧪 Test infrastructure change"
   wsl git push origin main-dev
   ```

2. **Check if sync PR is created:**
   - Go to your repository on GitHub
   - Look for a PR titled "🔄 Sync infrastructure changes from main-dev"
   - The workflow should have created this automatically

3. **Test project file changes:**

   ```bash
   wsl git checkout main
   echo "// Test project change" >> pkg/types.go
   wsl git add pkg/types.go
   wsl git commit -m "🧪 Test project change"
   wsl git push origin main
   ```

4. **Check if sync PR is created:**
   - Look for a PR titled "🔄 Sync project changes from main"

## 📊 **Step 5: Verify Branch Organization**

### **Check Branch Contents**

1. **Verify main branch (infrastructure):**

   ```bash
   wsl git checkout main
   wsl ls -la
   # Should see: .github/, scripts/, docs/, configs/, README.md, etc.
   # Should NOT see: pkg/, cmd/, go.mod, go.sum
   ```

2. **Verify main-dev branch (project code):**

   ```bash
   wsl git checkout main-dev
   wsl ls -la
   # Should see: pkg/, cmd/, test/, go.mod, go.sum, Makefile
   # Should NOT see: .github/, scripts/, docs/, configs/
   ```

## 🎯 **Step 6: Configure Secrets (For Full Workflow Functionality)**

### **Required Secrets**

Go to **Settings** → **Secrets and variables** → **Actions** → **Secrets**

1. **`WEBHOOK_SECRET`**: Generate a random secret for webhook validation
2. **`GITHUB_TOKEN`**: Usually auto-provided by GitHub
3. **`COPILOT_TOKEN`**: Your GitHub Copilot API token (if using)
4. **`DOCKERHUB_USERNAME`**: Your Docker Hub username
5. **`DOCKERHUB_TOKEN`**: Your Docker Hub access token

### **Generate Webhook Secret**

```bash
# In WSL, generate a random secret
wsl openssl rand -hex 32
# Copy the output and use it as WEBHOOK_SECRET
```

## ✅ **Success Criteria**

You'll know everything is working when:

- [ ] Both branches are pushed to GitHub
- [ ] Branch protection rules are active
- [ ] Default branch is set to `main`
- [ ] Sync workflow creates PRs automatically
- [ ] Infrastructure changes sync from main-dev to main
- [ ] Project changes sync from main to main-dev
- [ ] All autonomous workflows are functional

## 🚨 **Troubleshooting**

### **Push Issues**

- **Authentication failed**: Use personal access token instead of password
- **Permission denied**: Check repository permissions and branch protection rules

### **Sync Workflow Issues**

- **PR not created**: Check workflow permissions and repository variables
- **Auto-merge failed**: Verify branch protection rules allow auto-merge

### **Branch Protection Issues**

- **Can't push**: Ensure you have proper permissions or use PR workflow
- **Status checks failing**: Check that required workflows are passing

## 📞 **Support**

If you encounter issues:

1. Check the workflow logs in GitHub Actions
2. Review branch protection settings
3. Verify repository permissions
4. Check the sync workflow configuration

---

**🌿 Once completed, your repository will have a clean separation between infrastructure and project code, with automatic synchronization between branches!**
