#!/bin/bash

# OpenWrt Testing Environment Setup Script
# Run this inside the dedicated WSL instance

echo "Setting up OpenWrt testing environment..."

# Update Ubuntu
sudo apt-get update
sudo apt-get upgrade -y

# Install OpenWrt build dependencies
sudo apt-get install -y \
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
    curl \
    jq

# Clean up package lists
sudo rm -rf /var/lib/apt/lists/*

# Create test directories
sudo mkdir -p /etc/config /var/log /tmp/autonomy /usr/bin

# Create mock OpenWrt environment
sudo bash -c 'echo "config system" > /etc/config/system'
sudo bash -c 'echo "    option hostname \"openwrt-test\"" >> /etc/config/system'
sudo bash -c 'echo "    option timezone \"UTC\"" >> /etc/config/system'

# Create mock network config
sudo bash -c 'echo "config interface \"loopback\"" > /etc/config/network'
sudo bash -c 'echo "    option ifname \"lo\"" >> /etc/config/network'
sudo bash -c 'echo "    option proto \"static\"" >> /etc/config/network'
sudo bash -c 'echo "    option ipaddr \"127.0.0.1\"" >> /etc/config/network'

# Create mock mwan3 config
sudo bash -c 'echo "config globals \"globals\"" > /etc/config/mwan3'
sudo bash -c 'echo "    option mmx_mask \"0x3F00\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option local_source \"lan\"" >> /etc/config/mwan3'

sudo bash -c 'echo "config interface \"wan\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option enabled \"1\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option family \"ipv4\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option track_method \"ping\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option track_ip \"8.8.8.8\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option reliability \"1\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option count \"1\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option timeout \"2\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option interval \"5\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option down \"3\"" >> /etc/config/mwan3'
sudo bash -c 'echo "    option up \"3\"" >> /etc/config/mwan3'

# Create mock commands
sudo bash -c 'echo "#!/bin/bash" > /usr/bin/ubus'
sudo bash -c 'echo "echo \"Mock ubus - \$*\"" >> /usr/bin/ubus'
sudo chmod +x /usr/bin/ubus

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/uci'
sudo bash -c 'echo "echo \"Mock uci - \$*\"" >> /usr/bin/uci'
sudo chmod +x /usr/bin/uci

sudo bash -c 'echo "#!/bin/bash" > /usr/bin/opkg'
sudo bash -c 'echo "echo \"Mock opkg - \$*\"" >> /usr/bin/opkg'
sudo chmod +x /usr/bin/opkg

# Create workspace directory
mkdir -p /workspace

echo "OpenWrt testing environment setup complete!"
echo "This is a dedicated WSL instance for OpenWrt testing"
echo "Your main Ubuntu installation remains untouched"
