#!/bin/bash

PI_HOSTNAME="${1:-auracle.local}"
PI_USER="auracle"

echo "===================================================="
echo "🩺 CHRONY PURE TIME DIAGNOSTICS SUITE"
echo "===================================================="

# 1. Check local environment context
if grep -qi microsoft /proc/version; then
    echo "ℹ️  Environment: WSL2 Virtual Machine detected."
    if ! grep -qi "networkingMode=mirrored" /mnt/c/Users/*/.wslconfig 2>/dev/null; then
        echo "⚠️  WARNING: Mirrored mode look missing in .wslconfig! Pi won't see this clock."
    fi
else
    echo "ℹ️  Environment: Native Linux Hardware."
fi

# 2. Check Local Host Chrony Health
echo -e "\n💻 Inspecting Local Master Chrony Server Status..."
if systemctl is-active --quiet chrony; then
    echo "   👉 PC Chrony service status: ACTIVE"
    echo "📊 Host tracking reference layout:"
    chronyc tracking | grep -E "Stratum|System time"
else
    echo "   ❌ FAIL: PC Chrony service is dead. Run 'sudo systemctl restart chrony'."
fi

# 3. Test Pi Network Availability & NTP Ports
echo -e "\n🛰️  Verifying Pi Client Availability..."
PI_IP=$(getent ahostsv4 "$PI_HOSTNAME" | awk '{print $1; exit}')
if [ -n "$PI_IP" ]; then
    echo "   👉 Name Resolution: Success ($PI_HOSTNAME -> $PI_IP)"
    
    if ping -4 -c 2 "$PI_HOSTNAME" &>/dev/null; then
        echo "   👉 Ping Connectivity: Success"
    else
        echo "   ❌ Ping Connectivity: Pi is not dropping packets back safely."
    fi
else
    echo "   ❌ Name Resolution: Failed to resolve '$PI_HOSTNAME'."
fi

# 4. Check Remote Chrony Sync Tracking on Pi via SSH
echo -e "\n📊 Polling Pi Remote Synchronization Metrics..."
PI_SYNC=$(ssh -o ConnectTimeout=3 "$PI_USER@${PI_IP:-$PI_HOSTNAME}" "chronyc tracking" 2>/dev/null)

if [ -n "$PI_SYNC" ]; then
    OFFSET=$(echo "$PI_SYNC" | grep "System time" | sed 's/^[^:]*: *//')
    STRATUM=$(echo "$PI_SYNC" | grep "Stratum" | awk '{print $3}')
    REF_ID=$(echo "$PI_SYNC" | grep "Reference ID" | awk '{print $4, $5}')
    
    echo "   👉 Pi Stratum Rank: $STRATUM"
    echo "   👉 Target Reference Master ID: $REF_ID"
    echo "   👉 Time Offset from Master Clock: $OFFSET"
    
    if echo "$PI_SYNC" | grep -q "Leap status     : Normal"; then
        echo "   🎉 SUCCESS: Clock synchronization loop is completely healthy!"
    else
        echo "   ⚠️  WARNING: Sync loops are tracking but not fully locked."
    fi
else
    echo "   ❌ FAIL: Could not log in via SSH to read clock registers directly."
    echo "      Windows Firewall or an offline network block might be swallowing NTP traffic."
fi

echo "===================================================="