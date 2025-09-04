#!/bin/sh
set -eu
OUT=/overlay/autonomy_list_stdout
AUTON=/usr/local/usr/libexec/rpcd/autonomy
BACKUP=/usr/local/usr/libexec/rpcd/autonomy.backup

[ -f "$AUTON" ] || { echo "autonomy not found"; exit 1; }
# backup if not present
[ -f "$BACKUP" ] || cp -f "$AUTON" "$BACKUP"

# install wrapper that tees list output to /overlay
cat > "$AUTON" <<'WRAP'
#!/bin/sh
if [ "$1" = list ]; then
  /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@" | tee /overlay/autonomy_list_stdout
  exit 0
fi
exec /bin/sh /usr/local/usr/libexec/rpcd/autonomy.backup "$@"
WRAP
chmod +x "$AUTON"

: > "$OUT" || true

/etc/init.d/rpcd restart || true
sleep 5

# show capture
if [ -s "$OUT" ]; then
  echo "--- HEX ---"
  hexdump -C "$OUT" | head -120
  echo "--- TEXT ---"
  cat "$OUT"
else
  echo "no OUT captured"
fi

# restore original
mv -f "$BACKUP" "$AUTON"
chmod +x "$AUTON" || true
/etc/init.d/rpcd restart || true
sleep 2

echo "DONE"
