// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 * Copyright (C) 2020 B. Koblitz
 */
#ifndef _MACH_RTL838X_H_
#define _MACH_RTL838X_H_

struct rtl83xx_soc_info {
	unsigned char *name;
	unsigned int id;
	unsigned int family;
};

/* High 16 bits of MODEL_NAME_INFO register */
#define RTL8389_FAMILY_ID	0x8389
#define RTL8328_FAMILY_ID	0x8328
#define RTL8390_FAMILY_ID	0x8390
#define RTL8350_FAMILY_ID	0x8350
#define RTL8380_FAMILY_ID	0x8380
#define RTL8330_FAMILY_ID	0x8330

/* Register access macros */
#define rtl83xx_r8(reg)		__raw_readb(reg)
#define rtl83xx_w8(val, reg)	__raw_writeb(val, reg)
#define rtl83xx_r32(reg)	__raw_readl(reg)
#define rtl83xx_w32(val, reg)	__raw_writel(val, reg)

// TODO: move to DT
#define RTL83XX_SWITCH_BASE			((volatile void *) 0xBB000000)
#define RTL838X_MODEL_NAME_INFO		(RTL83XX_SWITCH_BASE + 0x00D4)
#define RTL839X_MODEL_NAME_INFO		(RTL83XX_SWITCH_BASE + 0x0FF0)


/*
 * Reset
 */
#define	RGCR				(RTL83XX_SWITCH_BASE + 0x1E70)
#define RTL839X_RST_GLB_CTRL		(RTL83XX_SWITCH_BASE + 0x14)
#define RTL838X_RST_GLB_CTRL_1		(RTL83XX_SWITCH_BASE + 0x40)

/* LED control by switch */
#define RTL838X_LED_MODE_SEL		(RTL83XX_SWITCH_BASE + 0x1004)
#define RTL838X_LED_MODE_CTRL		(RTL83XX_SWITCH_BASE + 0xA004)
#define RTL838X_LED_P_EN_CTRL		(RTL83XX_SWITCH_BASE + 0xA008)

/* LED control by software */
#define RTL838X_LED_SW_CTRL		(RTL83XX_SWITCH_BASE + 0xA00C)
#define RTL838X_LED0_SW_P_EN_CTRL	(RTL83XX_SWITCH_BASE + 0xA010)
#define RTL838X_LED1_SW_P_EN_CTRL	(RTL83XX_SWITCH_BASE + 0xA014)
#define RTL838X_LED2_SW_P_EN_CTRL	(RTL83XX_SWITCH_BASE + 0xA018)
#define RTL838X_LED_SW_P_CTRL(p)	(RTL83XX_SWITCH_BASE + 0xA01C + ((p) << 2))

/*
 * Switch interrupts
 */
#define RTL838X_IMR_GLB			(RTL83XX_SWITCH_BASE + 0x1100)
#define RTL838X_IMR_PORT_LINK_STS_CHG	(RTL83XX_SWITCH_BASE + 0x1104)
#define RTL838X_ISR_GLB_SRC		(RTL83XX_SWITCH_BASE + 0x1148)
#define RTL838X_ISR_PORT_LINK_STS_CHG	(RTL83XX_SWITCH_BASE + 0x114C)
#define RTL839X_IMR_GLB			(RTL83XX_SWITCH_BASE + 0x0064)
#define RTL839X_IMR_PORT_LINK_STS_CHG	(RTL83XX_SWITCH_BASE + 0x0068)
#define RTL839X_ISR_GLB_SRC		(RTL83XX_SWITCH_BASE + 0x009c)
#define RTL839X_ISR_PORT_LINK_STS_CHG	(RTL83XX_SWITCH_BASE + 0x00a0)


// TODO: not used
/*
 * MDIO via Realtek's SMI interface
 */
#define RTL838X_SMI_ACCESS_PHY_CTRL_3	(RTL83XX_SWITCH_BASE + 0xa1c4)
#define RTL838X_SMI_PORT0_5_ADDR_CTRL	(RTL83XX_SWITCH_BASE + 0xa1c8)


#endif   /* _MACH_RTL838X_H_ */
