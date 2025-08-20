---
applyTo: "**/*-rutos.sh"
description: "Color output formatting for RUTOS compatibility"
---

# RUTOS Color Formatting

Use Method 5 printf format for RUTOS compatibility.

## Correct Format (Shows colors in RUTOS)
```bash
printf "${RED}Error: %s${NC}\n" "$message"
printf "${GREEN}[INFO]${NC} %s\n" "$message"
```

## Wrong Format (Shows escape codes in RUTOS)
```bash
printf "%sError: %s%s\n" "$RED" "$message" "$NC"
```

## Color Detection
```bash
if [ -t 1 ] && [ "${TERM:-}" != "dumb" ] && [ "${NO_COLOR:-}" != "1" ]; then
    # Colors enabled
else
    # Colors disabled
fi
```
