#!/bin/bash

# OpenWrt Testing Environment Setup
# This script provides multiple options for testing OpenWrt packages

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-openwrt"

print_status "OpenWrt Testing Environment Setup"
print_status "=================================="

# Check if Docker is available
check_docker() {
    if command -v docker &> /dev/null; then
        return 0
    else
        return 1
    fi
}

# Check if QEMU is available
check_qemu() {
    if command -v qemu-system-x86_64 &> /dev/null; then
        return 0
    else
        return 1
    fi
}

# Option 1: Docker-based OpenWrt Simulator
setup_docker_openwrt() {
    print_status "Setting up Docker-based OpenWrt simulator..."

    # Create Dockerfile for OpenWrt testing
    cat > "$SCRIPT_DIR/docker/Dockerfile.openwrt-test" << 'EOF'
# OpenWrt Testing Environment
FROM ubuntu:22.04

# Install OpenWrt build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    ccache \
    ecj \
    fastjar \
    file \
    g++ \
    gawk \
    gettext \
    git \
    java-propose-classpath \
    libelf-dev \
    libncurses5-dev \
    libncursesw5-dev \
    libssl-dev \
    python3 \
    python3-distutils \
    python3-setuptools \
    python3-dev \
    rsync \
    subversion \
    swig \
    time \
    unzip \
    wget \
    xsltproc \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Install OpenWrt Image Builder
RUN git clone https://github.com/openwrt/openwrt.git /opt/openwrt
WORKDIR /opt/openwrt

# Update feeds
RUN ./scripts/feeds update -a
RUN ./scripts/feeds install -a

# Configure for x86_64 target
RUN make defconfig
RUN echo "CONFIG_TARGET_x86=y" >> .config
RUN echo "CONFIG_TARGET_x86_64=y" >> .config
RUN echo "CONFIG_TARGET_x86_64_DEVICE_generic=y" >> .config
RUN echo "CONFIG_PACKAGE_luci=y" >> .config
RUN echo "CONFIG_PACKAGE_luci-base=y" >> .config
RUN echo "CONFIG_PACKAGE_luci-compat=y" >> .config
RUN echo "CONFIG_PACKAGE_mwan3=y" >> .config
RUN echo "CONFIG_PACKAGE_ubus=y" >> .config
RUN echo "CONFIG_PACKAGE_uci=y" >> .config
RUN make defconfig

# Create test directories
RUN mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin

# Set up basic OpenWrt environment
RUN echo "config system" > /etc/config/system && \
    echo "    option hostname 'openwrt-test'" >> /etc/config/system && \
    echo "    option timezone 'UTC'" >> /etc/config/system

# Create mock network interfaces
RUN echo "config interface 'loopback'" > /etc/config/network && \
    echo "    option ifname 'lo'" >> /etc/config/network && \
    echo "    option proto 'static'" >> /etc/config/network && \
    echo "    option ipaddr '127.0.0.1'" >> /etc/config/network && \
    echo "    option netmask '255.0.0.0'" >> /etc/config/network

RUN echo "config interface 'lan'" >> /etc/config/network && \
    echo "    option ifname 'eth0'" >> /etc/config/network && \
    echo "    option proto 'static'" >> /etc/config/network && \
    echo "    option ipaddr '192.168.1.1'" >> /etc/config/network && \
    echo "    option netmask '255.255.255.0'" >> /etc/config/network

# Create mock mwan3 configuration
RUN mkdir -p /etc/config && \
    echo "config globals 'globals'" > /etc/config/mwan3 && \
    echo "    option mmx_mask '0x3F00'" >> /etc/config/mwan3 && \
    echo "    option local_source 'lan'" >> /etc/config/mwan3

RUN echo "config interface 'wan'" >> /etc/config/mwan3 && \
    echo "    option enabled '1'" >> /etc/config/mwan3 && \
    echo "    option family 'ipv4'" >> /etc/config/mwan3 && \
    echo "    option track_method 'ping'" >> /etc/config/mwan3 && \
    echo "    option track_ip '8.8.8.8'" >> /etc/config/mwan3 && \
    echo "    option reliability '1'" >> /etc/config/mwan3 && \
    echo "    option count '1'" >> /etc/config/mwan3 && \
    echo "    option timeout '2'" >> /etc/config/mwan3 && \
    echo "    option interval '5'" >> /etc/config/mwan3 && \
    echo "    option down '3'" >> /etc/config/mwan3 && \
    echo "    option up '3'" >> /etc/config/mwan3

# Create mock ubus
RUN echo '#!/bin/bash' > /usr/bin/ubus && \
    echo 'echo "Mock ubus - $*"' >> /usr/bin/ubus && \
    chmod +x /usr/bin/ubus

# Create mock uci
RUN echo '#!/bin/bash' > /usr/bin/uci && \
    echo 'echo "Mock uci - $*"' >> /usr/bin/uci && \
    chmod +x /usr/bin/uci

# Create mock opkg
RUN echo '#!/bin/bash' > /usr/bin/opkg && \
    echo 'echo "Mock opkg - $*"' >> /usr/bin/opkg && \
    chmod +x /usr/bin/opkg

# Expose ports
EXPOSE 80 443 8080

# Default command
CMD ["/bin/bash"]
EOF

    # Build the Docker image
    print_status "Building OpenWrt test environment..."
    docker build -f "$SCRIPT_DIR/docker/Dockerfile.openwrt-test" -t openwrt-test:latest "$SCRIPT_DIR/docker"

    print_success "Docker OpenWrt environment ready!"
    print_status "To run: docker run -it --rm -v $PROJECT_ROOT:/workdir openwrt-test:latest"
}

# Option 2: QEMU-based OpenWrt VM
setup_qemu_openwrt() {
    print_status "Setting up QEMU-based OpenWrt VM..."

    # Create QEMU setup script
    cat > "$SCRIPT_DIR/qemu-setup.sh" << 'EOF'
#!/bin/bash

# QEMU OpenWrt VM Setup
OPENWRT_VERSION="22.03.5"
ARCH="x86_64"
IMAGE_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/x86/64/openwrt-${OPENWRT_VERSION}-x86-64-generic-ext4-combined.img.gz"

print_status "Downloading OpenWrt ${OPENWRT_VERSION} image..."
wget -O openwrt.img.gz "$IMAGE_URL"
gunzip openwrt.img.gz

print_status "Creating QEMU VM..."
qemu-system-x86_64 \
    -m 512 \
    -smp 2 \
    -drive file=openwrt.img,format=raw \
    -net nic,model=virtio \
    -net user,hostfwd=tcp::2222-:22,hostfwd=tcp::8080-:80 \
    -serial stdio \
    -nographic \
    -enable-kvm

print_status "QEMU VM started!"
print_status "SSH access: ssh root@localhost -p 2222"
print_status "Web interface: http://localhost:8080"
EOF

    chmod +x "$SCRIPT_DIR/qemu-setup.sh"
    print_success "QEMU setup script created!"
    print_status "To run: ./test/qemu-setup.sh"
}

# Option 3: OpenWrt Image Builder
setup_image_builder() {
    print_status "Setting up OpenWrt Image Builder..."

    # Create image builder script
    cat > "$SCRIPT_DIR/image-builder-setup.sh" << 'EOF'
#!/bin/bash

# OpenWrt Image Builder Setup
OPENWRT_VERSION="22.03.5"
ARCH="x86_64"
BUILDER_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/x86/64/openwrt-imagebuilder-${OPENWRT_VERSION}-x86-64.Linux-x86_64.tar.xz"

print_status "Downloading OpenWrt Image Builder..."
wget -O imagebuilder.tar.xz "$BUILDER_URL"
tar -xf imagebuilder.tar.xz
cd openwrt-imagebuilder-${OPENWRT_VERSION}-x86-64.Linux-x86_64

print_status "Building custom OpenWrt image..."
make image PACKAGES="luci luci-base luci-compat mwan3 ubus uci"

print_status "Image built successfully!"
print_status "Output files in bin/targets/x86/64/"
EOF

    chmod +x "$SCRIPT_DIR/image-builder-setup.sh"
    print_success "Image Builder setup script created!"
    print_status "To run: ./test/image-builder-setup.sh"
}

# Option 4: OpenWrt SDK
setup_openwrt_sdk() {
    print_status "Setting up OpenWrt SDK..."

    # Create SDK setup script
    cat > "$SCRIPT_DIR/sdk-setup.sh" << 'EOF'
#!/bin/bash

# OpenWrt SDK Setup
OPENWRT_VERSION="22.03.5"
ARCH="x86_64"
SDK_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/x86/64/openwrt-sdk-${OPENWRT_VERSION}-x86-64_gcc-11.2.0_musl.Linux-x86_64.tar.xz"

print_status "Downloading OpenWrt SDK..."
wget -O sdk.tar.xz "$SDK_URL"
tar -xf sdk.tar.xz
cd openwrt-sdk-${OPENWRT_VERSION}-x86-64_gcc-11.2.0_musl.Linux-x86_64

print_status "Setting up SDK environment..."
source ./staging_dir/toolchain-x86_64_gcc-11.2.0_musl/bin/relocate-sdk.sh

print_status "SDK ready for building packages!"
print_status "To build: make package/autonomy/compile"
EOF

    chmod +x "$SCRIPT_DIR/sdk-setup.sh"
    print_success "SDK setup script created!"
    print_status "To run: ./test/sdk-setup.sh"
}

# Main menu
show_menu() {
    echo ""
    echo "OpenWrt Testing Environment Options:"
    echo "===================================="
    echo "1. Docker-based OpenWrt Simulator (Recommended)"
    echo "2. QEMU-based OpenWrt VM"
    echo "3. OpenWrt Image Builder"
    echo "4. OpenWrt SDK"
    echo "5. Build and test packages"
    echo "6. Exit"
    echo ""
    read -p "Select an option (1-6): " choice
}

# Build and test packages
build_and_test_packages() {
    print_status "Building and testing packages..."

    # Build OpenWrt-compatible packages
    if [ -f "$PROJECT_ROOT/build-openwrt-package.ps1" ]; then
        print_status "Building OpenWrt packages..."
        powershell -ExecutionPolicy Bypass -File "$PROJECT_ROOT/build-openwrt-package.ps1" -Architecture "x86_64"
    else
        print_error "OpenWrt build script not found!"
        return 1
    fi

    # Test package installation
    if check_docker; then
        print_status "Testing package installation in Docker..."
        docker run --rm -v "$BUILD_DIR:/packages" openwrt-test:latest bash -c "
            cd /packages
            echo 'Testing package installation...'
            opkg install autonomy_1.0.0_x86_64.ipk
            opkg install luci-app-autonomy_1.0.0_all.ipk
            echo 'Testing service...'
            /etc/init.d/autonomy start
            /etc/init.d/autonomy status
            echo 'Testing ubus interface...'
            ubus call autonomy status
            echo 'Package test completed!'
        "
    else
        print_warning "Docker not available, skipping package testing"
    fi
}

# Main execution
main() {
    while true; do
        show_menu

        case $choice in
            1)
                if check_docker; then
                    setup_docker_openwrt
                else
                    print_error "Docker not available. Please install Docker first."
                fi
                ;;
            2)
                if check_qemu; then
                    setup_qemu_openwrt
                else
                    print_error "QEMU not available. Please install QEMU first."
                fi
                ;;
            3)
                setup_image_builder
                ;;
            4)
                setup_openwrt_sdk
                ;;
            5)
                build_and_test_packages
                ;;
            6)
                print_success "Exiting..."
                exit 0
                ;;
            *)
                print_error "Invalid option. Please select 1-6."
                ;;
        esac

        echo ""
        read -p "Press Enter to continue..."
    done
}

# Check dependencies
print_status "Checking dependencies..."

if check_docker; then
    print_success "Docker available"
else
    print_warning "Docker not available (recommended for testing)"
fi

if check_qemu; then
    print_success "QEMU available"
else
    print_warning "QEMU not available"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Run main function
main
