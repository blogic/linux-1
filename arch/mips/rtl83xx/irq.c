// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek RTL83XX architecture specific IRQ handling
 *
 * Copyright  (C) 2020 B. Koblitz
 * based on the original BSP
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <mach-rtl83xx.h>
#include "irq.h"

extern struct rtl83xx_soc_info soc_info;

static DEFINE_RAW_SPINLOCK(irq_lock);
static void __iomem *rtl83xx_ictl_base;

extern irqreturn_t c0_compare_interrupt(int irq, void *dev_id);


static void rtl83xx_ictl_enable_irq(struct irq_data *i)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&irq_lock, flags);

	value = rtl83xx_r32(rtl83xx_ictl_base + RTL83XX_ICTL_GIMR);
	value |= BIT(i->irq);
	rtl83xx_w32(value, rtl83xx_ictl_base + RTL83XX_ICTL_GIMR);

	raw_spin_unlock_irqrestore(&irq_lock, flags);
}

static void rtl83xx_ictl_disable_irq(struct irq_data *i)
{
	unsigned long flags;
	u32 value;

	raw_spin_lock_irqsave(&irq_lock, flags);

	value = rtl83xx_r32(rtl83xx_ictl_base + RTL83XX_ICTL_GIMR);
	value &= ~BIT(i->irq);
	rtl83xx_w32(value, rtl83xx_ictl_base + RTL83XX_ICTL_GIMR);

	raw_spin_unlock_irqrestore(&irq_lock, flags);
}

static struct irq_chip rtl83xx_ictl_irq = {
	.name = "RTL83xx",
	.irq_enable = rtl83xx_ictl_enable_irq,
	.irq_disable = rtl83xx_ictl_disable_irq,
	.irq_ack = rtl83xx_ictl_disable_irq,
	.irq_mask = rtl83xx_ictl_disable_irq,
	.irq_unmask = rtl83xx_ictl_enable_irq,
	.irq_eoi = rtl83xx_ictl_enable_irq,
};

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending, ext_int;

	pending =  read_c0_cause();

	if (pending & CAUSEF_IP7) {
		c0_compare_interrupt(7, NULL);
	} else if (pending & CAUSEF_IP6) {
		do_IRQ(RTL83XX_IRQ_TC0);
	} else if (pending & CAUSEF_IP5) {
		ext_int = rtl83xx_r32(rtl83xx_ictl_base + RTL83XX_ICTL_GIMR) & rtl83xx_r32(rtl83xx_ictl_base + RTL83XX_ICTL_GISR);
		if (ext_int & BIT(RTL83XX_IRQ_NIC))
			do_IRQ(RTL83XX_IRQ_NIC);
		else if (ext_int & BIT(RTL83XX_IRQ_GPIO_ABCD))
			do_IRQ(RTL83XX_IRQ_GPIO_ABCD);
		else if ((ext_int & BIT(RTL83XX_IRQ_GPIO_EFGH)) && (soc_info.family == RTL8328_FAMILY_ID))
			do_IRQ(RTL83XX_IRQ_GPIO_EFGH);
		else
			spurious_interrupt();
	} else if (pending & CAUSEF_IP4) {
		do_IRQ(RTL83XX_IRQ_SWCORE);
	} else if (pending & CAUSEF_IP3) {
		do_IRQ(RTL83XX_IRQ_UART0);
	} else if (pending & CAUSEF_IP2) {
		ext_int = rtl83xx_r32(rtl83xx_ictl_base + RTL83XX_ICTL_GIMR) & rtl83xx_r32(rtl83xx_ictl_base + RTL83XX_ICTL_GISR);
		if (ext_int & BIT(RTL83XX_IRQ_TC1))
			do_IRQ(RTL83XX_IRQ_TC1);
		else if (ext_int & BIT(RTL83XX_IRQ_UART1))
			do_IRQ(RTL83XX_IRQ_UART1);
		else
			spurious_interrupt();
	} else {
		spurious_interrupt();
	}
}

static int intc_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	irq_set_chip_and_handler(hw, &rtl83xx_ictl_irq, handle_level_irq);

	return 0;
}

static const struct irq_domain_ops irq_domain_ops = {
	.map = intc_map,
	.xlate = irq_domain_xlate_onecell,
};

int __init icu_of_init(struct device_node *node, struct device_node *parent)
{
	struct irq_domain *domain;
	int i;

	mips_cpu_irq_init();

	domain = irq_domain_add_simple(node, 32, 0, &irq_domain_ops, NULL);

	rtl83xx_ictl_base = of_iomap(node, 0);
	if (!rtl83xx_ictl_base)
		return -EIO;

	/* Setup all external HW irqs */
	for (i = 8; i < RTL83XX_IRQ_ICTL_NUM; i++) {
		irq_domain_associate(domain, i, i);
		irq_set_chip_and_handler(RTL83XX_IRQ_ICTL_BASE + i,
					 &rtl83xx_ictl_irq, handle_level_irq);
	}

	if (request_irq(RTL83XX_ICTL1_IRQ, no_action, IRQF_NO_THREAD,
			"IRQ cascade 1", NULL)) {
		pr_err("request_irq() cascade 1 for irq %d failed\n", RTL83XX_ICTL1_IRQ);
	}
	if (request_irq(RTL83XX_ICTL2_IRQ, no_action, IRQF_NO_THREAD,
			"IRQ cascade 2", NULL)) {
		pr_err("request_irq() cascade 2 for irq %d failed\n", RTL83XX_ICTL2_IRQ);
	}
	if (request_irq(RTL83XX_ICTL3_IRQ, no_action, IRQF_NO_THREAD,
			"IRQ cascade 3", NULL)) {
		pr_err("request_irq() cascade 3 for irq %d failed\n", RTL83XX_ICTL3_IRQ);
	}
	if (request_irq(RTL83XX_ICTL4_IRQ, no_action, IRQF_NO_THREAD,
			"IRQ cascade 4", NULL)) {
		pr_err("request_irq() cascade 4 for irq %d failed\n", RTL83XX_ICTL4_IRQ);
	}
	if (request_irq(RTL83XX_ICTL5_IRQ, no_action, IRQF_NO_THREAD,
			"IRQ cascade 5", NULL)) {
		pr_err("request_irq() cascade 5 for irq %d failed\n", RTL83XX_ICTL5_IRQ);
	}

	/* Set up interrupt routing scheme */
	rtl83xx_w32(RTL83XX_IRR0_SETTING, rtl83xx_ictl_base + RTL83XX_IRR0);
	rtl83xx_w32(RTL83XX_IRR1_SETTING, rtl83xx_ictl_base + RTL83XX_IRR1);
	rtl83xx_w32(RTL83XX_IRR2_SETTING, rtl83xx_ictl_base + RTL83XX_IRR2);
	rtl83xx_w32(RTL83XX_IRR3_SETTING, rtl83xx_ictl_base + RTL83XX_IRR3);

	/* Enable timer0 and uart0 interrupts */
	rtl83xx_w32(BIT(RTL83XX_IRQ_TC0) | BIT(RTL83XX_IRQ_UART0), rtl83xx_ictl_base + RTL83XX_ICTL_GIMR);

	return 0;
}

void __init arch_init_irq(void)
{
	/* Do board-specific irq initialization */
	irqchip_init();
}

IRQCHIP_DECLARE(mips_cpu_intc, "rtl83xx,icu", icu_of_init);
