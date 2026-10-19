#!/bin/sh
# v6.19 (2026-09-19) **越界丢弃**：CESQ/GTCCINFO 偶发错位/粘连解析值不再放行——
#   RSRP 限 [-157,-44]dBm（raw 0..97 ⇒ raw-157，上限放宽容错）、RSRQ 限 [-43.5,-3]、
#   SINR 限 [-23.5,40]、天线 raw ≤120（⇒ ≥-36dBm 不可能）。越界 ⇒ 改判 "--"，
#   走 v6.16 stale-keep（沿用上一帧、不污染平滑器）。
#   实证：2026-09-19 22:21 帧 RSRP -28 dBm / SINR 67.6 直达面板（物理不可能值）。
# v6.18 (2026-09-15) **4 路天线真值恢复**: 官方《广和通模组常用AT指令含义及操作功能V4.0》
#   r21 明确定义 AT+GTCCINFO? 末 4 字段 = Ant0,Ant1,Ant2,Ant3（4 路天线 RSRP 原始编码值，
#   换算 RSRP_dBm = 编码值 − 156）。⇒ v6.17 的「pos11-14 = SINR/RxLev/RSRP/RSRQ，不是天线」
#   判断**有误**（当时无官方手册依据），现按官方定义解析并置 antSource="GTCCINFO"。
#   （AT+QRSRP 在本模组返回 ERROR，故用 GTCCINFO 的 Ant0~3。）
# v6.17 (2026-09-12) 4 路天线判定（**本条已被 v6.18 取代**）: 曾判 pos11-14 非天线 ⇒ 统一回退主 RSRP。
#   （旧头注曾写 "Ant0-3 real 4x antenna RSRP" —— 那是错的, 已随本条更正）
# v6.16: NR5G-SA/NSA normalize; TAC hex->dec
# 版本历史: v6.9 初版头(未随版本更新) / v6.14 speed_loop 0.5s 独立速率进程 / v6.15 QCI 联通兜底+CESQ tr -dc 清洗 / v6.16 (9/11) RSRP/RSRQ/SINR CESQ stale 保持
# 注: 版本号判据以 md5 为准（本头曾长期停 v6.9 致误判, 9/11 重构修正）
OUT="/tmp/modem_status.json"
OUT_TMP="/tmp/modem_status.json.tmp"
CACHE="/tmp/last_good_modem.json"
INTERVAL=30
SMOOTH_DIR=/tmp/modem_smooth
STAT_DIR=/tmp/modem_stat
mkdir -p "$SMOOTH_DIR" "$STAT_DIR"
LOCK_FILE="/tmp/qmi_status_collector.lock"
exec 9>"$LOCK_FILE"
flock -n 9 || exit 0

send_at() {
    sendat 2 "AT" >/dev/null 2>&1
    sleep 1
    sendat 2 "$1" 2>/dev/null
}

smooth() {
    local new="$1" old="$2" a_num="$3" a_den="$4"
    if [ -z "$old" ] || [ "$old" = "--" ]; then echo "$new"
    else awk "BEGIN {v=($a_num*$new+($a_den-$a_num)*$old)/$a_den; printf \"%.1f\",v}"; fi
}

load_cache() {
    local key="$1" fb="$2"
    if [ -f "$CACHE" ] && [ -s "$CACHE" ]; then
        local v=$(grep -o "\"$key\":\"[^\"]*\"" "$CACHE" | head -1 | cut -d'"' -f4)
        [ -n "$v" ] && [ "$v" != "--" ] && [ "$v" != "N/A" ] && echo "$v" && return
    fi
    echo "$fb"
}

IFACE="wwan0"

collect_once() {
    NOW_TS=$(date +%s)

    CESQ_RAW=$(send_at "AT+CESQ")
    CESQ_LINE=$(echo "$CESQ_RAW" | grep "+CESQ:" | tr -d "\r" | sed "s/+CESQ: //")
    NR_RSRQ_RAW=$(echo "$CESQ_LINE" | cut -d"," -f7 | tr -dc "0-9")
    NR_RSRP_RAW=$(echo "$CESQ_LINE" | cut -d"," -f8 | tr -dc "0-9")
    NR_SINR_RAW=$(echo "$CESQ_LINE" | cut -d"," -f9 | tr -dc "0-9")
    RSRP="--"; RSRQ="--"; SINR="--"
    if [ -n "$NR_RSRP_RAW" ] && [ "$NR_RSRP_RAW" != "99" ] && [ "$NR_RSRP_RAW" != "255" ]; then
        RSRP=$((NR_RSRP_RAW - 157))
    fi
    if [ -n "$NR_RSRQ_RAW" ] && [ "$NR_RSRQ_RAW" != "99" ] && [ "$NR_RSRQ_RAW" != "255" ]; then
        RSRQ=$(awk "BEGIN {printf \"%.1f\", $NR_RSRQ_RAW * 0.5 - 43.5}")
    fi
    if [ -n "$NR_SINR_RAW" ] && [ "$NR_SINR_RAW" != "99" ] && [ "$NR_SINR_RAW" != "255" ]; then
        SINR=$(awk "BEGIN {printf \"%.1f\", $NR_SINR_RAW * 0.5 - 23.5}")
    fi

    # v6.19 越界丢弃（实证 2026-09-19 22:21 帧：错位解析产出 RSRP -28dBm 等物理不可能值）：
    #   改判 "--" ⇒ 走 v6.16 stale-keep（沿用上一帧、不进平滑器），面板/屏保不再见不可能值。
    case "$RSRP" in
        --) ;;
        *)  [ "$RSRP" -ge -157 ] && [ "$RSRP" -le -44 ] || RSRP="--" ;;
    esac
    if [ "$RSRQ" != "--" ]; then
        awk "BEGIN{if (!($RSRQ >= -43.5 && $RSRQ <= -3.0)) exit 1}" || RSRQ="--"
    fi
    if [ "$SINR" != "--" ]; then
        awk "BEGIN{if (!($SINR >= -23.5 && $SINR <= 40.0)) exit 1}" || SINR="--"
    fi

    GTCC_RAW=$(send_at "AT+GTCCINFO?")
    GTCC_LINE=$(echo "$GTCC_RAW" | grep -E "^[0-9]+,[0-9]+,[0-9]+" | head -1 | tr -d "\r")
    MCC="--"; MNC="--"; TAC="--"; CELLID="--"; PCI="--"; ARFCN="--"; BAND="--"; BW="--"
    ANT_MAIN="--"; ANT_DIV="--"; ANT_MIMO1="--"; ANT_MIMO2="--"
    ANT_SOURCE="fallback"   # 解析失败时的标注；成功解析 GTCCINFO Ant0~3 后改为 "GTCCINFO"
    if [ -n "$GTCC_LINE" ]; then
        MCC=$(echo "$GTCC_LINE" | cut -d"," -f3)
        MNC=$(echo "$GTCC_LINE" | cut -d"," -f4)
        TAC_HEX=$(echo "$GTCC_LINE" | cut -d"," -f5)
        TAC=$(printf "%d" "0x$TAC_HEX" 2>/dev/null || echo "$TAC_HEX")
        CELLID_HEX=$(echo "$GTCC_LINE" | cut -d"," -f6)
        ARFCN_HEX=$(echo "$GTCC_LINE" | cut -d"," -f7)
        PCI_HEX=$(echo "$GTCC_LINE" | cut -d"," -f8)
        BAND_RAW=$(echo "$GTCC_LINE" | cut -d"," -f9)
        BW=$(echo "$GTCC_LINE" | cut -d"," -f10)
        [ -n "$CELLID_HEX" ] && CELLID=$(printf "%d" "0x$CELLID_HEX" 2>/dev/null || echo "$CELLID_HEX")
        [ -n "$ARFCN_HEX" ] && ARFCN=$(printf "%d" "0x$ARFCN_HEX" 2>/dev/null || echo "$ARFCN_HEX")
        [ -n "$PCI_HEX" ] && PCI=$(printf "%d" "0x$PCI_HEX" 2>/dev/null || echo "$PCI_HEX")
        if [ -n "$BAND_RAW" ] && [ ${#BAND_RAW} -gt 1 ]; then
            BAND_NUM=$(echo "$BAND_RAW" | sed 's/^[0-9]//')
            BAND="n${BAND_NUM#0}"
        fi
        [ -n "$BW" ] && BW="${BW}MHz"
        # v6.18 (2026-09-15) 4 路天线真值恢复（官方手册 r21 定义）：
        #   AT+GTCCINFO? 末 4 字段 = Ant0,Ant1,Ant2,Ant3（RSRP 原始编码值，RSRP_dBm = 值 − 156）
        #   ⇒ 解析为 4 路真实 RSRP；成功后 antSource="GTCCINFO"，失败则保留 "fallback"。
        _A0=$(echo "$GTCC_LINE" | cut -d"," -f11)
        _A1=$(echo "$GTCC_LINE" | cut -d"," -f12)
        _A2=$(echo "$GTCC_LINE" | cut -d"," -f13)
        _A3=$(echo "$GTCC_LINE" | cut -d"," -f14)
        # v6.19：raw 上界 120（⇒ 换算后 ≥-36dBm 均不可能），防粘连值混入天线分集
        _ant_dbm() { [ -n "$1" ] && [ "$1" -gt 0 ] 2>/dev/null && [ "$1" -le 120 ] 2>/dev/null && awk "BEGIN{printf \"%.1f\", $1 - 156}"; }
        _m=$(_ant_dbm "$_A0"); _d=$(_ant_dbm "$_A1"); _m1=$(_ant_dbm "$_A2"); _m2=$(_ant_dbm "$_A3")
        # 仅当 4 路全部解析成功（非空）才采用，避免半残数据
        if [ -n "$_m" ] && [ -n "$_d" ] && [ -n "$_m1" ] && [ -n "$_m2" ]; then
            ANT_MAIN="$_m"; ANT_DIV="$_d"; ANT_MIMO1="$_m1"; ANT_MIMO2="$_m2"
            ANT_SOURCE="GTCCINFO"
        fi
        # —— 以下为 v6.17 旧注（已被 v6.18 取代，保留供追溯）——
        # v64.5 (2026-09-12) P0 修复：**删除 pos11-14 → Ant0..Ant3 的误读**。
        # 真机 AT+GTCCINFO? 报文恰好 14 字段，pos11-14 = SINR / RXLEV / RSRP / RSRQ，
        # **报文里没有任何天线数据**（旧代码把 4 个不同物理量的 raw-156 当天线 RSRP 用）。
        # 追溯：8-28 曾出现「4 路同值」并被误判为"QMI 有真值"，实为同一处误读（循环论证）。
        # 本模组 AT 侧无任何天线命令（AT+QRSRP?/QNRSRP?/QENG/QCAU 全 ERROR）；
        # QMI 侧用户态被拨号守护独占（uqmi 挂起）、qmicli 与 libqmi 版本不匹配已损坏。
        # ⇒ 4 路值统一在下方（RSRP 平滑之后）回退主 RSRP，并以 antSource="fallback" 标注。
    else
        BAND=$(load_cache "band" "--")
        BW=$(load_cache "bandwidth" "--")
        PCI=$(load_cache "pci" "--")
        ARFCN=$(load_cache "arfcn" "--")
        CELLID=$(load_cache "cellId" "--")
        TAC=$(load_cache "tac" "--")
    fi

    OPERATOR="--"; OPERATOR_ZH="--"
    if [ -n "$MCC" ] && [ "$MCC" != "--" ]; then
        PLMN="${MCC}${MNC}"
        case "$PLMN" in
            46000|46002|46007|46008) OPERATOR="CMCC"; OPERATOR_ZH="中国移动" ;;
            46001|46006|46009)       OPERATOR="CUCC"; OPERATOR_ZH="中国联通" ;;
            46003|46005|46011)       OPERATOR="CTCC"; OPERATOR_ZH="中国电信" ;;
            46015|46004|46048)       OPERATOR="CBN";  OPERATOR_ZH="中国广电" ;;
            *)                       OPERATOR="$PLMN"; OPERATOR_ZH="$PLMN" ;;
        esac
    fi

    NET_MODE=$(send_at "AT+PSRAT?")
    NET_MODE=$(echo "$NET_MODE" | grep "+PSRAT:" | sed 's/+PSRAT: //' | tr -d '"' | tr -d '\r')
    # normalize to SA/NSA using GTCCINFO service-cell RAT line
    _rat=$(echo "$GTCC_RAW" | grep -i "service cell" | head -1 | awk '{print $1}' | tr -d '\r')
    case "$_rat" in
        NR) NET_MODE="NR5G-SA" ;;
        LTE-NR) NET_MODE="NR5G-NSA" ;;
        LTE) NET_MODE="LTE" ;;
    esac
    [ -z "$NET_MODE" ] && NET_MODE=$(load_cache "networkMode" "--")

    QCI="--"
    CGEQOS_RAW=$(send_at "AT+CGEQOS?")
    if [ -n "$CGEQOS_RAW" ]; then
        QCI_LINE=$(echo "$CGEQOS_RAW" | grep "+CGEQOS:" | head -1 | tr -d "\r")
        if [ -n "$QCI_LINE" ]; then
            _qci_val=$(echo "$QCI_LINE" | sed 's/+CGEQOS: //' | cut -d"," -f2 | tr -d " ")
            [ -n "$_qci_val" ] && [ "$_qci_val" -gt 0 ] 2>/dev/null && QCI="$_qci_val"
        fi
    fi
    [ "$QCI" = "--" ] && QCI=$(load_cache "qci" "--")
    # v6.12: CUCC(46001/06/09) NR SA never delivers QOS params (CGEQOS empty).
    # 2026-09-07 账号侧权威查询(unicomvue/net.2t.hk 官方API, 用户截图): 普通联通卡 QCI=9 (500Mbps 档)。
    # 联通分级: VVIP=QCI6 / VIP=QCI8 / 普通=QCI9(默认承载)。v6.11 的"6"系误记, 修正为 9。
    if [ "$QCI" = "--" ] && [ "$OPERATOR" = "CUCC" ]; then QCI="9"; fi
    # BUGFIX 2026-09-07: 其余运营商 NR 组网 CGEQOS 亦可能为空(联通实测, CGEQOSRDR/CGDSCI
    # 均 ERROR, 广和通 V4.0 手册无替代查询命令)。互联网默认承载 5QI=9 (3GPP TS 23.501 表5.7.4-1)。
    if [ "$QCI" = "--" ]; then
        case "$NET_MODE" in
            NR5G-SA|NR5G-NSA) QCI="9" ;;
        esac
    fi

    TEMP="--"
    TEMP_RAW=$(send_at "AT+QTEMP")
    TEMP_LINE=$(echo "$TEMP_RAW" | grep "+QTEMP:" | head -1 | tr -d "\r")
    if [ -n "$TEMP_LINE" ]; then
        _t=$(echo "$TEMP_LINE" | sed 's/+QTEMP: //' | cut -d, -f1 | tr -d ' "')
        [ -n "$_t" ] && TEMP="$_t"
    fi
    [ "$TEMP" = "--" ] && TEMP=$(load_cache "temperature" "--")
    if [ "$TEMP" = "--" ]; then
        _t=$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null)
        [ -n "$_t" ] && TEMP=$(awk "BEGIN {printf \"%.1f\", $_t/1000}")
    fi

    IMEI="--"
    IMEI_RAW=$(send_at "AT+CGSN")
    _imei=$(echo "$IMEI_RAW" | grep -oE "[0-9]{15}" | head -1)
    [ -n "$_imei" ] && IMEI="$_imei"
    [ "$IMEI" = "--" ] && IMEI=$(load_cache "imei" "--")

    # v6.15: 电压 AT+CBC
    VOLT="--"
    CBC_RAW=$(send_at "AT+CBC")
    VOLT_LINE=$(echo "$CBC_RAW" | grep "+CBC:" | head -1 | tr -d "\r")
    if [ -n "$VOLT_LINE" ]; then
        _volt=$(echo "$VOLT_LINE" | sed 's/+CBC: //' | awk -F, '{printf "%.2f", $2/1000}')
        [ -n "$_volt" ] && [ "$_volt" != "0.00" ] && VOLT="$_volt"
    fi

    # v6.15: 载波聚合 AT+GTCAINFO
    # v64.5 (2026-09-12) B13 修复：按**真机格式**解析 —— 旧实现 `CA_BAND/CA_ARFCN/CA_PCI/CA_BW`
    #   只被初始化、**从未被赋值** ⇒ JSON 里恒为空串，面板「载波聚合」永远显示 -- / --。
    # 真机报文（2026-09-12 02:0x 实测，FM170-EAU / 联通 n78 单载波）：
    #   +GTCAINFO:
    #   PCC: 5078,720,633984,100,1,,1,3,76
    # 格式 = <PCC|SCC>: <bandCode>,<pci 十进制>,<arfcn 十进制>,<dlBwMHz>,...
    #   · bandCode − 5000 = n 号（5078 → n78）
    #   · PCI / ARFCN 是**十进制**（不是 hex！照 hex 解析会得到 1824 / 6506884）
    #   · 出现 `SCC:` 行 = 真载波聚合；否则单载波
    # ⚠️ 本版只解析 **PCC**（已有真机样本可验证）；**SCC 明细未解析**（无样本，待 CA 激活后补），
    #    但 SCC 的**有无**已用于 caMode 判定。
    CA_MODE="none"; CA_BAND=""; CA_ARFCN=""; CA_PCI=""; CA_BW=""
    GTCA_RAW=$(send_at "AT+GTCAINFO?")
    _ca_norm=$(echo "$GTCA_RAW" | tr -d "\r")
    _ca_pcc=$(echo "$_ca_norm" | grep "PCC:" | head -1)
    _ca_scc=$(echo "$_ca_norm" | grep -c "SCC:")
    if [ -n "$_ca_pcc" ]; then
        _ca_v=$(echo "$_ca_pcc" | sed "s/.*PCC: *//")
        _ca_band=$(echo "$_ca_v" | cut -d"," -f1 | tr -dc "0-9")
        _ca_pci_v=$(echo "$_ca_v" | cut -d"," -f2 | tr -dc "0-9")
        _ca_arfcn_v=$(echo "$_ca_v" | cut -d"," -f3 | tr -dc "0-9")
        _ca_bw_v=$(echo "$_ca_v" | cut -d"," -f4 | tr -dc "0-9")
        if [ -n "$_ca_band" ]; then
            if [ "$_ca_band" -gt 5000 ] 2>/dev/null; then
                CA_BAND="n$((_ca_band - 5000))"
            else
                CA_BAND="$_ca_band"
            fi
        fi
        [ -n "$_ca_pci_v" ] && CA_PCI="$_ca_pci_v"
        [ -n "$_ca_arfcn_v" ] && CA_ARFCN="$_ca_arfcn_v"
        [ -n "$_ca_bw_v" ] && CA_BW="${_ca_bw_v}MHz"
        if [ "$_ca_scc" -gt 0 ] 2>/dev/null; then CA_MODE="ca"; else CA_MODE="single"; fi
    fi

    RX_BYTES=$(cat /sys/class/net/$IFACE/statistics/rx_bytes 2>/dev/null)
    TX_BYTES=$(cat /sys/class/net/$IFACE/statistics/tx_bytes 2>/dev/null)
    [ -z "$RX_BYTES" ] && RX_BYTES=0
    [ -z "$TX_BYTES" ] && TX_BYTES=0
    PREV_RX=$(cat "$STAT_DIR/rx_bytes" 2>/dev/null)
    PREV_TX=$(cat "$STAT_DIR/tx_bytes" 2>/dev/null)
    PREV_TS=$(cat "$STAT_DIR/ts" 2>/dev/null)
    DL_SPEED=0; UL_SPEED=0
    if [ -n "$PREV_TS" ] && [ "$PREV_TS" -gt 0 ] 2>/dev/null; then
        ELAPSED=$((NOW_TS - PREV_TS))
        if [ "$ELAPSED" -gt 0 ] 2>/dev/null; then
            [ -n "$PREV_RX" ] && [ "$RX_BYTES" -ge "$PREV_RX" ] 2>/dev/null && DL_SPEED=$(( (RX_BYTES - PREV_RX) * 8 / ELAPSED / 1000000 ))
            [ -n "$PREV_TX" ] && [ "$TX_BYTES" -ge "$PREV_TX" ] 2>/dev/null && UL_SPEED=$(( (TX_BYTES - PREV_TX) * 8 / ELAPSED / 1000000 ))
        fi
    fi
    echo "$RX_BYTES" > "$STAT_DIR/rx_bytes"
    echo "$TX_BYTES" > "$STAT_DIR/tx_bytes"
    echo "$NOW_TS" > "$STAT_DIR/ts"

    CARRIER=$(cat /sys/class/net/$IFACE/carrier 2>/dev/null)
    [ "$CARRIER" = "1" ] && NET_STATUS="connected" || NET_STATUS="disconnected"
    # Get IP from modem via AT+CGPADDR (wwan0 may not have IP assigned)
    CGPADDR_RAW=$(sendat 2 "AT+CGPADDR" 2>/dev/null)
    CGPADDR_LINE=$(echo "$CGPADDR_RAW" | grep "+CGPADDR: 1," | tr -d "\r")
    IPV4="--"; IPV6="--"
    # v70 M2（2026-09-19 大审计）：rxlev 此前从未赋值就进 JSON（恒空串，B13 同类病灶）。
    #   本模组无 LTE rxlev 数据源（NR 模式 CESQ 无此语义），显式置 "--"。
    RXLEV="--"
    if [ -n "$CGPADDR_LINE" ]; then
        _ipv4_raw=$(echo "$CGPADDR_LINE" | awk -F'"' '{print $2}')
        _ipv6_raw=$(echo "$CGPADDR_LINE" | awk -F'"' '{print $4}')
        [ -n "$_ipv4_raw" ] && [ "$_ipv4_raw" != "0.0.0.0" ] && IPV4="$_ipv4_raw"
        if [ -n "$_ipv6_raw" ]; then
            if echo "$_ipv6_raw" | grep -q '\.'; then
                IPV6=$(echo "$_ipv6_raw" | awk -F. '{for(i=1;i<=NF;i++)h=sprintf("%s%02x",h,$i);for(i=1;i<=8;i++)printf "%s%s",substr(h,(i-1)*4+1,4),(i<8?":":"")}')
            elif [ "$_ipv6_raw" != "0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0" ]; then
                IPV6="$_ipv6_raw"
            fi
        fi
    fi
    # v70 M3（2026-09-19 大审计）：本行原缩进在 `if [ -n "$_ipv6_raw" ]` 块**内** ⇒
    #   CGPADDR 只回报 IPv4（无 IPv6 字段）时 carrier 不一致回退永不生效。移到块外。
    [ "$NET_STATUS" != "connected" ] && [ "$IPV4" != "--" ] && NET_STATUS="connected"

    OLD_RSRP=$(cat "$SMOOTH_DIR/rsrp" 2>/dev/null)
    OLD_RSRQ=$(cat "$SMOOTH_DIR/rsrq" 2>/dev/null)
    OLD_SINR=$(cat "$SMOOTH_DIR/sinr" 2>/dev/null)
    # v6.16 (2026-09-11): stale 保持——CESQ 偶发解析失败('--')时保留上次有效值
    # (对齐 Go r4 lastValid 机制; 根治面板 RSRQ '--'/进度条 0% 闪烁)
    if [ "$RSRP" != "--" ]; then
        RSRP=$(smooth "$RSRP" "$OLD_RSRP" 3 10) && echo "$RSRP" > "$SMOOTH_DIR/rsrp"
    elif [ -n "$OLD_RSRP" ] && [ "$OLD_RSRP" != "--" ]; then
        RSRP="$OLD_RSRP"
    fi
    if [ "$RSRQ" != "--" ]; then
        RSRQ=$(smooth "$RSRQ" "$OLD_RSRQ" 3 10) && echo "$RSRQ" > "$SMOOTH_DIR/rsrq"
    elif [ -n "$OLD_RSRQ" ] && [ "$OLD_RSRQ" != "--" ]; then
        RSRQ="$OLD_RSRQ"
    fi
    if [ "$SINR" != "--" ]; then
        SINR=$(smooth "$SINR" "$OLD_SINR" 3 10) && echo "$SINR" > "$SMOOTH_DIR/sinr"
    elif [ -n "$OLD_SINR" ] && [ "$OLD_SINR" != "--" ]; then
        SINR="$OLD_SINR"
    fi

    # v6.18 (2026-09-15) 天线数据源：优先使用 GTCCINFO 的 Ant0~Ant3（官方手册 r21 定义，见上方解析）。
    #   仅当解析失败（ANT_SOURCE != "GTCCINFO"）时，才回退主 RSRP 并如实标注 "fallback"。
    if [ "$ANT_SOURCE" != "GTCCINFO" ]; then
        ANT_MAIN="$RSRP"; ANT_DIV="$RSRP"; ANT_MIMO1="$RSRP"; ANT_MIMO2="$RSRP"
        ANT_SOURCE="fallback"
    fi

    cat > "$OUT_TMP" << EOF
{
  "networkStatus": "$NET_STATUS",
  "networkMode": "$NET_MODE",
  "operator": "$OPERATOR",
  "operatorZh": "$OPERATOR_ZH",
  "band": "${BAND}",
  "bandwidth": "${BW}",
  "downloadSpeed": $DL_SPEED,
  "uploadSpeed": $UL_SPEED,
  "rsrp": "${RSRP}",
  "rsrq": "${RSRQ}",
  "sinr": "${SINR}",
  "arfcn": "${ARFCN}",
  "pci": "${PCI}",
  "cellId": "${CELLID}",
  "tac": "${TAC}",
  "qci": "${QCI}",
  "imei": "${IMEI}",
  "temperature": "${TEMP}",
  "caMode": "${CA_MODE}",
  "caBand": "${CA_BAND}",
  "caArfcn": "${CA_ARFCN}",
  "caPci": "${CA_PCI}",
  "caBw": "${CA_BW}",
  "voltage": "${VOLT}",
  "ipv4": "$IPV4",
  "ipv6": "$IPV6",
  "rxBytes": $RX_BYTES,
  "txBytes": $TX_BYTES,
  "rxlev": "${RXLEV}",
  "antSource": "$ANT_SOURCE",
  "antMain": "${ANT_MAIN}",
  "antDiv": "${ANT_DIV}",
  "antMimo1": "${ANT_MIMO1}",
  "antMimo2": "${ANT_MIMO2}",
  "timestamp": $NOW_TS
}
EOF
    mv "$OUT_TMP" "$OUT"
    cp "$OUT" "$CACHE" 2>/dev/null
}

# v6.14 (2026-09-08): 0.5s 独立速率进程 — speed_loop 后台子进程每 0.5s 采样 wwan0 计数器,
# 原子写 /tmp/speed_now.json (get_speed / get_5g_info 读取)。主循环保持 30s 全量 AT 采集。
# 面板 speedTick 以 500ms 轮询 /cgi-bin/get_speed, 上下行 0.5s 级响应且不影响其他模块。
speed_loop() {
    exec 8>/tmp/speed_loop.lock
    flock -n 8 || exit 0
    PRX=$(cat /sys/class/net/$IFACE/statistics/rx_bytes 2>/dev/null)
    PTX=$(cat /sys/class/net/$IFACE/statistics/tx_bytes 2>/dev/null)
    PUP=$(cut -d" " -f1 /proc/uptime)
    [ -z "$PRX" ] && PRX=0
    [ -z "$PTX" ] && PTX=0
    while :; do
        if usleep 500000 2>/dev/null; then :; else sleep 1; fi
        RX=$(cat /sys/class/net/$IFACE/statistics/rx_bytes 2>/dev/null)
        TX=$(cat /sys/class/net/$IFACE/statistics/tx_bytes 2>/dev/null)
        UP=$(cut -d" " -f1 /proc/uptime)
        [ -z "$RX" ] && RX=0
        [ -z "$TX" ] && TX=0
        DL=$(awk "BEGIN{d=($RX-$PRX)*8/($UP-$PUP)/1000000; if(d<0)d=0; printf \"%d\", d}")
        UL=$(awk "BEGIN{u=($TX-$PTX)*8/($UP-$PUP)/1000000; if(u<0)u=0; printf \"%d\", u}")
        printf '{"dl":%s,"ul":%s}\n' "$DL" "$UL" > /tmp/speed_now.json.tmp 2>/dev/null && mv /tmp/speed_now.json.tmp /tmp/speed_now.json
        PRX=$RX; PTX=$TX; PUP=$UP
    done
}
# v64.5 重构 (9/11, R1-⑧): speed_loop 退役——500ms 速率采样已并入 Go collector（speedSamplerLoop,
# 原子写同一文件）。三写者（C speed_sampler + shell speed_loop + Go）收敛为 Go 单写者。
# 原调用: speed_loop &
# 函数定义保留（便于回滚对照），不再启动。

while true; do
    collect_once
    sleep $INTERVAL
done