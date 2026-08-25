#!/bin/sh
# /usr/bin/aiqos-status - reads /tmp/aiqos_* status files + TC qdisc
# Outputs JSON for ubus/LuCI consumption

# Read status files written by aiqosd every ~1s
scenario=$(cat /tmp/aiqos_scenario 2>/dev/null || echo "unknown")
rsrp=$(cat /tmp/aiqos_rsrp 2>/dev/null || echo "0")
sinr=$(cat /tmp/aiqos_sinr 2>/dev/null || echo "0")
rsrq=$(cat /tmp/aiqos_rsrq 2>/dev/null || echo "0")
target_bw=$(cat /tmp/aiqos_target_bw 2>/dev/null || echo "0")

# Check aiqosd PID
pid=$(pgrep -x aiqosd 2>/dev/null)
if [ -n "$pid" ]; then
    uptime=$(ps -p "$pid" -o etime= 2>/dev/null | sed 's/^ *//')
    running="true"
else
    uptime="N/A"
    running="false"
fi

# Read CAKE stats from TC
cake_dl=$(tc -s qdisc show dev ifb0 2>/dev/null | grep -A2 "qdisc cake" | grep -oP 'rate \K[0-9]+[a-z]+' | head -1)
cake_ul=$(tc -s qdisc show dev wwan0 2>/dev/null | grep -A2 "qdisc cake" | grep -oP 'rate \K[0-9]+[a-z]+' | head -1)

# Read collector PID
collector_pid=$(pgrep -f aiqos-collector 2>/dev/null)
if [ -n "$collector_pid" ]; then
    collector_running="true"
else
    collector_running="false"
fi

cat << JSONEOF
{
    "scenario": "$scenario",
    "rsrp": $rsrp,
    "sinr": $sinr,
    "rsrq": $rsrq,
    "target_bw_kbps": $target_bw,
    "aiqosd_running": $running,
    "aiqosd_pid": "${pid:-0}",
    "aiqosd_uptime": "$uptime",
    "collector_running": $collector_running,
    "cake_dl_rate": "${cake_dl:-N/A}",
    "cake_ul_rate": "${cake_ul:-N/A}"
}
JSONEOF
