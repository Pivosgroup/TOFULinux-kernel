/*
 * customer/boards/board-m6g24.h
 *
 * Copyright (C) 2011-2012 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef __AML_M6_M712NC_H 
#define __AML_M6_M712NC_H 



#include <asm/page.h>
/***********************************************************************
 * IO Mapping
 **********************************************************************/
#define PHYS_MEM_START      (0x80000000)
#define PHYS_MEM_SIZE       (1024*SZ_1M)
#define PHYS_MEM_END        (PHYS_MEM_START + PHYS_MEM_SIZE -1 )

/******** Reserved memory setting ************************/
#define RESERVED_MEM_START  (0x80000000+64*SZ_1M)   /*start at the second 64M*/

/******** CODEC memory setting ************************/
//  Codec need 16M for 1080p decode
//  4M for sd decode;
#define ALIGN_MSK           ((SZ_1M)-1)
#define U_ALIGN(x)          ((x+ALIGN_MSK)&(~ALIGN_MSK))
#define D_ALIGN(x)          ((x)&(~ALIGN_MSK))

/******** AUDIODSP memory setting ************************/
#define AUDIODSP_ADDR_START U_ALIGN(RESERVED_MEM_START) /*audiodsp memstart*/
#define AUDIODSP_ADDR_END   (AUDIODSP_ADDR_START+SZ_1M-1)   /*audiodsp memend*/

/******** Frame buffer memory configuration ***********/
#define OSD_480_PIX         (640*480)
#define OSD_576_PIX         (768*576)
#define OSD_720_PIX         (1280*720)
#define OSD_1080_PIX        (1920*1080)
#define OSD_PANEL_PIX       (1280*800)
#define B16BpP  (2)
#define B32BpP  (4)
#define DOUBLE_BUFFER   (2)

#define OSD1_MAX_MEM        U_ALIGN(OSD_PANEL_PIX*B32BpP*DOUBLE_BUFFER)
#define OSD2_MAX_MEM        U_ALIGN(32*32*B32BpP)

/******** Reserved memory configuration ***************/
#define OSD1_ADDR_START     U_ALIGN(AUDIODSP_ADDR_END )
#define OSD1_ADDR_END       (OSD1_ADDR_START+OSD1_MAX_MEM - 1)
#define OSD2_ADDR_START     U_ALIGN(OSD1_ADDR_END)
#define OSD2_ADDR_END       (OSD2_ADDR_START +OSD2_MAX_MEM -1)

/******** OSD3 OSD4 begin ***************/
#if defined(CONFIG_AM_FB_EXT)
#define OSD3_MAX_MEM        U_ALIGN(OSD_PANEL_PIX*B32BpP*DOUBLE_BUFFER)
#define OSD4_MAX_MEM        U_ALIGN(32*32*B32BpP)

#define OSD3_ADDR_START     U_ALIGN(OSD2_ADDR_END)
#define OSD3_ADDR_END       (OSD3_ADDR_START+OSD3_MAX_MEM-1)
#define OSD4_ADDR_START     U_ALIGN(OSD3_ADDR_END)
#define OSD4_ADDR_END       (OSD4_ADDR_START+OSD4_MAX_MEM-1)
#endif
/******** OSD3 OSD4 end ***************/

#if defined(CONFIG_AM_VDEC_H264)
#define CODEC_MEM_SIZE      U_ALIGN(64*SZ_1M)
#else
#define CODEC_MEM_SIZE      U_ALIGN(16*SZ_1M)
#endif
#if defined(CONFIG_AM_FB_EXT)
#define CODEC_ADDR_START    U_ALIGN(OSD4_ADDR_END)
#else
#define CODEC_ADDR_START    U_ALIGN(OSD2_ADDR_END)
#endif
#define CODEC_ADDR_END      (CODEC_ADDR_START+CODEC_MEM_SIZE-1)

/********VDIN memory configuration ***************/
#define VDIN_ADDR_START     U_ALIGN(CODEC_ADDR_END)
#define VDIN_ADDR_END       (VDIN_ADDR_START + CODEC_MEM_SIZE - 1)

#if defined(CONFIG_AMLOGIC_VIDEOIN_MANAGER)
#define VM_SIZE             (SZ_1M*16)
#else
#define VM_SIZE             (0)
#endif /* CONFIG_AMLOGIC_VIDEOIN_MANAGER  */

#define VM_ADDR_START       U_ALIGN(VDIN_ADDR_END)
#define VM_ADDR_END         (VM_SIZE + VM_ADDR_START - 1)

#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
#define DI_MEM_SIZE         (SZ_1M*3)
#else
#define DI_MEM_SIZE         (SZ_1M*15)
#endif
#define DI_ADDR_START       U_ALIGN(VM_ADDR_END)
#define DI_ADDR_END         (DI_ADDR_START+DI_MEM_SIZE-1)

#ifdef CONFIG_POST_PROCESS_MANAGER
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
#define PPMGR_MEM_SIZE               1280 * 800 * 21
#else
#define PPMGR_MEM_SIZE               1280 * 800 * 18
#endif
#else
#define PPMGR_MEM_SIZE		0
#endif /* CONFIG_POST_PROCESS_MANAGER */

#define PPMGR_ADDR_START	U_ALIGN(DI_ADDR_END)
#define PPMGR_ADDR_END		(PPMGR_ADDR_START+PPMGR_MEM_SIZE-1)

#define STREAMBUF_MEM_SIZE          (SZ_1M*7)
#define STREAMBUF_ADDR_START        U_ALIGN(PPMGR_ADDR_END)
#define STREAMBUF_ADDR_END      (STREAMBUF_ADDR_START+STREAMBUF_MEM_SIZE-1)

#define RESERVED_MEM_END    (STREAMBUF_ADDR_END)
int  m6g26_m777nd_lcd_init(void);




/**********************************************************************************************
 *
 *										KEYPAD
 *
 *********************************************************************************************/
#define KEY_MAPS  { \
    {KEY_VOLUMEDOWN,    "vol-", CHAN_4, 275, 40}, \
    {KEY_VOLUMEUP,      "vol+", CHAN_4, 150, 40}, \
}

/**********************************************************************************************
 *
 *										CPU
 *
 *********************************************************************************************/
#define MIN_CORE_VOL 965000
#define MAX_CORE_VOL 1361000

#define VCCK_PDATA {\
    .meson_cs_init_data = &vcck_init_data, \
    .voltage_step_table = { \
        1361000, 1335000, 1308000, 1282000, \
        1256000, 1230000, 1203000, 1177000, \
        1150000, 1123000, 1097000, 1071000, \
        1045000, 1018000, 992000, 965000, \
    }, \
    .default_uV = 1177000, \
}

#define VCCK_OPP_TABLE  { \
    { \
        .freq   = 1512000, \
        .min_uV = 1361000, \
        .max_uV = 1361000, \
    }, \
    { \
        .freq   = 1320000, \
        .min_uV = 1256000, \
        .max_uV = 1256000, \
    }, \
    { \
        .freq   = 1200000, \
        .min_uV = 1230000, \
        .max_uV = 1230000, \
    }, \
    { \
        .freq   = 1080000, \
        .min_uV = 1123000, \
        .max_uV = 1123000, \
    }, \
    { \
        .freq   = 840000, \
        .min_uV = 1097000, \
        .max_uV = 1097000, \
    }, \
    { \
        .freq   = 600000, \
        .min_uV = 1071000, \
        .max_uV = 1071000, \
    }, \
    { \
        .freq   = 200000, \
        .min_uV = 1045000, \
        .max_uV = 1045000, \
    } \
}


/**********************************************************************************************
 *
 *										LCD
 *
 *********************************************************************************************/
#define H_ACTIVE		800
#define V_ACTIVE      	1280
#define H_PERIOD		1000
#define V_PERIOD		1320

#define SREEN_RATIO_WIDTH   800
#define SREEN_RATIO_HEIGHT  1280
#define LCD_BITS    6

#define R_COEFF     100
#define G_COEFF     99
#define B_COEFF     97

/* 
  * VIDEO_ON_PIXEL:     80 for LVDS lcd, and 48 for TTL LCD
  * VIDEO_ON_LINE:       32 for lVDS and 22 for TTL
  * Please noted this two values cannot be changed
  */
#define VIDEO_ON_PIXEL  80
#define VIDEO_ON_LINE   32

#define LCD_TYPE    LCD_DIGITAL_LVDS
#define LCD_TYPE_LVDS
/**Note: LCD pix clock = M*24/(N*2[OD]*(div+1)*XD) **/
/* 
  * PLL_CTRL_M:       prll_ctrl[8:0],          750M<(M*24/N)<1500M
  * PLL_CTRL_N:       prll_ctrl[13:9],         value:1~4
  * PLL_CTRL_OD:      prll_ctrl[17:16],        value:0~2 
  */
#define PLL_CTRL_M  39
#define PLL_CTRL_N  1
#define PLL_CTRL_OD 1
/* 
  * DIV_CTRL_DIV:      div_ctrl[7:4],        value:0~5 
  */
#define DIV_CTRL_DIV    0

/* 
  * CLK_CTRL_XD:      clk_ctrl[3:0],        value:1~15
  * For LVDS LCD, DIV_CTRL_DIV must be set to 7
  */
#define CLK_CTRL_XD    7
/* 
  * Please noted PW is included in BP in this definition, following must be comply with:
  *  LCD_H_BP < VIDEO_ON_PIXEL
  *  LCD_V_BP < VIDEO_ON_LINE
  */
#define LCD_H_PW    10
#define LCD_H_FP    60
#define LCD_H_BP    40

#define LCD_V_PW    3
#define LCD_V_FP    97
#define LCD_V_BP    23

#define LCD_PWR_PIN     PAD_GPIOA_27   
#define LCD_SANDBY_PIN  PAD_GPIOD_5
#define LCD_RST_PIN     PAD_GPIOD_6

#define LCD_PWR_EFECT_VALUE     HIGH
#define LCD_SANDBY_EFECT_VALUE  HIGH
#define LCD_RST_EFECT_VALUE	    HIGH
#define LCD_RST_CONTROL

/**********************************************************************************************
 *
 *										BACKLIGHT
 *
 *********************************************************************************************/
#define BL_PWR_PIN      PAD_GPIOD_1
#define BL_PWR_EFECT_VALUE  HIGH

#define PWM_CNT_VALUE	480
#define DIM_CTL_VALUE	0x0
#define PWM_MAX_VALUE	200
#define PWM_MIN_VALUE	30

#define BL_CTL      	BL_CTL_PWM_C
#define BL_CTL_EFFECTIVE	HIGH
/**********************************************************************************************
 *
 *										BATTERY
 *
 *********************************************************************************************/
#define BATTERY_CAPCITY  3000
#define INIT_CHG_CUR		500
#define SUSPEND_CHG_CUR		1200000
#define CHG_CUR				1000000
#define CHG_TARGET_VOL		4200000
#define USB_CHG_VOL_LINIT	1
#define USB_CHG_VOL			4500
#define USB_CHG_CUR_LINIT	1
#define USB_CHG_CUR			500
#define BAT_CAP_ARRAY		{0, 0, 0, 0, 0, 1, 8, 13, 22, 41, 52, 57, 71, 82, 90, 100}
/**********************************************************************************************
 *
 *										CAMERA
 *
 *********************************************************************************************/
#define GC0308_NR   0
#define GC0308_MIRROR 0
#define GC0308_FLIP 0

//#define GT2005_NR   1
//#define GT2005_MIRROR 1
//#define GT2005_FLIP 0

#endif

