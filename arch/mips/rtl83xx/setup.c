/*
 * Setup for the Realtek RTL83XX SoC series:
 *	Memory, Timer and Serial
 *
 * Copyright (C) 2020 B. Koblitz
 * based on the original BSP by
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 *
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>

#include <asm/addrspace.h>
#include <asm/io.h>

#include <asm/bootinfo.h>
#include <linux/of_fdt.h>
#include <asm/reboot.h>
#include <asm/time.h>		/* for mips_hpt_frequency */
#include <asm/prom.h>
#include <asm/smp-ops.h>
#include <mach-rtl83xx.h>
#include <rtl83xx-regs.h>

extern void rtl83xx_init_uart0(void);
extern void rtl83xx_init_uart1(void);
extern struct rtl83xx_soc_info soc_info;

u32 pll_reset_value;

static void rtl83xx_restart(char *command)
{
	u32 pll = rtl83xx_r32(RTL838X_PLL_CML_CTRL);
	 /* SoC reset vector (in flash memory): on RTL839x platform preferred way to reset */
	void (*f)(void) = (void *) 0xbfc00000;

	pr_info("System restart.\n");
	if (soc_info.family == RTL8390_FAMILY_ID) {
		f();
		/* If calling reset vector fails, reset entire chip */
		rtl83xx_w32(0xFFFFFFFF, RTL839X_RST_GLB_CTRL);
		/* If this fails, halt the CPU */
		while (1);
	}

	pr_info("PLL control register: %x, applying reset value %x\n",
		pll, pll_reset_value);
	rtl83xx_w32(3, RTL838X_INT_RW_CTRL);
	rtl83xx_w32(pll_reset_value, RTL838X_PLL_CML_CTRL);
	rtl83xx_w32(0, RTL838X_INT_RW_CTRL);

	pr_info("Resetting RTL838X SoC\n");
	/* Reset Global Control1 Register */
	rtl83xx_w32(1, RTL838X_RST_GLB_CTRL_1);
}

static void rtl83xx_halt(void)
{
	printk("System halted.\n");
	while(1);
}

static void __init rtl83xx_setup(void)
{
	unsigned int val;

	pr_info("Registering _machine_restart\n");
	_machine_restart = rtl83xx_restart;
	_machine_halt = rtl83xx_halt;

	/* Setup System LED. Bit 15 (14 for RTL8390) then allows to toggle it */
	if (soc_info.family == RTL8380_FAMILY_ID) {
		val = rtl83xx_r32(RTL838X_LED_GLB_CTRL);
		val |= 3 <<16;
		rtl83xx_w32(val, RTL838X_LED_GLB_CTRL);
	} else {
		val = rtl83xx_r32(RTL838X_LED_GLB_CTRL);
		val |= 3 <<15;
		rtl83xx_w32(val, RTL838X_LED_GLB_CTRL);
	}
}

void __init plat_mem_setup(void)
{
	void *dtb;

	pr_info("plat_mem_setup called \n");

	set_io_port_base(KSEG1);

	if (fw_passed_dtb) /* UHI interface */
		dtb = (void *)fw_passed_dtb;
	else if (__dtb_start != __dtb_end)
		dtb = (void *)__dtb_start;
	else
		panic("no dtb found");

	/*
	 * Load the devicetree. This causes the chosen node to be
	 * parsed resulting in our memory appearing
	 */
	__dt_setup_arch(dtb);

	rtl83xx_setup();
}


void __init plat_time_init(void)
{
	struct device_node *np;
	u32 freq = 500000000;
	const char *s;

	np = of_find_node_by_name(NULL, "cpus");
	if (!np) {
		pr_err("Missing 'cpus' DT node, using default frequency.");
	} else {
		if (of_property_read_u32(np, "frequency", &freq) < 0)
			pr_err("No 'frequency' property in DT, using default.");
		else
			pr_info("CPU frequency from device tree: %d", freq);
		of_node_put(np);
	}

	mips_hpt_frequency = freq / 2;

	pll_reset_value = rtl83xx_r32(RTL838X_PLL_CML_CTRL);
	pr_info("PLL control register: %x\n", pll_reset_value);

#ifdef CONFIG_SERIAL_8250
	rtl83xx_init_uart0();
	np = of_find_node_by_path("poe-uart");
	if (np) {
		if (!of_property_read_string(np, "status", &s))
			if (!strcmp(s, "okay"))
				rtl83xx_init_uart1();
	}
#endif
}
