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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>
#include <asm/bootinfo.h>
#include <asm/addrspace.h>
#include <asm/page.h>
#include <asm/cpu.h>
#include <mach-rtl83xx.h>

struct rtl83xx_soc_info soc_info;
const void *fdt;

extern char arcs_cmdline[];
extern const char __appended_dtb;


void prom_console_init(void)
{
	/* UART 16550A is initialized by the bootloader */
}

#ifdef CONFIG_EARLY_PRINTK
#define RTL83XX_UART0_BASE	((volatile void *)(0xb8002000UL))
#define UART0_THR		(RTL83XX_UART0_BASE + 0x000)
#define UART0_FCR		(RTL83XX_UART0_BASE + 0x008)
#define FCR_TXRST		0x04
#define FCR_RTRG		0xC0
#define UART0_LSR		(RTL83XX_UART0_BASE + 0x014)
#define LSR_THRE		0x20
#define TXCHAR_AVAIL		0x00

void unregister_prom_console(void)
{

}

void disable_early_printk(void)
{

}

void prom_putchar(char c)
{
	unsigned int retry = 0;

	do {
		// TODO: check if this stupid delay loop is necessary
		if (retry++ >= 30000) {
			/* Reset Tx FIFO */
			rtl83xx_w8(FCR_TXRST | FCR_RTRG, UART0_FCR);
			return;
		}
	} while ((rtl83xx_r8(UART0_LSR) & LSR_THRE) == TXCHAR_AVAIL);

	/* Send Character */
	rtl83xx_w8(c, UART0_THR);
}

char prom_getchar(void)
{
	return '\0';
}
#endif /* CONFIG_EARLY_PRINTK */


const char *get_system_type(void)
{
	return soc_info.name;
}

void __init prom_free_prom_memory(void)
{
	return;
}

void __init device_tree_init(void)
{
	if (!fdt_check_header(&__appended_dtb)) {
		fdt = &__appended_dtb;
		pr_info("Using appended Device Tree.\n");
	}
	initial_boot_params = (void *)fdt;
	unflatten_and_copy_device_tree();
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

/* Do basic initialization */
void __init prom_init(void)
{
	uint32_t model;

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
}

