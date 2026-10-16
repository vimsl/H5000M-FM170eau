/*
 * qmi_fix_skb.c — Kprobe hotfix for qmi_wwan_f skb headroom deficiency
 *
 * Problem:
 *   qmi_wwan_f (Quectel closed-source QMAP driver) allocates sk_buff without
 *   reserving sufficient tailroom. When HNAT/PPE hardware acceleration tries
 *   to append headers, the skb_tailroom check fails and packets are dropped.
 *   Root cause: __netdev_alloc_skb() normally provides LL_RESERVED_SPACE
 *   (includes LL_MAX_HEADER=176), but qmi_wwan_f uses __alloc_skb directly
 *   or __netdev_alloc_skb with insufficient length.
 *
 * Fix:
 *   Intercept __alloc_skb via kprobe. When the caller is qmi_wwan_f,
 *   add LL_MAX_HEADER bytes to the size parameter.
 *   Also try to intercept __netdev_alloc_skb as a bonus path (optional —
 *   it is static inline in kernel 6.6 and may not exist as a symbol).
 *
 * Design decisions:
 *   - Single mandatory kprobe on __alloc_skb (EXPORT_SYMBOL, always present).
 *   - __netdev_alloc_skb is optional; if the symbol doesn't exist, skip it.
 *   - Caller detection via ARM64 LR (x30) + lazy module text range caching.
 *   - count parameter exposed via sysfs for monitoring.
 *
 * Usage on router:
 *   insmod qmi_fix_skb.ko
 *   cat /sys/module/qmi_fix_skb/parameters/count
 *   dmesg | grep qmi_fix_skb
 *
 * AUTOLOAD:
 *   echo qmi_fix_skb >> /etc/modules.d/qmi-fix
 *   # or install as OpenWrt package with /etc/modules.d/ entry
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/skbuff.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvis");
MODULE_DESCRIPTION("Kprobe hotfix for qmi_wwan_f skb headroom (v3)");

#define LL_MAX_HEADER   176
#define TARGET_NAME     "qmi_wwan_f"

static int fix_count;
module_param_named(count, fix_count, int, 0444);
MODULE_PARM_DESC(count, "Total __alloc_skb fix applications");

/* Simple caller check via __module_text_address.
 * Not caching text range avoids struct module layout differences across kernel builds.
 */
static inline bool caller_is_qmi(struct pt_regs *regs)
{
	unsigned long caller = regs->regs[30]; /* ARM64 link register */
	struct module *mod = __module_text_address(caller);
	return mod && !strcmp(mod->name, TARGET_NAME);
}

/*
 * __alloc_skb(size, gfp, flags, node)
 * ARM64:  x0=size, x1=gfp, x2=flags, x3=node
 */
static int fix_alloc_skb_pre(struct kprobe *kp, struct pt_regs *regs)
{
	if (!caller_is_qmi(regs))
		return 0;

	regs->regs[0] += LL_MAX_HEADER;  /* x0 = size */
	fix_count++;
	return 0;
}

static struct kprobe kp_alloc_skb = {
	.symbol_name = "__alloc_skb",
	.pre_handler = fix_alloc_skb_pre,
};

/*
 * __netdev_alloc_skb(dev, length, gfp)
 * ARM64:  x0=dev, x1=length, x2=gfp
 * Optional: this symbol may not exist (static inline in kernel 6.6).
 * If it does exist, intercepting it adds headroom earlier in the call chain.
 */
static int fix_netdev_alloc_pre(struct kprobe *kp, struct pt_regs *regs)
{
	if (!caller_is_qmi(regs))
		return 0;

	regs->regs[1] += LL_MAX_HEADER;  /* x1 = length */
	pr_debug("qmi_fix_skb: __netdev_alloc_skb fix #%d\n", fix_count + 1);
	return 0;
}

static struct kprobe kp_netdev_alloc = {
	.symbol_name = "__netdev_alloc_skb",
	.pre_handler = fix_netdev_alloc_pre,
};

/* ---- Module init/exit ---- */

static int __init qmi_fix_init(void)
{
	int ret;

	/* Mandatory: __alloc_skb */
	ret = register_kprobe(&kp_alloc_skb);
	if (ret) {
		pr_err("qmi_fix_skb: register_kprobe(__alloc_skb) = %d\n", ret);
		return ret;
	}

	/* Optional: __netdev_alloc_skb (may be inlined away) */
	ret = register_kprobe(&kp_netdev_alloc);
	if (ret)
		pr_warn("qmi_fix_skb: register_kprobe(__netdev_alloc_skb) = %d (optional, continuing)\n", ret);
	else
		pr_info("qmi_fix_skb: __netdev_alloc_skb kprobe also installed\n");

	pr_info("qmi_fix_skb: installed, waiting for qmi_wwan_f calls...\n");
	return 0;
}

static void __exit qmi_fix_exit(void)
{
	unregister_kprobe(&kp_alloc_skb);
	if (kp_netdev_alloc.addr)
		unregister_kprobe(&kp_netdev_alloc);
	pr_info("qmi_fix_skb: removed, total fixes=%d\n", fix_count);
}

module_init(qmi_fix_init);
module_exit(qmi_fix_exit);
