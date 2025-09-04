#!/bin/sh
set -eu
OUT=/tmp/autonomy_list_stdout
AUTON=/usr/local/usr/libexec/rpcd/autonomy
BACKUP=/usr/local/usr/libexec/rpcd/autonomy.backup

[ -f "$AUTON" ] || { echo "autonomy not found"; exit 1; }
if [ ! -f "$BACKUP" ]; then cp -f "$AUTON" "$BACKUP"; fi
cat > "$AUTON" <<'WRAP'
#!/bin/sh
if [ "$1" = list ]; then
  /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@" | tee /tmp/autonomy_list_stdout
  exit 0
fi
exec /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@"
WRAP
chmod +x "$AUTON"
: > "$OUT" || true
sync
reboot
