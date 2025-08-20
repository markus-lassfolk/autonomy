# RUTX50 Factory Reset Recovery Guide

## 1. WireGuard VPN Configuration

### Required Information (You Need to Provide)
- **WireGuard Private Key** (for this RUTX50 device)
- **WireGuard Public Key** (for the remote Klara network peer)
- **Remote Endpoint** (IP address and port of Klara network)
- **Allowed IPs** for the tunnel
- **Local VPN IP** for this device (e.g., 10.0.0.2/24)

### Basic WireGuard Setup Commands
```bash
# 1. Install WireGuard (if not already installed)
opkg update
opkg install wireguard-tools kmod-wireguard

# 2. Create WireGuard interface
uci set network.wg_klara=interface
uci set network.wg_klara.proto='wireguard'
uci set network.wg_klara.private_key='YOUR_PRIVATE_KEY_HERE'
uci set network.wg_klara.listen_port='51820'
uci set network.wg_klara.addresses='10.0.0.2/24'  # Adjust as needed

# 3. Add peer configuration
uci add network wireguard_wg_klara
uci set network.@wireguard_wg_klara[0]=wireguard_wg_klara
uci set network.@wireguard_wg_klara[0].public_key='PEER_PUBLIC_KEY_HERE'
uci set network.@wireguard_wg_klara[0].endpoint_host='KLARA_ENDPOINT_IP'
uci set network.@wireguard_wg_klara[0].endpoint_port='51820'
uci set network.@wireguard_wg_klara[0].allowed_ips='192.168.1.0/24 192.168.20.0/24'
uci set network.@wireguard_wg_klara[0].persistent_keepalive='25'

# 4. Commit changes
uci commit network
/etc/init.d/network restart
```

## 2. Routing Configuration

### Selective Routing Rules
```bash
# Add custom routing table for VPN
echo "200 vpn_table" >> /etc/iproute2/rt_tables

# Route specific networks through VPN
ip route add 192.168.1.0/24 dev wg_klara table vpn_table
ip route add 192.168.20.0/24 dev wg_klara table vpn_table

# Add routing rules
ip rule add from 192.168.0.0/16 to 192.168.1.0/24 table vpn_table
ip rule add from 192.168.0.0/16 to 192.168.20.0/24 table vpn_table
```

## 3. Firewall Rules

### Required Firewall Zones and Rules
```bash
# 1. Create VPN firewall zone
uci set firewall.vpn_zone=zone
uci set firewall.vpn_zone.name='vpn'
uci set firewall.vpn_zone.input='ACCEPT'
uci set firewall.vpn_zone.output='ACCEPT'
uci set firewall.vpn_zone.forward='ACCEPT'
uci set firewall.vpn_zone.network='wg_klara'

# 2. Allow VPN to LAN
uci add firewall forwarding
uci set firewall.@forwarding[-1].src='vpn'
uci set firewall.@forwarding[-1].dest='lan'

# 3. Allow LAN to VPN
uci add firewall forwarding
uci set firewall.@forwarding[-1].src='lan'
uci set firewall.@forwarding[-1].dest='vpn'

# 4. Allow WireGuard port
uci add firewall rule
uci set firewall.@rule[-1].name='Allow-WireGuard'
uci set firewall.@rule[-1].src='wan'
uci set firewall.@rule[-1].dest_port='51820'
uci set firewall.@rule[-1].proto='udp'
uci set firewall.@rule[-1].target='ACCEPT'

# Commit firewall changes
uci commit firewall
/etc/init.d/firewall restart
```

## 4. Starlink Access (192.168.100.1)

### Direct Route to Starlink Management
```bash
# Add persistent route to Starlink regardless of MWAN3 status
ip route add 192.168.100.1/32 dev wan metric 1

# Add firewall rule to allow access
uci add firewall rule
uci set firewall.@rule[-1].name='Allow-Starlink-Access'
uci set firewall.@rule[-1].src='lan'
uci set firewall.@rule[-1].dest='wan'
uci set firewall.@rule[-1].dest_ip='192.168.100.1'
uci set firewall.@rule[-1].target='ACCEPT'

uci commit firewall
/etc/init.d/firewall restart
```

## 5. MWAN3 Configuration

### Basic Multi-WAN Setup
```bash
# Configure interfaces
uci set mwan3.wan.enabled='1'
uci set mwan3.wan.initial_state='online'
uci set mwan3.wan.family='ipv4'
uci set mwan3.wan.track_method='ping'
uci set mwan3.wan.track_hosts='8.8.8.8 1.1.1.1'
uci set mwan3.wan.ping_timeout='4'
uci set mwan3.wan.ping_interval='10'
uci set mwan3.wan.interface='wan'

# Configure cellular
uci set mwan3.mob1s1a1.enabled='1'
uci set mwan3.mob1s1a1.initial_state='online'
uci set mwan3.mob1s1a1.family='ipv4'
uci set mwan3.mob1s1a1.track_method='ping'
uci set mwan3.mob1s1a1.track_hosts='8.8.8.8'
uci set mwan3.mob1s1a1.ping_timeout='4'
uci set mwan3.mob1s1a1.ping_interval='60'
uci set mwan3.mob1s1a1.interface='mob1s1a1'

# Configure VPN
uci set mwan3.wg_klara.enabled='1'
uci set mwan3.wg_klara.initial_state='online'
uci set mwan3.wg_klara.family='ipv4'
uci set mwan3.wg_klara.track_method='ping'
uci set mwan3.wg_klara.track_hosts='192.168.1.1'
uci set mwan3.wg_klara.ping_timeout='4'
uci set mwan3.wg_klara.ping_interval='30'
uci set mwan3.wg_klara.interface='wg_klara'

uci commit mwan3
/etc/init.d/mwan3 restart
```

## Information Needed From You

Please provide the following to complete the setup:

1. **WireGuard Private Key** for this RUTX50
2. **WireGuard Public Key** for the Klara network peer
3. **Klara Network Endpoint** (IP:Port)
4. **VPN IP ranges** that should be routed through the tunnel
5. **Local VPN IP** for this device
6. **Any additional networks** that need special routing (you mentioned there might be one more)

## Next Steps

1. Gather the WireGuard configuration details
2. Run the automated setup script (I'll create this)
3. Test connectivity step by step
4. Deploy the monitoring and failover system

Would you like me to create an automated setup script that prompts for the required information and configures everything?
