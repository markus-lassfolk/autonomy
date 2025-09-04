Awesome — I pulled everything you shared together and produced a single, end-to-end guide. I saved it as:

**[Download the MASTER GUIDE (Markdown)](sandbox:/mnt/data/RUTOS-MASTER-PACKAGING-GUIDE.md)**

Below is the full content inline as well, with citations to your uploaded notes.

---

````markdown
# MASTER GUIDE — Building RutOS (Teltonika) Packages & VuCI Apps (RUTX 7.17.1)

This guide consolidates our internal notes, quick sheets, and integration write-ups into a single, end-to-end reference for **building RutOS-compliant packages**, registering **UBUS/RPCD** APIs, and shipping **VuCI** web apps that appear and function correctly in the **Package Manager** and **WebUI**.

> Scope: RUTX family on RutOS **00.07.17.1**; adapt variables for other device lines (RUT9/TRBx) and versions.  
> Audience: developers building packages and VuCI apps for production.

---

## 0) What’s different on RutOS vs “vanilla” OpenWrt

- **IPK format is strict**: A valid IPK is an **ar** archive with `debian-binary`, `control.tar.gz`, `data.tar.gz` — and **must be gzip-compressed** for `opkg` to accept it (when crafting manually). :contentReference[oaicite:0]{index=0}  
- **Install prefix reality**: Third-party/user packages end up under the **`/usr/local` overlay** at runtime (e.g., menus → `/usr/local/usr/share/vuci/menu.d`, APIs → `/usr/local/usr/lib/lua/api/services`). The SDK/Makefile installs to `/usr/...` paths, which get remapped via overlay. Plan your paths accordingly. :contentReference[oaicite:1]{index=1}  
- **WebUI technology**: RutOS uses **VuCI** (Vue-based, LuCI-like). UI is shipped as **compiled, gzipped JS bundles** under `/www/assets/` with the naming `app.<name>.app-<version|hash>.js.gz`. Views must match menu `view` paths exactly (case-sensitive). :contentReference[oaicite:2]{index=2}

---

## 1) SDK Setup (Host)

The SDK flow below mirrors our **Developer QuickSheet**; use WSL or native Linux. :contentReference[oaicite:3]{index=3}

### 1.1 Host prerequisites (Ubuntu/WSL) — **[Host]**
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
# (Optional) Node 20.x if you will compile VuCI locally
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs
````

### 1.2 Obtain and initialize the SDK — **\[Host]**

```bash
SDK_URL="https://firmware.teltonika-networks.com/7.17.1/RUTX/RUTX_R_GPL_00.07.17.1.tar.gz"
mkdir -p ~/rutos-sdk/{dl,src}
curl -L -o ~/rutos-sdk/dl/$(basename "$SDK_URL") "$SDK_URL"
tar -xzf ~/rutos-sdk/dl/$(basename "$SDK_URL") -C ~/rutos-sdk/src

# Locate SDK top (contains scripts/feeds)
SDK_TOP=$(find ~/rutos-sdk/src -maxdepth 2 -type f -path '*/scripts/feeds' -printf '%h\n' | head -n1)
cd "$SDK_TOP"

./scripts/feeds update -a
./scripts/feeds install -a
cat > .config <<'EOF'
CONFIG_TARGET_ipq40xx=y
CONFIG_TARGET_ipq40xx_generic=y
CONFIG_TARGET_ipq40xx_generic_DEVICE_teltonika_rutx=y
CONFIG_DEVEL=y
CONFIG_BUILD_LOG=y
EOF
make defconfig
```

> Kconfig parse errors (“`---help---`”) → normalize line endings, patch to `help`, then `rm -rf tmp/.config* tmp/info && make defconfig`.&#x20;

### 1.3 First build of tools & toolchain — **\[Host]**

```bash
make -j"$(nproc)" tools/install
make -j"$(nproc)" toolchain/install
```



---

## 2) Package Types & Anatomy

### 2.1 Standard package (no UI)

Typical payload:

```
package/<section>/<name>/
├── Makefile
└── files/
    ├── bin/<your-scripts>           # → /usr/local/bin/...
    ├── etc/init.d/<svc>              # procd service
    ├── etc/config/<cfg>              # UCI config
    └── etc/uci-defaults/<script>     # first-install defaults
```

Key conventions on RutOS:

* Install under standard `/usr`/`/etc` in `install` phase; the overlay maps to `/usr/local/...` at runtime.&#x20;
* Use **procd** init and **UCI** config + `uci-defaults` to seed settings.&#x20;

### 2.2 VuCI application = **two packages**

1. **API package** (Lua + RPCD): `/usr/lib/lua/api/services/<name>.lua` + ACL in `/usr/share/rpcd/acl.d/<name>.json`
2. **UI package** (Vue bundle): menu JSON in `/usr/share/vuci/menu.d/<name>.json` + compiled asset `app.<name>.app-*.js.gz` in `/www/assets/`
   (At runtime these live under `/usr/local/...`.)&#x20;

**Real-world pattern (NTPD/UPNP):**

```
/usr/local/usr/lib/lua/api/services/ntpd.lua
/usr/local/usr/share/rpcd/acl.d/upnp.json
/usr/local/usr/share/vuci/menu.d/ntpd.json
/usr/local/www/assets/app.ntpd.app-*.js.gz
```

### 2.3 Directory Structure & File Paths (CRITICAL!)

**⚠️ Understanding RutOS filesystem is ESSENTIAL to avoid path confusion!**

```
SDK Development Structure                    RutOS Runtime Structure
=====================================        =====================================

📁 SDK Package Structure:                   📁 RutOS Filesystem:
├── 📁 vuci-app-example-api/                ├── 📁 /www/ (READ-ONLY)
│   ├── 📁 files/                           │   ├── 📁 assets/ (READ-ONLY)
│   │   ├── 📁 usr/lib/lua/api/services/    │   │   ├── app.access-control.app-*.js.gz
│   │   │   └── example.lua                 │   │   ├── app.administration.app-*.js.gz
│   │   └── 📁 usr/share/rpcd/acl.d/        │   │   └── ... (firmware bundles)
│   │       └── example.json                │   └── 📁 cgi-bin/ (READ-ONLY)
│   └── Makefile                            ├── 📁 /usr/share/ (READ-ONLY)
└── 📁 vuci-app-example-ui/                 │   ├── 📁 vuci/menu.d/ (READ-ONLY)
    ├── 📁 files/                           │   │   ├── access-control.json
    │   └── 📁 usr/share/vuci/menu.d/       │   │   ├── administration.json
    │       └── example.json                │   │   └── ... (firmware menus)
    ├── 📁 src/                             ├── 📁 /usr/local/ (OVERLAY - WRITABLE)
    │   └── 📁 src/views/services/          │   ├── 📁 www/assets/ (USER PACKAGES)
    │       └── Example.vue                 │   │   ├── app.example.app-*.js.gz
    └── Makefile                            │   │   ├── app.ntpd.app-*.js.gz
                                           │   │   └── app.upnp.app-*.js.gz
                                           │   ├── 📁 usr/lib/lua/api/services/ (USER PACKAGES)
                                           │   │   ├── example.lua
                                           │   │   ├── ntpd.lua
                                           │   │   └── upnp.lua
                                           │   ├── 📁 usr/share/rpcd/acl.d/ (USER PACKAGES)
                                           │   │   ├── example.json
                                           │   │   ├── ntpd.json
                                           │   │   └── upnp.json
                                           │   └── 📁 usr/share/vuci/menu.d/ (USER PACKAGES)
                                           │       ├── example.json
                                           │       ├── ntpd.json
                                           │       └── upnp.json
                                           └── 📁 /etc/ (READ-ONLY)
                                               └── 📁 config/ (READ-ONLY)

🔴 READ-ONLY PATHS (Firmware - Cannot Write):
   /www/assets/          - Firmware JavaScript bundles
   /usr/share/vuci/menu.d/ - Firmware menu definitions
   /etc/config/          - Firmware configuration

🟢 OVERLAY PATHS (User Packages - Can Write):
   /usr/local/www/assets/     - User JavaScript bundles
   /usr/local/usr/share/vuci/menu.d/ - User menu definitions
   /usr/local/usr/lib/lua/api/services/ - User API services
   /usr/local/usr/share/rpcd/acl.d/ - User ACL definitions

📋 SDK → RutOS Installation Mapping:
   SDK: files/usr/share/vuci/menu.d/example.json
   → RutOS: /usr/local/usr/share/vuci/menu.d/example.json

   SDK: files/usr/lib/lua/api/services/example.lua
   → RutOS: /usr/local/usr/lib/lua/api/services/example.lua

   SDK: compiled Vue.js bundle (via app.mk)
   → RutOS: /usr/local/www/assets/app.example.app-*.js.gz
```

**⚠️ CRITICAL PATH RULES:**

1. **SDK Makefiles install to `/usr/...` paths** → RutOS overlay maps to `/usr/local/...`
2. **User packages CANNOT write to read-only paths** (`/www/`, `/usr/share/`)
3. **All user content goes to `/usr/local/` overlay**
4. **WebUI looks in overlay paths first, then read-only paths**
5. **Use `opkg install` to ensure proper path mapping**

---

## 3) Package Manager Compliance (WebUI → Services → Package Manager)

To upload custom packages via WebUI:

* Name your IPK **`tlt_custom_pkg_<name>.ipk`**.
* In your Makefile, set **Package Manager metadata**:

  ```make
  PKG_ROUTER:=RUTX
  PKG_FIRMWARE:=00.07.17.1
  PKG_TLT_NAME:=My App
  PKG_VERSION_PM:=1.0
  ```
* Custom (unsigned) packages will show as **Unauthorized** (expected).&#x20;

> You can still install via CLI with `opkg install /tmp/<pkg>.ipk`. Newer RutOS limits upstream OpenWrt feeds; prefer local IPKs or Package Manager.&#x20;

---

## 4) UBUS/RPCD Integration (Backend)

You have two supported backends; both are recognized by **rpcd** and callable from VuCI. See our focused RPCD guide for drop-in templates.&#x20;

### 4.1 Executable **rpcd plugin** (simple; no daemon)

* Place **`/usr/libexec/rpcd/<service>`** (executable).
* It must respond to:

  * `list` → prints JSON describing available methods and argument schema
  * `call <method>` → reads JSON args from stdin and prints JSON result
* rpcd auto-loads it and exposes an UBUS object with your methods.&#x20;

### 4.2 **Daemon** with libubus (advanced)

* Your process registers an UBUS object and methods via `libubus` (`ubus_add_object`, `UBUS_METHOD`, etc.), then runs `uloop_run()`.&#x20;

### 4.3 **ACLs (rpcd)** are mandatory

* Install JSON ACL at `/usr/share/rpcd/acl.d/<name>.json` to authorize read/write. Without this the UI sees “Access denied.”&#x20;

### 4.4 Minimal runtime tests — **\[RUTOS]**

```sh
ubus -v list <service>
ubus call <service> status
ubus call <service> set '{"enabled":true}'
```



---

## 5) VuCI Frontend Integration (UI)

### 5.1 Menu registration

Create `/usr/share/vuci/menu.d/<name>.json`:

```json
{
  "services/<name>": {
    "title": "My App",
    "index": 220,
    "view": "services/MyApp",
    "acls": ["services/<name>"]
  }
}
```

* `view` must match the compiled Vue view path name (case-sensitive).&#x20;

### 5.2 Compiled Vue assets

* Build your Vue view(s) to **gzipped** bundle(s) named: `app.<name>.app-<version or hash>.js.gz` → deploy to `/www/assets/`.
* VuCI dynamically imports bundles based on menu/view.&#x20;

### 5.3 Calling backend from UI

* In VuCI components, call via `$ubus` or the API layer your app exposes (`this.$axios` → `/api/<service>/*`, depending on your template).
* Our standard Lua API services export a single **`handle(method, path, query, body)`** that routes to functions.&#x20;

---

## 6) Example: **vuci-app-example** (API + UI)

### 6.1 API package (`vuci-app-example-api`) — **\[Host]**

**Tree**

```
vuci-app-example-api/
├── Makefile
└── files/
    ├── usr/lib/lua/api/services/example.lua
    └── usr/share/rpcd/acl.d/example.json
```

**Makefile (core)**

```make
include $(TOPDIR)/rules.mk
PKG_NAME:=vuci-app-example-api
PKG_VERSION:=1.0
PKG_RELEASE:=9
include $(INCLUDE_DIR)/package.mk
define Package/vuci-app-example-api
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Example API Service
  DEPENDS:=+lua +api-core
endef
define Package/vuci-app-example-api/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/api/services
	$(INSTALL_DATA) ./files/usr/lib/lua/api/services/example.lua $(1)/usr/lib/lua/api/services/
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DATA) ./files/usr/share/rpcd/acl.d/example.json $(1)/usr/share/rpcd/acl.d/
endef
$(eval $(call BuildPackage,vuci-app-example-api))
```

**Lua service (`example.lua`)**

```lua
local M = {}
function M.test() return { success=true, message="Example API working", ts=os.time() } end
function M.get_status() return { status="active", version="1.0-9" } end
function M.handle(method, path, query, body)
  if path:match("/test") then return M.test()
  elseif path:match("/status") then return M.get_status()
  else return {error="Unknown endpoint", path=path} end
end
return M
```

**ACL (`example.json`)**

```json
{
  "services/example": {
    "description": "Example App permissions",
    "read": { "api": { "/example/*": ["*"] } },
    "write": { "api": ["/example/*"] }
  }
}
```



### 6.2 UI package (`vuci-app-example-ui`) — **\[Host]**

**Tree**

```
vuci-app-example-ui/
├── Makefile
├── files/
│   ├── usr/share/vuci/menu.d/example.json
│   └── www/assets/app.example.app-1.0-9.js.gz
└── src/
    └── src/views/services/Example.vue
```

**Makefile (core)**

```make
include $(TOPDIR)/rules.mk
PKG_NAME:=vuci-app-example-ui
PKG_VERSION:=1.0
PKG_RELEASE:=9
include $(INCLUDE_DIR)/package.mk
define Package/vuci-app-example-ui
  SECTION:=vuci
  CATEGORY:=VUCI
  TITLE:=Example UI
  DEPENDS:=+vuci-app-example-api +vuci-ui-core
endef
define Package/vuci-app-example-ui/install
	$(INSTALL_DIR) $(1)/usr/share/vuci/menu.d
	$(INSTALL_DATA) ./files/usr/share/vuci/menu.d/example.json $(1)/usr/share/vuci/menu.d/
	$(INSTALL_DIR) $(1)/www/assets
	$(INSTALL_DATA) ./files/www/assets/app.example.app-1.0-9.js.gz $(1)/www/assets/
endef
$(eval $(call BuildPackage,vuci-app-example-ui))
```

**Menu (`example.json`)**

```json
{ "services/example": { "title":"Example", "index":500, "view":"services/Example", "acls":["services/example"] } }
```



---

## 7) Build as **modules** and iterate fast

```bash
# Enable modules and build just your packages — **[Host]**
echo 'CONFIG_PACKAGE_vuci-app-example-api=m' >> .config
echo 'CONFIG_PACKAGE_vuci-app-example-ui=m'  >> .config
make defconfig
make -j"$(nproc)" V=sc \
  package/vuci-app-example-api/{clean,compile} \
  package/vuci-app-example-ui/{clean,compile}
```

Resulting IPKs appear under `bin/packages/<arch>/...`. Deploy via Package Manager (rename to `tlt_custom_pkg_*.ipk` if uploading) or via `opkg`. &#x20;

---

## 8) Manual IPK creation (when SDK is unavailable)

> Use only for emergency/debug; prefer SDK builds.

**Recipe (critical detail: gzip the ar archive):**

```bash
# control
cat > control <<EOF
Package: vuci-app-example-api
Version: 1.0-9
Depends: libc, api-core
Section: vuci
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 1024
Description: Example API service
EOF

# postinst (optional)
cat > postinst <<'EOF'
#!/bin/sh
[ "${IPKG_NO_SCRIPT}" = "1" ] && exit 0
exit 0
EOF
chmod +x postinst

echo "2.0" > debian-binary
mkdir -p data/usr/lib/lua/api/services
# ... add payload under data/...

tar -czf control.tar.gz --owner=root --group=root control postinst
tar -czf data.tar.gz    --owner=root --group=root -C data .
ar cr package.ipk debian-binary control.tar.gz data.tar.gz

# CRITICAL: compress with gzip for opkg compatibility
gzip -c package.ipk > package.ipk.gz && mv package.ipk.gz package.ipk

# Verify
file package.ipk
gunzip -c package.ipk > temp.ipk && ar t temp.ipk
```

&#x20;

---

## 9) Full system integration example (Autonomy)

For a multi-package, production-style integration (daemon + API + UI + deploy), use the **Autonomy** blueprint: package layout, ACL/menu, build, tests, and deployment steps.&#x20;

* **Daemon package**: `package/autonomy` (procd service, UCI config, defaults).
* **VuCI API**: `vuci-app-autonomy-api` with `autonomy.lua` under API services and `autonomy.json` ACL.
* **VuCI UI**: `vuci-app-autonomy-ui` with menu JSON and compiled Vue bundle(s).
* **Testing**: install IPKs, validate files at `/usr/local/...`, restart `rpcd`/`uhttpd`, and hit `/api/autonomy/*` endpoints via `ubus`.
* **Deployment**: generate a feed (`opkg-make-index`) or ship via Package Manager.&#x20;

---

## 10) Calling UBUS from LuCI/VuCI

If you need raw LuCI (JS) or examples of browser → rpcd → ubus → service, see our bridge guide; it shows: rpcd plugin in `/usr/libexec/rpcd/`, ACLs in `/usr/share/rpcd/acl.d/`, and a LuCI page that calls into your service. VuCI follows the same RPCD/UBUS authorization model.&#x20;

---

## 11) End-to-end developer workflow

1. **Create package(s)** under `package/<section>/<name>` or `package/feeds/vuci/` (for VuCI).&#x20;
2. **Enable as modules**: append `CONFIG_PACKAGE_<name>=m` and `make defconfig`.&#x20;
3. **Build fast**: `make package/<name>/{clean,compile} V=sc -j$(nproc)`.&#x20;
4. **Install** on device: WebUI Package Manager (rename to `tlt_custom_pkg_*`) **or** `opkg install`.&#x20;
5. **Register APIs**: ship rpcd plugin or libubus daemon + ACLs; verify with `ubus`.&#x20;
6. **Ship UI**: menu JSON + compiled `.js.gz` assets; check view names and ACL bindings.&#x20;
7. **Restart services** after install: `/etc/init.d/rpcd restart` and `/etc/init.d/uhttpd restart`.&#x20;
8. **Test**: View menu entry, call endpoints (`ubus call api get '{"path":"/<name>/status"}'`).&#x20;

---

## 12) Troubleshooting (Most Common)

| Symptom                                      | Likely Cause                                     | Fix                                                                                  |   |
| -------------------------------------------- | ------------------------------------------------ | ------------------------------------------------------------------------------------ | - |
| **“Malformed package file”**                 | IPK not gzip-compressed (manual build)           | Gzip the ar archive; `file` should show “gzip compressed data”                       |   |
| **Page loads blank / “Failed to load page”** | Missing/bad Vue bundle or wrong `view` path case | Ensure `/www/assets/app.<name>.app-*.js.gz` exists and `view` matches component      |   |
| **“Access denied.”** from UI/ubus            | Missing/wrong **ACL** in `/usr/share/rpcd/acl.d` | Add minimal ACL; restart `rpcd`                                                      |   |
| **UBUS method not found**                    | Plugin not discovered / daemon not registered    | Place executable under `/usr/libexec/rpcd/` or register via `libubus`; reload `rpcd` |   |
| **Files in odd locations**                   | RutOS overlay prefixes `/usr/local`              | Expect runtime under `/usr/local/...`, plan install paths accordingly                |   |
| **Not visible in Package Manager**           | Missing PM metadata or filename                  | Use `tlt_custom_pkg_*.ipk` and set `PKG_ROUTER/FIRMWARE/TLT_NAME/VERSION_PM`         |   |
| **Online `opkg` can’t fetch packages**       | Feeds restricted on RutOS                        | Install local IPKs or use Package Manager                                            |   |

---

## 13) Security & Ops Best Practices

* Run daemons as dedicated users, keep ACLs least-privilege, and validate/sanitize all UCI inputs.&#x20;
* Add `postinst` hooks to reload `rpcd`/`uhttpd` so UI/API are immediately available after install.&#x20;
* Version your UI bundles (`app.<name>.app-<semver>.js.gz`) and bump `PKG_RELEASE` for re-installs.&#x20;

---

## 14) Copy-Paste Templates

### 14.1 Standard package Makefile (files-only)

```make
include $(TOPDIR)/rules.mk
PKG_NAME:=mytool
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_LICENSE:=MIT
include $(INCLUDE_DIR)/package.mk

define Package/mytool
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=My Tool
  DEPENDS:=+busybox
endef

define Build/Compile
endef

define Package/mytool/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) ./files/bin/mytool.sh $(1)/bin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/mytool $(1)/etc/init.d/mytool
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/mytool $(1)/etc/config/mytool
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_CONF) ./files/etc/uci-defaults/99-mytool $(1)/etc/uci-defaults/99-mytool
endef

$(eval $(call BuildPackage,mytool))
```



### 14.2 rpcd executable plugin (backend)

```sh
#!/bin/sh
# /usr/libexec/rpcd/myapp
. /usr/share/libubox/jshn.sh
case "$1" in
  list) echo '{"status":{},"set":{"enabled":true}}' ;;
  call)
    case "$2" in
      status) echo '{"enabled":true,"version":"1.0"}' ;;
      set) read input; json_load "$input"; json_get_var enabled enabled; echo '{"ok":true}';;
    esac
  ;;
esac
```



### 14.3 rpcd ACL (backend authorization)

```json
{
  "services/myapp": {
    "description": "MyApp permissions",
    "read":  { "api": { "/myapp/*": ["*"] } },
    "write": { "api": ["/myapp/*"] }
  }
}
```



### 14.4 VuCI menu JSON (UI registration)

```json
{
  "services/myapp": {
    "title": "My App",
    "index": 250,
    "view": "services/MyApp",
    "acls": ["services/myapp"]
  }
}
```



---

## 15) Validation Checklist (print & pin)

* [ ] Build with matching **target** (RUTX / 00.07.17.1).&#x20;
* [ ] **Package Manager**: filename `tlt_custom_pkg_*.ipk` + `PKG_ROUTER/FIRMWARE/TLT_NAME/VERSION_PM`.&#x20;
* [ ] **IPK** structure good; **gzip** compressed if handcrafted.&#x20;
* [ ] Backend registered (rpcd plugin or libubus daemon) and **ACLs** present.&#x20;
* [ ] UI menu + compiled **Vue bundle** in `/www/assets/`.&#x20;
* [ ] Post-install reload of `rpcd`/`uhttpd`.&#x20;
* [ ] Runtime tests (`ubus call`, menu visible, API endpoints work).&#x20;

---

### Credits (internal sources used)

* RutOS SDK – Developer QuickSheet.&#x20;
* RUTOS SDK Quick Reference.&#x20;
* SDK RPCD Guide (Q\&A).&#x20;
* Prior combined RutOS packaging guide.&#x20;
* LuCI ↔ RPCD bridge guide.&#x20;
* SDK Integration (Autonomy) blueprint.&#x20;

```

---

Want me to drop this straight into your repo as `RUTOS-MASTER-PACKAGING-GUIDE.md` (and optionally break out a “Quick Start” 2-pager for newcomers)?
::contentReference[oaicite:64]{index=64}
```
