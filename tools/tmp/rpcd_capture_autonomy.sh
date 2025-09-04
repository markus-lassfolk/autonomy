#!/bin/sh
set -eu
LOG=/tmp/rpcd_capture_autonomy.log
OUT=/tmp/autonomy_list_stdout

log(){ echo "$1" | tee -a "$LOG"; }

AUTON=/usr/local/usr/libexec/rpcd/autonomy
BACKUP=/usr/local/usr/libexec/rpcd/autonomy.backup

[ -f "$AUTON" ] || { echo "autonomy not found"; exit 1; }

cp -f "$AUTON" "$BACKUP"
log "Backed up autonomy to $BACKUP"

# Wrap autonomy to tee its stdout when called with list
cat > "$AUTON" <<'WRAP'
#!/bin/sh
if [ "$1" = list ]; then
  # Execute original (backup) and tee bytes into OUT; preserve exit code
  /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@" | tee /tmp/autonomy_list_stdout
  exit ${PIPESTATUS:-0}
fi
exec /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@"
WRAP
chmod +x "$AUTON"
log "Installed capture wrapper"

# Trigger discovery
: > "$OUT" || true
/etc/init.d/rpcd restart || true
sleep 5

log "Captured stdout (hexdump):"
if [ -s "$OUT" ]; then hexdump -C "$OUT" | head -200 | tee -a "$LOG"; else log "no stdout captured"; fi

# Restore
mv -f "$BACKUP" "$AUTON"
chmod +x "$AUTON" || true
/etc/init.d/rpcd restart || true
sleep 3
log "DONE"
