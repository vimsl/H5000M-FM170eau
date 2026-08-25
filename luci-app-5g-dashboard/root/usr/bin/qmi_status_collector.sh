#!/bin/sh
# v6.9: FM170 GTCCINFO Ant0-3 real 4x antenna RSRP (value-156); NR5G-SA/NSA normalize; TAC hex->dec; GTCCINFO f11-f14 = SINR/RxLev/RSRP/RSRQ (not 4-antenna); antenna diversity unsupported on FM170-EAU
OUT="/tmp/modem_status.json"
OUT_TMP="/tmp/modem_status.json.tmp"
CACHE="/tmp/last_good_modem.json"
INTERVAL=15
SMOOTH_DIR=/tmp/modem_smooth
STAT_DIR=/tmp/modem_stat
mkdir -p "$SMOOTH_DIR" "$STAT_DIR"

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
    NR_RSRQ_RAW=$(echo "$CESQ_LINE" | cut -d"," -f7)
    NR_RSRP_RAW=$(echo "$CESQ_LINE" | cut -d"," -f8)
    NR_SINR_RAW=$(echo "$CESQ_LINE" | cut -d"," -f9)
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

    GTCC_RAW=$(send_at "AT+GTCCINFO?")
    GTCC_LINE=$(echo "$GTCC_RAW" | grep -E "^[0-9]+,[0-9]+,[0-9]+" | head -1 | tr -d "\r")
    MCC="--"; MNC="--"; TAC="--"; CELLID="--"; PCI="--"; ARFCN="--"; BAND="--"; BW="--"
    ANT_MAIN="--"; ANT_DIV="--"; ANT_MIMO1="--"; ANT_MIMO2="--"
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
        # FM170-EAU AT+GTCCINFO field map (per Fibocom AT V4.0 manual):
        # f11..f14 = Ant0..Ant3 (4x antenna RSRP raw), RSRP_dBm = value - 156
        _g_ant0=$(echo "$GTCC_LINE" | cut -d"," -f11)
        _g_ant1=$(echo "$GTCC_LINE" | cut -d"," -f12)
        _g_ant2=$(echo "$GTCC_LINE" | cut -d"," -f13)
        _g_ant3=$(echo "$GTCC_LINE" | cut -d"," -f14)
        if [ -n "$_g_ant0" ] && [ "$_g_ant0" != "255" ]; then ANT_MAIN=$(($_g_ant0 - 156)); fi
        if [ -n "$_g_ant1" ] && [ "$_g_ant1" != "255" ]; then ANT_DIV=$(($_g_ant1 - 156)); fi
        if [ -n "$_g_ant2" ] && [ "$_g_ant2" != "255" ]; then ANT_MIMO1=$(($_g_ant2 - 156)); fi
        if [ -n "$_g_ant3" ] && [ "$_g_ant3" != "255" ]; then ANT_MIMO2=$(($_g_ant3 - 156)); fi
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

    # Fallback: if carrier says disconnected but CGPADDR returned valid IP, mark connected
    [ "$NET_STATUS" != "connected" ] && [ "$IPV4" != "--" ] && NET_STATUS="connected"

        fi
    fi

    OLD_RSRP=$(cat "$SMOOTH_DIR/rsrp" 2>/dev/null)
    OLD_RSRQ=$(cat "$SMOOTH_DIR/rsrq" 2>/dev/null)
    OLD_SINR=$(cat "$SMOOTH_DIR/sinr" 2>/dev/null)
    [ "$RSRP" != "--" ] && RSRP=$(smooth "$RSRP" "$OLD_RSRP" 3 10) && echo "$RSRP" > "$SMOOTH_DIR/rsrp"
    [ "$RSRQ" != "--" ] && RSRQ=$(smooth "$RSRQ" "$OLD_RSRQ" 3 10) && echo "$RSRQ" > "$SMOOTH_DIR/rsrq"
    [ "$SINR" != "--" ] && SINR=$(smooth "$SINR" "$OLD_SINR" 3 10) && echo "$SINR" > "$SMOOTH_DIR/sinr"

    [ "$ANT_MAIN" = "--" ] && ANT_MAIN="$RSRP"

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
  "caMode": "none",
  "caBand": "",
  "caArfcn": "",
  "caPci": "",
  "caBw": "",
  "ipv4": "$IPV4",
  "ipv6": "$IPV6",
  "rxBytes": $RX_BYTES,
  "txBytes": $TX_BYTES,
  "rxlev": "${RXLEV}",
  "antSource": "gtccinfo",
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

while true; do
    collect_once
    sleep $INTERVAL
done