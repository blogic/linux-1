// SPDX-License-Identifier: GPL-2.0-only
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/spi-nor.h>

#include <mach-rtl83xx.h>
#include "rtl83xx-spiflash.h"

extern struct rtl83xx_soc_info soc_info;

static int rtl83xx_nor_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf, size_t len);
static int rtl83xx_nor_write_reg(struct spi_nor *nor, u8 opcode, const u8 *buf, size_t len);


struct rtl83xx_nor {
	struct spi_nor nor;
	struct device *dev;
	void __iomem *base;
	u32 cs;
	bool fourByteMode;
};

static inline u32 rtl83xx_read(struct rtl83xx_nor *rtnor, u32 reg, bool wait)
{
	if (wait)
		while (!(__raw_readl(rtnor->base + RTL8XXX_SPI_SFCSR) & RTL8XXX_SPI_SFCSR_RDY))
			;
	return __raw_readl(rtnor->base + reg);
}

static inline void rtl83xx_write(struct rtl83xx_nor *rtnor, u32 reg, u32 value, bool wait)
{
	if (wait)
		while (!(__raw_readl(rtnor->base + RTL8XXX_SPI_SFCSR) & RTL8XXX_SPI_SFCSR_RDY))
			;
	__raw_writel(value, rtnor->base + reg);
}

static uint32_t spi_prep(struct rtl83xx_nor *rtnor)
{
	uint32_t ret, init;

	/* TODO: is this nonsense even necessary? */
	init = (RTL8XXX_SPI_SFCSR_CSB0 | RTL8XXX_SPI_SFCSR_CSB1) & RTL8XXX_SPI_SFCSR_LEN_MASK;
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, init, true);	// deactivate CS0, CS1
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, 0, true);	// activate CS0,CS1
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, init, true);	// deactivate CS0, CS1

	/* CS bitfield is active low, so reversed logic */
	if (rtnor->cs == 0)
		ret = RTL8XXX_SPI_SFCSR_LEN1 | RTL8XXX_SPI_SFCSR_CSB1;
	else
		ret = RTL8XXX_SPI_SFCSR_LEN1 | RTL8XXX_SPI_SFCSR_CSB0 | RTL8XXX_SPI_SFCSR_CS;

	return ret;
}

static inline u32 rtl83xx_nor_get_sr(struct rtl83xx_nor *rtnor)
{
	u32 value;

	rtl83xx_nor_read_reg(&rtnor->nor, SPINOR_OP_RDSR, (u8 *)&value, 4);

	return value;
}

static inline void spi_write_enable(struct rtl83xx_nor *rtnor)
{
	rtl83xx_nor_write_reg(&rtnor->nor, SPINOR_OP_WREN, NULL, 0);
}

static inline void spi_write_disable(struct rtl83xx_nor *rtnor)
{
	rtl83xx_nor_write_reg(&rtnor->nor, SPINOR_OP_WRDI, NULL, 0);
}

static inline void spi_4b_set(struct rtl83xx_nor *rtnor)
{
	rtl83xx_nor_write_reg(&rtnor->nor, SPINOR_OP_EN4B, NULL, 0);
}

/* Do fast read in 3 or 4 Byte addressing mode */
static ssize_t rtl83xx_do_4bf_read(struct rtl83xx_nor *rtnor, loff_t offset,
				   size_t len, u_char *buf, uint8_t command)
{
	uint32_t sfcsr;
	ssize_t ret = 0;
	int sfcsr_addr_len = rtnor->fourByteMode? 0x3 : 0x2;
	int sfdr_addr_shift = rtnor->fourByteMode? 0 : 8;

	sfcsr = spi_prep(rtnor);
	sfcsr &= RTL8XXX_SPI_SFCSR_LEN_MASK;

	/* Send read command */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, command << 24, false);

	/* Send address */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | (sfcsr_addr_len << 28), true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, offset << sfdr_addr_shift, false);

	// TODO: necessary?
	/* Dummy cycles */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, 0, false);

	/* Read Data, 4 bytes at a time */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN4, true);
	while (len >= 4){
		*((uint32_t*) buf) = rtl83xx_read(rtnor, RTL8XXX_SPI_SFDR, true);
		buf += 4;
		len -= 4;
		ret += 4;
	}

	/* Read remainder 1 byte at a time */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	while (len > 0) {
		*(buf) = rtl83xx_read(rtnor, RTL8XXX_SPI_SFDR, true) >> 24;
		buf++;
		len--;
		ret++;
	}

	return ret;
}

/* Do write (Page Programming) in 3 or 4 Byte addressing mode */
static ssize_t rtl83xx_do_4b_write(struct rtl83xx_nor *rtnor, loff_t to,
				   size_t len, const u_char *buf, uint8_t command)
{
	uint32_t sfcsr;
	ssize_t ret = 0;
	int sfcsr_addr_len = rtnor->fourByteMode? 0x3 : 0x2;
	int sfdr_addr_shift = rtnor->fourByteMode? 0 : 8;

	sfcsr = spi_prep(rtnor);
	sfcsr &= RTL8XXX_SPI_SFCSR_LEN_MASK;

	/* Send write command, command IO-width is 1 (bit 25/26) */
	// TODO: set IO_WIDTH (25:26)
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, command << 24, false);

	/* Send address */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | (sfcsr_addr_len << 28), true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, to << sfdr_addr_shift, false);

	/* Write Data, 1 byte at a time, if we are not 4-byte aligned */
	if (((long)buf) % 4) {
		rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
		while (len > 0 && (((long)buf) % 4)) {
			rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, *buf << 24, true);
			buf++;
			len--;
			ret++;
		}
	}

	/* Now we can write 4 bytes at a time */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN4, true);
	while (len >= 4) {
		rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, *((uint32_t*)buf), true);
		buf += 4;
		len -= 4;
		ret += 4;
	}

	/* Final bytes might need to be written 1 byte at a time, again */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	while (len > 0) {
		rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, *buf << 24, true);
		buf++;
		len--;
		ret++;
	}

	return ret;
}

static int rtl83xx_nor_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf, size_t len)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	uint32_t sfcsr;

	sfcsr = spi_prep(rtnor);

	/* TODO: optimize */
	sfcsr &= RTL8XXX_SPI_SFCSR_LEN_MASK | RTL8XXX_SPI_SFCSR_LEN1;

	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, opcode << 24, true);

	while (len > 0) {
		*(buf) = rtl83xx_read(rtnor, RTL8XXX_SPI_SFDR, true) >> 24;
		buf++;
		len--;
	}

	return 0;
}

static int rtl83xx_nor_write_reg(struct spi_nor *nor, u8 opcode, const u8 *buf,
				 size_t len)
{
	uint32_t sfcsr, sfdr;
	struct rtl83xx_nor *rtnor = nor->priv;

	sfcsr = spi_prep(rtnor);
	sfdr = opcode << 24;

	if (len == 1) { /* SPINOR_OP_WRSR */
		sfdr |= buf[0];
		sfcsr |= RTL8XXX_SPI_SFCSR_LEN2; // TODO: this can't be right
	}
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, sfdr, true);

	return 0;
}

static ssize_t rtl83xx_nor_read(struct spi_nor *nor, loff_t from, size_t len,
				u_char *buf)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	ssize_t ret = 0;
	uint32_t offset = 0;
	uint8_t command = SPINOR_OP_READ_FAST;

	/* Do fast read in 3, or 4-byte mode on large Macronix chips */
	if (rtnor->fourByteMode) {
		command = SPINOR_OP_READ_FAST_4B;
		spi_4b_set(rtnor);
	}

	/* TODO: do timeout and return error */
	while(rtl83xx_nor_get_sr(rtnor) & SR_WIP)
		;
	do {
		spi_write_enable(rtnor);
	} while (!(rtl83xx_nor_get_sr(rtnor) & SR_WEL));

	while (len >= SPI_MAX_TRANSFER_SIZE) {
		ret += rtl83xx_do_4bf_read(rtnor, from + offset, SPI_MAX_TRANSFER_SIZE,
					   buf + offset, command);
		len -= SPI_MAX_TRANSFER_SIZE;
		offset += SPI_MAX_TRANSFER_SIZE;
	}

	if (len > 0)
		ret += rtl83xx_do_4bf_read(rtnor, from + offset, len, buf + offset,
					   command);

	return ret;
}

static ssize_t rtl83xx_nor_write(struct spi_nor *nor, loff_t to, size_t len,
				 const u_char *buf)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	ssize_t ret = 0;
	uint32_t offset = 0;
	uint8_t command = SPINOR_OP_PP;

	/* Do write in 4-byte mode on large Macronix chips */
	if (rtnor->fourByteMode) {
		command = SPINOR_OP_PP_4B;
		spi_4b_set(rtnor);
	}

	while (len >= SPI_MAX_TRANSFER_SIZE) {
		while (rtl83xx_nor_get_sr(rtnor) & SR_WIP)
			;
		do {
			spi_write_enable(rtnor);
		} while (!(rtl83xx_nor_get_sr(rtnor) & SR_WEL));

		ret += rtl83xx_do_4b_write(rtnor, to + offset, SPI_MAX_TRANSFER_SIZE,
					   buf + offset, command);
		len -= SPI_MAX_TRANSFER_SIZE;
		offset += SPI_MAX_TRANSFER_SIZE;
	}

	if (len > 0) {
		while (rtl83xx_nor_get_sr(rtnor) & SR_WIP)
			;
		do {
			spi_write_enable(rtnor);
		} while (!(rtl83xx_nor_get_sr(rtnor) & SR_WEL));
		ret += rtl83xx_do_4b_write(rtnor, to + offset, len, buf + offset,
					   command);
	}

	return ret;
}

static int rtl83xx_erase(struct spi_nor *nor, loff_t offs)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	int sfcsr_addr_len = rtnor->fourByteMode? 0x3 : 0x2;
	int sfdr_addr_shift = rtnor->fourByteMode? 0 : 8;
	uint32_t sfcsr;

	/* Do erase in 4-byte mode on large Macronix chips */
	if (rtnor->fourByteMode)
		spi_4b_set(rtnor);

	/* TODO: do timeout and return error */
	while (rtl83xx_nor_get_sr(rtnor) & SR_WIP)
		;
	do {
		spi_write_enable(rtnor);
	} while (!(rtl83xx_nor_get_sr(rtnor) & SR_WEL));

	sfcsr = spi_prep(rtnor);

	/* Send erase command, command IO-width is 1 (bit 25/26) */
	// TODO: set IO_WIDTH (25:26)
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, nor->erase_opcode << 24, false);

	/* Send address */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | (sfcsr_addr_len << 28), true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, offs << sfdr_addr_shift, false);

	return 0;
}

static const struct spi_nor_controller_ops rtl83xx_controller_ops = {
	.read_reg = rtl83xx_nor_read_reg,
	.write_reg = rtl83xx_nor_write_reg,
	.read = rtl83xx_nor_read,
	.write = rtl83xx_nor_write,
	.erase = rtl83xx_erase,
};

/* Vendor BSP's "OTTO838x_SPIF_CTRLR_ADDR_MODE", used for RTL833x and RTL838x */
static int rtl838x_get_addr_mode(void)
{
	int addrmode = 3;
	u32 reg;

	__raw_writel(0x3, RTL838X_INT_RW_CTRL);
	if (!__raw_readl(RTL838X_EXT_VERSION)) {
		if (__raw_readl(RTL838X_STRAP_DBG) & (1 << 29))
			addrmode = 4;
	} else {
		reg = __raw_readl(RTL838X_PLL_CML_CTRL);
		if ((reg & (1 << 30)) && (reg & (1 << 31)))
			addrmode = 4;
		if ((!(reg & (1 << 30)))
		     && __raw_readl(RTL838X_STRAP_DBG) & (1 << 29))
			addrmode = 4;
	}
	__raw_writel(0x0, RTL838X_INT_RW_CTRL);

	return addrmode;
}

/* RTL835x and RTL839x */
static int rtl8390_get_addr_mode(struct rtl83xx_nor *rtnor)
{
	if (rtl83xx_read(rtnor, RTL8XXX_SPI_SFCR2, true) & RTL8XXX_SPI_SFCR2_ADDRMODE)
		return 4;
	else
		return 3;
}

int rtl83xx_spi_nor_scan(struct spi_nor *nor, const char *name)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	static const struct spi_nor_hwcaps hwcaps = {
		.mask = SNOR_HWCAPS_READ | SNOR_HWCAPS_PP
			| SNOR_HWCAPS_READ_FAST
	};
	u32 sfcr;
	int ret;

	/* Turn on big-endian byte ordering */
	sfcr = rtl83xx_read(rtnor, RTL8XXX_SPI_SFCR, true);
	sfcr |= RTL8XXX_SPI_SFCR_RBO | RTL8XXX_SPI_SFCR_WBO;
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCR, sfcr, true);

	ret = spi_nor_scan(nor, NULL, &hwcaps);

	return ret;
}

int rtl83xx_nor_init(struct rtl83xx_nor *rtnor,
		     struct device_node *flash_node)
{
	struct spi_nor *nor;
	int ret;

	nor = &rtnor->nor;
	nor->dev = rtnor->dev;
	nor->priv = rtnor;
	spi_nor_set_flash_node(nor, flash_node);
	nor->controller_ops = &rtl83xx_controller_ops;
	nor->mtd.name = "rtl83xx-spiflash";
	nor->erase_opcode = rtnor->fourByteMode? SPINOR_OP_SE_4B
					: SPINOR_OP_SE;

	/* initialized with NULL */
	ret = rtl83xx_spi_nor_scan(nor, NULL);
	if (ret)
		return ret;

	spi_write_disable(rtnor);

	ret = mtd_device_parse_register(&nor->mtd, NULL, NULL, NULL, 0);

	return ret;
}

static int rtl83xx_nor_drv_probe(struct platform_device *pdev)
{
	struct device_node *flash_np;
	struct resource *res;
	struct rtl83xx_nor *rtnor;
	int addrMode, ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No DT found\n");
		return -EINVAL;
	}

	rtnor = devm_kzalloc(&pdev->dev, sizeof(*rtnor), GFP_KERNEL);
	if (!rtnor)
		return -ENOMEM;
	platform_set_drvdata(pdev, rtnor);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtnor->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtnor->base))
		return PTR_ERR(rtnor->base);

	rtnor->dev = &pdev->dev;

	/* Only support one attached flash */
	flash_np = of_get_next_available_child(pdev->dev.of_node, NULL);
	if (!flash_np) {
		dev_err(&pdev->dev, "no SPI flash device to configure\n");
		ret = -ENODEV;
		goto nor_free;
	}

	/* Optional chip select, defaults to 0 */
	ret = of_property_read_u32(flash_np, "reg", &rtnor->cs);
	if (ret)
		rtnor->cs = 0;

	/* Get the 3/4 byte address mode as configured by strapped SoC pins */
	if (soc_info.family == RTL8390_FAMILY_ID)
		addrMode = rtl8390_get_addr_mode(rtnor);
	else
		addrMode = rtl838x_get_addr_mode();
	pr_info("SPI flash address mode is %d bytes\n", addrMode);
	if (addrMode == 4)
		rtnor->fourByteMode = true;

	ret = rtl83xx_nor_init(rtnor, flash_np);

nor_free:
	return ret;
}

// TODO
static int rtl83xx_nor_drv_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id rtl83xx_nor_of_ids[] = {
	{ .compatible = "realtek,rtl83xx-spiflash"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rtl83xx_nor_of_ids);

static struct platform_driver rtl83xx_nor_driver = {
	.probe = rtl83xx_nor_drv_probe,
	.remove = rtl83xx_nor_drv_remove,
	.driver = {
		.name = "rtl83xx-spiflash",
		.pm = NULL,
		.of_match_table = rtl83xx_nor_of_ids,
	},
};

module_platform_driver(rtl83xx_nor_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RTL83xx SPI NOR Flash Driver");
