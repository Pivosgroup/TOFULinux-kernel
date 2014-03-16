/*
 * File:         byd_bfxxxx_ts.h
 *
 * Created:	     2012-10-07
 * Depend on:    byd_bfxxxx_ts.c
 * Description:  BYD TouchScreen IC driver for Android
 *
 * Copyright (C) 2012 BYD Company Limited 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __BFXXXX_TS_H__
#define __BFXXXX_TS_H__

/*******************************************************************************
* section 1: basic definition
*******************************************************************************/
#define TS_DEBUG_MSG
//#define CONFIG_BF685X
//#define CONFIG_CTS11C
#define CONFIG_CTS12_13 //CTS-12: BF6713A1, BF6932A2, CTS-13: BF6721A1, BF6711A1, BF6712A1
//#define CONFIG_ANDROID_4_ICS
#define EINT_NUM		0 // board specific, initialized zero
#define BUS_NUM		2   // if present, TS device is added by TS driver, not by platform

//#define CONFIG_CTS11C
#define CONFIG_CTS12 //CTS-12: BF6713A1, BF6932A2
//#define CONFIG_CTS13 //CTS-13: BF6721A1, BF6711A1, BF6712A1, BF6931A2
/*******************************************************************************
* section 2: definition for individual chip categories
*******************************************************************************/

#ifdef CONFIG_CTS12_13
#define BYD_TS_SLAVE_ADDRESS	0x2c //0x2c	// 0x34  used in mach-smdkv210.c
#define I2C_CTPM_ADDRESS        0x1b //0x70 //0x3a //0x1a //70 
#define TS_X_MAX		480 // the max value of 1st coordinate of point register
#define TS_Y_MAX		320 //190 // the max value of 2nd coordinate of point register
//#define REPORT_X_REVERSED
//#define REPORT_Y_REVERSED
//#define REPORT_XY_SWAPPED
#define BYD_TS_NAME		"bf6711_ts"
#define BYD_TS_MT_NUMBER_POINTS	10
#define CONFIG_SUPPORT_FIRMWARE_UPG
//#define CONFIG_SUPPORT_PARAMETER_UPG
#define PARA_FILENAME	"mnt/sdcard/byd_cts12_ts.dat"
#endif

#ifdef CONFIG_CTS12
  
	  // I2C address for burning, possibly 0xfd, 0x1b, 0x3a, 0x1a or 0x70  
	  #define I2C_CTPM_ADDRESS	0x7e

/* 4. The name of this TP driver, used as device ID */
	  #define BYD_TS_NAME		"bf6721a_ts" 

/* 5. The max number of points/fingers for muti-points TP */
	  #define BYD_TS_MT_NUMBER_POINTS	5

#endif

#ifdef CONFIG_CTS13
  
	  // I2C address for burning, possibly 0xfd, 0x1b, 0x3a, 0x1a or 0x70  
	  #define I2C_CTPM_ADDRESS	0xff

/* 4. The name of this TP driver, used as device ID */
	  #define BYD_TS_NAME		"bf6721a_ts" 

/* 5. The max number of points/fingers for muti-points TP */
	  #define BYD_TS_MT_NUMBER_POINTS	5

#endif


#ifdef CONFIG_BF685X
#define BYD_TS_SLAVE_ADDRESS	0x2c	// 0x0a 0x14
#define TS_X_MAX		2047
#define TS_Y_MAX		2047
//#define REPORT_X_REVERSED
#define REPORT_Y_REVERSED
#define REPORT_XY_SWAPPED
#define BYD_TS_NAME		"bf685xa_ts"	
#define BYD_TS_MT_NUMBER_POINTS	2	
//#define CONFIG_SUPPORT_PARAMETER_UPG // compiled default parameter, overriding chip's defaults
#define CONFIG_SUPPORT_PARAMETER_FILE  // overriding chip's defaults and compiled defaults
#define CONFIG_CRC_REQUIRED
#define PARA_FILENAME	"system/etc/byd_bf685x_ts.dat"
#endif

#ifdef CONFIG_CTS11C
#define BYD_TS_SLAVE_ADDRESS	0x2c  // 0x2c/0x58	
#define TS_X_MAX		800 		//4096 //1024
#define TS_Y_MAX		480 		//4096 //600
//#define REPORT_X_REVERSED
#define REPORT_Y_REVERSED
//#define REPORT_XY_SWAPPED
#define BYD_TS_NAME		"bf693xa_ts"	
#define BYD_TS_MT_NUMBER_POINTS	10		
#define CONFIG_SUPPORT_PARAMETER_UPG  // parameter UPG or FILE is must for CTS11C
//#define CONFIG_SUPPORT_PARAMETER_FILE // parameter UPG or FILE is must for CTS11C
#define PARA_FILENAME	"system/etc/byd_cts11c_ts.dat"
#endif

/*******************************************************************************
* section 3: definition for registers and some others
*******************************************************************************/
#define BYD_TS_DEVICE		BYD_TS_NAME	
//#define BYD_TS_BUFFER_SIZE	 1 * 4 + 2
#define BYD_TS_REGISTER_BASE	0x5c
#define BYD_TS_SLEEP_REG	0x07
#define BYD_TS_MT_POINT_REG	0x5d
#define BYD_TS_MT_TOUCH_REG	0x5c
#define BYD_TS_MT_TOUCH_MASK	0xF0
#define BYD_TS_MT_TOUCH_FLAG	0x80
#define BYD_TS_MT_KEY_FLAG	0x90
#define BYD_TS_MT_NPOINTS_MASK	0x0f
#define BYD_TS_MT_BUFFER_SIZE	BYD_TS_MT_NUMBER_POINTS * 4 + BYD_TS_MT_POINT_REG - BYD_TS_REGISTER_BASE + 1
#define BYD_TS_PRESS_MAX	255
#define BYD_TS_TOUCHSIZE	200

#endif

