// SPDX-License-Identifier: GPL-2.0-only
/* Register definitions for RTL83xx drivers */

#ifndef _RTL83XX_REGS_H_
#define _RTL83XX_REGS_H_

/* Used to detect address length pin strapping on RTL833x/RTL838x */
#define RTL838X_INT_RW_CTRL		(RTL83XX_SWITCH_BASE + 0x58)
#define RTL838X_EXT_VERSION		(RTL83XX_SWITCH_BASE + 0xD0)
#define RTL838X_PLL_CML_CTRL		(RTL83XX_SWITCH_BASE + 0xFF8)
#define RTL838X_STRAP_DBG		(RTL83XX_SWITCH_BASE + 0x100C)

/* Interrupt numbers/bits */
#define RTL83XX_IRQ_UART0		31
#define RTL83XX_IRQ_UART1		30
#define RTL83XX_IRQ_TC0			29
#define RTL83XX_IRQ_TC1			28
#define RTL83XX_IRQ_OCPTO		27
#define RTL83XX_IRQ_HLXTO		26
#define RTL83XX_IRQ_SLXTO		25
#define RTL83XX_IRQ_NIC			24
#define RTL83XX_IRQ_GPIO_ABCD		23
#define RTL83XX_IRQ_GPIO_EFGH		22
#define RTL83XX_IRQ_RTC			21
#define	RTL83XX_IRQ_SWCORE		20
#define RTL83XX_IRQ_WDT_IP1		19
#define RTL83XX_IRQ_WDT_IP2		18

/* GMII pinmux on RTL838x */
#define RTL838X_GMII_INTF_SEL		(RTL83XX_SWITCH_BASE + 0x1000)
#define RTL838X_GMII_INTF_SEL_UART1	BIT(4)
#define RTL838X_GMII_INTF_SEL_JTAG	(BIT(2) | BIT(3))
#define RTL838X_GMII_INTF_SEL_GMII	(BIT(0) | BIT(1))

/* GMII pinmux on RTL839x */
#define RTL839X_MAC_IF_CTRL		(RTL83XX_SWITCH_BASE + 0x04)
#define RTL839X_MAC_IF_CTRL_JTAG	BIT(1)
#define RTL839X_MAC_IF_CTRL_UART	BIT(0)


/* UART */
#define SYSTEM_FREQ		200000000


/* SPI */
#define SPI_MAX_TRANSFER_SIZE		256

/* SPI Flash Configuration Register */
#define RTL8XXX_SPI_SFCR		0x00
#define RTL8XXX_SPI_SFCR_RBO		BIT(28)
#define RTL8XXX_SPI_SFCR_WBO		BIT(27)

/* SPI Flash Configuration Register 2 */
#define RTL8XXX_SPI_SFCR2		0x04
#define RTL8XXX_SPI_SFCR2_ADDRMODE	BIT(9)

/* SPI Flash Control and Status Register */
#define RTL8XXX_SPI_SFCSR		0x08
#define RTL8XXX_SPI_SFCSR_CSB0		BIT(31)
#define RTL8XXX_SPI_SFCSR_CSB1		BIT(30)
#define RTL8XXX_SPI_SFCSR_RDY		BIT(27)
#define RTL8XXX_SPI_SFCSR_CS		BIT(24)
#define RTL8XXX_SPI_SFCSR_LEN_MASK	~(0x03 << 28)
#define RTL8XXX_SPI_SFCSR_LEN1		(0x00 << 28)
#define RTL8XXX_SPI_SFCSR_LEN2		(0x01 << 28)
#define RTL8XXX_SPI_SFCSR_LEN4		(0x03 << 28)

/* SPI Flash Data Register */
#define RTL8XXX_SPI_SFDR	0x0c

/* SPI Flash Data Register 2 */
#define RTL8XXX_SPI_SFDR2	0x10

#endif /* _RTL83XX_REGS_H_ */

