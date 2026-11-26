#!/bin/bash

PI_HOSTNAME="${1:-auracle.local}"
# Configuration - your unique ROS domain ID (must match the Pi's zenoh-router.service and ~/.bashrc)
DOMAIN_ID=89

echo "===================================================="
echo "📡 Initializing ROS 2 Jazzy & Zenoh LAN Connection"
echo "===================================================="

# 1. Force ROS 2 to use Zenoh
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
echo "✅ Set RMW_IMPLEMENTATION to rmw_zenoh_cpp"

# 2. Set the unique ROS_DOMAIN_ID (overrides any leftover 0 in the shell)
export ROS_DOMAIN_ID=$DOMAIN_ID
echo "🆔 Active ROS_DOMAIN_ID: $ROS_DOMAIN_ID"

# 3. Resolve the Pi's IP address on the local network (mDNS)
echo "🔍 Searching for your Pi ($PI_HOSTNAME)..."
PI_IP=$(getent ahostsv4 "$PI_HOSTNAME" | awk '{print $1; exit}')

# Fallback if mDNS fails: try to ping it
if [ -z "$PI_IP" ]; then
    PI_IP=$(ping -4 -c 1 "$PI_HOSTNAME" 2>/dev/null | head -n 1 | awk -F'[()]' '{print $2}')
fi

if [ -n "$PI_IP" ]; then
    echo "📌 Found Pi at IP: $PI_IP"
    
    # 4. Point rmw_zenoh at the Pi's router (rmw_zenoh does not use multicast by default)
    # (fixed: the old config file + ZENOH_ROUTER_CONFIG_URI configured the router, not the nodes)
    export ZENOH_CONFIG_OVERRIDE=$(printf 'connect/endpoints=["tcp/%s:7447"]' "$PI_IP")
    echo "🔗 ZENOH_CONFIG_OVERRIDE=$ZENOH_CONFIG_OVERRIDE"
    echo "🚀 Ready! PC will connect to the Pi's Zenoh router at tcp://$PI_IP:7447"
else
    echo "⚠️  Could not resolve '$PI_HOSTNAME' automatically."
    echo "💡 Note: rmw_zenoh does not use multicast by default, so nodes will only look for a router on localhost."
    echo "   Retry with the Pi's IP: source ./connect_pi.sh <pi_ip>"
    unset ZENOH_CONFIG_OVERRIDE
fi

# 5. Restart the ROS 2 CLI daemon so it picks up the new RMW/config
command -v ros2 >/dev/null 2>&1 && ros2 daemon stop >/dev/null 2>&1

echo "===================================================="