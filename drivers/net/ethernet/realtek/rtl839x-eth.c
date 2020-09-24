// SPDX-License-Identifier: GPL-2.0-only
/*
 * linux/drivers/net/ethernet/rtl83xx_eth.c
 * Copyright (C) 2020 B. Koblitz
 */

#include <linux/io.h>
#include <linux/etherdevice.h>
#include <linux/phylink.h>
#include "rtl83xx-eth.h"

extern struct rtl83xx_soc_info soc_info;
extern struct mutex smi_lock;


inline volatile void __iomem *rtl839x_mac_port_ctrl(int p)
{
	return RTL839X_MAC_PORT_CTRL + (p << 7);
}

static inline volatile void __iomem *rtl839x_mac_force_mode_ctrl(int p)
{
	return RTL839X_MAC_FORCE_MODE_CTRL + (p << 2);
}

inline volatile void __iomem *rtl839x_dma_rx_base(int i)
{
	return RTL839X_DMA_RX_BASE + (i << 2);
}

inline volatile void __iomem *rtl839x_dma_tx_base(int i)
{
	return RTL839X_DMA_TX_BASE + (i << 2);
}

inline volatile void __iomem *rtl839x_dma_if_rx_ring_size(int i)
{
	return RTL839X_DMA_IF_RX_RING_SIZE + ((i >> 3) << 2);
}

inline volatile void __iomem *rtl839x_dma_if_rx_ring_cntr(int i)
{
	return RTL839X_DMA_IF_RX_RING_CNTR + ((i >> 3) << 2);
}

inline volatile void __iomem *rtl839x_dma_if_rx_cur(int i)
{
	return RTL839X_DMA_IF_RX_CUR + (i << 2);
}

inline u32 rtl839x_get_mac_link_sts(int p)
{
	return (sw_r32(RTL839X_MAC_LINK_STS + ((p >> 5) << 2)) & (1 << p));
}

inline u32 rtl839x_get_mac_link_dup_sts(int p)
{
	return (sw_r32(RTL839X_MAC_LINK_DUP_STS + ((p >> 5) << 2)) & (1 << p));
}

inline u32 rtl839x_get_mac_link_spd_sts(int port)
{
	volatile void __iomem *r = RTL839X_MAC_LINK_SPD_STS + ((port >> 4) << 2);
	u32 speed = sw_r32(r);
	speed >>= (port % 16) << 1;
	return (speed & 0x3);
}

inline u32 rtl839x_get_mac_rx_pause_sts(int p)
{
	return (sw_r32(RTL839X_MAC_RX_PAUSE_STS + ((p >> 5) << 2)) & (1 << p));
}

inline u32 rtl839x_get_mac_tx_pause_sts(int p)
{
	return (sw_r32(RTL839X_MAC_TX_PAUSE_STS + ((p >> 5) << 2)) & (1 << p));
}

const struct rtl83xx_reg rtl839x_reg = {
	.mac_port_ctrl = rtl839x_mac_port_ctrl,
	.dma_if_intr_sts = RTL839X_DMA_IF_INTR_STS,
	.dma_if_intr_msk = RTL839X_DMA_IF_INTR_MSK,
	.dma_if_ctrl = RTL839X_DMA_IF_CTRL,
	.mac_force_mode_ctrl = rtl839x_mac_force_mode_ctrl,
	.dma_rx_base = rtl839x_dma_rx_base,
	.dma_tx_base = rtl839x_dma_tx_base,
	.dma_if_rx_ring_size = rtl839x_dma_if_rx_ring_size,
	.dma_if_rx_ring_cntr = rtl839x_dma_if_rx_ring_cntr,
	.dma_if_rx_cur = rtl839x_dma_if_rx_cur,
	.rst_glb_ctrl = RTL839X_RST_GLB_CTRL,
	.get_mac_link_sts = rtl839x_get_mac_link_sts,
	.get_mac_link_dup_sts = rtl839x_get_mac_link_dup_sts,
	.get_mac_link_spd_sts = rtl839x_get_mac_link_spd_sts,
	.get_mac_rx_pause_sts = rtl839x_get_mac_rx_pause_sts,
	.get_mac_tx_pause_sts = rtl839x_get_mac_tx_pause_sts,
	.mac = RTL839X_MAC,
	.l2_tbl_flush_ctrl = RTL839X_L2_TBL_FLUSH_CTRL,
};

int rtl8390_init_mac(struct rtl83xx_eth_priv *priv)
{
	printk("Configuring RTL8390 MAC\n");
	// from mac_config_init
	sw_w32(0x80,  RTL839X_MAC_EFUSE_CTRL);
	sw_w32(0x4, RTL839X_RST_GLB_CTRL);
	sw_w32(0x3c324f40, RTL839X_MAC_GLB_CTRL);
	/* Unlimited egress rate */
	sw_w32(0x1297b961, RTL839X_SCHED_LB_TICK_TKN_CTRL);

	/* L2 Table default entry
        MEM32_WRITE(SWCORE_BASE_ADDR| RTL8390_TBL_ACCESS_L2_DATA_ADDR(0), 0x7FFFFFFF);
        MEM32_WRITE(SWCORE_BASE_ADDR| RTL8390_TBL_ACCESS_L2_DATA_ADDR(1), 0xFFFFF800);
        MEM32_WRITE(SWCORE_BASE_ADDR| RTL8390_TBL_ACCESS_L2_CTRL_ADDR, 0x38000);
	*/

	/* 250 MHz Scheduling */
	return 0;
}

static int rtl839x_read_phy(u32 port, u32 page, u32 reg, u32 *val)
{
	u32 v;

	if (port > 63 || page > 4095 || reg > 31)
		return -ENOTSUPP;

	mutex_lock(&smi_lock);

	sw_w32_mask(0xffff0000, port << 16, RTL839X_PHYREG_DATA_CTRL);
	v = reg << 5 | page << 10 | ((page == 0x1fff)? 0x1f: 0) << 23;
	sw_w32(v, RTL839X_PHYREG_ACCESS_CTRL);

	sw_w32(0x1ff, RTL839X_PHYREG_CTRL);

	v |= 1;
	sw_w32(v, RTL839X_PHYREG_ACCESS_CTRL);

	do {
	} while(sw_r32(RTL839X_PHYREG_ACCESS_CTRL) & 0x1);

	*val = sw_r32(RTL839X_PHYREG_DATA_CTRL) & 0xffff;
/*	printk("PHY-read: port %d, page %x, reg %x res: %x\n", port, page, reg, *val)); */

	mutex_unlock(&smi_lock);
	return 0;
}

static int rtl839x_write_phy(u32 port, u32 page, u32 reg, u32 val)
{
	u32 v;
	int err = 0;
	val &= 0xffff;

	if (port > 63 || page > 4095 || reg > 31)
		return -ENOTSUPP;

	mutex_lock(&smi_lock);
	/* Clear both port registers */
	sw_w32(0, RTL839X_PHYREG_PORT_CTRL(0));
	sw_w32(0, RTL839X_PHYREG_PORT_CTRL(0) + 4);
	sw_w32_mask(0, 1 << port, RTL839X_PHYREG_PORT_CTRL(port));

	sw_w32_mask(0xffff0000, val << 16, RTL839X_PHYREG_DATA_CTRL);

	v = reg << 5 | page << 10 | ((page == 0x1fff)? 0x1f: 0) << 23;
	sw_w32(v, RTL839X_PHYREG_ACCESS_CTRL);

	sw_w32(0x1ff, RTL839X_PHYREG_CTRL);

	v |= 1 << 3 | 1; /* Write operation and execute */
	sw_w32(v, RTL839X_PHYREG_ACCESS_CTRL);

	do {
	} while(sw_r32(RTL839X_PHYREG_ACCESS_CTRL) & 0x1);

	if(sw_r32(RTL839X_PHYREG_ACCESS_CTRL) & 0x2)
		err = -EIO;

	mutex_unlock(&smi_lock);
	return err;
}

int rtl839x_mdio_read(struct mii_bus *bus, int mii_id, int regnum)
{
	u32 val;
	int err;
	err = rtl839x_read_phy(mii_id, 0, regnum, &val);
	if (err)
		return err;
	return val;
}

int rtl839x_mdio_write(struct mii_bus *bus, int mii_id,
			      int regnum, u16 value)
{
	return rtl839x_write_phy(mii_id, 0, regnum, value);
}

int rtl839x_mdio_reset(struct mii_bus *bus)
{
	int i;
	printk("rtl839x_mdio_reset\n");
	/* Disable MAC polling the PHY so that we can start configuration */
	sw_w32(0x00000000, RTL839X_SMI_PORT_POLLING_CTRL);
	sw_w32(0x00000000, RTL839X_SMI_PORT_POLLING_CTRL + 4);
	/* Disable PHY polling via SoC */
	sw_w32_mask(1 << 7, 0, RTL839X_SMI_GLB_CTRL);

	for (i = 0; i < PHY_MAX_ADDR; i++) {
		// Probably should reset all PHYs here...
	}
	return 0;
}
