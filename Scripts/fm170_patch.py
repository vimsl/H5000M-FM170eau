#!/usr/bin/env python3
"""Apply FM170eau patches to kernel source: USB IDs, QMI interface, tailroom fix."""
import os, re, sys

kernel_src = os.environ.get('KERNEL_SRC', '')
if not kernel_src:
    print("ERROR: KERNEL_SRC not set")
    sys.exit(1)

print(f"Kernel source: {kernel_src}")

# 1. Patch option.c - add FM170 USB PIDs
option_c = os.path.join(kernel_src, 'drivers/usb/serial/option.c')
if os.path.exists(option_c):
    with open(option_c, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    if '0x0102' not in content or 'Fibocom FM170' not in content:
        lines = content.split('\n')
        new_lines = []
        applied = False
        for line in lines:
            new_lines.append(line)
            if 'Fibocom FM101-GL' in line and 'RSVD(4)' in line:
                applied = True
                new_lines.append('\t{ USB_DEVICE_INTERFACE_CLASS(0x2cb7, 0x0102, 0xff) },\t/* Fibocom FM170 (ECM/RNDIS mode) */')
                new_lines.append('\t{ USB_DEVICE(0x2cb7, 0x01a0) },\t\t\t\t/* Fibocom FM170-GL (MBIM mode) */')

        if applied:
            with open(option_c, 'w', encoding='utf-8') as f:
                f.write('\n'.join(new_lines))
            print("Patched option.c - added FM170 0x0102 and 0x01a0 PIDs")
        else:
            print("WARNING: option.c anchor NOT matched (needs 'Fibocom FM101-GL' AND 'RSVD(4)' on the SAME line) - FM170 PID patch NOT applied")
    else:
        print("option.c already patched (skipped)")
else:
    print("WARNING: option.c not found at expected path")

# 2. Patch qmi_wwan.c - add QMI_FIXED_INTF entries
qmi_c = os.path.join(kernel_src, 'drivers/net/usb/qmi_wwan.c')
if os.path.exists(qmi_c):
    with open(qmi_c, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    if '0x01a0' not in content or 'Fibocom FM170' not in content:
        lines = content.split('\n')
        new_lines = []
        applied = False
        for line in lines:
            new_lines.append(line)
            if 'Fibocom FG132' in line:
                applied = True
                new_lines.append('\t{QMI_FIXED_INTF(0x2cb7, 0x01a0, 4)},\t\t/* Fibocom FM170-GL (MBIM mode) */')
                new_lines.append('\t{QMI_FIXED_INTF(0x2cb7, 0x0102, 4)},\t\t/* Fibocom FM170 (ECM/RNDIS mode) */')

        if applied:
            with open(qmi_c, 'w', encoding='utf-8') as f:
                f.write('\n'.join(new_lines))
            print("Patched qmi_wwan.c - added FM170 QMI interface entries")
        else:
            print("WARNING: qmi_wwan.c anchor 'Fibocom FG132' NOT found - FM170 QMI entries NOT applied")
    else:
        print("qmi_wwan.c already patched (skipped)")
else:
    print("WARNING: qmi_wwan.c not found at expected path")

# 3. Apply tailroom fix to qmi_wwan.c
if os.path.exists(qmi_c):
    with open(qmi_c, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    if 'skb_tailroom' not in content or 'ETH_DATA_LEN' not in content:
        lines = content.split('\n')
        new_lines = []
        applied = False
        for i, line in enumerate(lines):
            new_lines.append(line)
            if 'while (skb->len > 0) {' in line:
                applied = True
                new_lines[-1] = '\t\t/* Ensure enough tailroom for PPPoE/VLAN encapsulation */'
                new_lines.append('\t\tif (skb_tailroom(skb) < ETH_DATA_LEN) {')
                new_lines.append('\t\t\tstruct sk_buff *new_skb = skb_copy_expand(skb, 0, ETH_DATA_LEN, GFP_ATOMIC);')
                new_lines.append('\t\t\tif (new_skb) {')
                new_lines.append('\t\t\t\tdev_kfree_skb_any(skb);')
                new_lines.append('\t\t\t\tskb = new_skb;')
                new_lines.append('\t\t\t}')
                new_lines.append('\t\t}')
                new_lines.append(line)

        if applied:
            with open(qmi_c, 'w', encoding='utf-8') as f:
                f.write('\n'.join(new_lines))
            print("Patched qmi_wwan.c - added tailroom fix")
        else:
            print("WARNING: qmi_wwan.c anchor 'while (skb->len > 0) {' NOT found - tailroom fix NOT applied")
    else:
        print("tailroom fix already applied (skipped)")

print("=== FM170eau patches complete ===")
