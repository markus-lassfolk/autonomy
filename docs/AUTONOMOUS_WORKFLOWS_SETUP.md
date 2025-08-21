# Autonomous Workflows Setup Guide

This guide provides complete setup instructions for all autonomous workflows implemented in the autonomy project.

## 🎯 Overview

The autonomy project now has **8 comprehensive autonomous workflows** that provide:

- **🔒 Security**: Automated scanning and protection
- **✨ Quality**: Consistent code formatting and validation  
- **🧪 Testing**: Multi-platform testing environments
- **🚀 Deployment**: Automated package building and publishing
- **🤖 AI Integration**: Copilot-powered issue resolution
- **📊 Monitoring**: Comprehensive metrics and reporting
- **📦 Dependencies**: Automated dependency management
- **📚 Documentation**: Auto-generated documentation

## 📋 Workflow Status

| Workflow | Status | File | Description |
|----------|--------|------|-------------|
| Security Scanning | ✅ Complete | `.github/workflows/security-scan.yml` | Secret detection, vulnerability scanning |
| Code Quality | ✅ Complete | `.github/workflows/code-quality.yml` | Auto-formatting, linting, quality checks |
| RUTOS Testing | ✅ Complete | `.github/workflows/rutos-test-environment.yml` | Multi-platform testing |
| Webhook Server | ✅ Complete | `scripts/webhook-server.go` | Production webhook receiver |
| Package Building | ✅ Complete | `.github/workflows/package-release.yml` | Multi-platform builds |
| Copilot Resolution | ✅ Complete | `.github/workflows/copilot-autonomous-fix.yml` | AI-powered issue fixes |
| Dependency Management | ✅ Complete | `.github/workflows/dependency-management.yml` | Automated dependency updates |
| Performance Monitoring | ✅ Complete | `.github/workflows/performance-monitoring.yml` | Performance benchmarking |
| Documentation | ✅ Complete | `.github/workflows/documentation.yml` | Auto-generated docs |

## 🔧 Setup Instructions

### 1. Required GitHub Secrets

Configure these secrets in your GitHub repository (Settings → Secrets and variables → Actions):

```bash
# Security & Authentication
WEBHOOK_SECRET=your-hmac-secret-key
GITHUB_TOKEN=your-github-personal-access-token
COPILOT_TOKEN=your-copilot-access-token

# Docker Publishing (for package builds)
DOCKERHUB_USERNAME=your-dockerhub-username
DOCKERHUB_TOKEN=your-dockerhub-access-token

# Optional: Custom domains for webhook
CUSTOM_DOMAIN=your-domain.com
SSL_CERT=your-ssl-certificate
SSL_KEY=your-ssl-private-key
```

### 2. Repository Variables

Set these variables in Settings → Secrets and variables → Actions → Variables:

```bash
# Webhook Configuration
SUPPORTED_VERSIONS=RUTX_R_00.07.17,RUTX_R_00.07.18,RUTX_R_00.08.00
MIN_SEVERITY=warn
COPILOT_ENABLED=true
AUTO_ASSIGN=true

# Build Configuration
BUILD_PLATFORMS=linux/amd64,linux/arm64,linux/arm/v7
DOCKER_REGISTRY=docker.io

# Performance Configuration
BENCHMARK_THRESHOLDS=memory:100mb,cpu:5%,network:100ms
```

### 3. GitHub Copilot Setup

1. **Enable GitHub Copilot** in your repository settings
2. **Configure Copilot rules** in `.github/copilot.yml`:

```yaml
# Copilot configuration for autonomous issue resolution
rules:
  - name: "Auto-fix autonomy issues"
    description: "Automatically fix common autonomyd issues"
    patterns:
      - "daemon_down"
      - "crash_loop"
      - "system_degraded"
    actions:
      - create_pull_request
      - run_tests
      - update_documentation
    allowed_files:
      - "pkg/**/*.go"
      - "cmd/**/*.go"
      - "scripts/**/*"
      - "docs/**/*.md"
```

### 4. Webhook Server Deployment

#### Option A: GitHub Actions (Recommended)

The webhook receiver is already configured to run via GitHub Actions. No additional deployment needed.

#### Option B: Self-hosted Server

1. **Build the webhook server**:
   ```bash
   go build -o webhook-server scripts/webhook-server.go
   ```

2. **Deploy to your server**:
   ```bash
   # Set environment variables
   export WEBHOOK_SECRET=your-secret
   export GITHUB_TOKEN=your-token
   export GITHUB_OWNER=your-username
   export GITHUB_REPO=autonomy
   
   # Run the server
   ./webhook-server
   ```

3. **Configure your autonomy clients** to use the webhook URL:
   ```ini
   # In /etc/autonomy/watch.conf
   WEBHOOK_URL=https://your-domain.com/webhook/starwatch
   WATCH_SECRET=your-secret-key
   ```

## 🚀 Workflow Usage

### Manual Triggering

```bash
# Security scan
gh workflow run security-scan.yml

# Code quality checks
gh workflow run code-quality.yml

# RUTOS testing
gh workflow run rutos-test-environment.yml --field test_type=full

# Package building
gh workflow run package-release.yml --field version=1.0.0

# Copilot issue resolution
gh workflow run copilot-autonomous-fix.yml --field issue_number=123

# Dependency updates
gh workflow run dependency-management.yml --field update_type=security

# Performance monitoring
gh workflow run performance-monitoring.yml --field benchmark_type=all

# Documentation generation
gh workflow run documentation.yml --field doc_type=all
```

### Automated Triggers

- **Security Scanning**: Runs on every push and PR
- **Code Quality**: Runs on every push and PR
- **RUTOS Testing**: Runs on every push and PR
- **Package Building**: Runs on tags and releases
- **Copilot Resolution**: Runs when issues are opened/labeled
- **Dependency Management**: Runs weekly (Mondays 9 AM UTC)
- **Performance Monitoring**: Runs daily (2 AM UTC)
- **Documentation**: Runs on code changes

## 📊 Monitoring & Analytics

### Workflow Analytics

Monitor workflow performance in GitHub Actions:

```bash
# View workflow runs
gh run list --workflow=security-scan.yml
gh run list --workflow=code-quality.yml
gh run list --workflow=package-release.yml

# View workflow logs
gh run view --log <run-id>

# Download artifacts
gh run download <run-id>
```

### Security Dashboard

Access security findings in GitHub Security tab:
- Code scanning alerts
- Secret scanning results
- Dependency vulnerabilities
- Security advisories

### Performance Metrics

Track performance improvements:
- Build times
- Test coverage
- Binary sizes
- Memory usage
- CPU utilization

## 🔍 Troubleshooting

### Common Issues

1. **Workflow Failures**
   ```bash
   # Check workflow status
   gh run list --status=failure
   
   # View detailed logs
   gh run view --log <run-id>
   ```

2. **Security Scan Issues**
   ```bash
   # Run security scan locally
   make security-scan
   
   # Check for secrets
   gitleaks detect --source .
   ```

3. **Build Issues**
   ```bash
   # Clean and rebuild
   make clean && make build
   
   # Check dependencies
   go mod tidy
   go mod verify
   ```

4. **Webhook Issues**
   ```bash
   # Test webhook locally
   curl -X POST http://localhost:8080/webhook/starwatch \
     -H "Content-Type: application/json" \
     -H "X-Starwatch-Signature: sha256=test" \
     -d '{"test":"payload"}'
   ```

### Performance Optimization

1. **Workflow Optimization**
   - Use caching for dependencies
   - Parallel job execution
   - Optimize build times

2. **Resource Management**
   - Monitor memory usage
   - Optimize CPU usage
   - Implement connection pooling

## 🎯 Success Metrics

### Workflow Performance
- [ ] Security scans complete in <5 minutes
- [ ] Code quality checks complete in <3 minutes
- [ ] Test environment builds complete in <10 minutes
- [ ] Package builds complete in <15 minutes

### Quality Metrics
- [ ] Zero critical security issues
- [ ] 100% test coverage maintained
- [ ] All code quality checks passing
- [ ] Successful deployments to all platforms

### Automation Goals
- [ ] 90% of issues automatically processed by Copilot
- [ ] 100% of code automatically formatted
- [ ] 100% of security issues automatically detected
- [ ] 100% of packages automatically built and published

## 📞 Support

### Documentation
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Security Scanning Tools](https://github.com/securecodewarrior/gosec)
- [Copilot Documentation](https://docs.github.com/en/copilot)

### Community
- Create issues in this repository for workflow problems
- Check existing issues for known solutions
- Review workflow logs for detailed error information

## 🎉 Conclusion

Your autonomy project now has a comprehensive set of autonomous workflows that provide:

- **🔒 Security**: Automated scanning and protection
- **✨ Quality**: Consistent code formatting and validation
- **🧪 Testing**: Multi-platform testing environments
- **🚀 Deployment**: Automated package building and publishing
- **🤖 AI Integration**: Copilot-powered issue resolution
- **📊 Monitoring**: Comprehensive metrics and reporting
- **📦 Dependencies**: Automated dependency management
- **📚 Documentation**: Auto-generated documentation

These workflows enable a fully autonomous development process where code quality, security, and deployment are handled automatically, allowing developers to focus on building features and improving the autonomy system.

---

**Next Steps**: 
1. Configure all secrets and variables
2. Test all workflows manually
3. Monitor workflow performance
4. Optimize based on usage patterns
5. Plan future enhancements
