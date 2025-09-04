#!/usr/bin/env bash
# Teltonika RutOS SDK setup (WSL, checkpointed, RUTX50 auto, examples as modules)
# Usage:
#   ./rutos-wsl-setup.sh
#   FORCE=1 ./rutos-wsl-setup.sh                 # force all steps
#   RESET_FROM=feeds_update ./rutos-wsl-setup.sh # redo from a step onward
set -euo pipefail

# ===== User-tweakables =======================================================
SDK_DIR="/mnt/wsl/SDK"
SDK_URL="${SDK_URL:-https://firmware.teltonika-networks.com/7.17.1/RUTX/RUTX_R_GPL_00.07.17.1.tar.gz}"
DL_DIR="$SDK_DIR/dl"
WORK_DIR="$SDK_DIR/work"
JOBS="${JOBS:-$(nproc)}"
# ============================================================================

# ----- checkpoint machinery --------------------------------------------------
CKPT_DIR="$WORK_DIR/.checkpoints"; mkdir -p "$CKPT_DIR"
STEP_ORDER=(deps download verify extract locate patch_meta feeds_update select_target examples_conf toolchain build_examples)

mark_done(){ touch "$CKPT_DIR/$1.done"; }
is_done(){ [[ -f "$CKPT_DIR/$1.done" ]]; }
reset_from(){
  local found=0
  for s in "${STEP_ORDER[@]}"; do
    if [[ "$s" == "$1" ]]; then found=1; fi
    if (( found )); then rm -f "$CKPT_DIR/$s.done"; fi
  done
}
if [[ -n "${RESET_FROM:-}" ]]; then
  echo ">>> RESET_FROM=$RESET_FROM — clearing later checkpoints"
  reset_from "$RESET_FROM"
fi

run_step(){
  local id="$1"; shift
  local desc="$*"
  if [[ -n "${FORCE:-}" ]]; then rm -f "$CKPT_DIR/$id.done"; fi
  if is_done "$id"; then
    echo "=== [SKIP] $desc (checkpoint: $id)"
    return 1
  fi
  echo "=== [RUN ] $desc"
}

say(){ printf '\n\033[1;36m%s\033[0m\n' "$*"; }

# ----- sanity ---------------------------------------------------------------
say "Teltonika RutOS SDK setup (checkpointed)"
mkdir -p "$SDK_DIR" "$DL_DIR" "$WORK_DIR"
touch "$SDK_DIR/.rwtest" && rm -f "$SDK_DIR/.rwtest" || { echo "ERROR: $SDK_DIR not writable"; exit 1; }
if [[ "$(id -u)" == "0" ]]; then echo "Do NOT run as root. Use your normal WSL user."; exit 1; fi

# ===== deps =================================================================
if run_step deps "Install host build dependencies (once)"; then
  sudo apt-get update -y
  sudo apt-get install -y \
    binutils binutils-gold bison build-essential bzip2 ca-certificates curl cmake \
    default-jdk device-tree-compiler devscripts file flex g++ gawk gcc gettext git \
    gnupg gperf help2man jq libc6-dev libffi-dev libexpat1-dev libncurses-dev \
    libpcre3-dev libsqlite3-dev libssl-dev libxml-parser-perl lz4 liblz4-dev \
    libzstd-dev make patch pkg-config psmisc python-is-python3 python3 python3-dev \
    python3-setuptools python3-yaml rsync ruby sharutils subversion swig \
    u-boot-tools unzip uuid-dev vim-common wget zip zlib1g-dev time
  # Node 20 (for Vuci builds); safe to re-run
  if ! command -v node >/dev/null 2>&1 || ! node -v | grep -qE '^v20\.' ; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt-get install -y nodejs
  fi
  node -v && npm -v || true
  mark_done deps
fi

# ===== download =============================================================
TARBALL="$DL_DIR/$(basename "$SDK_URL")"
if run_step download "Download SDK tarball → $TARBALL"; then
  [[ -f "$TARBALL" ]] || curl -L --fail -o "$TARBALL" "$SDK_URL"
  mark_done download
fi

# ===== verify (best-effort; skips if no .md5 published) =====================
if run_step verify "Verify SDK tarball MD5 (if provided by Teltonika)"; then
  if curl -fsI "${SDK_URL}.md5" >/dev/null 2>&1 ; then
    curl -fsSL "${SDK_URL}.md5" -o "$TARBALL.md5"
    (cd "$DL_DIR" && md5sum -c "$(basename "$TARBALL").md5")
  else
    echo "No MD5 file published; skipping integrity check."
  fi
  mark_done verify
fi

# ===== extract ==============================================================
SRC_ROOT="$SDK_DIR/src"
if run_step extract "Extract SDK into $SRC_ROOT"; then
  rm -rf "$SRC_ROOT"; mkdir -p "$SRC_ROOT"
  tar -xzf "$TARBALL" -C "$SRC_ROOT"
  mark_done extract
fi

# ===== locate build root =====================================================
if run_step locate "Locate SDK top (contains scripts/feeds)"; then
  SDK_TOP="$(find "$SRC_ROOT" -maxdepth 6 -type f -path '*/scripts/feeds' -printf '%h\n' | head -n1 || true)"
  if [[ -z "${SDK_TOP:-}" ]]; then echo "ERROR: Could not find scripts/feeds in SDK"; exit 1; fi
  echo "$SDK_TOP" > "$WORK_DIR/SDK_TOP.path"
  mark_done locate
fi
SDK_TOP="$(cat "$WORK_DIR/SDK_TOP.path")"
cd /mnt/wsl/SDK/src/rutos-ipq40xx-rutx-sdk/
#cd "$SDK_TOP"

[[ -e dl ]] || ln -s "$DL_DIR" dl  # reuse downloads cache

# ===== patch metadata (avoid ---help---) ====================================
if run_step patch_meta "Patch metadata generator to use 'help' instead of legacy '---help---'"; then
  sed -i 's/---help---/help/g' scripts/metadata.pl            2>/dev/null || true
  sed -i 's/---help---/help/g' scripts/package-metadata.pl    2>/dev/null || true
  mark_done patch_meta
fi

# ===== feeds update (selective) =============================================
if run_step feeds_update "Update feeds (avoid installing huge 'packages' feed)"; then
  ./scripts/feeds update -a
  # Install only VUCI feed (for vuci-examples) if present; skip global install
  if ./scripts/feeds list | grep -q '^vuci'; then
    ./scripts/feeds install -p vuci -a || true
  fi
  mark_done feeds_update
fi

# ===== select target RUTX50 ==================================================

cd /mnt/wsl/SDK/src/rutos-ipq40xx-rutx-sdk

if run_step select_target "Non-interactive target selection: RUTX50"; then
  cat > .config <<'EOF'
CONFIG_TARGET_ipq40xx=y
CONFIG_TARGET_ipq40xx_generic=y
CONFIG_TARGET_ipq40xx_generic_DEVICE_teltonika_rutx=y
CONFIG_DEVEL=y
CONFIG_BUILD_LOG=y
EOF
  # Generate Kconfig metadata first (catches parse issues early)
  make prepare-tmpinfo -j"$JOBS" V=s
  make defconfig
  grep -q '^CONFIG_TARGET_ipq40xx_generic_DEVICE_teltonika_rutx=y' .config || {
    echo "ERROR: target selection did not stick"; exit 1; }
  mark_done select_target
fi

# ===== examples: enable as modules (m) =======================================
cd /mnt/wsl/SDK/src/rutos-ipq40xx-rutx-sdk

if run_step examples_conf "Enable example packages as modules (IPKs)"; then
  mapfile -t PKGS < <( \
    { [[ -d ipk-example      ]] && grep -RhoE '^[ \t]*define[ \t]+Package/[^ ]+' ipk-example      || true; \
      [[ -d vuci-examples    ]] && grep -RhoE '^[ \t]*define[ \t]+Package/[^ ]+' vuci-examples    || true; } \
      | sed -E 's/.*Package\///' | sort -u )
  if (( ${#PKGS[@]} )); then
    printf '  -> %s\n' "${PKGS[@]}"
    for p in "${PKGS[@]}"; do
      sym="CONFIG_PACKAGE_${p}"
      if ! grep -q "^${sym}=" .config 2>/dev/null; then
        echo "${sym}=m" >> .config
      else
        sed -i -E "s|^${sym}=.+|${sym}=m|" .config
      fi
    done
    make defconfig
  else
    echo "No example packages detected (ipk-example/ or vuci-examples/)."
  fi
  mark_done examples_conf
fi

# ===== build toolchain =======================================================

cd /mnt/wsl/SDK/src/rutos-ipq40xx-rutx-sdk 

if run_step toolchain "Build tools & toolchain"; then
  make tools/install     -j"$JOBS"
  make toolchain/install -j"$JOBS"
  mark_done toolchain
fi

# ===== build example IPKs ====================================================

cd /mnt/wsl/SDK/src/rutos-ipq40xx-rutx-sdk
if run_step build_examples "Build example IPKs only"; then
  mapfile -t PKGS < <( \
    { [[ -d ipk-example      ]] && grep -RhoE '^[ \t]*define[ \t]+Package/[^ ]+' ipk-example      || true; \
      [[ -d vuci-examples    ]] && grep -RhoE '^[ \t]*define[ \t]+Package/[^ ]+' vuci-examples    || true; } \
      | sed -E 's/.*Package\///' | sort -u )
  if (( ${#PKGS[@]} )); then
    for p in "${PKGS[@]}"; do
      say "--- make package/${p}/compile ---"
      make -j"$JOBS" "package/${p}/compile" V=s
    done
  else
    echo "No example packages selected to compile."
  fi
  mark_done build_examples
fi

# ===== result paths ==========================================================
ARCH_DIR="$(find bin/packages -maxdepth 2 -type d -not -path '*/zipped_packages' -name '*' 2>/dev/null | head -n1 || true)"
say "Done. IPKs (if built): ${ARCH_DIR:-bin/packages/<arch>/}"
echo "Tip: scp *.ipk to the router, then:  opkg install ./yourpkg.ipk"

