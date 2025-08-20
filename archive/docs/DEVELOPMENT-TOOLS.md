# Development Tools Setup

Version: 2.7.1

This directory contains setup scripts for installing code quality tools locally for the RUTOS Starlink Failover project.

## Quick Setup

### Linux/WSL/macOS

```bash
# Install all development tools
./scripts/setup-dev-tools.sh

# Check what tools are available
./scripts/setup-dev-tools.sh --check

# Install only Node.js tools (markdownlint, prettier)
./scripts/setup-dev-tools.sh --node-only

# Install only shell tools (shellcheck, shfmt)
./scripts/setup-dev-tools.sh --shell-only
```

### Windows PowerShell

```powershell
# Install all development tools
.\scripts\setup-dev-tools.ps1

# Check what tools are available
.\scripts\setup-dev-tools.ps1 -Check

# Install only Node.js tools
.\scripts\setup-dev-tools.ps1 -NodeOnly

# Install only shell tools (via WSL)
.\scripts\setup-dev-tools.ps1 -ShellOnly
```

## What Gets Installed

### Node.js Tools

- **markdownlint-cli**: Lints markdown files for formatting and style issues
- **prettier**: Formats markdown and code files consistently

### Shell Tools

- **shellcheck**: Validates shell scripts for common issues and POSIX compliance
- **shfmt**: Formats shell scripts consistently

### Configuration Files

- **package.json**: NPM dependencies and scripts
- **.markdownlint.json**: Markdownlint configuration
- **.prettierrc**: Prettier formatting configuration
- **.gitignore**: Updated with Node.js entries

## Usage After Setup

### Pre-commit Validation

```bash
# Full validation (all files)
./scripts/pre-commit-validation.sh --all

# Staged files only
./scripts/pre-commit-validation.sh --staged

# With debug output
DEBUG=1 ./scripts/pre-commit-validation.sh --staged
```

### Manual Tool Usage

#### Markdown

```bash
# Lint markdown files
markdownlint "**/*.md" --ignore node_modules

# Auto-fix markdown issues
markdownlint "**/*.md" --ignore node_modules --fix

# Format markdown with prettier
prettier --write "**/*.md" --ignore-path .gitignore

# Check markdown formatting
prettier --check "**/*.md" --ignore-path .gitignore
```

#### Shell Scripts

```bash
# Validate shell script
shellcheck scripts/example.sh

# Format shell script
shfmt -i 4 -ci -ln posix -w scripts/example.sh

# Check shell script formatting
shfmt -i 4 -ci -ln posix -d scripts/example.sh
```

### NPM Scripts (if Node.js tools installed)

```bash
# Lint and fix markdown
npm run lint:markdown

# Format markdown with prettier
npm run format:markdown

# Check all markdown formatting
npm run check:markdown
```

## Helper Scripts

After setup, the following helper scripts are available:

- `./scripts/validate-markdown.sh` - Validate all markdown files
- `./scripts/format-markdown.sh` - Auto-format all markdown files

## Prerequisites

### For Node.js Tools

- **Node.js** (v16 or later)
- **npm** (comes with Node.js)

Install Node.js:

- **Ubuntu/WSL**: `sudo apt install nodejs npm`
- **macOS**: `brew install node`
- **Windows**: Download from [nodejs.org](https://nodejs.org/) or `choco install nodejs`

### For Shell Tools

- **shellcheck**: Usually available via package manager
- **shfmt**: Can be installed via Go or package manager
- **Go** (optional, for installing shfmt)

## Troubleshooting

### Node.js Tools Not Found

If you get "command not found" errors after installation:

1. **Check npm global bin directory**:

   ```bash
   npm bin -g
   ```

2. **Add to PATH** (if needed):

   ```bash
   export PATH="$(npm bin -g):$PATH"
   ```

3. **Use npx** (alternative):

   ```bash
   npx markdownlint "**/*.md"
   npx prettier --check "**/*.md"
   ```

### Shell Tools in WSL

For Windows users, shell tools work best in WSL:

```powershell
# Install WSL (if not already installed)
wsl --install

# Run validation in WSL
wsl ./scripts/pre-commit-validation.sh --staged
```

### Permission Issues

If you get permission errors:

```bash
# Make scripts executable
chmod +x scripts/*.sh

# Fix npm permissions (if needed)
npm config set prefix ~/.local
export PATH="$HOME/.local/bin:$PATH"
```

## Integration with VS Code

The setup script configures tools that integrate well with VS Code:

### Recommended Extensions

- **markdownlint**: Real-time markdown linting
- **Prettier**: Auto-formatting on save
- **ShellCheck**: Shell script validation
- **shell-format**: Shell script formatting

### VS Code Settings

Add to your `.vscode/settings.json`:

```json
{
  "editor.formatOnSave": true,
  "markdownlint.config": {
    "extends": ".markdownlint.json"
  },
  "prettier.configPath": ".prettierrc"
}
```

## Continuous Integration

The pre-commit validation script runs automatically in GitHub Actions and can be used as a pre-commit hook:

```bash
# Install as pre-commit hook
ln -sf ../../scripts/pre-commit-validation.sh .git/hooks/pre-commit
```

---

For more information, see the main project [README.md](../README.md) and [TESTING.md](../TESTING.md).
