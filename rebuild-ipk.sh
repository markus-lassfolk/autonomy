#!/bin/bash
set -euo pipefail

# Build proper ar-format IPKs for simple API/UI in SDK-RUTOS

SDK_DIR="/mnt/wsl/SDK/src/rutos-ipq40xx-rutx-sdk"
OUT_DIR="/mnt/wsl/SDK/work/fixed"
VER="1.0-9"
ARCH="arm_cortex-a7_neon-vfpv4"

API_SRC="$SDK_DIR/package/vuci-app-simple-api/files"
UI_SRC="$SDK_DIR/package/vuci-app-simple-ui/files"

mkdir -p "$OUT_DIR"

build_ipk() {
  local name="$1"; shift
  local depends="$1"; shift
  local src_usr_dir="$1"; shift
  local src_www_dir="${1:-}"

  local work
  work=$(mktemp -d)
  mkdir -p "$work/CONTROL" "$work/data"

  # Copy payload
  if [ -n "$src_usr_dir" ] && [ -d "$src_usr_dir" ]; then
    mkdir -p "$work/data/usr"
    ( cd "$src_usr_dir" && tar -cf - . ) | ( cd "$work/data/usr" && tar -xf - )
  fi
  if [ -n "$src_www_dir" ] && [ -d "$src_www_dir" ]; then
    mkdir -p "$work/data/www"
    ( cd "$src_www_dir" && tar -cf - . ) | ( cd "$work/data/www" && tar -xf - )
  fi

  # Control
  cat > "$work/CONTROL/control" <<EOF
Package: $name
Version: ${VER}
Depends: ${depends}
Section: vuci
Architecture: ${ARCH}
Installed-Size: 1024
Description: $name (manually assembled, proper ar ipk)
EOF

  if [ "$name" = "vuci-app-simple-ui" ]; then
    cat > "$work/CONTROL/postinst" <<'EOF'
#!/bin/sh
[ -z "${IPKG_INSTROOT}" ] && {
    mkdir -p /overlay/root/upper/usr/share/vuci/menu.d 2>/dev/null
    ln -sf /usr/local/usr/share/vuci/menu.d/simple.json /overlay/root/upper/usr/share/vuci/menu.d/simple.json 2>/dev/null
    mkdir -p /overlay/root/upper/www/simple 2>/dev/null
    if [ -f /usr/local/www/simple/index.html ]; then
      ln -sf /usr/local/www/simple/index.html /overlay/root/upper/www/simple/index.html 2>/dev/null
    fi
    /etc/init.d/uhttpd restart 2>/dev/null
}
exit 0
EOF
    chmod +x "$work/CONTROL/postinst"
  fi

  echo "2.0" > "$work/debian-binary"
  ( cd "$work/CONTROL" && tar -czf "$work/control.tar.gz" . )
  ( cd "$work/data"    && tar -czf "$work/data.tar.gz"    . )

  local ipk="$OUT_DIR/${name}_${VER}_${ARCH}.ipk"
  rm -f "$ipk"
  # Ensure LF endings and correct ownership in tars
  if [ -f "$work/CONTROL/control" ]; then
    sed -i 's/\r$//' "$work/CONTROL/control" || true
  fi
  if [ -f "$work/CONTROL/postinst" ]; then
    sed -i 's/\r$//' "$work/CONTROL/postinst" || true
  fi
  rm -f "$work/control.tar.gz" "$work/data.tar.gz"
  ( cd "$work/CONTROL" && tar --numeric-owner --owner=0 --group=0 -czf "$work/control.tar.gz" control postinst 2>/dev/null || tar --numeric-owner --owner=0 --group=0 -czf "$work/control.tar.gz" control )
  ( cd "$work/data"    && tar --numeric-owner --owner=0 --group=0 -czf "$work/data.tar.gz"    . )
  ( cd "$work" && ar cr "$(basename "$ipk")" debian-binary control.tar.gz data.tar.gz )
  mv "$work/$(basename "$ipk")" "$ipk"
  rm -rf "$work"
  echo "$ipk"
}

API_IPK=$(build_ipk vuci-app-simple-api "libc, lua" "$API_SRC/usr" )
UI_IPK=$(build_ipk vuci-app-simple-ui  "libc, vuci-app-simple-api" "$UI_SRC/usr" "$UI_SRC/www" )

ls -l "$API_IPK" "$UI_IPK"


