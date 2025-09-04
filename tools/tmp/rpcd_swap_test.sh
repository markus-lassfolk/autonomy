#!/bin/sh
set -eu
LOG=/tmp/rpcd_swap_test.log

log() { echo "$1" | tee -a "$LOG"; }

log "=== rpcd secondary plugin swap test === $(date)"

AUTON=/usr/local/usr/libexec/rpcd/autonomy
MWAN=/usr/local/usr/libexec/rpcd/mwan3
BACKUP=/usr/local/usr/libexec/rpcd/mwan3.backup

[ -f "$AUTON" ] || { log "autonomy plugin missing"; exit 1; }
[ -f "$MWAN" ] || { log "mwan3 plugin missing"; exit 1; }
chmod +x "$AUTON" || true

cp -f "$MWAN" "$BACKUP"
log "Backed up mwan3 to $BACKUP"

cat > "$MWAN" <<'WRAP'
#!/bin/sh
echo "$(date) [$$] args: $@" >> /tmp/mwan3_autonomy.log
exec /usr/local/usr/libexec/rpcd/autonomy "$@"
WRAP
chmod +x "$MWAN"
log "Installed wrapper as mwan3"

/etc/init.d/rpcd restart || true
sleep 5
log "UBUS after swap:"
ubus list | grep -E 'mwan3|autonomy' | tee -a "$LOG" || true
log "Exec log (/tmp/mwan3_autonomy.log):"
if [ -f /tmp/mwan3_autonomy.log ]; then cat /tmp/mwan3_autonomy.log | tee -a "$LOG"; else log "no exec log"; fi

mv -f "$BACKUP" "$MWAN"
chmod +x "$MWAN" || true
/etc/init.d/rpcd restart || true
sleep 3
log "UBUS after restore:"
ubus list | grep -E 'mwan3|autonomy' | tee -a "$LOG" || true

log "DONE"
