#!/bin/sh
set -eu
AUTON=/usr/local/usr/libexec/rpcd/autonomy
BACKUP=/usr/local/usr/libexec/rpcd/autonomy.backup
if [ -f "$BACKUP" ]; then mv -f "$BACKUP" "$AUTON"; chmod +x "$AUTON"; fi
/etc/init.d/rpcd restart || true
sleep 3
