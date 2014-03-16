/*
 * arch/arm/mach-meson3/board-m6g24-panel.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <plat/platform.h>
#include <plat/plat_dev.h>
#include <plat/lm.h>
#include <mach/clock.h>
#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/gpio_data.h>
#include <linux/delay.h>
#include <plat/regops.h>
#include <mach/reg_addr.h>

#include <linux/vout/lcdoutc.h>
#include <linux/aml_bl.h>
#include <mach/lcd_aml.h>

#include "board-m6g24.h"

#ifdef CONFIG_AW_AXP
extern int axp_gpio_set_io(int gpio, int io_state);
extern int axp_gpio_get_io(int gpio, int *io_state);
extern int axp_gpio_set_value(int gpio, int value);
extern int axp_gpio_get_value(int gpio, int *value);
#endif

//#define PRINT_DEBUG_INFO
#ifdef PRINT_DEBUG_INFO
#define PRINT_INFO(...)        printk(__VA_ARGS__)
#else
#define PRINT_INFO(...)    
#endif

extern Lcd_Config_t m6g24_lcd_config;

//*****************************************
// Define backlight control method
//*****************************************
#define BL_CTL_GPIO		0
#define BL_CTL_PWM		1
#define BL_CTL			BL_CTL_GPIO

//backlight controlled parameters in driver, define the real backlight level
#if (BL_CTL==BL_CTL_GPIO)
#define	DIM_MAX			0x0
#define	DIM_MIN			0xd	
#elif (BL_CTL==BL_CTL_PWM)
#define	PWM_CNT			60000			//PWM_CNT <= 65535
#define	PWM_PRE_DIV		0				//pwm_freq = 24M / (pre_div + 1) / PWM_CNT
#define PWM_MAX         (PWM_CNT * 8 / 10)		
#define PWM_MIN         (PWM_CNT * 3 / 10)  
#endif

//brightness level in Android UI menu
#define BL_MAX_LEVEL    	255
#define BL_MIN_LEVEL    	10		
#define DEFAULT_BL_LEVEL	128	//please keep this value the same as uboot

static unsigned bl_level = 0;
static Bool_t data_status = ON;
static Bool_t bl_status = ON;
//*****************************************

#include "../include/panel/EJ070NA.h"
//*****************************************
// defined by hardware design
//*****************************************
// redefine LCD_BITS if the panel support bits option
#if (BITS_OPTION == 1)
#undef LCD_BITS
#define LCD_BITS			8
#endif

#define CLK_SS_LEVEL		0	//0~5, 0 for disable spread spectrum

#define TTL_RB_SWAP			0	//0: normal, 1: swap
#define TTL_RGB_BIT_SWAP	0	//0: normal, 1: swap

#define LVDS_PN_SWAP		0	//0:normal, 1:swap
#if (LCD_BITS == 6)
#define LVDS_REPACK			0
#else
#define LVDS_REPACK			1
#endif
//*****************************************

static void lcd_mdelay(int n)
{
	n *= 10;
	while (n--)
		udelay(100);
}

static void ttl_ports_ctrl(Bool_t status)
{
    printk(KERN_INFO "%s: %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if (status) {
        aml_write_reg32(P_PERIPHS_PIN_MUX_1, aml_read_reg32(P_PERIPHS_PIN_MUX_1) | ((1<<14)|(1<<17)|(1<<18)|(1<<19))); //set tcon pinmux
#if (LCD_BITS == 6)		
        aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) | ((1<<0)|(1<<2)|(1<<4)));  //enable RGB 18bit
#else
		aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) | ((3<<0)|(3<<2)|(3<<4)));  //enable RGB 24bit
#endif
    }else {
#if (LCD_BITS == 6)	
        aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) & ~((1<<0)|(1<<2)|(1<<4))); //disable RGB 18bit
		aml_write_reg32(P_PREG_PAD_GPIO1_EN_N, aml_read_reg32(P_PREG_PAD_GPIO1_EN_N) | (0x3f << 2) | (0x3f << 10) | (0x3f << 18));	//RGB set input
#else
		aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) & ~((3<<0)|(3<<2)|(3<<4))); //disable RGB 24bit
		aml_write_reg32(P_PREG_PAD_GPIO1_EN_N, aml_read_reg32(P_PREG_PAD_GPIO1_EN_N) | (0xff << 0) | (0xff << 8) | (0xff << 16));	//RGB set input
#endif		
		aml_write_reg32(P_PERIPHS_PIN_MUX_1, aml_read_reg32(P_PERIPHS_PIN_MUX_1) & ~((1<<14)|(1<<17)|(1<<18)|(1<<19)));  //clear tcon pinmux		 		
		aml_write_reg32(P_PREG_PAD_GPIO2_EN_N, aml_read_reg32(P_PREG_PAD_GPIO2_EN_N) | ((1<<18)|(1<<19)|(1<<20)|(1<<23)));  //GPIOD_2 D_3 D_4 D_7 		
    }
}

static void lvds_ports_ctrl(Bool_t status)
{
	printk(KERN_INFO "%s: %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
	if (status) {
		aml_set_reg32_bits(P_LVDS_GEN_CNTL, 1, 3, 1);	//enable lvds fifo
#if (LCD_BITS == 6)         
		aml_set_reg32_bits(P_LVDS_PHY_CNTL4, 0x27, 0, 7);	//enable LVDS 3 channels
#else		
		aml_set_reg32_bits(P_LVDS_PHY_CNTL4, 0x2f, 0, 7);	//enable LVDS 4 channels
#endif
	}
	else {    	
		aml_set_reg32_bits(P_LVDS_PHY_CNTL3, 0, 0, 1);		
		aml_set_reg32_bits(P_LVDS_PHY_CNTL5, 0, 11, 1);	//shutdown lvds phy
		aml_set_reg32_bits(P_LVDS_PHY_CNTL4, 0, 0, 7);   //disable LVDS 4 channels     
		aml_set_reg32_bits(P_LVDS_GEN_CNTL, 0, 3, 1);	//disable lvds fifo
	}	
}

static void lcd_signals_ports_ctrl(Bool_t status)
{
    switch(LCD_TYPE) {
        case LCD_DIGITAL_TTL:
			ttl_ports_ctrl(status);
            break;
        case LCD_DIGITAL_LVDS:
			lvds_ports_ctrl(status);
			break;
        case LCD_DIGITAL_MINILVDS:
			//minilvds_ports_ctrl(status);
            break;
        default:
            printk("Invalid LCD type.\n");
			break;
    }
}

static void backlight_power_ctrl(Bool_t status)
{ 
	PRINT_INFO("%s(): bl_status=%s, data_status=%s, bl_level=%u\n", __FUNCTION__, (bl_status ? "ON" : "OFF"), (data_status ? "ON" : "OFF"), bl_level);
    if( status == ON ){
		if ((bl_status == ON) || (data_status == OFF) || (bl_level == 0))
			return;
        aml_set_reg32_bits(P_LED_PWM_REG0, 1, 12, 2);
        lcd_mdelay(20); 
		
#if (BL_CTL==BL_CTL_GPIO)
		//BL_EN -> GPIOD_1: 1
		gpio_out(PAD_GPIOD_1, 1);
#elif (BL_CTL==BL_CTL_PWM)
		aml_set_reg32_bits(P_PWM_MISC_REG_CD, PWM_PRE_DIV, 16, 7);
		aml_set_reg32_bits(P_PWM_MISC_REG_CD, 1, 23, 1);
		aml_set_reg32_bits(P_PWM_MISC_REG_CD, 1, 1, 1);  //enable pwm_d		
		aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<3));  //enable pwm pinmux
#endif
    }
    else{
		if (bl_status == OFF)
			return;
#if (BL_CTL==BL_CTL_GPIO)		
		gpio_out(PAD_GPIOD_1, 0);		
#elif (BL_CTL==BL_CTL_PWM)		
		aml_set_reg32_bits(P_PWM_MISC_REG_CD, 0, 1, 1);	//disable pwm_d		
#endif		
    }
	bl_status = status;
	printk(KERN_INFO "%s() Power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
}

#define BL_MID_LEVEL    		128
#define BL_MAPPED_MID_LEVEL		102
static void set_backlight_level(unsigned level)
{
    PRINT_INFO(KERN_INFO "set_backlight_level: %u, last level: %u\n", level, bl_level);
	level = (level > BL_MAX_LEVEL ? BL_MAX_LEVEL : (level < BL_MIN_LEVEL ? 0 : level));	
	bl_level = level;    

	if (level == 0) {
		backlight_power_ctrl(OFF);		
	}
	else {	
		if (level > BL_MID_LEVEL) {
			level = ((level - BL_MID_LEVEL)*(BL_MAX_LEVEL-BL_MAPPED_MID_LEVEL))/(BL_MAX_LEVEL - BL_MID_LEVEL) + BL_MAPPED_MID_LEVEL; 
		} else {
			//level = (level*BL_MAPPED_MID_LEVEL)/BL_MID_LEVEL;
			level = ((level - BL_MIN_LEVEL)*(BL_MAPPED_MID_LEVEL - BL_MIN_LEVEL))/(BL_MID_LEVEL - BL_MIN_LEVEL) + BL_MIN_LEVEL; 
		}
#if (BL_CTL==BL_CTL_GPIO)
		level = DIM_MIN - ((level - BL_MIN_LEVEL) * (DIM_MIN - DIM_MAX)) / (BL_MAX_LEVEL - BL_MIN_LEVEL);	
		aml_set_reg32_bits(P_LED_PWM_REG0, level, 0, 4);	
#elif (BL_CTL==BL_CTL_PWM)
		level = (PWM_MAX - PWM_MIN) * (level - BL_MIN_LEVEL) / (BL_MAX_LEVEL - BL_MIN_LEVEL) + PWM_MIN;	
		aml_write_reg32(P_PWM_PWM_D, (level << 16) | (PWM_CNT - level));  //pwm	duty	
#endif
		if (bl_status == OFF) 
			backlight_power_ctrl(ON);		
	} 
}

static unsigned get_backlight_level(void)
{
    printk(KERN_DEBUG "%s: %d\n", __FUNCTION__, bl_level);
    return bl_level;
}

static void lcd_power_ctrl(Bool_t status)
{
    printk(KERN_INFO "%s() Power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if (status) {
        //GPIOA27 -> LCD_PWR_EN#: 0  lcd 3.3v
		gpio_out(PAD_GPIOA_27, 1);        
        lcd_mdelay(20);
	
        //GPIOC2 -> VCCx3_EN: 0
        //gpio_out(PAD_GPIOC_2, 1);
#ifdef CONFIG_AW_AXP
		axp_gpio_set_io(3,1);     
		axp_gpio_set_value(3, 0); 
#endif			
		lcd_mdelay(20);
		
        lcd_signals_ports_ctrl(ON);
		lcd_mdelay(200);
		data_status = status;
    }
    else {
		data_status = status;
		lcd_mdelay(30);
        lcd_signals_ports_ctrl(OFF);
		lcd_mdelay(20);
		
        //GPIOC2 -> VCCx3_EN: 1        
        //gpio_out(PAD_GPIOC_2, 0);
#ifdef CONFIG_AW_AXP
		axp_gpio_set_io(3,0);
#endif
		lcd_mdelay(20);
		
        //GPIOA27 -> LCD_PWR_EN#: 1  lcd 3.3v
        gpio_out(PAD_GPIOA_27, 0);
		lcd_mdelay(100);        //power down sequence, needed
    }
}

static int lcd_suspend(void *args)
{
    args = args;
    
    PRINT_INFO(KERN_INFO "LCD suspending...\n");
	backlight_power_ctrl(OFF);
	lcd_power_ctrl(OFF);
    return 0;
}

static int lcd_resume(void *args)
{
    args = args;
    PRINT_INFO(KERN_INFO "LCD resuming...\n");
	lcd_power_ctrl(ON);
	backlight_power_ctrl(ON);    
    return 0;
}

static Lvds_Phy_Control_t lcd_lvds_phy_control = 
{
    .lvds_prem_ctl = 0x0,		
    .lvds_swing_ctl = 0x4,	    
    .lvds_vcm_ctl = 0x7,
    .lvds_ref_ctl = 0x15, 
};

static Lvds_Config_t lcd_lvds_config=
{
    .lvds_repack = LVDS_REPACK,
	.pn_swap = LVDS_PN_SWAP,
};

Lcd_Config_t m6g24_lcd_config = {
    .lcd_basic = {
        .h_active = H_ACTIVE,
        .v_active = V_ACTIVE,
        .h_period = H_PERIOD,
        .v_period = V_PERIOD,
    	.screen_ratio_width = ACITVE_AREA_WIDTH,
     	.screen_ratio_height = ACITVE_AREA_HEIGHT,
		.screen_actual_width = ACITVE_AREA_WIDTH,
     	.screen_actual_height = ACITVE_AREA_HEIGHT,
        .lcd_type = LCD_TYPE,
        .lcd_bits = LCD_BITS,
    },

    .lcd_timing = {
        .lcd_clk = LCD_CLK,
        //.pll_ctrl = 0x10232, //0x1023b, //0x10232, //clk=101MHz, frame_rate=59.9Hz
        //.div_ctrl = 0x18803,
        .clk_ctrl = 0x1111 | (CLK_AUTO_GEN << CLK_CTRL_AUTO) | (CLK_SS_LEVEL << CLK_CTRL_SS),
    
		.video_on_pixel = VIDEO_ON_PIXEL,
		.video_on_line = VIDEO_ON_LINE,
		
		.hsync_width = HS_WIDTH,
		.hsync_bp = HS_BACK_PORCH,
		.vsync_width = VS_WIDTH,
		.vsync_bp = VS_BACK_PORCH,
		
        .pol_cntl_addr = (CLK_POL << LCD_CPH1_POL) |(HS_POL << LCD_HS_POL) | (VS_POL << LCD_VS_POL),
		.inv_cnt_addr = (0<<LCD_INV_EN) | (0<<LCD_INV_CNT),
		.tcon_misc_sel_addr = (1<<LCD_STV1_SEL) | (1<<LCD_STV2_SEL),
		.dual_port_cntl_addr = (TTL_RB_SWAP<<LCD_RGB_SWP) | (TTL_RGB_BIT_SWAP<<LCD_BIT_SWP),		
    },

    .lcd_effect = {
        .gamma_cntl_port = (1 << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
        .gamma_vcom_hswitch_addr = 0,
        .rgb_base_addr = 0xf0,
        .rgb_coeff_addr = 0x74a,
    },

	.lvds_mlvds_config = {
        .lvds_config = &lcd_lvds_config,
		.lvds_phy_control = &lcd_lvds_phy_control,
    },
	
    .lcd_power_ctrl = {
        .cur_bl_level = 0,
        .power_ctrl = lcd_power_ctrl,
        .backlight_ctrl = backlight_power_ctrl,
        .get_bl_level = get_backlight_level,
        .set_bl_level = set_backlight_level,
        .lcd_suspend = lcd_suspend,
        .lcd_resume = lcd_resume,
    },
};

static void lcd_setup_gamma_table(Lcd_Config_t *pConf)
{
    int i;
	
	const unsigned short gamma_adjust[256] = {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    };

    for (i=0; i<256; i++) {
        pConf->lcd_effect.GammaTableR[i] = gamma_adjust[i] << 2;
        pConf->lcd_effect.GammaTableG[i] = gamma_adjust[i] << 2;
        pConf->lcd_effect.GammaTableB[i] = gamma_adjust[i] << 2;
    }
}

static void lcd_video_adjust(Lcd_Config_t *pConf)
{
	int i;
	
	const signed short video_adjust[33] = { -999, -937, -875, -812, -750, -687, -625, -562, -500, -437, -375, -312, -250, -187, -125, -62, 0, 62, 125, 187, 250, 312, 375, 437, 500, 562, 625, 687, 750, 812, 875, 937, 1000};
	
	for (i=0; i<33; i++)
	{
		pConf->lcd_effect.brightness[i] = video_adjust[i];
		pConf->lcd_effect.contrast[i]   = video_adjust[i];
		pConf->lcd_effect.saturation[i] = video_adjust[i];
		pConf->lcd_effect.hue[i]        = video_adjust[i];
	}
}

static void lcd_tcon_config(Lcd_Config_t *pConf)
{
	unsigned short hstart, hend, vstart, vend;
	unsigned short de_hstart, de_vstart;
	
	if (LCD_TYPE == LCD_DIGITAL_TTL) {
		if (H_OFFSET_SIGN)
			de_hstart = (VIDEO_ON_PIXEL + TTL_DELAY + H_PERIOD + TTL_H_OFFSET) % H_PERIOD;	
		else
			de_hstart = (VIDEO_ON_PIXEL + TTL_DELAY + H_PERIOD - TTL_H_OFFSET) % H_PERIOD;
		
		if (V_OFFSET_SIGN)
			de_vstart = (VIDEO_ON_LINE + V_PERIOD + TTL_V_OFFSET) % V_PERIOD;	
		else
			de_vstart = (VIDEO_ON_LINE + V_PERIOD - TTL_V_OFFSET) % V_PERIOD;
	}
	else if (LCD_TYPE == LCD_DIGITAL_LVDS) {
		de_hstart = VIDEO_ON_PIXEL + LVDS_DELAY;
		de_vstart = VIDEO_ON_LINE;
	}
	else if (LCD_TYPE == LCD_DIGITAL_MINILVDS) {
		de_hstart = VIDEO_ON_PIXEL + MLVDS_DELAY;
		de_vstart = VIDEO_ON_LINE;
	}
	
	pConf->lcd_timing.de_hstart = de_hstart;
	pConf->lcd_timing.de_vstart = de_vstart;
	
	hstart = (de_hstart + H_PERIOD - HS_BACK_PORCH) % H_PERIOD;
	hend = (de_hstart + H_PERIOD - HS_BACK_PORCH + HS_WIDTH) % H_PERIOD;
	vstart = (de_vstart + V_PERIOD - VS_BACK_PORCH) % V_PERIOD;
	vend = (de_vstart + V_PERIOD - VS_BACK_PORCH + VS_WIDTH - 1) % V_PERIOD;
	if (LCD_TYPE == LCD_DIGITAL_TTL) {
		if (HS_POL) {
			pConf->lcd_timing.sth1_hs_addr = hstart;
			pConf->lcd_timing.sth1_he_addr = hend;
		}
		else {
			pConf->lcd_timing.sth1_he_addr = hstart;
			pConf->lcd_timing.sth1_hs_addr = hend;
		}
		if (VS_POL) {
			pConf->lcd_timing.stv1_vs_addr = vstart;
			pConf->lcd_timing.stv1_ve_addr = vend;
		}
		else {
			pConf->lcd_timing.stv1_ve_addr = vstart;
			pConf->lcd_timing.stv1_vs_addr = vend;
		}
	}
	else if (LCD_TYPE == LCD_DIGITAL_LVDS) {
		if (HS_POL) {
			pConf->lcd_timing.sth1_he_addr = hstart;
			pConf->lcd_timing.sth1_hs_addr = hend;
		}
		else {
			pConf->lcd_timing.sth1_hs_addr = hstart;
			pConf->lcd_timing.sth1_he_addr = hend;
		}
		if (VS_POL) {
			pConf->lcd_timing.stv1_ve_addr = vstart;
			pConf->lcd_timing.stv1_vs_addr = vend;
		}
		else {
			pConf->lcd_timing.stv1_vs_addr = vstart;
			pConf->lcd_timing.stv1_ve_addr = vend;
		}
	}
	else if (LCD_TYPE == LCD_DIGITAL_MINILVDS) {
		//none
	}
	pConf->lcd_timing.sth1_vs_addr = 0;
	pConf->lcd_timing.sth1_ve_addr = V_PERIOD - 1;
	pConf->lcd_timing.stv1_hs_addr = 0;
	pConf->lcd_timing.stv1_he_addr = H_PERIOD - 1;
	
	pConf->lcd_timing.oeh_hs_addr = de_hstart;
	pConf->lcd_timing.oeh_he_addr = de_hstart + H_ACTIVE;
	pConf->lcd_timing.oeh_vs_addr = de_vstart;
	pConf->lcd_timing.oeh_ve_addr = de_vstart + V_ACTIVE - 1;

	PRINT_INFO("sth1_hs_addr=%d, sth1_he_addr=%d, sth1_vs_addr=%d, sth1_ve_addr=%d\n", pConf->lcd_timing.sth1_hs_addr, pConf->lcd_timing.sth1_he_addr, pConf->lcd_timing.sth1_vs_addr, pConf->lcd_timing.sth1_ve_addr);
	PRINT_INFO("stv1_hs_addr=%d, stv1_he_addr=%d, stv1_vs_addr=%d, stv1_ve_addr=%d\n", pConf->lcd_timing.stv1_hs_addr, pConf->lcd_timing.stv1_he_addr, pConf->lcd_timing.stv1_vs_addr, pConf->lcd_timing.stv1_ve_addr);
	PRINT_INFO("oeh_hs_addr=%d, oeh_he_addr=%d, oeh_vs_addr=%d, oeh_ve_addr=%d\n", pConf->lcd_timing.oeh_hs_addr, pConf->lcd_timing.oeh_he_addr, pConf->lcd_timing.oeh_vs_addr, pConf->lcd_timing.oeh_ve_addr);
}

static struct aml_bl_platform_data m6g24_backlight_data =
{
    //.power_on_bl = power_on_backlight,
    //.power_off_bl = power_off_backlight,
    .get_bl_level = get_backlight_level,
    .set_bl_level = set_backlight_level,
    .max_brightness = BL_MAX_LEVEL,
    .dft_brightness = DEFAULT_BL_LEVEL,
};

static struct platform_device m6g24_backlight_device = {
    .name = "aml-bl",
    .id = -1,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &m6g24_backlight_data,
    },
};

static struct aml_lcd_platform __initdata m6g24_lcd_data = {
    .lcd_conf = &m6g24_lcd_config,
};

static struct platform_device __initdata * m6g24_lcd_devices[] = {
    &meson_device_lcd,
//    &meson_device_vout,
    &m6g24_backlight_device,
};


int m6g24_lcd_init(void)
{
    int err;
	printk("\nload panel model: %s\n", PANEL_MODEL);
	lcd_tcon_config(&m6g24_lcd_config);
	lcd_setup_gamma_table(&m6g24_lcd_config);
	lcd_video_adjust(&m6g24_lcd_config);	
    meson_lcd_set_platdata(&m6g24_lcd_data, sizeof(struct aml_lcd_platform));
    err = platform_add_devices(m6g24_lcd_devices, ARRAY_SIZE(m6g24_lcd_devices));
    return err;
}

