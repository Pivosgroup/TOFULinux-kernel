/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Secure Definition
 *
 * Copyright (C) 2012 Amlogic, Inc.
 *
 * Author: Platform-SH@amlogic.com
 *
 */

#ifndef MESON_ARCH_MESON_SECURE_H
#define MESON_ARCH_MESON_SECURE_H

/* MESON Secure Monitor mode APIs */
#define CALL_TRUSTZONE_API                  0x1
#define CALL_TRUSTZONE_MON                  0x4
#define CALL_TRUSTZONE_HAL_API			0x5

#define MESON_MON_L2X0                      0x100
#define MESON_MON_L2X0_CTRL_INDEX           0x101
#define MESON_MON_L2X0_AUXCTRL_INDEX        0x102
#define MESON_MON_L2X0_PREFETCH_INDEX       0x103

#define MESON_MON_CORE                      0x200
#define MESON_MON_CORE_RD_CTRL_INDEX        0x201
#define MESON_MON_CORE_WR_CTRL_INDEX        0x202
#define MESON_MON_CORE_RD_STATUS0_INDEX     0x203
#define MESON_MON_CORE_WR_STATUS0_INDEX     0x204
#define MESON_MON_CORE_RD_STATUS1_INDEX     0x205
#define MESON_MON_CORE_WR_STATUS1_INDEX     0x206
#define MESON_MON_CORE_BOOTADDR_INDEX       0x207

#define MESON_MON_SUSPNED_FIRMWARE      0x300
#define MESON_MON_SAVE_CPU_GIC         0x400

/* secure HAL APIs*/
#define TRUSTZONE_HAL_API_EFUSE					0x100
#define TRUSTZONE_HAL_API_STORAGE				0x200
#define TRUSTZONE_HAL_API_MEMCONFIG		0x300

#ifndef __ASSEMBLER__
extern void meson_smc1(u32 fn, u32 arg);
extern int meson_smc_hal_api(u32 cmdidx, u32 arg);
extern int meson_smc_tz(u32 arg);
extern int meson_smc2(u32 arg);
extern int meson_smc3(u32 arg1, u32 arg2);
extern u32 meson_read_corectrl(void);
extern u32 meson_modify_corectrl(u32 arg);
extern u32 meson_read_corestatus(u32 cpu);
extern u32 meson_modify_corestatus(u32 cpu, u32 arg);
extern void meson_auxcoreboot_addr(u32 arg);
extern void meson_suspend_firmware(void);

// efuse HAL_API arg
struct efuse_hal_api_arg{
	unsigned int cmd;		// R/W
	unsigned int offset;
	unsigned int size;
	unsigned int buffer_phy;
	unsigned int retcnt_phy;	
};
#define EFUSE_HAL_API_READ	0
#define EFUSE_HAL_API_WRITE 1
extern int meson_trustzone_efuse(struct efuse_hal_api_arg* arg);


//memconfig HAL_API arg
struct memconfig{
	unsigned char name[64];
	unsigned int start_phy_addr;
	unsigned int end_phy_addr;
};
struct memconfig_hal_api_arg{
	unsigned int memconfigbuf_phy_addr;
	unsigned int memconfigbuf_count;
};
#define MEMCONFIG_NUM	2
extern int meson_trustzone_memconfig();
extern unsigned int meson_trustzone_getmemsecure_size();
extern int meson_trustzone_getmemconfig(unsigned char* name, unsigned int* startphyaddr, unsigned int* endphyaddr);

#endif


#endif
