// SPDX-License-Identifier: GPL-2.0-only

/*
 * Setup for the Realtek RTL83XX SoC series:
 *	Memory, Timer and Serial
 *
 * Copyright (C) 2020 B. Koblitz
 * based on the original BSP by
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 *
 */

#include <linux/of_fdt.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <mach-rtl83xx.h>

extern struct rtl83xx_soc_info soc_info;


static void rtl83xx_restart(char *command)
{
	if (soc_info.family == RTL8380_FAMILY_ID) {
		/* Reset Global Control1 Register */
		rtl83xx_w32(1, RTL838X_RST_GLB_CTRL_1);
	} else if (soc_info.family == RTL8390_FAMILY_ID) {
		/* If calling reset vector fails, reset entire chip */
		rtl83xx_w32(0xFFFFFFFF, RTL839X_RST_GLB_CTRL);
	}
}

static void rtl83xx_halt(void)
{
	printk("System halted.\n");
	while(1);
}

void __init plat_mem_setup(void)
{
	set_io_port_base(KSEG1);

	_machine_restart = rtl83xx_restart;
	_machine_halt = rtl83xx_halt;
}

void __init plat_time_init(void)
{
	struct device_node *np;
	u32 freq = 500000000;

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
}
