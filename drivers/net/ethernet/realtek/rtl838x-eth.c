// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/drivers/net/ethernet/rtl83xx_eth.c
 * Copyright (C) 2020 B. Koblitz
 */

#include <linux/io.h>
#include <linux/etherdevice.h>
#include <linux/phylink.h>

#include <mach-rtl83xx.h>
#include "rtl83xx-eth.h"

DEFINE_MUTEX(smi_lock);

extern struct rtl83xx_soc_info soc_info;
extern struct mutex smi_lock;


inline volatile void __iomem *rtl838x_mac_port_ctrl(int p)
{
	return RTL838X_MAC_PORT_CTRL + (p << 7);
}

static inline volatile void __iomem *rtl838x_mac_force_mode_ctrl(int p)
{
	return RTL838X_MAC_FORCE_MODE_CTRL + (p << 2);
}

inline volatile void __iomem *rtl838x_dma_rx_base(int i)
{
	return RTL838X_DMA_RX_BASE + (i << 2);
}

inline volatile void __iomem *rtl838x_dma_tx_base(int i)
{
	return RTL838X_DMA_TX_BASE + (i << 2);
}

inline volatile void __iomem *rtl838x_dma_if_rx_ring_size(int i)
{
	return RTL838X_DMA_IF_RX_RING_SIZE + ((i >> 3) << 2);
}

inline volatile void __iomem *rtl838x_dma_if_rx_ring_cntr(int i)
{
	return RTL838X_DMA_IF_RX_RING_CNTR + ((i >> 3) << 2);
}

inline volatile void __iomem *rtl838x_dma_if_rx_cur(int i)
{
	return RTL838X_DMA_IF_RX_CUR + (i << 2);
}

inline u32 rtl838x_get_mac_link_sts(int port)
{
	return (sw_r32(RTL838X_MAC_LINK_STS) & (1 << port));
}

inline u32 rtl838x_get_mac_link_dup_sts(int port)
{
	return (sw_r32(RTL838X_MAC_LINK_DUP_STS) & (1 << port));
}

inline u32 rtl838x_get_mac_link_spd_sts(int port)
{
	volatile void __iomem *r = RTL838X_MAC_LINK_SPD_STS + ((port >> 4) << 2);
	u32 speed = sw_r32(r);
	speed >>= (port % 16) << 1;
	return (speed & 0x3);
}

inline u32 rtl838x_get_mac_rx_pause_sts(int port)
{
	return (sw_r32(RTL838X_MAC_RX_PAUSE_STS) & (1 << port));
}

inline u32 rtl838x_get_mac_tx_pause_sts(int port)
{
	return (sw_r32(RTL838X_MAC_TX_PAUSE_STS) & (1 << port));
}

const struct rtl83xx_reg rtl838x_reg = {
	.mac_port_ctrl = rtl838x_mac_port_ctrl,
	.dma_if_intr_sts = RTL838X_DMA_IF_INTR_STS,
	.dma_if_intr_msk = RTL838X_DMA_IF_INTR_MSK,
	.dma_if_ctrl = RTL838X_DMA_IF_CTRL,
	.mac_force_mode_ctrl = rtl838x_mac_force_mode_ctrl,
	.dma_rx_base = rtl838x_dma_rx_base,
	.dma_tx_base = rtl838x_dma_tx_base,
	.dma_if_rx_ring_size = rtl838x_dma_if_rx_ring_size,
	.dma_if_rx_ring_cntr = rtl838x_dma_if_rx_ring_cntr,
	.dma_if_rx_cur = rtl838x_dma_if_rx_cur,
	.rst_glb_ctrl = RTL838X_RST_GLB_CTRL_0,
	.get_mac_link_sts = rtl838x_get_mac_link_sts,
	.get_mac_link_dup_sts = rtl838x_get_mac_link_dup_sts,
	.get_mac_link_spd_sts = rtl838x_get_mac_link_spd_sts,
	.get_mac_rx_pause_sts = rtl838x_get_mac_rx_pause_sts,
	.get_mac_tx_pause_sts = rtl838x_get_mac_tx_pause_sts,
	.mac = RTL838X_MAC,
	.l2_tbl_flush_ctrl = RTL838X_L2_TBL_FLUSH_CTRL,
};

int rtl8380_init_mac(struct rtl83xx_eth_priv *priv)
{
	int i;

	printk("rtl8380_init_mac\n");
	/* fix timer for EEE */
	sw_w32(0x5001411, RTL838X_EEE_TX_TIMER_GIGA_CTRL);
	sw_w32(0x5001417, RTL838X_EEE_TX_TIMER_GELITE_CTRL);

	/* Init VLAN */
	// TODO: what is this
	if (soc_info.id == 0x8382) {
		for (i = 0; i <= 28; i++)
			sw_w32(0, (volatile void *)0xBB00d57c + i * 0x80);
	} else if (soc_info.id == 0x8380) {
		for (i = 8; i <= 28; i++)
			sw_w32(0, (volatile void *)0xBB00d57c + i * 0x80);
	}
	return 0;
}

int rtl838x_smi_wait_op(int timeout)
{
	do {
		timeout--;
		udelay(10);
	} while ((sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_1) & 0x1) && (timeout >= 0));
	if (timeout <= 0) {
		return -1;
	}
	return 0;
}

/* Reads a register in a page from the PHY */
static int rtl838x_read_phy(u32 port, u32 page, u32 reg, u32 *val)
{
	u32 v;
	u32 park_page;
	
	if (port > 31 || page > 4095 || reg > 31)
		return -ENOTSUPP;

	mutex_lock(&smi_lock);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;
		
	sw_w32_mask(0xffff0000, port << 16, RTL838X_SMI_ACCESS_PHY_CTRL_2);
	
	park_page = sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_1) & ((0x1f << 15) | 0x2);
	v = reg << 20 | page << 3;
	sw_w32(v | park_page, RTL838X_SMI_ACCESS_PHY_CTRL_1);
	sw_w32_mask(0, 1, RTL838X_SMI_ACCESS_PHY_CTRL_1);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	*val = sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_2) & 0xffff;
//	printk("PHY-read: port %d reg: %x res: %x\n", port, reg, *val);

	mutex_unlock(&smi_lock);
	return 0;

timeout:
	mutex_unlock(&smi_lock);
	return -ETIMEDOUT;
}

/* Write to a register in a page of the PHY */
static int rtl838x_write_phy(u32 port, u32 page, u32 reg, u32 val)
{
	u32 v;
	u32 park_page;
	val &= 0xffff;

	if (port > 31 || page > 4095 || reg > 31)
		return -ENOTSUPP;

	mutex_lock(&smi_lock);
	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	sw_w32(1 << port, RTL838X_SMI_ACCESS_PHY_CTRL_0);
	mdelay(10);

	sw_w32_mask(0xffff0000, val << 16, RTL838X_SMI_ACCESS_PHY_CTRL_2);

	park_page = sw_r32(RTL838X_SMI_ACCESS_PHY_CTRL_1) & ((0x1f << 15) | 0x2);
	v = reg << 20 | page << 3 | 0x4;
	sw_w32(v | park_page, RTL838X_SMI_ACCESS_PHY_CTRL_1);
	sw_w32_mask(0, 1, RTL838X_SMI_ACCESS_PHY_CTRL_1);

	if(rtl838x_smi_wait_op(10000))
		goto timeout;

	mutex_unlock(&smi_lock);
	return 0;

timeout:
	mutex_unlock(&smi_lock);
	return -ETIMEDOUT;
}

int rtl838x_mdio_read(struct mii_bus *bus, int mii_id, int regnum)
{
	u32 val;
	u32 offset = 0;
	int err;

	if (mii_id >= 24 && mii_id <= 27 && soc_info.id == 0x8380) {
		if (mii_id == 26)
			offset = 0x100;
		val = sw_r32(MAPLE_SDS4_FIB_REG0r + offset + (regnum << 2)) & 0xffff;
//		printk("PHYread from SDS: port %d reg: %x res: %x\n", mii_id, regnum, val);
		return val;
	}
	err = rtl838x_read_phy(mii_id, 0, regnum, &val);
	pr_info("eth: rtl838x_mdio_read: %d, %d: %.8x", mii_id, regnum, val);
	if(err)
		return err;
	return val;
}

int rtl838x_mdio_write(struct mii_bus *bus, int mii_id,
			      int regnum, u16 value)
{
	u32 offset = 0;

	if (mii_id >= 24 && mii_id <= 27 && soc_info.id == 0x8380) {
//		printk("PHYwrite to SDS, port %d\n", mii_id);
		if (mii_id == 26)
			offset = 0x100;
		sw_w32(value, MAPLE_SDS4_FIB_REG0r + offset + (regnum << 2));
		return 0;
	}
	return rtl838x_write_phy(mii_id, 0, regnum, value);
}

int rtl838x_mdio_reset(struct mii_bus *bus)
{
	int i;
	printk("rtl838x_mdio_reset\n");
	/* Disable MAC polling the PHY so that we can start configuration */
	sw_w32(0x00000000, RTL838X_SMI_POLL_CTRL);

	/* Enable PHY control via SoC */
	sw_w32_mask(0, 1 << 15, RTL838X_SMI_GLB_CTRL);

	for (i = 0; i < PHY_MAX_ADDR; i++) {
		// Probably should reset all PHYs here...
	}
	return 0;
}
