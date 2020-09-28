// SPDX-License-Identifier: GPL-2.0-only
/* Register definitions for RTL83xx drivers */
#ifndef _RTL83XX_SPIFLASH_H_
#define _RTL83XX_SPIFLASH_H_

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

/* SPI Flash Data Registers */
#define RTL8XXX_SPI_SFDR	0x0c
#define RTL8XXX_SPI_SFDR2	0x10

#endif /* _RTL83XX_SPIFLASH_H_ */

