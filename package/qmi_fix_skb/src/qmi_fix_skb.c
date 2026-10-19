/*
 * qmi_fix_skb.c - Kprobe hotfix for qmi_wwan_f skb tailroom bug
 *
 * Problem:
 *   qmi_wwan_f (Fibocom QMAP driver) allocates skb without sufficient
 *   tailroom for HNAT hardware acceleration markers. This causes:
 *   - "skb_tailroom small!" warnings (50+ per boot)
 *   - Packet drops and reduced throughput
 *   - Severe impact on IPv6 NAT scenarios
 *
 * Fix strategy:
 *   1. (Legacy) Intercept generic __netdev_alloc_skb/__alloc_skb to add
 *      headroom for ALL allocations - broad but wasteful.
 *   2. (Enhanced) Intercept qmi_wwan_f's rx_fixup/tx_fixup/start_xmit
 *      to check and expand tailroom on demand - targeted and efficient.
 *
 * The enhanced kprobes cover the three code paths where qmi_wwan_f
 * processes skb before passing to the network stack:
 *   - qmi_wwan_f_rx_fixup(): USB receive path
 *   - qmi_wwan_f_tx_fixup(): USB transmit path
 *   - __qmi_wwan_f_start_xmit(): netdev transmit entry
 *
 * Upstream reference:
 *   commit 2e4233870557 ("qmi_wwan: Increase headroom for QMAP SKBs")
 * Usage on router:
 *   insmod qmi_fix_skb.ko
 *   cat /sys/module/qmi_fix_skb/parameters/count
 *   cat /sys/module/qmi_fix_skb/parameters/tailroom_fixes
 *   dmesg | grep qmi_fix_skb
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvis");
MODULE_DESCRIPTION("Kprobe hotfix for qmi_wwan_f skb tailroom deficiency");

#define LL_MAX_HEADER       176
#define QMI_MIN_TAILROOM    64  /* HNAT marker needs ~48 bytes, 64 for safety */

/* Statistics */
static int fix_count;
static int tailroom_fix_count;
module_param_named(count, fix_count, int, 0444);
MODULE_PARM_DESC(count, "Total headroom fix applications (legacy)");
module_param_named(tailroom_fixes, tailroom_fix_count, int, 0444);
MODULE_PARM_DESC(tailroom_fixes, "Total tailroom expansions (enhanced)");


/* ============================================================
 * Part 1: Legacy generic skb allocation interception
 * Covers headroom deficiency for all network drivers.
 * Kept for backward compatibility.
 * ============================================================
 */

/* kprobe pre_handler: __netdev_alloc_skb
 * __netdev_alloc_skb(dev, length, gfp)
static struct kprobe kp_alloc_skb = {
	.symbol_name = "__alloc_skb",
	.pre_handler = fix_alloc_skb_pre,
};


/* ============================================================
 * Part 2: Enhanced qmi_wwan_f specific interception
 * Checks tailroom at driver entry points and expands if needed.
 * This is the targeted fix for the "skb_tailroom small!" issue.
 * ============================================================
 */

/*
 * check_and_fix_tailroom() - Expand skb tailroom if insufficient
 * @skb: socket buffer to check
 * @min_tailroom: minimum bytes needed after tail pointer
 *
 * Called from kprobe pre_handler, must use GFP_ATOMIC.
 * Returns 0 on success (or no fix needed), -ENOMEM on failure.
 */
static inline int check_and_fix_tailroom(struct sk_buff *skb, int min_tailroom)
{
	int current_tailroom;

	if (!skb)
		return 0;

	current_tailroom = skb_end_pointer(skb) - skb_tail_pointer(skb);

	if (current_tailroom < min_tailroom) {
		int expand_by = min_tailroom - current_tailroom + 32; /* +32 margin */

		if (pskb_expand_head(skb, 0, expand_by, GFP_ATOMIC)) {
			pr_warn_ratelimited("qmi_fix_skb: tailroom expand failed\n");
			return -ENOMEM;
		}
		tailroom_fix_count++;
	}

	return 0;
}

/*
 * kprobe pre_handler: qmi_wwan_f_rx_fixup
 *
 * USB receive path. The driver reads QMAP headers and prepares skb
 * for the network stack. HNAT may need to append acceleration markers.
 *
 * Function signature (from qmi_wwan_f.c):
 *   int qmi_wwan_f_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
 * ARM64 ABI: x0=dev, x1=skb
 */
static int fix_rx_fixup_pre(struct kprobe *kp, struct pt_regs *regs)
{
	struct sk_buff *skb = (struct sk_buff *)regs->regs[1];

	check_and_fix_tailroom(skb, QMI_MIN_TAILROOM);
	return 0;
}

/*
 * kprobe pre_handler: qmi_wwan_f_tx_fixup
 *
 * USB transmit path. The driver prepends QMAP headers before sending.
 * The skb will be passed to USB core, but HNAT checks happen here.
 *
 * Function signature (from qmi_wwan_f.c):
 *   struct sk_buff *qmi_wwan_f_tx_fixup(struct usbnet *dev,
 *                                        struct sk_buff *skb, gfp_t flags)
 * ARM64 ABI: x0=dev, x1=skb, x2=flags
 */
static int fix_tx_fixup_pre(struct kprobe *kp, struct pt_regs *regs)
{
	struct sk_buff *skb = (struct sk_buff *)regs->regs[1];

	check_and_fix_tailroom(skb, QMI_MIN_TAILROOM);
	return 0;
}

/*
 * kprobe pre_handler: __qmi_wwan_f_start_xmit
 *
 * Netdev ndo_start_xmit entry point. This is where the kernel hands
 * an skb to the driver for transmission. HNAT hooks run before this,
 * so we ensure tailroom is sufficient.
 *
 * Function signature:
 *   netdev_tx_t __qmi_wwan_f_start_xmit(struct sk_buff *skb,
 *                                         struct net_device *netdev)
 * ARM64 ABI: x0=skb, x1=netdev
 */
static int fix_start_xmit_pre(struct kprobe *kp, struct pt_regs *regs)
{
	struct sk_buff *skb = (struct sk_buff *)regs->regs[0];

	check_and_fix_tailroom(skb, QMI_MIN_TAILROOM);
	return 0;
}

/* kprobe structures for qmi_wwan_f functions */
static struct kprobe kp_rx_fixup = {
	.symbol_name = "qmi_wwan_f_rx_fixup",
	.pre_handler = fix_rx_fixup_pre,
};

static struct kprobe kp_tx_fixup = {
	.symbol_name = "qmi_wwan_f_tx_fixup",
	.pre_handler = fix_tx_fixup_pre,
};

static struct kprobe kp_start_xmit = {
	.symbol_name = "__qmi_wwan_f_start_xmit",
	.pre_handler = fix_start_xmit_pre,
};


/* ============================================================
 * Module init/exit
 * ============================================================
 */

/* Register a kprobe, skip silently if symbol not found (driver not loaded) */
static int register_kprobe_optional(struct kprobe *kp, const char *name)
{
	int ret = register_kprobe(kp);

	if (ret == -ENOENT) {
		pr_info("qmi_fix_skb: symbol '%s' not found, skipped\n", name);
		return 0;  /* Not an error, driver may not be loaded */
	}
	if (ret) {
		pr_err("qmi_fix_skb: register_kprobe(%s) = %d\n", name, ret);
		return ret;
	}
	pr_info("qmi_fix_skb: kprobe registered on '%s'\n", name);
	return 0;
}

static int __init qmi_fix_init(void)
{

	ret = register_kprobe(&kp_netdev_alloc);
	if (ret) {
		pr_err("qmi_fix_skb: register_kprobe(__netdev_alloc_skb) = %d\n", ret);
		return ret;
	}

	ret = register_kprobe(&kp_alloc_skb);
	if (ret) {
		pr_err("qmi_fix_skb: register_kprobe(__alloc_skb) = %d\n", ret);
		unregister_kprobe(&kp_netdev_alloc);
		return ret;
	}

	/* Part 2: Enhanced qmi_wwan_f specific kprobes (optional) */
	register_kprobe_optional(&kp_rx_fixup, "qmi_wwan_f_rx_fixup");
	register_kprobe_optional(&kp_tx_fixup, "qmi_wwan_f_tx_fixup");
	register_kprobe_optional(&kp_start_xmit, "__qmi_wwan_f_start_xmit");

	pr_info("qmi_fix_skb: installed (legacy headroom + enhanced tailroom fix)\n");
	pr_info("qmi_fix_skb: tailroom threshold = %d bytes\n", QMI_MIN_TAILROOM);
	return 0;
}

static void __exit qmi_fix_exit(void)
{
	/* Unregister in reverse order */
	unregister_kprobe(&kp_start_xmit);
	unregister_kprobe(&kp_tx_fixup);
	unregister_kprobe(&kp_rx_fixup);
	unregister_kprobe(&kp_alloc_skb);
	unregister_kprobe(&kp_netdev_alloc);

	pr_info("qmi_fix_skb: removed, headroom_fixes=%d, tailroom_fixes=%d\n",
		fix_count, tailroom_fix_count);
}

module_init(qmi_fix_init);