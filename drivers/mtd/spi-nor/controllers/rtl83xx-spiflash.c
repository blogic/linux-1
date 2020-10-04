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

/*
 * Deactivate both chip selects and return base SFCSR register settings: correct
 * CS, data length 1, IO width 1.
 */
static uint32_t spi_prep(struct rtl83xx_nor *rtnor)
{
	uint32_t init, ret;

	/* Deactivate CS0 and CS1 first */
	init = (RTL8XXX_SPI_SFCSR_CSB0 | RTL8XXX_SPI_SFCSR_CSB1) & RTL8XXX_SPI_SFCSR_LEN_MASK;
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, init, true);

	/* CS bitfield is active low, so reversed logic */
	if (rtnor->cs == 0)
		ret = RTL8XXX_SPI_SFCSR_LEN1 | RTL8XXX_SPI_SFCSR_CSB1;
	else
		ret = RTL8XXX_SPI_SFCSR_LEN1 | RTL8XXX_SPI_SFCSR_CSB0 | RTL8XXX_SPI_SFCSR_CS;

	return ret;
}

static int rtl83xx_nor_read_reg(struct spi_nor *nor, u8 opcode, u8 *buf, size_t len)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	uint32_t sfcsr;

	sfcsr = spi_prep(rtnor);
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
	struct rtl83xx_nor *rtnor = nor->priv;
	uint32_t sfcsr, sfdr, len_bits;

	sfcsr = spi_prep(rtnor);
	sfdr = opcode << 24;

	if (len == 0) {
		len_bits = RTL8XXX_SPI_SFCSR_LEN1;
	} else if (len == 1) {
		sfdr |= buf[0] << 16;
		len_bits = RTL8XXX_SPI_SFCSR_LEN2;
	} else if (len == 2) {
		sfdr |= buf[0] << 16;
		sfdr |= buf[1] << 8;
		len_bits = RTL8XXX_SPI_SFCSR_LEN3;
	} else {
		return -EINVAL;
	}
	sfcsr |= len_bits;

	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, sfdr, true);

	return 0;
}

static ssize_t rtl83xx_nor_read(struct spi_nor *nor, loff_t from, size_t len,
				u_char *buf)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	uint32_t sfcsr;
	ssize_t ret = 0;
	int sfcsr_addrlen_bits, i;

	if (rtnor->nor.addr_width == 4) {
		sfcsr_addrlen_bits = 0x03;
	} else {
		sfcsr_addrlen_bits = 0x02;
		from <<= 8;
	}

	sfcsr = spi_prep(rtnor);
	sfcsr &= RTL8XXX_SPI_SFCSR_LEN_MASK;

	/* Send read command */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, rtnor->nor.read_opcode << 24, false);

	/* Send address */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | (sfcsr_addrlen_bits << 28), true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, from, false);

	/* Dummy cycles */
	for (i = 0; i < nor->read_dummy / 8; i++) {
		rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr, true);
		rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, 0, false);
	}

	/* Read 4 bytes at a time */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN4, true);
	while (len >= 4){
		*((uint32_t*) buf) = rtl83xx_read(rtnor, RTL8XXX_SPI_SFDR, false);
		buf += 4;
		len -= 4;
		ret += 4;
	}

	/* Read remainder 1 byte at a time */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	while (len > 0) {
		*(buf) = rtl83xx_read(rtnor, RTL8XXX_SPI_SFDR, false) >> 24;
		buf++;
		len--;
		ret++;
	}

	return ret;
}

static ssize_t rtl83xx_nor_write(struct spi_nor *nor, loff_t to, size_t len,
				 const u_char *buf)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	uint32_t sfcsr;
	ssize_t ret = 0;
	int sfcsr_addrlen_bits;

	if (rtnor->nor.addr_width == 4) {
		sfcsr_addrlen_bits = 0x03;
	} else {
		sfcsr_addrlen_bits = 0x02;
		to <<= 8;
	}

	sfcsr = spi_prep(rtnor);
	sfcsr &= RTL8XXX_SPI_SFCSR_LEN_MASK;

	/* Send write command */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, nor->program_opcode << 24, false);

	/* Send address */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | (sfcsr_addrlen_bits << 28), true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, to, false);

	/* Write 4 bytes at a time */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN4, true);
	while (len >= 4) {
		rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, *((uint32_t*)buf), true);
		buf += 4;
		len -= 4;
		ret += 4;
	}

	/* Write remainder 1 byte at a time */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	while (len > 0) {
		rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, *buf << 24, true);
		buf++;
		len--;
		ret++;
	}

	return ret;
}

static int rtl83xx_erase(struct spi_nor *nor, loff_t offset)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	uint32_t sfcsr;
	int sfcsr_addrlen_bits;

	if (rtnor->nor.addr_width == 4) {
		sfcsr_addrlen_bits = 0x03;
	} else {
		sfcsr_addrlen_bits = 0x02;
		offset <<= 8;
	}

	sfcsr = spi_prep(rtnor);

	/* Send erase command */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | RTL8XXX_SPI_SFCSR_LEN1, true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, nor->erase_opcode << 24, false);

	/* Send address */
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFCSR, sfcsr | (sfcsr_addrlen_bits << 28), true);
	rtl83xx_write(rtnor, RTL8XXX_SPI_SFDR, offset, false);

	return 0;
}

static const struct spi_nor_controller_ops rtl83xx_controller_ops = {
	.read_reg = rtl83xx_nor_read_reg,
	.write_reg = rtl83xx_nor_write_reg,
	.read = rtl83xx_nor_read,
	.write = rtl83xx_nor_write,
	.erase = rtl83xx_erase,
};

static int rtl83xx_spi_nor_scan(struct spi_nor *nor)
{
	struct rtl83xx_nor *rtnor = nor->priv;
	static const struct spi_nor_hwcaps hwcaps = {
		.mask = SNOR_HWCAPS_READ | SNOR_HWCAPS_READ_FAST | SNOR_HWCAPS_PP
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

static int rtl83xx_nor_init(struct rtl83xx_nor *rtnor,
		     struct device_node *flash_node)
{
	struct spi_nor *nor;
	int ret;

	nor = &rtnor->nor;
	nor->priv = rtnor;
	spi_nor_set_flash_node(nor, flash_node);
	nor->controller_ops = &rtl83xx_controller_ops;
	nor->mtd.name = "rtl83xx-spiflash";

	ret = rtl83xx_spi_nor_scan(nor);
	if (ret)
		return ret;
	pr_info("SPI flash address width is %d bytes\n", nor->addr_width);

	ret = mtd_device_parse_register(&nor->mtd, NULL, NULL, NULL, 0);

	return ret;
}

static int rtl83xx_nor_drv_probe(struct platform_device *pdev)
{
	struct device_node *flash_np;
	struct resource *res;
	struct rtl83xx_nor *rtnor;
	int ret;

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

	rtnor->nor.dev = &pdev->dev;

	/* Only support one attached flash */
	flash_np = of_get_next_available_child(pdev->dev.of_node, NULL);
	if (!flash_np) {
		dev_err(&pdev->dev, "no SPI flash device to configure\n");
		return -ENODEV;
	}

	/* Optional chip select, defaults to 0 */
	ret = of_property_read_u32(flash_np, "reg", &rtnor->cs);
	if (ret)
		rtnor->cs = 0;

	ret = rtl83xx_nor_init(rtnor, flash_np);

	return ret;
}

static int rtl83xx_nor_drv_remove(struct platform_device *pdev)
{
	struct rtl83xx_nor *rtnor;

	rtnor = platform_get_drvdata(pdev);
	mtd_device_unregister(&rtnor->nor.mtd);

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
