#!/bin/sh
set -eu
LOG=/tmp/rpcd_capture_autonomy2.log
OUT=/tmp/autonomy_list_stdout
AUTON=/usr/local/usr/libexec/rpcd/autonomy
BACKUP=/usr/local/usr/libexec/rpcd/autonomy.backup

log(){ echo "$1" | tee -a "$LOG"; }

log "=== rpcd capture autonomy stdout === $(date)"
[ -f "$AUTON" ] || { log "autonomy not found"; exit 1; }
cp -f "$AUTON" "$BACKUP"
log "Backed up to $BACKUP"

cat > "$AUTON" <<'WRAP'
#!/bin/sh
if [ "$1" = list ]; then
  /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@" | tee /tmp/autonomy_list_stdout
  exit 0
fi
exec /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@"
WRAP
chmod +x "$AUTON"
log "Wrapper installed"

: > "$OUT" || true
/etc/init.d/rpcd restart || true
sleep 5
log "Captured (hexdump):"
if [ -s "$OUT" ]; then hexdump -C "$OUT" | head -80 | tee -a "$LOG"; else log "no stdout captured"; fi

mv -f "$BACKUP" "$AUTON"
chmod +x "$AUTON" || true
/etc/init.d/rpcd restart || true
sleep 2
log "RESTORED"
