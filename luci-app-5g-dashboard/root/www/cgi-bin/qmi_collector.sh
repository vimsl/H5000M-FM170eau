#!/bin/sh
# QMI collector daemon - writes modem status to /tmp/modem_status.json
QMICLI="/tmp/qmicli_p"
LIBDIR="/tmp"
DEVICE="/dev/cdc-wdm0"
OUT="/tmp/modem_status.json"
TMPF="/tmp/modem_status.json.tmp"

[ -x "$QMICLI" ] || exit 1

while true; do
    SIG=$(LD_LIBRARY_PATH=$LIBDIR $QMICLI -d $DEVICE --device-open-qmi --nas-get-signal-info 2>/dev/null)
    RSRP=$(echo "$SIG" | grep "RSRP:" | head -1 | sed "s/.*: '//;s/'.*//" | grep -oE '[-0-9.]+')
    RSRQ=$(echo "$SIG" | grep "RSRQ:" | head -1 | sed "s/.*: '//;s/'.*//" | grep -oE '[-0-9.]+')
    SINR=$(echo "$SIG" | grep "SNR:" | head -1 | sed "s/.*: '//;s/'.*//" | grep -oE '[-0-9.]+')

    CELL=$(LD_LIBRARY_PATH=$LIBDIR $QMICLI -d $DEVICE --device-open-qmi --nas-get-cell-location-info 2>/dev/null)
    PCI=$(echo "$CELL" | grep "Physical Cell ID:" | sed "s/.*: '//;s/'.*//" | grep -oE '[0-9]+')
    ARFCN=$(echo "$CELL" | grep "5GNR ARFCN:" | sed "s/.*: '//;s/'.*//" | grep -oE '[0-9]+')
    CID=$(echo "$CELL" | grep "Global Cell ID:" | sed "s/.*: '//;s/'.*//" | grep -oE '[0-9]+')

    RF=$(LD_LIBRARY_PATH=$LIBDIR $QMICLI -d $DEVICE --device-open-qmi --nas-get-rf-band-info 2>/dev/null)
    BAND=$(echo "$RF" | grep "Active Band Class:" | head -1 | sed "s/.*: '//;s/'.*//" | sed 's/nr5g-//' | grep -oE '[0-9]+')

    SERV=$(LD_LIBRARY_PATH=$LIBDIR $QMICLI -d $DEVICE --device-open-qmi --nas-get-serving-system 2>/dev/null)
    MCC=$(echo "$SERV" | grep "MCC:" | head -1 | sed "s/.*: '//;s/'.*//" | grep -oE '[0-9]+' | head -1)
    MNC=$(echo "$SERV" | grep "MNC:" | head -1 | sed "s/.*: '//;s/'.*//" | grep -oE '[0-9]+' | head -1)
    REG=$(echo "$SERV" | grep "Registration state:" | sed "s/.*: '//;s/'.*//")
    RADIO=$(echo "$SERV" | grep "\[0\]:" | head -1 | sed "s/.*: '//;s/'.*//")

    DMS=$(LD_LIBRARY_PATH=$LIBDIR $QMICLI -d $DEVICE --device-open-qmi --dms-get-ids 2>/dev/null)
    IMEI=$(echo "$DMS" | grep "IMEI:" | sed "s/.*IMEI: '//;s/'.*//" | grep -oE '[0-9]+')

    [ -z "$RSRP" ] && RSRP="--"
    [ -z "$RSRQ" ] && RSRQ="--"
    [ -z "$SINR" ] && SINR="--"
    [ -z "$PCI" ] && PCI="--"
    [ -z "$ARFCN" ] && ARFCN="--"
    [ -z "$CID" ] && CID="--"
    [ -z "$BAND" ] && BAND="--"
    [ -z "$MCC" ] && MCC="--"
    [ -z "$MNC" ] && MNC="--"
    [ -z "$REG" ] && REG="unknown"
    [ -z "$RADIO" ] && RADIO="--"
    [ -z "$IMEI" ] && IMEI="--"

    cat > "$TMPF" << ENDJSON
{"rsrp":"$RSRP","rsrq":"$RSRQ","sinr":"$SINR","pci":"$PCI","arfcn":"$ARFCN","cellId":"$CID","band":"$BAND","mcc":"$MCC","mnc":"$MNC","registration":"$REG","radioInterface":"$RADIO","imei":"$IMEI","qci":"--","source":"QMI","updated":$(date +%s)}
ENDJSON
    mv "$TMPF" "$OUT"
    sleep 2
done