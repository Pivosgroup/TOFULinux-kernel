/*
 * arch/arm/mach-meson6/board-m6g26-panel.c
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

#include "board-m6g26-M777ND.h"

#ifdef CONFIG_AW_AXP
extern int axp_gpio_set_io(int gpio, int io_state);
extern int axp_gpio_get_io(int gpio, int *io_state);
extern int axp_gpio_set_value(int gpio, int value);
extern int axp_gpio_get_value(int gpio, int *value);
#endif

extern Lcd_Config_t m6g26_lcd_config;
#ifndef GAMMA_EN_BITS
#define GAMMA_EN_BITS 0
#endif
#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif
// Define backlight control method
//*****************************************
#define BL_CTL_GPIO 	0
#define BL_CTL_PWM_D  	1
#define BL_CTL_PWM_C  	2

//backlight controlled level in driver, define the real backlight level
#if (BL_CTL==BL_CTL_GPIO)
#define	DIM_MAX			0x3
#define	DIM_MIN			0xf	
#define	PWM_CNT			480		//40KHz //PWM_CNT <= 65535
#define	PWM_PRE_DIV		0			//pwm_freq = 24M / (pre_div + 1) / PWM_CNT
#define PWM_MAX         (PWM_CNT * 200 / 200)	
#define PWM_MIN         (PWM_CNT * 100 / 200)
#elif (BL_CTL==BL_CTL_PWM_D)
#define	DIM_CTL			DIM_CTL_VALUE	//0x3			//80% brightness
#define	PWM_CNT			PWM_CNT_VALUE		//40KHz //PWM_CNT <= 65535
#define	PWM_PRE_DIV		0			//pwm_freq = 24M / (pre_div + 1) / PWM_CNT
#define PWM_MAX         (PWM_CNT * PWM_MAX_VALUE / 200)		
#define PWM_MIN         (PWM_CNT * PWM_MIN_VALUE / 200)	
#elif (BL_CTL==BL_CTL_PWM_C)
#define	PWM_CNT			PWM_CNT_VALUE		//40KHz //PWM_CNT <= 65535
#define	PWM_PRE_DIV		0			//pwm_freq = 24M / (pre_div + 1) / PWM_CNT
#define PWM_MAX         (PWM_CNT * PWM_MAX_VALUE / 200)		
#define PWM_MIN         (PWM_CNT * PWM_MIN_VALUE / 200)	
#endif

//backlight level in UI menu
#define BL_MAX_LEVEL    	255
#define BL_MIN_LEVEL    	20		//Keep this value the same as UI min-limit, must BL_MIN_LEVEL > 0

static unsigned bl_level = 0;
static Bool_t data_status = ON;
static Bool_t bl_status = ON;
#ifdef SPI_INTERFACE_LCD
//**************************************
//lcd spi init
//cs:	  GPIOX_31
//scl:	GPIOX_33
//sda:	GPIOX_34
//**************************************
static void set_lcd_csb(int v)
{
    if(v)
    {
		//gpio_out(PAD_GPIOX_31,1);
		WRITE_MPEG_REG(0x2019, READ_MPEG_REG(0x2019) | (1 << 31));   //GPIOX_31 
		//set_gpio_val(GPIOX_bank_bit0_31(31), GPIOX_bit_bit0_31(31), 1);	   
    }
    else
    {
		//gpio_out(PAD_GPIOX_31,0);
		WRITE_MPEG_REG(0x2019, READ_MPEG_REG(0x2019) & ~(1 << 31));  //GPIOX_31
		//set_gpio_val(GPIOX_bank_bit0_31(31), GPIOX_bit_bit0_31(31), 0);	    
    }
}

static void set_lcd_scl(int v)
{
    if(v)
    {
		//gpio_out(PAD_GPIOX_33,1);
		WRITE_MPEG_REG(0x2016, READ_MPEG_REG(0x2016) | (1 << 21));   //GPIOX_33 
		//set_gpio_val(GPIOX_bank_bit32_35(33), GPIOX_bit_bit32_35(33), 1);	   
    }
    else
    {
		//gpio_out(PAD_GPIOX_33,0);
		WRITE_MPEG_REG(0x2016, READ_MPEG_REG(0x2016) & ~(1 << 21));  //GPIOX_33
		//set_gpio_val(GPIOX_bank_bit32_35(33), GPIOX_bit_bit32_35(33), 0);	   
    }
}
    
static void set_lcd_sda(int v)
{
    if(v)
    {
		//gpio_out(PAD_GPIOX_34,1);
		WRITE_MPEG_REG(0x2016, READ_MPEG_REG(0x2016) | (1 << 22));   //GPIOX_34 
		//set_gpio_val(GPIOX_bank_bit32_35(34), GPIOX_bit_bit32_35(34), 1);	   
    }
    else
    {
		//gpio_out(PAD_GPIOX_34,0);
		WRITE_MPEG_REG(0x2016, READ_MPEG_REG(0x2016) & ~(1 << 22));  //GPIOX_34
		//set_gpio_val(GPIOX_bank_bit32_35(34), GPIOX_bit_bit32_35(34), 0);	   
    }
}

static void SPI_Init(void)
{
    //aml_set_reg32_bits(CBUS_REG_ADDR(0x2019),1,31,1);
    //aml_set_reg32_bits(CBUS_REG_ADDR(0x2015),3,21,2);
    WRITE_MPEG_REG(0x2019, READ_MPEG_REG(0x2019) | (1 << 31));   //3 wire output 1
    WRITE_MPEG_REG(0x2016, READ_MPEG_REG(0x2016) | (3 << 21));
		//set_lcd_csb(1);
		//set_lcd_scl(1);
    //set_lcd_sda(1);
	
		//aml_set_reg32_bits(CBUS_REG_ADDR(0x2018),0,31,1);
    //aml_set_reg32_bits(CBUS_REG_ADDR(0x2015),0,21,2);    
    WRITE_MPEG_REG(0x2018, READ_MPEG_REG(0x2018) & ~(1 << 31));  //3 wire output enable
    WRITE_MPEG_REG(0x2015, READ_MPEG_REG(0x2015) & ~(3 << 21)); 
		//set_gpio_mode(GPIOX_bank_bit0_31(31), GPIOX_bit_bit0_31(31), GPIO_OUTPUT_MODE);
		//set_gpio_mode(GPIOX_bank_bit32_35(33), GPIOX_bit_bit32_35(33), GPIO_OUTPUT_MODE);
		//set_gpio_mode(GPIOX_bank_bit32_35(34), GPIOX_bit_bit32_35(34), GPIO_OUTPUT_MODE);    
}

static void SPI_off(void)
{
		//aml_set_reg32_bits(CBUS_REG_ADDR(0x2019),1,31,1);
    //aml_set_reg32_bits(CBUS_REG_ADDR(0x2015),3,21,2);
    WRITE_MPEG_REG(0x2019, READ_MPEG_REG(0x2019) & ~(0 << 31));   //3 wire output 1
    WRITE_MPEG_REG(0x2016, READ_MPEG_REG(0x2016) & ~(3 << 21));
		//set_lcd_csb(1);
		//set_lcd_scl(1);
	  //set_lcd_sda(1);
	
	//aml_set_reg32_bits(CBUS_REG_ADDR(0x2018),0,31,1);
    //aml_set_reg32_bits(CBUS_REG_ADDR(0x2015),0,21,2);    
    WRITE_MPEG_REG(0x2018, READ_MPEG_REG(0x2018) & ~(1 << 31));  //3 wire output enable
    WRITE_MPEG_REG(0x2015, READ_MPEG_REG(0x2015) & ~(3 << 21)); 
	//set_gpio_mode(GPIOX_bank_bit0_31(31), GPIOX_bit_bit0_31(31), GPIO_OUTPUT_MODE);
	//set_gpio_mode(GPIOX_bank_bit32_35(33), GPIOX_bit_bit32_35(33), GPIO_OUTPUT_MODE);
	//set_gpio_mode(GPIOX_bank_bit32_35(34), GPIOX_bit_bit32_35(34), GPIO_OUTPUT_MODE);    
}

void SPI_WriteShort(unsigned int addr, unsigned int sdata)
{
    int i;
    addr &= 0x3F;
    addr <<= 10;
    sdata &= 0xFF;
    sdata |= addr;
    sdata &= ~(1<<9);  //write flag
    set_lcd_csb(1);
    set_lcd_scl(1);
    set_lcd_sda(1);
    udelay(30);   
    set_lcd_csb(0);
    udelay(30);   
    for (i = 0; i < 16; i++)
    {
        set_lcd_scl(0);
    udelay(30);   
        if (sdata & 0x8000)
            set_lcd_sda(1);
        else
            set_lcd_sda(0);
        sdata <<= 1;
    udelay(30);   
        set_lcd_scl(1);
    udelay(30);   
        
    }
    set_lcd_csb(1);
    set_lcd_scl(1);
    set_lcd_sda(1);
    udelay(50);   
}

static void lcd_spi_init(void)
{
    SPI_Init();
	
	SPI_WriteShort(0x00,0x29);  //reset
	SPI_WriteShort(0x00,0x25);  //standby
	SPI_WriteShort(0x02,0x40);  //enable normally black
	SPI_WriteShort(0x01,0x30);  //enable FRC/Dither
	SPI_WriteShort(0x0e,0x5f);  //enable test mode1
	SPI_WriteShort(0x0f,0xa4);  //enable test mode2
	SPI_WriteShort(0x0d,0x09);  //enable SDRRS, enlarge OE width
	SPI_WriteShort(0x10,0x41);  //adopt 2 line/1 dot
	msleep(50);
	SPI_WriteShort(0x00,0xad);  //display on
}

static void lcd_spi_off(void)
{
  SPI_Init();
	
	SPI_WriteShort(0x00,0xa5);  //standby		
}
#endif
//*****************************************

#if 0
static void m6g26_lvds_ports_ctrl(Bool_t status)
{
    printk(KERN_INFO "%s: %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if (status) {		
        aml_write_reg32(P_VPP_VADJ1_MA_MB, 0x1400000);
		aml_write_reg32(P_VPP_VADJ1_MC_MD, 0x140);
		aml_write_reg32(P_VPP_VADJ2_MA_MB, 0x1400000);
		aml_write_reg32(P_VPP_VADJ2_MC_MD, 0x140);
		
		aml_write_reg32(P_LVDS_PHY_CNTL3, aml_read_reg32(P_LVDS_PHY_CNTL3) | (1<<0));		
        aml_write_reg32(P_LVDS_GEN_CNTL, aml_read_reg32(P_LVDS_GEN_CNTL) | (1 << 3)); // enable fifo
        aml_write_reg32(P_LVDS_PHY_CNTL4, aml_read_reg32(P_LVDS_PHY_CNTL4) | (0x2f<<0));  //enable LVDS phy port
		msleep(200);		
    }else {
        aml_write_reg32(P_LVDS_PHY_CNTL3, aml_read_reg32(P_LVDS_PHY_CNTL3) & ~(1<<0));
        aml_write_reg32(P_LVDS_PHY_CNTL5, aml_read_reg32(P_LVDS_PHY_CNTL5) & ~(1<<11));  //shutdown lvds phy
        aml_write_reg32(P_LVDS_PHY_CNTL4, aml_read_reg32(P_LVDS_PHY_CNTL4) & ~(0x7f<<0));  //disable LVDS phy port
        aml_write_reg32(P_LVDS_GEN_CNTL, aml_read_reg32(P_LVDS_GEN_CNTL) & ~(1 << 3)); // disable fifo		
    }
	data_status = status;
}
#else

static void m6g26_lvds_ports_ctrl(Bool_t status)
{
    printk("%s: %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if (status) {		
        aml_write_reg32(P_VPP_VADJ1_MA_MB, 0x1000000);
		aml_write_reg32(P_VPP_VADJ1_MC_MD, 0x100);
		aml_write_reg32(P_VPP_VADJ2_MA_MB, 0x1000000);
		aml_write_reg32(P_VPP_VADJ2_MC_MD, 0x100);
		
		//aml_write_reg32(P_HHI_VIID_PLL_CNTL, aml_read_reg32(P_HHI_VIID_PLL_CNTL) | (1 << 30)); // power down vid2_pll
		//aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL) & ~(1 << 11)); // lvds gate
		//msleep(50);
		aml_write_reg32(P_LVDS_PHY_CNTL3 , aml_read_reg32(P_LVDS_PHY_CNTL3 ) | (1 << 0)); 
		//aml_write_reg32(P_HHI_VIID_PLL_CNTL, aml_read_reg32(P_HHI_VIID_PLL_CNTL) & ~(1 << 30)); // power on vid2_pll
		//aml_write_reg32(P_HHI_VIID_DIVIDER_CNTL, aml_read_reg32(P_HHI_VIID_DIVIDER_CNTL) | (1 << 11)); // lvds gate
		//msleep(20);
		
		//aml_write_reg32(P_LVDS_PHY_CNTL3, aml_read_reg32(P_LVDS_PHY_CNTL3) | (1<<0));		
        aml_write_reg32(P_LVDS_GEN_CNTL, aml_read_reg32(P_LVDS_GEN_CNTL) | (1 << 3)); // enable fifo
        aml_write_reg32(P_LVDS_PHY_CNTL4, aml_read_reg32(P_LVDS_PHY_CNTL4) | (0x2f<<0));  //enable LVDS phy port			
    }else {        
		aml_write_reg32(P_LVDS_PHY_CNTL3, aml_read_reg32(P_LVDS_PHY_CNTL3) & ~(1<<0));
        aml_write_reg32(P_LVDS_PHY_CNTL5, aml_read_reg32(P_LVDS_PHY_CNTL5) & ~(1<<11));  //shutdown lvds phy
        aml_write_reg32(P_LVDS_PHY_CNTL4, aml_read_reg32(P_LVDS_PHY_CNTL4) & ~(0x7f<<0));  //disable LVDS phy port
        aml_write_reg32(P_LVDS_GEN_CNTL, aml_read_reg32(P_LVDS_GEN_CNTL) & ~(1 << 3)); // disable fifo		
    }
}
#endif

static void m6g26_ttl_ports_ctrl(Bool_t status)
{
    printk(KERN_INFO "%s: %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if (status) {		
		/* saturation adjust */
		aml_write_reg32(P_VPP_VADJ1_MA_MB, 0x1000000);
		aml_write_reg32(P_VPP_VADJ1_MC_MD, 0x100);
		aml_write_reg32(P_VPP_VADJ2_MA_MB, 0x1000000);
		aml_write_reg32(P_VPP_VADJ2_MC_MD, 0x100);
        aml_write_reg32(P_PERIPHS_PIN_MUX_1, aml_read_reg32(P_PERIPHS_PIN_MUX_1) | ((1<<14)|(1<<17)|(1<<18)|(1<<19))); //set tcon pinmux
        aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) | ((3<<0)|(3<<2)|(3<<4)));  //enable RGB 24bit
        msleep(40);
    }else {
        aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) & ~((3<<0)|(3<<2)|(3<<4))); //disable RGB 24bit
		aml_write_reg32(P_PERIPHS_PIN_MUX_1, aml_read_reg32(P_PERIPHS_PIN_MUX_1) & ~((1<<14)|(1<<17)|(1<<18)|(1<<19)));  //clear tcon pinmux        		
		aml_write_reg32(P_PREG_PAD_GPIO1_EN_N, aml_read_reg32(P_PREG_PAD_GPIO1_EN_N) | (0xffffff << 0));       //GPIOB_0--GPIOB_23  set input 		
		aml_write_reg32(P_PREG_PAD_GPIO2_EN_N, aml_read_reg32(P_PREG_PAD_GPIO2_EN_N) | ((1<<18)|(1<<19)|(1<<20)|(1<<22) |(1<<23)));  //GPIOD_2 D_3 D_4 D_6 D_7 		
    }
}

// Refer to H/W schematics
static void m6g26_backlight_power_ctrl(Bool_t status)
{
	if( status == ON ){
		if ((bl_status == ON) || (data_status == OFF) || (bl_level == 0))
			return;
        aml_set_reg32_bits(P_LED_PWM_REG0, 1, 12, 2);
        msleep(30); // wait for PWM charge        
		
#if (BL_CTL==BL_CTL_GPIO)	
		//BL_EN -> GPIOD_1: 1
		//gpio_out(PAD_GPIOD_1, 0); 
		aml_write_reg32(P_PWM_MISC_REG_CD, ((aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<16)) | ((1 << 23) | (PWM_PRE_DIV<<16) | (1<<1))));  //enable pwm clk & pwm output
		aml_write_reg32(P_PERIPHS_PIN_MUX_2, (aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<3)));  //enable pwm pinmux
#elif (BL_CTL==BL_CTL_PWM_D)		
		aml_set_reg32_bits(P_LED_PWM_REG0, DIM_CTL, 0, 4);
		aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<16)) | ((1 << 23) | (PWM_PRE_DIV<<16) | (1<<1)));  //enable pwm clk & pwm output
		aml_write_reg32(P_PERIPHS_PIN_MUX_2, (aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<3)));  //enable pwm pinmux
#elif (BL_CTL==BL_CTL_PWM_C)
        gpio_out(PAD_GPIOD_1, 1); 
        /*	
        //set PWM_D to high	
		aml_write_reg32(P_PWM_MISC_REG_CD, ((aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<16)) | ((1 << 23) | (PWM_PRE_DIV<<16) | (1<<1))));  //enable pwm clk & pwm output
		aml_write_reg32(P_PERIPHS_PIN_MUX_2, (aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<3)));  //enable pwm pinmux
		aml_write_reg32(P_PWM_PWM_D, (PWM_CNT << 16) | 0);  //pwm	duty
        */
        //for pwm C
	    aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<8)) | ((1 << 15) | (PWM_PRE_DIV<<8) | (1<<0)));  //enable pwm clk & pwm output
		aml_write_reg32(P_PERIPHS_PIN_MUX_2, (aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<2)));  //enable pwm pinmux
#endif
    }
    else{
		if (bl_status == OFF)
			return;
	#if (BL_CTL==BL_CTL_PWM_C)	
		gpio_out(PAD_GPIOD_1, 0); 
		//set PWM_C to full duty
        aml_write_reg32(P_PWM_PWM_C, (PWM_CNT << 16) | 0);  //pwm	duty
    #else
        //BL_EN -> GPIOD_1: 0		
		aml_write_reg32(P_PREG_PAD_GPIO1_O, (aml_read_reg32(P_PREG_PAD_GPIO1_O) & ~(1<<17)));
		aml_write_reg32(P_PREG_PAD_GPIO1_EN_N, (aml_read_reg32(P_PREG_PAD_GPIO1_EN_N) & ~(1<<17)));
		aml_write_reg32(P_PWM_MISC_REG_CD, aml_read_reg32(P_PWM_MISC_REG_CD) & ~((1 << 23) | (1<<1)));  //disable pwm clk & pwm output
	#endif
		
    }
	bl_status = status;
	printk("%s() Power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
}

//extern int is_bl_off;
#define BL_MID_LEVEL    128
#define BL_MAPPED_MID_LEVEL    102
#define SWITCH_POINT	50		//define dim_ctrl_current/PWM switch level
static void m6g26_set_backlight_level(unsigned level)
{
	//printk("set_backlight_level: %u, last level: %u\n", level, bl_level);
	level = (level > BL_MAX_LEVEL ? BL_MAX_LEVEL : (level < BL_MIN_LEVEL ? 0 : level));	
    bl_level = level;
	
	if (level == 0) {
		m6g26_backlight_power_ctrl(OFF);		
	}
	else {	
		if (level > BL_MID_LEVEL) {
			level = ((level - BL_MID_LEVEL)*(BL_MAX_LEVEL - BL_MAPPED_MID_LEVEL))/(BL_MAX_LEVEL - BL_MID_LEVEL) + BL_MAPPED_MID_LEVEL; 
		} else {
			//level = (level*BL_MAPPED_MID_LEVEL)/BL_MID_LEVEL;
			level = ((level - BL_MIN_LEVEL)*(BL_MAPPED_MID_LEVEL - BL_MIN_LEVEL))/(BL_MID_LEVEL - BL_MIN_LEVEL) + BL_MIN_LEVEL; 
		}		
#if (BL_CTL==BL_CTL_GPIO)
		if (level >= SWITCH_POINT)
		{			
			aml_write_reg32(P_PWM_PWM_D, (PWM_MAX << 16) | (PWM_CNT - PWM_MAX));  //pwm duty	
			level = DIM_MIN - ((level - SWITCH_POINT) * (DIM_MIN - DIM_MAX)) / (BL_MAX_LEVEL - SWITCH_POINT);	       
			aml_set_reg32_bits(P_LED_PWM_REG0, level, 0, 4);
			//printk("pwm_level: %u, dim_level: %u\n", PWM_MAX, level);
		}
		else
		{
			aml_set_reg32_bits(P_LED_PWM_REG0, DIM_MIN, 0, 4);
			level = (PWM_MAX - PWM_MIN) * (level - BL_MIN_LEVEL) / (SWITCH_POINT - BL_MIN_LEVEL) + PWM_MIN;			
			aml_write_reg32(P_PWM_PWM_D, (level << 16) | (PWM_CNT - level));  //pwm	duty
			//printk("pwm_level: %u, dim_level: %u\n", level, DIM_MIN);
		}		
#elif (BL_CTL==BL_CTL_PWM_D)
		level = (PWM_MAX - PWM_MIN) * (level - BL_MIN_LEVEL) / (BL_MAX_LEVEL - BL_MIN_LEVEL) + PWM_MIN;	
        //printk("----PWM level = %d\n", level);
		aml_write_reg32(P_PWM_PWM_D, (level << 16) | (PWM_CNT - level));  //pwm	duty	
#elif (BL_CTL==BL_CTL_PWM_C)
		level = (PWM_MAX - PWM_MIN) * (level - BL_MIN_LEVEL) / (BL_MAX_LEVEL - BL_MIN_LEVEL) + PWM_MIN;	
        //printk("----PWM level = %d\n", level);
	#if (BL_CTL_EFFECTIVE == HIGH)
		aml_write_reg32(P_PWM_PWM_C, (level<<16 ) | (PWM_CNT - level));  //pwm	duty	
	#else
		aml_write_reg32(P_PWM_PWM_C, (level ) | ((PWM_CNT - level)<< 16));  //pwm	duty	
	#endif
#endif
		if ((bl_status == OFF))
			m6g26_backlight_power_ctrl(ON);			
	} 
}

static unsigned m6g26_get_backlight_level(void)
{
    printk("%s: %d\n", __FUNCTION__, bl_level);
    return bl_level;
}

static void m6g26_lcd_power_ctrl(Bool_t status)
{
	printk(KERN_INFO "%s() Power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
	if (status) {
		//for LCD RESET, all LCD need to be set high
		gpio_out(LCD_RST_PIN, !LCD_RST_EFECT_VALUE);  
		//for LCD VDD
#ifdef LCD_PWR_PIN
	if(!LCD_PWR_EFECT_VALUE)	
	{
		gpio_set_status(LCD_PWR_PIN, gpio_status_out);
		gpio_out(LCD_PWR_PIN, LCD_PWR_EFECT_VALUE);  
	}
	else
	{
		gpio_set_status(LCD_PWR_PIN, gpio_status_in);
	}
#endif

		//for LCD STBYB
#ifdef LCD_SANDBY_PIN
		gpio_out(LCD_SANDBY_PIN, LCD_SANDBY_EFECT_VALUE);
#endif
        //use usleep_range to sleep the exact time
		usleep_range(1000,1000);
#ifdef LCD_RST_PIN
		gpio_out(LCD_RST_PIN, LCD_RST_EFECT_VALUE);  
#endif
#ifndef LCD_RST_CONTROL 
		msleep(20);
#endif
		//msleep(5);
		//for LCD LVDS
#ifdef SPI_INTERFACE_LCD
		lcd_spi_init();
#endif
#ifdef LCD_TYPE_LVDS
		m6g26_lvds_ports_ctrl(ON);
#endif
		//for LCD AVDD
#ifdef CONFIG_AW_AXP
		axp_gpio_set_io(3,1);     
		axp_gpio_set_value(3, 0); 
#endif	
		msleep(20);	
#ifdef LCD_TYPE_TTL
		m6g26_ttl_ports_ctrl(ON);
#endif  
#ifdef LCD_RST_CONTROL 
		msleep(20);
		//for LCD RESET
		gpio_out(LCD_RST_PIN, !LCD_RST_EFECT_VALUE);  
		msleep(5);
		gpio_out(LCD_RST_PIN, LCD_RST_EFECT_VALUE);  
#endif
		msleep(200);		
		data_status = status;
	} else {
		data_status = status;
		msleep(30); 
#ifdef LCD_SANDBY_PIN
		gpio_out(LCD_SANDBY_PIN, !LCD_SANDBY_EFECT_VALUE);
#endif	
#ifndef LCD_RST_CONTROL 
    #ifdef LCD_RST_PIN
		//GPIOA27 -> LCD_PWR_EN#: 1  lcd 3.3v
		gpio_out(LCD_RST_PIN, !LCD_RST_EFECT_VALUE);
    #endif
#else 
		msleep(105);
#endif
#ifdef LCD_TYPE_TTL
		m6g26_ttl_ports_ctrl(OFF); 
#endif
		//GPIOC2 -> VCCx3_EN: 1        
		//gpio_out(PAD_GPIOC_2, 0);
#ifdef CONFIG_AW_AXP
		axp_gpio_set_io(3,0);		
#endif		
		msleep(100);
#ifdef LCD_TYPE_LVDS
		m6g26_lvds_ports_ctrl(OFF); 
#endif
        //for 9.7 inch lcd
        msleep(30);
#ifdef SPI_INTERFACE_LCD
		SPI_off();
#endif
#ifdef LCD_RST_CONTROL 
		//GPIOA27 -> LCD_PWR_EN#: 1  lcd 3.3v
		gpio_out(LCD_RST_PIN, !LCD_RST_EFECT_VALUE);
#endif  
#ifdef LCD_PWR_PIN
	if(!LCD_PWR_EFECT_VALUE)
	{
		//gpio_out(LCD_PWR_PIN, !LCD_PWR_EFECT_VALUE);
		gpio_set_status(LCD_PWR_PIN, gpio_status_in);
	}
	else
	{
		gpio_set_status(LCD_PWR_PIN, gpio_status_out);
		gpio_out(LCD_PWR_PIN, LCD_PWR_EFECT_VALUE);  
	}	
#endif
		msleep(250);

	}
}

static int m6g26_lcd_suspend(void *args)
{
    args = args;

    printk("LCD suspending...\n");
    m6g26_backlight_power_ctrl(OFF);
    m6g26_lcd_power_ctrl(OFF);
    return 0;
}

static int m6g26_lcd_resume(void *args)
{
    args = args;

    printk("LCD resuming...\n");
     m6g26_lcd_power_ctrl(ON);
     m6g26_backlight_power_ctrl(ON);
    return 0;
}


// Define LVDS physical PREM SWING VCM REF
static Lvds_Phy_Control_t lcd_lvds_phy_control = 
{
    .lvds_prem_ctl = 0x0,		
    .lvds_swing_ctl = 0x4,	    
    .lvds_vcm_ctl = 0x0, 
    .lvds_ref_ctl = 0x15, 
};

//Define LVDS data mapping, pn swap.
static Lvds_Config_t lcd_lvds_config=
{
    .lvds_repack=1,   //data mapping  //0:JEDIA mode, 1:VESA mode
    .pn_swap=0,       //0:normal, 1:swap    
};

Lcd_Config_t m6g26_lcd_config = {
    // Refer to LCD Spec
    .lcd_basic = {
        .h_active = H_ACTIVE,
        .v_active = V_ACTIVE,
        .h_period = H_PERIOD,
        .v_period = V_PERIOD,
        .screen_ratio_width = SREEN_RATIO_WIDTH,
        .screen_ratio_height = SREEN_RATIO_HEIGHT,
        .lcd_type = LCD_TYPE,    //LCD_DIGITAL_TTL  //LCD_DIGITAL_LVDS  //LCD_DIGITAL_MINILVDS
        .lcd_bits = LCD_BITS,  //8  //6
    },

    .lcd_timing = {      
    .pll_ctrl =   PLL_CTRL_M | PLL_CTRL_N<<9 | PLL_CTRL_OD<<16,  //0x10220,  //54.2M
    .div_ctrl = 0x18803 | DIV_CTRL_DIV<<4,  //0x18803
#ifdef LCD_TYPE_LVDS
	.clk_ctrl = 0x1111,	//pll_sel,div_sel,vclk_sel,xd
#endif
#ifdef LCD_TYPE_TTL
    .clk_ctrl = 0x1110 | CLK_CTRL_XD,	//pll_sel,div_sel,vclk_sel,xd
#endif
        //.sync_duration_num = 501,
        //.sync_duration_den = 10,
   
		.video_on_pixel = VIDEO_ON_PIXEL,
        .video_on_line = VIDEO_ON_LINE,
    #ifdef LCD_TYPE_LVDS		 
        .sth1_hs_addr = VIDEO_ON_PIXEL - LCD_H_BP,
        .sth1_he_addr = VIDEO_ON_PIXEL - LCD_H_BP + LCD_H_PW,
    #endif
    #ifdef LCD_TYPE_TTL
        .sth1_hs_addr = VIDEO_ON_PIXEL - LCD_H_BP + LCD_H_PW,
        .sth1_he_addr = VIDEO_ON_PIXEL - LCD_H_BP,
    #endif
        .sth1_vs_addr = 0,
        .sth1_ve_addr = V_PERIOD - 1,
        .oeh_hs_addr = 67,
        .oeh_he_addr = 67+H_ACTIVE,
        .oeh_vs_addr = VIDEO_ON_LINE,
        .oeh_ve_addr = VIDEO_ON_LINE+V_ACTIVE-1,
        .vcom_hswitch_addr = 0,
        .vcom_vs_addr = 0,
        .vcom_ve_addr = 0,
        .cpv1_hs_addr = 0,
        .cpv1_he_addr = 0,
        .cpv1_vs_addr = 0,
        .cpv1_ve_addr = 0,
    #ifdef LCD_TYPE_LVDS
    	.stv1_hs_addr = 10,
		.stv1_he_addr = 20,
	#endif
	#ifdef LCD_TYPE_TTL	
        .stv1_hs_addr = 0,
        .stv1_he_addr = H_PERIOD-1,
    #endif
    #ifdef LCD_TYPE_LVDS	
        .stv1_vs_addr = VIDEO_ON_LINE - LCD_V_BP,
        .stv1_ve_addr = VIDEO_ON_LINE - LCD_V_BP + LCD_V_PW - 1,
    #endif
    #ifdef LCD_TYPE_TTL
        .stv1_vs_addr = VIDEO_ON_LINE - LCD_V_BP + LCD_V_PW - 1,
        .stv1_ve_addr = VIDEO_ON_LINE - LCD_V_BP,
    #endif
        .oev1_hs_addr = 0,
        .oev1_he_addr = 0,
        .oev1_vs_addr = 0,
        .oev1_ve_addr = 0,

        .pol_cntl_addr = (0x0 << LCD_CPH1_POL) |(0x1 << LCD_HS_POL) | (0x1 << LCD_VS_POL),
        .inv_cnt_addr = (0<<LCD_INV_EN) | (0<<LCD_INV_CNT),
        .tcon_misc_sel_addr = (1<<LCD_STV1_SEL) | (1<<LCD_STV2_SEL),
        .dual_port_cntl_addr = (1<<LCD_TTL_SEL) | (1<<LCD_ANALOG_SEL_CPH3) | (1<<LCD_ANALOG_3PHI_CLK_SEL) | (0<<LCD_RGB_SWP) | (0<<LCD_BIT_SWP),
    },

    .lcd_effect = {
        .gamma_cntl_port = (GAMMA_EN_BITS << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
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
        .power_ctrl = m6g26_lcd_power_ctrl,
        .backlight_ctrl = m6g26_backlight_power_ctrl,
        .get_bl_level = m6g26_get_backlight_level,
        .set_bl_level = m6g26_set_backlight_level,
        .lcd_suspend = m6g26_lcd_suspend,
        .lcd_resume = m6g26_lcd_resume,
    },    
};

static unsigned short r_coeff=R_COEFF;
static unsigned short g_coeff=G_COEFF;
static unsigned short b_coeff=B_COEFF;
static void lcd_setup_gamma_table(Lcd_Config_t *pConf)
{
    int i;
	
	const unsigned short gamma_adjust[256] = {
        // 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        // 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        // 64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        // 96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        // 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        // 160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        // 192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        // 224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
		
		0,2,3,5,7,9,10,12,14,16,17,19,21,23,24,26,27,29,30,32,33,35,36,38,39,41,42,44,45,47,48,49,
		50,52,53,54,55,56,58,59,60,61,62,64,65,66,67,68,69,70,71,72,73,75,76,77,78,79,80,81,82,83,84,85,
		86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,104,105,106,107,108,109,110,111,112,113,114,115,116,
		117,118,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,
		146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,174,175,
		176,177,178,179,180,181,182,183,184,185,186,187,188,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,202,203,204,205,
		206,207,208,209,210,211,212,212,213,214,215,216,217,218,219,219,220,221,222,223,224,224,225,226,227,228,228,229,230,231,232,232,
		233,234,235,235,236,237,238,238,239,240,241,241,242,243,244,244,245,246,246,247,248,248,249,250,250,251,252,252,253,254,254,255,
    };

    for (i=0; i<256; i++) {
        pConf->lcd_effect.GammaTableR[i] = (gamma_adjust[i]*r_coeff/100) << 2;
        pConf->lcd_effect.GammaTableG[i] = (gamma_adjust[i]*g_coeff/100) << 2;
        pConf->lcd_effect.GammaTableB[i] = (gamma_adjust[i]*b_coeff/100) << 2;
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

static void lcd_sync_duration(Lcd_Config_t *pConf)
{
	unsigned m, n, od, div, xd, pre_div;
	unsigned sync_duration;
	
	m = ((pConf->lcd_timing.pll_ctrl) >> 0) & 0x1ff;
	n = ((pConf->lcd_timing.pll_ctrl) >> 9) & 0x1f;
	od = ((pConf->lcd_timing.pll_ctrl) >> 16) & 0x3;
	div = ((pConf->lcd_timing.div_ctrl) >> 4) & 0x7;
	
	od = (od == 0) ? 1:((od == 1) ? 2:4);
	switch(pConf->lcd_basic.lcd_type)
	{
		case LCD_DIGITAL_TTL:
			xd = ((pConf->lcd_timing.clk_ctrl) >> 0) & 0xf;
			pre_div = 1;
			break;
		case LCD_DIGITAL_LVDS:
			xd = 1;
			pre_div = 7;
			break;
		case LCD_DIGITAL_MINILVDS:
			xd = 1;
			pre_div = 6;
			break;	
		default:
			pre_div = 1;
			break;
	}
	
	sync_duration = m*24*1000/(n*od*(div+1)*xd*pre_div);		
	sync_duration = ((sync_duration * 10000 / pConf->lcd_basic.h_period) * 10) / pConf->lcd_basic.v_period;
	sync_duration = (sync_duration + 5) / 10;	
	
	pConf->lcd_timing.sync_duration_num = sync_duration;
	pConf->lcd_timing.sync_duration_den = 10;
}

static struct aml_bl_platform_data m6g26_backlight_data =
{
    //.power_on_bl = power_on_backlight,
    //.power_off_bl = power_off_backlight,
    .get_bl_level = m6g26_get_backlight_level,
    .set_bl_level = m6g26_set_backlight_level,
    .max_brightness = 255,
    .dft_brightness = 200,
};

static struct platform_device m6g26_backlight_device = {
    .name = "aml-bl",
    .id = -1,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &m6g26_backlight_data,
    },
};

static struct aml_lcd_platform __initdata m6g26_lcd_data = {
    .lcd_conf = &m6g26_lcd_config,
};

static struct platform_device __initdata * m6g26_lcd_devices[] = {
    &meson_device_lcd,
//    &meson_device_vout,
    &m6g26_backlight_device,
};

int  m6g26_m777nd_lcd_init(void)
{
    int err;
    int i, ret;
#ifdef LCD_97_INCH
    gpio_set_status(PAD_GPIOC_6, gpio_status_in);
    if(gpio_in_get(PAD_GPIOC_6))
    {
        printk("---TIANMA 9.7 inch LCD is used---- \n");
        m6g26_lcd_config.lcd_basic.h_period = H_PERIOD_TM;
        m6g26_lcd_config.lcd_basic.v_period = V_PERIOD_TM;

        m6g26_lcd_config.lcd_timing.pll_ctrl = PLL_CTRL_M_TM | PLL_CTRL_N_TM<<9 | PLL_CTRL_OD_TM<<16;
        m6g26_lcd_config.lcd_timing.div_ctrl = 0x18803 | DIV_CTRL_DIV_TM<<4; 

        m6g26_lcd_config.lcd_timing.sth1_hs_addr = VIDEO_ON_PIXEL - LCD_H_BP_TM;
        m6g26_lcd_config.lcd_timing.sth1_he_addr = VIDEO_ON_PIXEL - LCD_H_BP_TM + LCD_H_PW_TM;

        m6g26_lcd_config.lcd_timing.sth1_vs_addr = 0;
        m6g26_lcd_config.lcd_timing.sth1_ve_addr = V_PERIOD_TM - 1;
        m6g26_lcd_config.lcd_timing.oeh_hs_addr = 67;
        m6g26_lcd_config.lcd_timing.oeh_he_addr = 67+H_ACTIVE;
        m6g26_lcd_config.lcd_timing.oeh_vs_addr = VIDEO_ON_LINE;
        m6g26_lcd_config.lcd_timing.oeh_ve_addr = VIDEO_ON_LINE+V_ACTIVE-1;
        m6g26_lcd_config.lcd_timing.vcom_hswitch_addr = 0;
        m6g26_lcd_config.lcd_timing.vcom_vs_addr = 0;
        m6g26_lcd_config.lcd_timing.vcom_ve_addr = 0;
        m6g26_lcd_config.lcd_timing.cpv1_hs_addr = 0;
        m6g26_lcd_config.lcd_timing.cpv1_he_addr = 0;
        m6g26_lcd_config.lcd_timing.cpv1_vs_addr = 0;
        m6g26_lcd_config.lcd_timing.cpv1_ve_addr = 0;

        m6g26_lcd_config.lcd_timing.stv1_vs_addr = VIDEO_ON_LINE - LCD_V_BP_TM;
        m6g26_lcd_config.lcd_timing.stv1_ve_addr = VIDEO_ON_LINE - LCD_V_BP + LCD_V_PW_TM - 1;
    }
    else
    {
        printk("---OTHER(CMI or LG) 9.7 inch LCD is used, use the default parameter---- \n");
    }  
#endif
    if((m6g26_lcd_config.lcd_basic.lcd_type == LCD_DIGITAL_LVDS) && (m6g26_lcd_config.lcd_basic.lcd_bits == 6))
    {
        m6g26_lcd_config.lvds_mlvds_config.lvds_config->lvds_repack = 0;
    }
    /*
    if(m6g26_lcd_config.lcd_basic.lcd_type == LCD_DIGITAL_LVDS) 
    {
        if(VIDEO_ON_PIXEL - LCD_H_BP < 0)
            m6g26_lcd_config.lcd_timing.sth1_hs_addr = VIDEO_ON_PIXEL - LCD_H_BP + H_PERIOD - 1;
        if(VIDEO_ON_PIXEL - LCD_H_BP + LCD_H_PW < 0)
            m6g26_lcd_config.lcd_timing.sth1_he_addr = VIDEO_ON_PIXEL - LCD_H_BP + LCD_H_PW + H_PERIOD - 1;

        if(VIDEO_ON_LINE - LCD_V_BP < 0)
            m6g26_lcd_config.lcd_timing.stv1_vs_addr = VIDEO_ON_LINE - LCD_H_BP + V_PERIOD - 1;
        if(VIDEO_ON_LINE - LCD_V_BP + LCD_V_PW - 1 < 0)
            m6g26_lcd_config.lcd_timing.stv1_ve_addr = VIDEO_ON_LINE - LCD_V_BP + LCD_V_PW - 1 + V_PERIOD - 1;
    }
    else if(m6g26_lcd_config.lcd_basic.lcd_type == LCD_DIGITAL_TTL)
    {
        if(VIDEO_ON_PIXEL - LCD_H_BP + LCD_H_PW < 0)
            m6g26_lcd_config.lcd_timing.sth1_hs_addr = VIDEO_ON_PIXEL - LCD_H_BP + LCD_H_PW + H_PERIOD - 1;
        if(VIDEO_ON_PIXEL - LCD_H_BP < 0)
            m6g26_lcd_config.lcd_timing.sth1_he_addr = VIDEO_ON_PIXEL - LCD_H_BP + H_PERIOD - 1;

        if(VIDEO_ON_LINE - LCD_V_BP + LCD_V_PW - 1 < 0)
            m6g26_lcd_config.lcd_timing.stv1_vs_addr = VIDEO_ON_LINE - LCD_V_BP + LCD_V_PW - 1 + V_PERIOD - 1;
        if(VIDEO_ON_LINE - LCD_V_BP < 0)
            m6g26_lcd_config.lcd_timing.stv1_ve_addr = VIDEO_ON_LINE - LCD_V_BP + V_PERIOD - 1;
    }
    */
    if(m6g26_lcd_config.lcd_timing.sth1_hs_addr < 0)
        m6g26_lcd_config.lcd_timing.sth1_hs_addr = m6g26_lcd_config.lcd_timing.sth1_hs_addr + m6g26_lcd_config.lcd_basic.h_period - 1;
    if(m6g26_lcd_config.lcd_timing.sth1_he_addr < 0)
        m6g26_lcd_config.lcd_timing.sth1_he_addr =m6g26_lcd_config.lcd_timing.sth1_he_addr + m6g26_lcd_config.lcd_basic.h_period - 1;

    if(m6g26_lcd_config.lcd_timing.stv1_vs_addr < 0)
        m6g26_lcd_config.lcd_timing.stv1_vs_addr = m6g26_lcd_config.lcd_timing.stv1_vs_addr + m6g26_lcd_config.lcd_basic.v_period - 1;
    if(m6g26_lcd_config.lcd_timing.stv1_ve_addr < 0)
        m6g26_lcd_config.lcd_timing.stv1_ve_addr = m6g26_lcd_config.lcd_timing.stv1_ve_addr + m6g26_lcd_config.lcd_basic.v_period - 1;
    
	lcd_sync_duration(&m6g26_lcd_config);
	lcd_setup_gamma_table(&m6g26_lcd_config);
	lcd_video_adjust(&m6g26_lcd_config);	
    meson_lcd_set_platdata(&m6g26_lcd_data, sizeof(struct aml_lcd_platform));
    err = platform_add_devices(m6g26_lcd_devices, ARRAY_SIZE(m6g26_lcd_devices));
/*  
    printk("------pll_ctrl: %x\n", m6g26_lcd_config.lcd_timing.pll_ctrl);
    printk("------clk_ctrl: %x\n", m6g26_lcd_config.lcd_timing.clk_ctrl);
    printk("------div_ctrl: %x\n", m6g26_lcd_config.lcd_timing.div_ctrl);
    
    printk("------sth1_hs_addr: %d\n", m6g26_lcd_config.lcd_timing.sth1_hs_addr);
    printk("------sth1_he_addr: %d\n", m6g26_lcd_config.lcd_timing.sth1_he_addr);
    printk("------stv1_vs_addr: %d\n", m6g26_lcd_config.lcd_timing.stv1_vs_addr);
    printk("------stv1_ve_addr: %d\n", m6g26_lcd_config.lcd_timing.stv1_ve_addr);

    printk("------h_period: %d\n", m6g26_lcd_config.lcd_basic.h_period);
    printk("------v_period: %d\n", m6g26_lcd_config.lcd_basic.v_period);
*/
	/*int ret;
	ret = class_register(&aml_lcd_class);
    if(ret){
		printk(" class register aml_lcd_class fail!\n");
	}
    return err;*/
}

void shut_down_lcd(void)
{
	printk("shut down lcd...\n");
	m6g26_backlight_power_ctrl(OFF);
	m6g26_lcd_power_ctrl(OFF);
    mdelay(300);
}


