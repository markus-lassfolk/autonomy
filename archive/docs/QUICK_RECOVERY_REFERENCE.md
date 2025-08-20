# Quick Reference: Critical Information Needed

## WireGuard Configuration
You need to provide these details to restore your VPN connection:

### 1. Private Key for RUTX50
```
Your RUTX50's WireGuard private key
Format: Base64 string (44 characters)
Example: aBcD1234eFgH5678+ijKl9012MnOp3456QrSt7890UvWx=
```

### 2. Klara Network Public Key
```
The public key of your Klara network WireGuard peer
Format: Base64 string (44 characters)
Example: XyZ9876wVuT5432+SrQp1098OnMl7654KjIh3210GfEd=
```

### 3. Klara Network Endpoint
```
IP address and port of your Klara network
Format: IP:PORT
Example: 203.0.113.100:51820
```

### 4. VPN Network Configuration
```
Local VPN IP for RUTX50: 10.0.0.2/24 (or your specific IP)
Networks to route through VPN:
- 192.168.1.0/24 (primary Klara network)
- 192.168.20.0/24 (secondary network)
- Any additional networks you mentioned
```

## Network Layout Reminder

```
RUTX50 Router (192.168.0.1/24)
├── WAN (Starlink) → 192.168.100.1
├── Cellular (mob1s1a1) → Backup internet
├── LAN → 192.168.0.0/24 (local devices)
└── VPN (wg_klara) → Routes to:
    ├── 192.168.1.0/24 (Klara primary)
    ├── 192.168.20.0/24 (Klara secondary)
    └── [Additional network if any]
```

## Quick Recovery Steps

1. **Run the recovery script:**
   ```bash
   ./scripts/rutos-factory-reset-recovery-rutos.sh
   ```

2. **When prompted, provide:**
   - WireGuard private key (keep this secure!)
   - Klara public key
   - Klara endpoint IP:port
   - Network ranges

3. **The script will automatically:**
   - Install WireGuard
   - Configure VPN tunnel
   - Set up selective routing
   - Configure firewall rules
   - Setup MWAN3 failover
   - Ensure Starlink access from all interfaces

## Manual Commands (if needed)

### Check WireGuard Status
```bash
wg show
ip addr show wg_klara
```

### Check MWAN3 Status
```bash
mwan3 status
```

### Test Starlink Access
```bash
ping -c 3 192.168.100.1
```

### Test VPN Connectivity
```bash
ping -c 3 192.168.1.1  # or your Klara gateway
```

## Firewall Rules Summary
The script will create these essential rules:
- Allow WireGuard port (51820/udp) from WAN
- Forward traffic between LAN ↔ VPN zones
- Allow LAN/VPN → Starlink management (192.168.100.1)
- Block unwanted traffic while allowing necessary routing

## Missing Information?
If you're missing any WireGuard details, you'll need to:
1. Check your Klara network documentation
2. Generate new keys if the old ones are lost
3. Contact your Klara network administrator

The recovery script will guide you through each step!
