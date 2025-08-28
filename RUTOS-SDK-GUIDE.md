# RutOS SDK – Developer QuickSheet (RUTX50 / WSL)

A compact, **step‑by‑step** reference for building and deploying custom packages and VuCI apps to Teltonika **RutOS** (tested on **RUTX50**, SDK 00.07.17.1) using **WSL** on Windows.

---

## 1) WSL Disk & Permissions (One-Time Setup)

* Create a dedicated, **case‑sensitive** VHD and mount it at e.g. `/mnt/wsl/SDK` (we used `/dev/sdd1`).
* Verify it's **writable** and **case‑sensitive**:

  ```bash
  touch /mnt/wsl/SDK/.rwtest && rm /mnt/wsl/SDK/.rwtest
  printf 'lower\n' > /mnt/wsl/SDK/readme
  printf 'upper\n' > /mnt/wsl/SDK/README   # both files must coexist
  ```
* Recommended directory layout (keep downloads persistent):

  ```
  /mnt/wsl/SDK/
  ├─ dl/         # shared download cache for the SDK
  ├─ src/        # SDK source extracts
  ├─ work/       # logs, temp, scripts
  └─ keys/       # SSH keys for router access
  ```

---

## 2) Install Host Dependencies (Ubuntu 24.04 / WSL)

```bash
sudo apt-get update -y
sudo apt-get install -y \
  binutils binutils-gold bison build-essential bzip2 ca-certificates curl cmake \
  default-jdk device-tree-compiler devscripts file flex g++ gawk gcc gettext git \
  gnupg gperf help2man jq libc6-dev libffi-dev libexpat1-dev libncurses-dev \
  libpcre3-dev libsqlite3-dev libssl-dev libxml-parser-perl lz4 liblz4-dev \
  libzstd-dev make patch pkg-config psmisc python-is-python3 python3 python3-dev \
  python3-setuptools python3-yaml rsync ruby sharutils subversion swig \
  u-boot-tools unzip uuid-dev vim-common wget zip zlib1g-dev time dos2unix

# Node 20.x for VuCI (if not already)
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs
node -v; npm -v
```

---

## 3) Get the RutOS SDK and Select Target (RUTX50)

```bash
SDK_URL="https://firmware.teltonika-networks.com/7.17.1/RUTX/RUTX_R_GPL_00.07.17.1.tar.gz"
mkdir -p /mnt/wsl/SDK/{dl,src,work}
cd /mnt/wsl/SDK
curl -L -o dl/$(basename "$SDK_URL") "$SDK_URL"
rm -rf src && mkdir src && tar -xzf dl/$(basename "$SDK_URL") -C src

# Locate SDK top (has scripts/feeds)
SDK_TOP=$(find src -maxdepth 2 -type f -path '*/scripts/feeds' -printf '%h\n' | head -n1)
cd "$SDK_TOP"

# Feeds
./scripts/feeds update -a
./scripts/feeds install -a

# Non-interactive target selection (RUTX profile)
cat > .config <<'EOF'
CONFIG_TARGET_ipq40xx=y
CONFIG_TARGET_ipq40xx_generic=y
CONFIG_TARGET_ipq40xx_generic_DEVICE_teltonika_rutx=y
CONFIG_DEVEL=y
CONFIG_BUILD_LOG=y
EOF
make defconfig
```

> **Tip:** If `make defconfig` fails with a Kconfig parse error (e.g. `---help---`), locate the offending package and patch or temporarily move it out of `package/`:
>
> ```bash
> grep -Rin '^\s*---help---' package feeds | head -n 20
> # Patch to `help`, normalize CRLF -> LF with dos2unix, then:
> rm -rf tmp/.config* tmp/info && make defconfig
> ```

---

## 4) Build Base Tools/Toolchain (First Time Only)

```bash
make -j"$(nproc)" tools/install
make -j"$(nproc)" toolchain/install
```

---

## 5) Package Anatomy (Files-Only Example)

**Repository layout:** put your package under `package/<section>/<name>`.

```
package/base/example_package/
├─ Makefile
├─ README.md (optional)
└─ files/
   └─ bin/example.sh   # will become /usr/local/bin/example.sh on device
```

**Minimal Makefile template:**

```makefile
include $(TOPDIR)/rules.mk

PKG_NAME:=example_package
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk

define Package/example_package
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=Example package
  DEPENDS:=+busybox
endef

define Build/Compile
endef

define Package/example_package/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) ./files/example.sh $(1)/bin/example.sh
endef

$(eval $(call BuildPackage,example_package))
```

**Important conventions on RutOS paths:**

* RutOS prefixes third‑party files with **`/usr/local`** at install time.

  * Installing to `$(1)/bin` → ends up as **`/usr/local/bin`** on the router.
  * Installing to `$(1)/usr/bin` → becomes **`/usr/local/usr/bin`** (usually *not* desired).
* Web assets & VuCI:

  * `/usr/share` is symlinked to `/usr/local/share`. Expect files to live under `/usr/local/share/...` at runtime.

**Make your payload executable:**

```bash
chmod 0755 package/base/example_package/files/bin/example.sh
```

Enable and build:

```bash
# avoid duplicate lines in .config
sed -i '/^CONFIG_PACKAGE_example_package=/d' .config
printf 'CONFIG_PACKAGE_example_package=m\n' >> .config
make defconfig

# build only this package
make -j"$(nproc)" V=sc package/example_package/{clean,compile}
# IPK → bin/packages/arm_cortex-a7_neon-vfpv4/base/example_package_*_arm_cortex-a7_neon-vfpv4.ipk
```

---

## 6) VuCI Applications (UI + API)

**Place the examples** (or your own) into the VuCI feed directory inside the SDK:

```bash
cp -r vuci-app-example-api vuci-app-example-ui package/feeds/vuci/
./scripts/feeds install -p vuci -a

# Enable as modules
sed -i '/^CONFIG_PACKAGE_vuci-app-example-api=/d' .config; echo 'CONFIG_PACKAGE_vuci-app-example-api=m' >> .config
sed -i '/^CONFIG_PACKAGE_vuci-app-example-ui=/d'  .config; echo 'CONFIG_PACKAGE_vuci-app-example-ui=m'  >> .config
make defconfig

# Build just these
make -j"$(nproc)" V=sc \
  package/vuci-app-example-api/{clean,compile} \
  package/vuci-app-example-ui/{clean,compile}
```

**Deploying VuCI apps:** after install, refresh services if the page doesn't appear:

```sh
/etc/init.d/uhttpd restart
/etc/init.d/rpcd restart
# hard refresh browser (avoid cached JS)
```

**Runtime location reminders:**

* VuCI menu JSON lives under `/usr/local/share/vuci/menu.d/menu.json`.
* Compiled web assets typically under `/usr/local/share/vuci/dist` (varies by SDK/app).

---

## 7) Deploy to the Router

**Manual install (quick sanity):**

```bash
ROUTER=192.168.80.1
KEY=/mnt/wsl/SDK/keys/rusos_private_key_openssh
IPK=bin/packages/arm_cortex-a7_neon-vfpv4/base/example_package_1.0-1_arm_cortex-a7_neon-vfpv4.ipk

scp -i "$KEY" -o StrictHostKeyChecking=accept-new "$IPK" root@$ROUTER:/tmp/
ssh -i "$KEY" root@$ROUTER "opkg -V4 -t /tmp install /tmp/$(basename "$IPK")"
```

**Scripted deploy (multiple IPKs):**

* Use a deploy script that:

  * uploads to `/tmp/ipks/`
  * runs `opkg -V4 -t /tmp install` with optional `--force-reinstall`
  * logs `/tmp/opkg-install.log` back to `/mnt/wsl/SDK/work/logs/`
* Example usage:

  ```bash
  PACKAGES="example_package vuci-app-example-api vuci-app-example-ui" \
  OPKG_UPDATE=1 FORCE=1 \
  /mnt/wsl/SDK/deploy-ipks-to-rutox50.sh
  ```

**Verify on device:**

```sh
opkg list-installed | grep -E 'example_package|vuci-app-example-'
opkg files example_package
which example.sh && example.sh
```

---

## 8) Troubleshooting (TL;DR)

**Kconfig parse errors (e.g. `---help---`):**

* Identify bad token:

  ```bash
  nl -ba tmp/.config-package.in | sed -n 'N1,N2p'   # use the line range from the error
  grep -Rin '^\s*---help---' package feeds | head -n 20
  ```
* Patch to `help` and/or `dos2unix` the file; or temporarily move the package out of `package/`.
* Regenerate: `rm -rf tmp/.config* tmp/info && make defconfig`.

**BusyBox download fails (busybox.net down):**

* Change BusyBox `PKG_SOURCE_URL` to `@OPENWRT`, or manually drop the tarball into `dl/` from OpenWrt mirrors.
* Re‑run: `make package/busybox/download`.

**Duplicate lines in `.config`:**

```bash
awk '!seen[$0]++' .config > .config.clean && mv .config.clean .config
make defconfig
```

**Toolchain missing libs during `package/.../compile`:**

```bash
make -j"$(nproc)" tools/install toolchain/install
```

**Installed paths look odd (e.g. `/usr/local/usr/bin/...`):**

* RutOS prefixes with `/usr/local`. Install to `$(1)/bin` (→ `/usr/local/bin`).
* For convenience, create a symlink to `/usr/bin` in `install` phase if needed.

**VuCI page says *"Failed to load page … is missing or could not be loaded"*:**

* Ensure the app's **view name** matches what the menu JSON expects.
* Confirm files exist under `/usr/local/share/vuci/...`.
* Restart `uhttpd` and `rpcd`; clear browser cache.

**Reinstall same version during iteration:**

* Use `FORCE=1` with your deploy script, or bump `PKG_RELEASE` in the Makefile.

---

## 9) Useful One-Liners

```bash
# Show last defconfig error region
nl -ba tmp/.config-package.in | sed -n '112940,113020p'

# Find packages by Kconfig symbol in generated file
grep -n 'config PACKAGE_example_package' tmp/.config-package.in

# List produced IPKs
find bin/packages -type f -name '*.ipk' | sort

# Clean just one package
make package/example_package/clean

# Clean toolchain (only when absolutely necessary)
rm -rf staging_dir/toolchain-* build_dir/toolchain-*
```

---

## 10) What to Remember (Cheat Notes)

* **Install prefix:** third‑party → `/usr/local/...` (plan your `install` paths accordingly).
* **Place packages:** `package/<section>/<name>`; **VuCI apps** go under `package/feeds/vuci/`.
* **Enable as module:** add `CONFIG_PACKAGE_<name>=m` → `make defconfig`.
* **Build just what you need:** `make package/<name>/{clean,compile}`.
* **Troubleshoot Kconfig:** fix malformed `---help---`, normalize line endings, regenerate `tmp/`.
* **Services:** post‑install restart `uhttpd`/`rpcd` for UI, use init scripts for daemons.

---

## 11) Quick Start Examples

### Basic Shell Script Package

```bash
# Create package structure
mkdir -p package/base/hello-world/files/bin
cat > package/base/hello-world/files/bin/hello.sh << 'EOF'
#!/bin/sh
echo "Hello from RutOS!"
EOF
chmod +x package/base/hello-world/files/bin/hello.sh

# Create Makefile
cat > package/base/hello-world/Makefile << 'EOF'
include $(TOPDIR)/rules.mk
PKG_NAME:=hello-world
PKG_VERSION:=1.0
PKG_RELEASE:=1
include $(INCLUDE_DIR)/package.mk

define Package/hello-world
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=Hello World Example
endef

define Build/Compile
endef

define Package/hello-world/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) ./files/bin/hello.sh $(1)/bin/
endef

$(eval $(call BuildPackage,hello-world))
EOF

# Build it
echo 'CONFIG_PACKAGE_hello-world=m' >> .config
make defconfig
make -j$(nproc) package/hello-world/{clean,compile}
```

### VuCI App Structure

```bash
# API package structure
vuci-app-myapp-api/
├── Makefile
└── files/
    └── usr/lib/lua/api/services/
        └── myapp.lua

# UI package structure  
vuci-app-myapp-ui/
├── Makefile
├── files/
│   └── usr/share/vuci/menu.d/
│       └── myapp.json
└── src/
    └── src/views/services/
        └── MyApp.vue
```

---

## 12) Common Issues & Solutions

| Issue | Solution |
|-------|----------|
| **"Failed to load page"** in VuCI | Vue components need webpack compilation - use SDK's app.mk |
| **Files in wrong location** | Remember `/usr/local/` prefix for user packages |
| **Package not in Package Manager** | Check opkg status: `opkg list-installed | grep yourpackage` |
| **Circular dependencies** | Remove problematic packages from feeds, rebuild |
| **Build takes forever** | Use `make -j$(nproc)` for parallel builds |
| **IPK format error** | Ensure gzip compression: `file yourpackage.ipk` should show "gzip compressed" |

---

## 13) Deployment Script Template

```bash
#!/bin/bash
# deploy.sh - Deploy packages to RutOS device

ROUTER_IP="${ROUTER_IP:-192.168.80.1}"
SSH_KEY="${SSH_KEY:-$HOME/.ssh/rusos_key}"
PACKAGES="$@"

for pkg in $PACKAGES; do
    IPK=$(find bin/packages -name "${pkg}_*.ipk" | head -1)
    if [ -f "$IPK" ]; then
        echo "Deploying $(basename $IPK)..."
        scp -i "$SSH_KEY" "$IPK" root@$ROUTER_IP:/tmp/
        ssh -i "$SSH_KEY" root@$ROUTER_IP \
            "opkg install --force-reinstall /tmp/$(basename $IPK)"
    fi
done

ssh -i "$SSH_KEY" root@$ROUTER_IP "/etc/init.d/uhttpd restart"
```

---

**That's it.** This sheet is meant to be copy‑paste friendly. Save this as `RUTOS-SDK-GUIDE.md` in your project root for quick reference.


