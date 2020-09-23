// SPDX-License-Identifier: GPL-2.0-only
/*
 * 8250 serial console setup for the Realtek RTL83XX SoC
 *
 * based on the original BSP by
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 *
 * Copyright (C) 2020 B. Koblitz
 *
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/tty.h>
#include <linux/clk.h>

#include <rtl83xx-regs.h>
#include <mach-rtl83xx.h>

extern struct rtl83xx_soc_info soc_info;
extern char arcs_cmdline[];

#ifdef CONFIG_SERIAL_8250
/* Configure UART0 */
void __init rtl83xx_init_uart0(void)
{
	struct device_node *np;
	struct uart_port p;
	resource_size_t base;
	int baud = 0, ret;
	char *s;

	np = of_find_node_by_path("serial0");
	if (!np) {
		pr_info("no DT node found for serial0\n");
		return;
	}
	ret = of_property_read_u32(np, "reg", &base);
	if (ret) {
		pr_info("no base address found for serial0\n");
		return;
	}

	s = strstr(arcs_cmdline, "console=ttyS0,");
	if (s) {
		s += 14;
		baud = simple_strtoul(s, NULL, 10);
		memset(&p, 0, sizeof(p));
		p.type = PORT_16550A;
		p.iotype = UPIO_MEM;
		p.membase = (unsigned char *)base;
		p.irq = RTL83XX_IRQ_UART0;
		p.uartclk = SYSTEM_FREQ - (24 * baud);
		p.flags = UPF_SKIP_TEST | UPF_LOW_LATENCY | UPF_FIXED_TYPE;
		p.regshift = 2;
		p.fifosize = 1;
		early_serial_setup(&p);
	}
}

/* Enable UART1 */
void __init rtl83xx_init_uart1(void)
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
#endif
