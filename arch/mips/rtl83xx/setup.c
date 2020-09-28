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


extern struct rtl83xx_soc_info soc_info;


static void rtl83xx_restart(char *command)
{
	void (*reset_8390)(void) = (void *) 0xbfc00000;

	if (soc_info.family == RTL8380_FAMILY_ID) {
		/* Reset Global Control1 Register */
		rtl83xx_w32(1, RTL838X_RST_GLB_CTRL_1);
	} else if (soc_info.family == RTL8390_FAMILY_ID) {
		/* Call SoC reset vector in flash memory */
		reset_8390();
		/* If calling reset vector fails, reset entire chip */
		rtl83xx_w32(0xFFFFFFFF, RTL839X_RST_GLB_CTRL);
		/* If this fails, halt the CPU */
		while (1);
	}
}

static void rtl83xx_halt(void)
{
	printk("System halted.\n");
	while(1);
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

	__dt_setup_arch(dtb);

	_machine_restart = rtl83xx_restart;
	_machine_halt = rtl83xx_halt;
}

static void __init rtl83xx_init_uart1(void)
{
	u32 value;

	if (soc_info.family == RTL8380_FAMILY_ID) {
		rtl83xx_w32(RTL838X_GMII_INTF_SEL_UART1, RTL838X_GMII_INTF_SEL);
	} else if (soc_info.family == RTL8390_FAMILY_ID) {
		value = rtl83xx_r32(RTL839X_MAC_IF_CTRL);
		value &= ~(RTL839X_MAC_IF_CTRL_JTAG);
		value |= RTL839X_MAC_IF_CTRL_UART;
		rtl83xx_w32(value, RTL839X_MAC_IF_CTRL);
	}
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

	np = of_find_node_by_path("poe-uart");
	if (np) {
		/* If specified and not disabled, can only be uart1 */
		if (!of_property_read_string(np, "status", &s))
			if (!strcmp(s, "okay"))
				rtl83xx_init_uart1();
	}
}
