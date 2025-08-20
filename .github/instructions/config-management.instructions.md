---
applyTo: "**/config/**"
description: "Configuration management for RUTOS environment"
---

# Configuration Management

Use template-based configuration with migration support.

## Variable Pattern
```bash
VARIABLE_NAME="${VARIABLE_NAME:-default_value}"  # Description
```

## Template Rules
- Clean templates: No ShellCheck comments in template files
- Preserve user values during migration
- Create backups before modifications
- Separate structure validation from content validation

## Boolean Values
```bash
ENABLE_FEATURE="${ENABLE_FEATURE:-true}"  # Enable/disable (true/false)
```
