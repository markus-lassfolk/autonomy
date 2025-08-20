---
applyTo: "**/*.sh"
description: "RUTOS shell compatibility rules for busybox environment"
---

# Shell Compatibility for RUTOS

Use POSIX sh only - NO bash syntax.
Target: busybox shell on RUTX50 router.

## Critical Rules
- NO arrays, [[]], function() syntax, local variables
- NO echo -e, source command, $'\n' syntax
- Use [ ] for conditions, printf for output
- Use . (dot) for sourcing files

## BusyBox Compatibility
- Commands may have limited options
- Use simple patterns, avoid complex regex
- Test on actual RUTOS environment when possible
