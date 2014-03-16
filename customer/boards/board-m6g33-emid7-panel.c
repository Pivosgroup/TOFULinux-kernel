/*
 * arch/arm/mach-meson6/board-m6g06-848-panel.c
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

extern int axp_gpio_set_io(int gpio, int io_state);
extern int axp_gpio_get_io(int gpio, int *io_state);
extern int axp_gpio_set_value(int gpio, int value);
extern int axp_gpio_get_value(int gpio, int *value);

extern Lcd_Config_t m6g06_848_lcd_config;

// Define backlight control method
#define BL_CTL_GPIO 	0
#define BL_CTL_PWM  	1
#define BL_CTL      	BL_CTL_GPIO

#if(BL_CTL==BL_CTL_PWM)
#define PWM_MAX         60000   //PWM_MAX <= 65535
#define	PWM_PRE_DIV		0		//pwm_freq = 24M / (pre_div + 1) / PWM_MAX	
#endif

#define BL_MAX_LEVEL    255
#define BL_MIN_LEVEL    0

static unsigned m6g06_848_bl_level = 0;

static Bool_t data_status = ON;
static Bool_t bl_status = ON;

// resolve lcd shark after unplug hdmi	for svn 324 path   by add ming.huang 20121224
static int hd_flagg=0;

static void lcd_mdelay(int n)
{
	n *= 10;
	while (n--)
		udelay(100);
}

static void m6g06_848_ttl_ports_ctrl(Bool_t status)
{
    printk(KERN_INFO "%s: %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
    if (status) {		
        aml_write_reg32(P_PERIPHS_PIN_MUX_1, aml_read_reg32(P_PERIPHS_PIN_MUX_1) | ((1<<14)|(1<<17)|(1<<18)|(1<<19))); //set tcon pinmux
        aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) | ((3<<0)|(3<<2)|(3<<4)));  //enable RGB 24bit
    }else {
    // resolve lcd shark after unplug hdmi	for svn 324 path   by add ming.huang 20121224
    	if(hd_flagg ==0){
        aml_write_reg32(P_PERIPHS_PIN_MUX_0, aml_read_reg32(P_PERIPHS_PIN_MUX_0) & ~((3<<0)|(3<<2)|(3<<4))); //disable RGB 24bit
		aml_write_reg32(P_PERIPHS_PIN_MUX_1, aml_read_reg32(P_PERIPHS_PIN_MUX_1) & ~((1<<14)|(1<<17)|(1<<18)|(1<<19)));  //clear tcon pinmux        		
    	}
		else
    	hd_flagg =0;
    }
}

// Refer to H/W schematics
static DEFINE_MUTEX(bl_power_mutex);
static void m6g06_848_backlight_power_ctrl(Bool_t status)
{
	mutex_lock(&bl_power_mutex);
	//printk("%s(): bl_status=%s, data_status=%s, bl_level=%u\n", __FUNCTION__, (bl_status ? "ON" : "OFF"), (data_status ? "ON" : "OFF"), bl_level);
    if( status == ON ){
		
		if ((bl_status == ON) || (data_status == OFF) || (m6g06_848_bl_level  == 0)) {
			mutex_unlock(&bl_power_mutex);
			return;
		}
        lcd_mdelay(30);		
		m6g06_848_ttl_ports_ctrl(ON);
        aml_set_reg32_bits(P_LED_PWM_REG0, 1, 12, 2);
        lcd_mdelay(300); // wait for PWM charge   
        #if defined(CONFIG_GSLX680_TOUCHSCREEN)
		extern int gsl_suspend_flag;
		if(gsl_suspend_flag==1){
		 gsl_suspend_flag=0;
		 lcd_mdelay(800);
		 printk("delay 800 ms for GSLX680 tp\n");
		 
		}
		#endif
		//BL_EN -> GPIOD_1: 1
#if (BL_CTL==BL_CTL_GPIO)	
		gpio_out(PAD_GPIOD_1, 1); 
#elif (BL_CTL==BL_CTL_PWM)		
		aml_set_reg32_bits(P_PWM_PWM_D, 0, 0, 16);  		//pwm low
		aml_set_reg32_bits(P_PWM_PWM_D, PWM_MAX, 16, 16);	//pwm high
		aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<16)) | ((1 << 23) | (PWM_PRE_DIV<<16) | (1<<1)));  //enable pwm clk & pwm output
		aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<3));  //enable pwm pinmux
#endif
    }
    else{
		if (bl_status == OFF) {
			mutex_unlock(&bl_power_mutex);
			return;
		}
//#if (BL_CTL==BL_CTL_GPIO)		
		aml_write_reg32(P_PWM_MISC_REG_CD, aml_read_reg32(P_PWM_MISC_REG_CD) & ~((1 << 23) | (1<<1)));  //disable pwm clk & pwm output
		gpio_out(PAD_GPIOD_1, 0); 		
        
        lcd_mdelay(30); 
        m6g06_848_ttl_ports_ctrl(OFF);
		lcd_mdelay(30);
    }
	bl_status = status;
	printk(KERN_INFO "%s() Power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));
	mutex_unlock(&bl_power_mutex);
}

#define MIN_LEVEL  0x0f
#define MAX_LEVEL  0x00
static DEFINE_MUTEX(bl_level_mutex);

static void m6g06_848_set_backlight_level(unsigned level)
{
    mutex_lock(&bl_level_mutex);
    m6g06_848_bl_level = level;
   printk(KERN_DEBUG "%s: %d\n", __FUNCTION__, m6g06_848_bl_level);

    level = (level > BL_MAX_LEVEL ? BL_MAX_LEVEL : (level < BL_MIN_LEVEL ? BL_MIN_LEVEL : level));
	if (m6g06_848_bl_level == 0) {
		m6g06_848_backlight_power_ctrl(OFF);		
	}

#if (BL_CTL==BL_CTL_GPIO)
	if(level < 20){
		level =12;
	}else{	
    	//level = level * 15 / BL_MAX_LEVEL;
    	//level = 15 - level;
    	 level = MIN_LEVEL - ((level - 20) * (MIN_LEVEL - MAX_LEVEL)) / 235;	
	    //add by emdoor jim.kuang 2012-5-15
	    //level=((level<3)?3:(level));
		level=((level>=13)?12:(level));
     }
   // printk("%s: %d\n", __FUNCTION__, level);
    aml_set_reg32_bits(P_LED_PWM_REG0, level, 0, 4);
#elif (BL_CTL==BL_CTL_PWM)
    level = level * PWM_MAX / BL_MAX_LEVEL ;
    aml_set_reg32_bits(P_PWM_PWM_D, (PWM_MAX - level), 0, 16);  //pwm low
    aml_set_reg32_bits(P_PWM_PWM_D, level, 16, 16);				//pwm high
#endif
if (bl_status == OFF&&m6g06_848_bl_level != 0) 
		   m6g06_848_backlight_power_ctrl(ON); 
	printk(KERN_INFO "set_backlight_level finished.\n"); 
	mutex_unlock(&bl_level_mutex);
}

static unsigned m6g06_848_get_backlight_level(void)
{
    printk(KERN_DEBUG "%s: %d\n", __FUNCTION__, m6g06_848_bl_level);
    return m6g06_848_bl_level;
}

static DEFINE_MUTEX(lcd_power_mutex);
static void m6g06_848_lcd_power_ctrl(Bool_t status)
{
    mutex_lock(&lcd_power_mutex);
    printk(KERN_INFO "%s() Power %s\n", __FUNCTION__, (status ? "ON" : "OFF"));


		int hdmi_flag=0;	
/*************************************/	
// resolve lcd shark after unplug hdmi  for svn 324 path  by add ming.huang 20121224
	/*HDMI_HPD: GPIOC_10 */		
	   aml_set_reg32_bits(P_PREG_PAD_GPIO2_EN_N, 1, 10, 1);    // mode
	   hdmi_flag = aml_get_reg32_bits(P_PREG_PAD_GPIO2_I, 10, 1);  // value
/*************************************/	
#if defined(EMID7_V1)
#if defined(CONFIG_ZFORCE_TS_TOUCHSCREEN)
	extern u8 zforce_work_flag;
#endif
#endif
    if (status) {
		printk("@enble lcd 3v3 cs \n");
		lcd_mdelay(30);        	
#if defined(EM62V3)	
	//VCCx3_EN#: 0
	axp_gpio_set_io(1,1);
	axp_gpio_set_value(1, 0);
	//pmu gpio3 -> VCCx3_EN: 0
	axp_gpio_set_io(3,1);	  
	axp_gpio_set_value(3, 0); 
	lcd_mdelay(20);
	//lcd reset enble
	gpio_out(PAD_GPIOD_8, 1);
	//GPIOA27 -> LCD_PWR_EN#: 0  lcd 3.3v
    gpio_out(PAD_GPIOA_27, 0);
	lcd_mdelay(30);
#endif	
#if defined(EMID7_V1)
	#if defined(CONFIG_ZFORCE_TS_TOUCHSCREEN)
	if(zforce_work_flag == 245)
	{
		//GPIOA27 -> LCD_PWR_EN#: 0  lcd 3.3v
		gpio_out(PAD_GPIOA_27, 1);
		lcd_mdelay(10);     	 
		//GPIOA27 -> LCD_CS
     	gpio_out(PAD_GPIOC_0, 1);
		printk("################%s  %d ###########\r\n",__func__,zforce_work_flag);
	}else{
		lcd_mdelay(100);
	}

    #endif
#endif	
		data_status = status;
// resolve lcd shark after unplug hdmi	for svn 324 path   by add ming.huang 20121224
		if(hdmi_flag==1)
			hd_flagg=1;
//end lcd shark 	
    }
    else {
		data_status = status;
		printk("@@@@@@@@disable  lcd 3v3 cs \n");
#if defined(EM62V3)
		//VCCx3_EN#: 0
		axp_gpio_set_io(1,1);
		axp_gpio_set_value(1, 1);
		
		axp_gpio_set_io(3,1);
		axp_gpio_set_value(3, 1);
		gpio_out(PAD_GPIOD_8, 0);              // open LCD power 
		lcd_mdelay(20);
#endif
#if defined(EMID7_V1)
	#if defined(CONFIG_ZFORCE_TS_TOUCHSCREEN)
	if(zforce_work_flag == 245)
	{
		//GPIOA27 -> LCD_3V3		
    	gpio_out(PAD_GPIOC_0, 0);
		lcd_mdelay(10);
		//GPIOC0 -> LCD_CS	
		gpio_out(PAD_GPIOA_27,0);
		printk("################%s  %d ###########\r\n",__func__,zforce_work_flag);
	}else{
		lcd_mdelay(20);
	}
	#endif
#endif
    }
	printk(KERN_INFO "%s() Power %s finished\n", __FUNCTION__, (status ? "ON" : "OFF"));
	mutex_unlock(&lcd_power_mutex);
}

static int m6g06_848_lcd_suspend(void *args)
{
    args = args;

    printk(KERN_INFO "LCD suspending...\n");
    return 0;
}

static int m6g06_848_lcd_resume(void *args)
{
    args = args;

    printk(KERN_INFO "LCD resuming...\n");
    return 0;
}

#define H_ACTIVE		800
#define V_ACTIVE      	480 
#define H_PERIOD		1056 
#define V_PERIOD		525 
#define VIDEO_ON_PIXEL  48
#define VIDEO_ON_LINE   22

Lcd_Config_t m6g06_848_lcd_config = {
    // Refer to LCD Spec
    .lcd_basic = {
        .h_active = H_ACTIVE,
        .v_active = V_ACTIVE,
        .h_period = H_PERIOD,
        .v_period = V_PERIOD,
        .screen_ratio_width = 16,
        .screen_ratio_height = 9,
        .lcd_type = LCD_DIGITAL_TTL,   //LCD_DIGITAL_TTL  //LCD_DIGITAL_LVDS  //LCD_DIGITAL_MINILVDS
        .lcd_bits = 6,  //8  //6
    },

    .lcd_timing = {      
		.pll_ctrl = 0x1022a,//42MHZ
        .div_ctrl = 0x18803, 
        .clk_ctrl = 0x5111c,  //[19:16]ss_ctrl, [12]pll_sel, [8]div_sel, [4]vclk_sel, [3:0]xd
        //.sync_duration_num = 501,
        //.sync_duration_den = 10,
   
		.video_on_pixel = VIDEO_ON_PIXEL,
        .video_on_line = VIDEO_ON_LINE,
		 
        .sth1_hs_addr = 9,
        .sth1_he_addr = 1033,
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
        .stv1_hs_addr = 0,
        .stv1_he_addr = H_PERIOD-1,
        .stv1_vs_addr = 5,
        .stv1_ve_addr = 512,
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
        .gamma_cntl_port = (1 << LCD_GAMMA_EN) | (0 << LCD_GAMMA_RVS_OUT) | (1 << LCD_GAMMA_VCOM_POL),
        .gamma_vcom_hswitch_addr = 0,
        .rgb_base_addr = 0xf0,
		.rgb_coeff_addr = 0x712, 
    },

    .lcd_power_ctrl = {
        .cur_bl_level = 0,
        .power_ctrl = m6g06_848_lcd_power_ctrl,
        .backlight_ctrl = m6g06_848_backlight_power_ctrl,
        .get_bl_level = m6g06_848_get_backlight_level,
        .set_bl_level = m6g06_848_set_backlight_level,
        .lcd_suspend = m6g06_848_lcd_suspend,
        .lcd_resume = m6g06_848_lcd_resume,
    },    
};

static void lcd_setup_gamma_table(Lcd_Config_t *pConf)
{
    int i;
	
	const unsigned short gamma_adjust[256] = {
		#if 1
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
		#else
		0,2,4,6,8,9,11,13,15,17,19,21,23,24,26,30,32,33,35,37,38,40,42,43,45,47,48,50,52,53,55,56,
		58,60,61,63,64,66,67,69,70,72,73,75,76,77,79,80,82,83,85,86,87,89,90,91,93,94,96,97,99,100,101,103,
		104,105,107,108,109,110,112,113,115,116,117,118,120,121,123,124,125,127,128,129,131,132,133,135,136,137,139,140,141,142,144,145,
		146,147,148,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,169,170,171,172,173,174,175,176,177,
		178,179,180,180,181,182,183,184,185,186,187,188,189,190,191,191,192,193,194,195,196,196,197,198,199,200,200,201,202,203,203,204,
		205,205,206,207,208,208,209,210,210,211,211,212,213,213,214,215,215,216,216,217,218,218,219,219,220,221,221,222,222,223,223,224,
		225,225,226,226,227,227,228,228,229,229,230,230,231,231,232,233,233,234,234,235,235,235,236,236,237,237,238,238,239,239,240,240,
		241,241,242,242,243,243,243,244,244,245,245,246,246,247,247,248,248,249,249,249,250,250,251,251,252,252,253,253,254,254,255,255,
		#endif
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

static void lcd_sync_duration(Lcd_Config_t *pConf)
{
	unsigned m, n, od, div, xd;
	unsigned pre_div;
	unsigned sync_duration;
	
	m = ((pConf->lcd_timing.pll_ctrl) >> 0) & 0x1ff;
	n = ((pConf->lcd_timing.pll_ctrl) >> 9) & 0x1f;
	od = ((pConf->lcd_timing.pll_ctrl) >> 16) & 0x3;
	div = ((pConf->lcd_timing.div_ctrl) >> 4) & 0x7;
	xd = ((pConf->lcd_timing.clk_ctrl) >> 0) & 0xf;
	
	od = (od == 0) ? 1:((od == 1) ? 2:4);
	switch(pConf->lcd_basic.lcd_type)
	{
		case LCD_DIGITAL_TTL:
			pre_div = 1;
			break;
		case LCD_DIGITAL_LVDS:
			pre_div = 7;
			break;
		default:
			pre_div = 1;
			break;
	}
	
	sync_duration = m*24*100/(n*od*(div+1)*xd*pre_div);	
	sync_duration = ((sync_duration * 100000 / H_PERIOD) * 10) / V_PERIOD;
	sync_duration = (sync_duration + 5) / 10;	
	
	pConf->lcd_timing.sync_duration_num = sync_duration;
	pConf->lcd_timing.sync_duration_den = 10;
}

static struct aml_bl_platform_data m6g06_848_backlight_data =
{
    //.power_on_bl = power_on_backlight,
    //.power_off_bl = power_off_backlight,
    .get_bl_level = m6g06_848_get_backlight_level,
    .set_bl_level = m6g06_848_set_backlight_level,
    .max_brightness = 255,
    .dft_brightness = 200,
};

static struct platform_device m6g06_848_backlight_device = {
    .name = "aml-bl",
    .id = -1,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &m6g06_848_backlight_data,
    },
};

static struct aml_lcd_platform __initdata m6g06_848_lcd_data = {
    .lcd_conf = &m6g06_848_lcd_config,
};

static struct platform_device __initdata * m6g06_848_lcd_devices[] = {
    &meson_device_lcd,
//    &meson_device_vout,
    &m6g06_848_backlight_device,
};

int  m6g04_lcd_init(void)
{
	printk("@@@@@@@@@@m6g04_lcd_init\n");
    int err;
	lcd_sync_duration(&m6g06_848_lcd_config);
	lcd_setup_gamma_table(&m6g06_848_lcd_config);
	lcd_video_adjust(&m6g06_848_lcd_config);	
    meson_lcd_set_platdata(&m6g06_848_lcd_data, sizeof(struct aml_lcd_platform));
    err = platform_add_devices(m6g06_848_lcd_devices, ARRAY_SIZE(m6g06_848_lcd_devices));
    return err;
}

