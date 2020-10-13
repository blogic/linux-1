// SPDX-License-Identifier: GPL-2.0-only
/*
 * prom.c
 * Early intialization code for the Realtek RTL83XX SoC
 *
 * based on the original BSP by
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 * Copyright (C) 2020 B. Koblitz
 *
 */

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>
#include <asm/prom.h>

#include <mach-rtl83xx.h>


extern char arcs_cmdline[];
extern const char __appended_dtb;

struct rtl83xx_soc_info soc_info;
const void *fdt;


const char *get_system_type(void)
{
	return soc_info.name;
}

void __init prom_free_prom_memory(void)
{
	return;
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

void __init device_tree_init(void)
{
	struct device_node *np;
	const char *s;

	if (!fdt_check_header(&__appended_dtb)) {
		fdt = &__appended_dtb;
		pr_info("Using appended Device Tree.\n");
	}
	initial_boot_params = (void *)fdt;
	unflatten_and_copy_device_tree();

	np = of_find_node_by_path("poe-uart");
	if (np) {
		/* If specified and not disabled, can only be uart1 */
		if (!of_property_read_string(np, "status", &s))
			if (!strcmp(s, "okay"))
				rtl83xx_init_uart1();
		of_node_put(np);
	}
}

static void __init prom_init_cmdline(void)
{
	int argc = fw_arg0;
	char **argv = (char **) KSEG1ADDR(fw_arg1);
	int i;

	arcs_cmdline[0] = '\0';

	for (i = 0; i < argc; i++) {
		char *p = (char *) KSEG1ADDR(argv[i]);

		if (CPHYSADDR(p) && *p) {
			strlcat(arcs_cmdline, p, sizeof(arcs_cmdline));
			strlcat(arcs_cmdline, " ", sizeof(arcs_cmdline));
		}
	}
	pr_info("Kernel command line: %s\n", arcs_cmdline);
}

void __init prom_init(void)
{
	void *dtb;
	uint32_t model;

	/* uart0 */
	setup_8250_early_printk_port(0xb8002000, 2, 0);

	model = rtl83xx_r32(RTL838X_MODEL_NAME_INFO) >> 16;
	if (model != 0x8330 && model != 0x8332 && model != 0x8380 &&
	    model != 0x8382 ) {
		model = rtl83xx_r32(RTL839X_MODEL_NAME_INFO) >> 16;
	}

	soc_info.id = model;

	switch (model) {
		case 0x8328:
			soc_info.name="RTL8328";
			soc_info.family = RTL8328_FAMILY_ID;
			break;
		case 0x8332:
			soc_info.name="RTL8332";
			soc_info.family = RTL8380_FAMILY_ID;
			break;
		case 0x8380:
			soc_info.name="RTL8380";
			soc_info.family = RTL8380_FAMILY_ID;
			break;
		case 0x8382:
			soc_info.name="RTL8382";
			soc_info.family = RTL8380_FAMILY_ID;
			break;
		case 0x8390:
			soc_info.name="RTL8390";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		case 0x8391:
			soc_info.name="RTL8391";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		case 0x8392:
			soc_info.name="RTL8392";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		case 0x8393:
			soc_info.name="RTL8393";
			soc_info.family = RTL8390_FAMILY_ID;
			break;
		default:
			soc_info.name="DEFAULT";
			soc_info.family = 0;
	}
	pr_info("SoC Type: %s\n", soc_info.name);
	prom_init_cmdline();

	if (fw_passed_dtb)
		dtb = (void *)fw_passed_dtb;
	else if (__dtb_start != __dtb_end)
		dtb = (void *)__dtb_start;
	else
		panic("no dtb found");

	__dt_setup_arch(dtb);
}

