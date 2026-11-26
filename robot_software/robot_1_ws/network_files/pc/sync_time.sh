#!/bin/bash

PI_HOSTNAME="${1:-auracle.local}"
PI_USER="auracle"

echo "===================================================="
echo "⏱️  Initializing Master Clock & Client Synchronization"
echo "===================================================="
echo "💻 Host Machine Identified As: $(hostname).local"

# 1. Enforce local Chrony take-over on the PC/WSL instance
echo "🧹 Optimizing Host Time Management Framework..."
if systemctl is-enabled systemd-timesyncd &>/dev/null || systemctl is-active systemd-timesyncd &>/dev/null; then
    echo "🛑 Disabling systemd-timesyncd to prevent clock conflicts..."
    sudo systemctl stop systemd-timesyncd 2>/dev/null
    sudo systemctl disable systemd-timesyncd 2>/dev/null
fi

# 2. Ensure host Chrony daemon is running cleanly
if ! systemctl is-active --quiet chrony; then
    echo "🔄 Starting Chrony server daemon..."
    sudo systemctl restart chrony
fi

# 3. Force PC clock to snap instantly (clears virtualization drift/sleep pauses)
sudo chronyc makestep
echo "✅ Local Chrony server active and tracking correctly."

# 4. Resolve the Pi's IP address on the local network (mDNS)
echo "🔍 Searching for your Pi ($PI_HOSTNAME)..."
PI_IP=$(getent ahostsv4 "$PI_HOSTNAME" | awk '{print $1; exit}')

if [ -z "$PI_IP" ]; then
    PI_IP=$(ping -4 -c 1 "$PI_HOSTNAME" 2>/dev/null | head -n 1 | awk -F'[()]' '{print $2}')
fi

if [ -n "$PI_IP" ]; then
    echo "📌 Found Pi at IP: $PI_IP"

    # The address the Pi must use to reach this PC (the Pi cannot be pointed at itself)
    PC_IP=$(ip -4 route get "$PI_IP" | awk '{for(i=1;i<NF;i++) if($i=="src"){print $(i+1); exit}}')

    # Point the Pi at this PC, then send a fast sync instruction down to the Pi
    echo "⚡ Commanding Raspberry Pi to snap its clock to PC ($PC_IP)..."
    if ssh -t -o ConnectTimeout=5 "$PI_USER@$PI_IP" "echo 'server $PC_IP iburst minpoll 2 maxpoll 4' | sudo tee /etc/chrony/sources.d/pc.sources >/dev/null && sudo chronyc reload sources >/dev/null && sudo chronyc waitsync 20 0 0 1 && sudo chronyc makestep"; then
        echo "🚀 Success! System time loops aligned across machines."
    else
        echo "❌ Pi sync failed (check the ssh login to $PI_USER@$PI_IP, then run debug_time.sh)."
    fi
else
    echo "❌ Error: Could not resolve '$PI_HOSTNAME' automatically via network."
    echo "💡 Please check physical network cables or WiFi connections, or pass the Pi's IP: source ./sync_time.sh <pi_ip>"
fi

echo "===================================================="