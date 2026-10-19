#!/bin/sh
# 系统假死检测守护脚本 v2 (2026-08-28 热更)
# v1: 只检测 CPU/内存/procd/ubusd
# v2: + 天线通路断线自检(4路 RSRP 异常检测,只告警不重启)
LOG_TAG="watchdog"
FAIL_COUNT=0
MAX_FAIL=5
CHECK_INTERVAL=30
ANTENNA_ALERT_THRESHOLD=3   # 连续 N 次检测到天线异常才告警(去抖)

log_msg() {
    logger -t "$LOG_TAG" -p daemon.warning "$1"
    echo "$(date '+%Y-%m-%d %H:%M:%S') $1" >> /tmp/watchdog.log
}

check_system_health() {
    cat /proc/uptime >/dev/null 2>&1 || return 1

    local load=$(cat /proc/loadavg | awk '{print $1}' | cut -d. -f1)
    local cores=$(grep -c ^processor /proc/cpuinfo)
    local max_load=$((cores * 8))
    [ "$load" -gt "$max_load" ] 2>/dev/null && return 1

    pidof procd >/dev/null 2>&1 || return 1
    pidof ubusd >/dev/null 2>&1 || return 1

    local mem_free=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
    [ "$mem_free" -lt 10240 ] 2>/dev/null && return 1

    echo "test" > /tmp/.watchdog_test 2>/dev/null || return 1
    rm -f /tmp/.watchdog_test

    return 0
}

# ============================================================
# 天线通路断线自检 (v2 新增, 2026-08-28 热更)
# 数据源: aiqosd 从 SHM 镜像的 4 路 RSRP (/tmp/aiqos_spatial_rsrp0..3)
# 断线特征:
#   1. 某路 RSRP = 0 或 > -30 (模组读不到该路,通常报 0)
#   2. 某路相对最强路差 > 15dB (单路明显衰减,典型断线/松动)
# 只告警不重启——天线问题重启设备无意义,提示用户物理排查。
# BUGFIX 2026-08-28 15:45: busybox awk 不支持 "d=-85.0--85.0" 连减号
#   (-- 被解析为自减运算符→Unexpected token), 减法改用括号隔离 "d=($max)-($v)"
# ============================================================
ANTENNA_FAIL_COUNT=0

check_antenna_health() {
    local bad=0 msg=""

    # 4 路 RSRP 文件不存在 = aiqosd 未跑,跳过(由系统健康检查兜底)
    [ -f /tmp/aiqos_spatial_rsrp0 ] || return 0

    local a0=$(cat /tmp/aiqos_spatial_rsrp0 2>/dev/null)
    local a1=$(cat /tmp/aiqos_spatial_rsrp1 2>/dev/null)
    local a2=$(cat /tmp/aiqos_spatial_rsrp2 2>/dev/null)
    local a3=$(cat /tmp/aiqos_spatial_rsrp3 2>/dev/null)

    # 任一文件缺失或非数字视为异常
    for v in "$a0" "$a1" "$a2" "$a3"; do
        [ -n "$v" ] || return 1
        case "$v" in
            -*.*|*.*) ;;
            *) return 1 ;;
        esac
    done

    # 特征1: 某路 = 0 或 > -30 (读不到,断线典型)
    for v in "$a0" "$a1" "$a2" "$a3"; do
        _num=$(awk "BEGIN{printf \"%d\", $v}")
        [ "$_num" -gt -30 ] 2>/dev/null && bad=1
    done

    # 特征2: 单路相对最强差 > 15dB
    local max="$a0"
    for v in "$a1" "$a2" "$a3"; do
        _cmp=$(awk "BEGIN{if($v>$max)print 1;else print 0}")
        [ "$_cmp" = "1" ] && max="$v"
    done
    for v in "$a0" "$a1" "$a2" "$a3"; do
        _d=$(awk "BEGIN{d=($max)-($v); if(d<0)d=-d; printf \"%d\", d}")
        [ "$_d" -gt 15 ] 2>/dev/null && bad=1
    done

    [ "$bad" = "1" ] && return 1
    return 0
}

log_msg "Watchdog daemon started (interval=${CHECK_INTERVAL}s, max_fail=${MAX_FAIL}, antenna-check=on)"

while true; do
    if check_system_health; then
        if [ "$FAIL_COUNT" -gt 0 ]; then
            log_msg "System recovered after $FAIL_COUNT failed checks"
            FAIL_COUNT=0
        fi
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        log_msg "Health check FAILED ($FAIL_COUNT/$MAX_FAIL)"

        if [ "$FAIL_COUNT" -ge "$MAX_FAIL" ]; then
            # v68 安全裁决：禁 reboot -f（打断 sysupgrade 写盘 = 半写砖元凶）
            log_msg "System appears hung! ALERT ONLY (no reboot per v68 safety). Manual: power-cycle."
            FAIL_COUNT=0
        fi
    fi

    # 天线自检(独立计数,只告警)
    if check_antenna_health; then
        [ "$ANTENNA_FAIL_COUNT" -gt 0 ] && log_msg "Antenna recovered after $ANTENNA_FAIL_COUNT alerts"
        ANTENNA_FAIL_COUNT=0
    else
        ANTENNA_FAIL_COUNT=$((ANTENNA_FAIL_COUNT + 1))
        if [ "$ANTENNA_FAIL_COUNT" -eq "$ANTENNA_ALERT_THRESHOLD" ]; then
            log_msg "ANTENNA ALERT: 4路 RSRP 异常(断线/松动?), 当前 a0=$(cat /tmp/aiqos_spatial_rsrp0 2>/dev/null) a1=$(cat /tmp/aiqos_spatial_rsrp1 2>/dev/null) a2=$(cat /tmp/aiqos_spatial_rsrp2 2>/dev/null) a3=$(cat /tmp/aiqos_spatial_rsrp3 2>/dev/null). 请检查天线连接"
        fi
    fi

    sleep "$CHECK_INTERVAL"
done
