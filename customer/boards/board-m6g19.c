/*
 * customer/boards/board-m6g19.c
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
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach/map.h>
#include <plat/platform.h>
#include <plat/plat_dev.h>
#include <plat/platform_data.h>

#include <linux/io.h>
#include <plat/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/device.h>
#include <linux/spi/flash.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <plat/platform.h>
#include <plat/plat_dev.h>
#include <plat/platform_data.h>
#include <plat/lm.h>
#include <plat/regops.h>
#include <linux/io.h>
#include <plat/io.h>

#include <mach/map.h>
#include <mach/i2c_aml.h>
#include <mach/nand.h>
#include <mach/usbclock.h>
#include <mach/usbsetting.h>
#include <mach/pm.h>
#include <mach/vdac_switch.h>

#include <mach/pinmux.h>
#include <mach/voltage.h>

#include <linux/uart-aml.h>
#include <linux/i2c-aml.h>
#include <linux/syscore_ops.h>

#include <asm/page.h>

#ifdef CONFIG_CARDREADER
#include <mach/card_io.h>
#endif // CONFIG_CARDREADER

#include <mach/gpio.h>
#include <mach/gpio_data.h>


#ifdef CONFIG_EFUSE
#include <linux/efuse.h>
#endif
#ifdef CONFIG_SND_SOC_DUMMY_CODEC
#include <sound/dummy_codec.h>
#endif
#ifdef CONFIG_AM_WIFI
#include <plat/wifi_power.h>
#endif
#ifdef CONFIG_AML_HDMI_TX
#include <plat/hdmi_config.h>
#endif

#ifdef CONFIG_AM_ETHERNET
#include <mach/am_regs.h>
#include <mach/am_eth_reg.h>
#endif

unsigned int board_ver = 2;

/***********************************************************************
 * IO Mapping
 **********************************************************************/
#define PHYS_MEM_START      (0x80000000)
#define PHYS_MEM_SIZE       (1024*SZ_1M)
#define PHYS_MEM_END        (PHYS_MEM_START + PHYS_MEM_SIZE -1 )

#define	PWM_PRE_DIV		0		//pwm_freq = 24M / (pre_div + 1) / PWM_MAX	
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
#define OSD_PANEL_PIX       (800*480)
#define B16BpP  (2)
#define B32BpP  (4)
#define DOUBLE_BUFFER   (2)

#define OSD1_MAX_MEM        U_ALIGN(OSD_1080_PIX*B32BpP*DOUBLE_BUFFER)
#define OSD2_MAX_MEM        U_ALIGN(32*32*B32BpP)

/******** Reserved memory configuration ***************/
#define OSD1_ADDR_START     U_ALIGN(AUDIODSP_ADDR_END )
#define OSD1_ADDR_END       (OSD1_ADDR_START+OSD1_MAX_MEM - 1)
#define OSD2_ADDR_START     U_ALIGN(OSD1_ADDR_END)
#define OSD2_ADDR_END       (OSD2_ADDR_START +OSD2_MAX_MEM -1)

#if defined(CONFIG_AM_VDEC_H264MVC)
	#define CODEC_MEM_SIZE U_ALIGN(64*SZ_1M) 
#elif defined(CONFIG_AM_VDEC_H264) 
	#define CODEC_MEM_SIZE U_ALIGN(32*SZ_1M) 
#else 
	#define CODEC_MEM_SIZE U_ALIGN(16*SZ_1M) 
#endif

#define CODEC_ADDR_START    U_ALIGN(OSD2_ADDR_END)
#define CODEC_ADDR_END      (CODEC_ADDR_START+CODEC_MEM_SIZE-1)

/********VDIN memory configuration ***************/
#ifdef CONFIG_TVIN_VIUIN
#define VDIN_MEM_SIZE			(SZ_1M*15)
#define VDIN_ADDR_START		U_ALIGN(CODEC_ADDR_END)
#define VDIN_ADDR_END		(VDIN_ADDR_START +VDIN_MEM_SIZE -1)
#else
#define VDIN_ADDR_START     U_ALIGN(CODEC_ADDR_END)
#define VDIN_ADDR_END       (VDIN_ADDR_START + CODEC_MEM_SIZE - 1)
#endif

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
#define DI_ADDR_START       U_ALIGN(VDIN_ADDR_END)
#define DI_ADDR_END         (DI_ADDR_START+DI_MEM_SIZE-1)

#ifdef CONFIG_POST_PROCESS_MANAGER
#ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
#define PPMGR_MEM_SIZE               1920 * 1088 * 22
#else
#define PPMGR_MEM_SIZE               1920 * 1088 * 15
#endif
#else
#define PPMGR_MEM_SIZE		0
#endif /* CONFIG_POST_PROCESS_MANAGER */

#define PPMGR_ADDR_START	U_ALIGN(DI_ADDR_END)
#define PPMGR_ADDR_END		(PPMGR_ADDR_START+PPMGR_MEM_SIZE-1)

#define STREAMBUF_MEM_SIZE          (SZ_1M*10)
#define STREAMBUF_ADDR_START        U_ALIGN(PPMGR_ADDR_END)
#define STREAMBUF_ADDR_END      (STREAMBUF_ADDR_START+STREAMBUF_MEM_SIZE-1)

#define RESERVED_MEM_END    (STREAMBUF_ADDR_END)
static struct resource meson_fb_resource[] = {
    [0] = {
        .start = OSD1_ADDR_START,
        .end   = OSD1_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = OSD2_ADDR_START,
        .end   = OSD2_ADDR_END,
        .flags = IORESOURCE_MEM,
    },

};

static struct resource meson_codec_resource[] = {
    [0] = {
        .start = CODEC_ADDR_START,
        .end   = CODEC_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = STREAMBUF_ADDR_START,
        .end   = STREAMBUF_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

#ifdef CONFIG_POST_PROCESS_MANAGER
static struct resource ppmgr_resources[] = {
    [0] = {
        .start = PPMGR_ADDR_START,
        .end   = PPMGR_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device ppmgr_device = {
    .name       = "ppmgr",
    .id         = 0,
    .num_resources = ARRAY_SIZE(ppmgr_resources),
    .resource      = ppmgr_resources,
};
#endif

#ifdef CONFIG_FREE_SCALE
static struct resource freescale_resources[] = {
    [0] = {
        .start = FREESCALE_ADDR_START,
        .end   = FREESCALE_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device freescale_device =
{
    .name           = "freescale",
    .id             = 0,
    .num_resources  = ARRAY_SIZE(freescale_resources),
    .resource       = freescale_resources,
};
#endif

#if defined(CONFIG_TVIN_VDIN)
static struct resource vdin_resources[] = {
    [0] = {
        .start =  VDIN_ADDR_START,  //pbufAddr
        .end   = VDIN_ADDR_END,     //pbufAddr + size
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = VDIN_ADDR_START,
        .end   = VDIN_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
    [2] = {
        .start = INT_VDIN_VSYNC,
        .end   = INT_VDIN_VSYNC,
        .flags = IORESOURCE_IRQ,
    },
    [3] = {
        .start = INT_VDIN_VSYNC,
        .end   = INT_VDIN_VSYNC,
        .flags = IORESOURCE_IRQ,
    },
};

static struct platform_device vdin_device = {
    .name       = "vdin",
    .id         = -1,
    .num_resources = ARRAY_SIZE(vdin_resources),
    .resource      = vdin_resources,
};
#endif

static  int __init setup_devices_resource(void)
{
    setup_fb_resource(meson_fb_resource, ARRAY_SIZE(meson_fb_resource));
#ifdef CONFIG_AM_STREAMING
    setup_codec_resource(meson_codec_resource, ARRAY_SIZE(meson_codec_resource));
#endif
    return 0;
}

/***********************************************************
*Remote Section
************************************************************/
#ifdef CONFIG_AM_REMOTE
#include <plat/remote.h>

static pinmux_item_t aml_remote_pins[] = {
	{
		.reg = PINMUX_REG(AO),
		.clrmask = 0,
		.setmask = 1 << 0,
	},
	PINMUX_END_ITEM
};

static struct aml_remote_platdata aml_remote_pdata __initdata = {
	.pinmux_items  = aml_remote_pins,
	.ao_baseaddr = P_AO_IR_DEC_LDR_ACTIVE, 
};

static void __init setup_remote_device(void)
{
	meson_remote_set_platdata(&aml_remote_pdata);
}
#endif

/***********************************************************************
 * UART Section
 **********************************************************************/
static pinmux_item_t uart_pins[] = {
    {
        .reg = PINMUX_REG(AO),
        .setmask = 3 << 11
    },
#ifdef CONFIG_BT_DEVICE
    {
        .reg = PINMUX_REG(4),
        .setmask = 0xf << 10
    },
#endif
    PINMUX_END_ITEM
};

static pinmux_set_t aml_uart_ao = {
    .chip_select = NULL,
    .pinmux = &uart_pins[0]
};
#ifdef CONFIG_BT_DEVICE
static pinmux_set_t aml_uart_a = {
    .chip_select = NULL,
    .pinmux = &uart_pins[1]
};
#endif
static struct aml_uart_platform  __initdata aml_uart_plat = {
#ifdef CONFIG_BT_SLEEP
    .rts_control_pin = PAD_GPIOD_3,
#endif
    .uart_line[0]   = UART_AO,
    .uart_line[1]   = UART_A,
    .uart_line[2]   = UART_B,
    .uart_line[3]   = UART_C,
    .uart_line[4]   = UART_D,

    .pinmux_uart[0] = (void*)&aml_uart_ao,
#ifdef CONFIG_BT_DEVICE
    .pinmux_uart[1] = (void*)&aml_uart_a,
#else
    .pinmux_uart[1] = NULL,
#endif
    .pinmux_uart[2] = NULL,
    .pinmux_uart[3] = NULL,
    .pinmux_uart[4] = NULL
};

static struct platform_device aml_uart_device = {
    .name       = "mesonuart",
    .id     = -1,
    .num_resources  = 0,
    .resource   = NULL,
    .dev = {
        .platform_data = &aml_uart_plat,
    },
};

#ifdef CONFIG_AM_ETHERNET
#include <plat/eth.h>
//#define ETH_MODE_RGMII
//#define ETH_MODE_RMII_INTERNAL
#define ETH_MODE_RMII_EXTERNAL
static void aml_eth_reset(void)
{
        unsigned int val = 0;

        printk(KERN_INFO "****** aml_eth_reset() ******\n");
#ifdef ETH_MODE_RGMII
        val = 0x211;
#else
        val = 0x241;
#endif
        /* setup ethernet mode */
        aml_write_reg32(P_PREG_ETHERNET_ADDR0, val);

        /* setup ethernet interrupt */
        aml_set_reg32_mask(P_SYS_CPU_0_IRQ_IN0_INTR_MASK, 1 << 8);
        aml_set_reg32_mask(P_SYS_CPU_0_IRQ_IN1_INTR_STAT, 1 << 8);

        /* hardware reset ethernet phy */
        gpio_out(PAD_GPIOY_15, 0);
        msleep(20);
        gpio_out(PAD_GPIOY_15, 1);
}
static void aml_eth_clock_enable(void)
{
        unsigned int val = 0;

        printk(KERN_INFO "****** aml_eth_clock_enable() ******\n");
#ifdef ETH_MODE_RGMII
        val = 0x309;
#elif defined(ETH_MODE_RMII_EXTERNAL)
        val = 0x130;
#else
        val = 0x702;
#endif
        /* setup ethernet clk */
        aml_write_reg32(P_HHI_ETH_CLK_CNTL, val);
}

static void aml_eth_clock_disable(void)
{
        printk(KERN_INFO "****** aml_eth_clock_disable() ******\n");
        /* disable ethernet clk */
        aml_clr_reg32_mask(P_HHI_ETH_CLK_CNTL, 1 << 8);
}

static pinmux_item_t aml_eth_pins[] = {
        /* RMII pin-mux */
        {
                .reg = PINMUX_REG(6),
                .clrmask = 0,
#ifdef ETH_MODE_RMII_EXTERNAL
                .setmask = 0x8007ffe0,
#else
                .setmask = 0x4007ffe0,
#endif
        },
        PINMUX_END_ITEM
};
static pinmux_set_t aml_eth_pinmux = {
        .chip_select = NULL,
        .pinmux = aml_eth_pins,
};

static void aml_eth_pinmux_setup(void)
{
        printk(KERN_INFO "****** aml_eth_pinmux_setup() ******\n");

        pinmux_set(&aml_eth_pinmux);
}

static void aml_eth_pinmux_cleanup(void)
{
        printk(KERN_INFO "****** aml_eth_pinmux_cleanup() ******\n");

        pinmux_clr(&aml_eth_pinmux);
}

static void aml_eth_init(void)
{
        aml_eth_pinmux_setup();
        aml_eth_clock_enable();
        aml_eth_reset();
}

static struct aml_eth_platdata aml_eth_pdata __initdata = {
        .pinmux_items   = aml_eth_pins,
        .pinmux_setup   = aml_eth_pinmux_setup,
        .pinmux_cleanup = aml_eth_pinmux_cleanup,
        .clock_enable   = aml_eth_clock_enable,
        .clock_disable  = aml_eth_clock_disable,
        .reset          = aml_eth_reset,
};
static void __init setup_eth_device(void)
{
        meson_eth_set_platdata(&aml_eth_pdata);
        aml_eth_init();
}
#endif

#ifdef CONFIG_USB_G_ANDROID
static struct platform_device android_usb_device = {
	.name = "android_usb",
	.id = -1,
};
#endif

/***********************************************************************
 * Nand Section
 **********************************************************************/

//defined in uboot customer/board/configs/m6_duokan_mbx_g19ref.h
#ifdef CONFIG_INAND
#define CONFIG_ENV_OFFSET       0x400000	//4M
#define CONFIG_ENV_SIZE         0x8000		//32K

#define BOOTLOADER_OFFSET 0
#define BOOTLOADER_SIZE 0x100000	//1M
#endif

#if defined(CONFIG_AM_NAND)|| defined(CONFIG_INAND)
static struct mtd_partition normal_partition_info[] = {
#if defined(CONFIG_INAND)
    {
        .name = "bootloader",
        .offset = BOOTLOADER_OFFSET,
        .size = BOOTLOADER_SIZE,
    },
    {
        .name = "ubootenv",
        .offset = CONFIG_ENV_OFFSET,
        .size = CONFIG_ENV_SIZE,
    },
#endif
    {
        .name = "logo",
        .offset = 8*SZ_1M,
        .size = 4*SZ_1M,
    },
    {
        .name = "recovery",
        .offset = 12*SZ_1M,
        .size = 8*SZ_1M,
    },
    {
        .name = "boot",
        .offset = 20*SZ_1M,
        .size = 8*SZ_1M,
    },
    {
        .name = "boot1",
        .offset = 28*SZ_1M,
        .size = 8*SZ_1M,
    },
    {
        .name = "system",
        .offset = 36*SZ_1M,
        .size = 512*SZ_1M,
    },
    {
        .name = "system1",
        .offset = 548*SZ_1M,
        .size = 512*SZ_1M,
    },
    {
        .name = "cache",
        .offset = 1060*SZ_1M,
        .size = 192*SZ_1M,
    },
    {
        .name = "userdata",
        .offset = MTDPART_OFS_APPEND,
        .size = MTDPART_SIZ_FULL,
    },
};

#ifdef CONFIG_AM_NAND
static struct aml_nand_platform aml_nand_mid_platform[] = {
#ifndef CONFIG_AMLOGIC_SPI_NOR
    {
        .name = NAND_BOOT_NAME,
        .chip_enable_pad = AML_NAND_CE0,
        .ready_busy_pad = AML_NAND_CE0,
        .platform_nand_data = {
            .chip =  {
                .nr_chips = 1,
                .options = (NAND_TIMING_MODE5 | NAND_ECC_BCH60_1K_MODE),
            },
        },
        .rbpin_detect = 1,
        .T_REA = 20,
        .T_RHOH = 15,
    },
#endif
    {
        .name = NAND_NORMAL_NAME,
        .chip_enable_pad = (AML_NAND_CE0/* | (AML_NAND_CE1 << 4) | (AML_NAND_CE2 << 8) | (AML_NAND_CE3 << 12)*/),
        .ready_busy_pad = (AML_NAND_CE0 /*| (AML_NAND_CE0 << 4) | (AML_NAND_CE1 << 8) | (AML_NAND_CE1 << 12)*/),
        .platform_nand_data = {
            .chip =  {
                .nr_chips = 1,
                .nr_partitions = ARRAY_SIZE(normal_partition_info),
                .partitions = normal_partition_info,
                .options = (NAND_TIMING_MODE5 | NAND_ECC_BCH60_1K_MODE | NAND_TWO_PLANE_MODE),
            },
        },
        .rbpin_detect = 1,
        .T_REA = 20,
        .T_RHOH = 15,
        .ran_mode = 1,
    }
};

static struct aml_nand_device aml_nand_mid_device = {
    .aml_nand_platform = aml_nand_mid_platform,
    .dev_num = ARRAY_SIZE(aml_nand_mid_platform),
};

static struct resource aml_nand_resources[] = {
    {
        .start = 0xc1108600,
        .end = 0xc1108624,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device aml_nand_device = {
    .name = "aml_nand",
    .id = 0,
    .num_resources = ARRAY_SIZE(aml_nand_resources),
    .resource = aml_nand_resources,
    .dev = {
        .platform_data = &aml_nand_mid_device,
    },
};
#endif
#endif

/***********************************************************************
 * WIFI  Section
 **********************************************************************/
/**
*GPIOX_0			-->WIFI_SD_D0
*GPIOX_1			-->WIFI_SD_D1
*GPIOX_2			-->WIFI_SD_D2
*GPIOX_3			-->WIFI_SD_D3

*GPIOX_8			-->WIFI_SD_CMD
*GPIOX_9			-->WIFI_SD_CLK

*WIFI_EN			-->GPIOD_5
*BT_WAKE		        -->GPIOD_4
*BT_RST		                -->GPIOD_6
*BT_HOST_WAKE                   -->GPIOAO_2
*WIFI_WAKE		        -->GPIOAO_3
*32K_CLOCK_OUT	                -->GPIOAO_6 (CLK_OUT2)
*/
#ifdef  CONFIG_AM_WIFI
#define DBG_LINE_INFO()  printk(KERN_INFO "[%s] in\n",__func__)

static void wifi_gpio_init(void)
{
    DBG_LINE_INFO();
    //WIFI_EN disable ---> out high
    gpio_out(PAD_GPIOD_5, 1);

#ifdef CONFIG_BCMDHD_OOB
    //WIFI_WAKE --> in
    gpio_set_status(PAD_GPIOAO_3, gpio_status_in);
    gpio_irq_set(PAD_GPIOAO_3, GPIO_IRQ(4, GPIO_IRQ_HIGH));
#endif

    //set pull-up X0 X1 X2 X3 X8 X9 SDIO 6 pin
    aml_clr_reg32_mask(P_PAD_PULL_UP_REG4,0xf|1<<8|1<<9);
    //set pull-up D4 D5 D6
    aml_clr_reg32_mask(P_PAD_PULL_UP_REG2,1<<20|1<<21|1<<22);
    //Disable pull-up AO2 AO3
    aml_set_reg32_mask(P_AO_RTI_PULL_UP_REG,1<<2|1<<3);
}

static pinmux_item_t clk32k_pins[] = {
    {
        .reg = PINMUX_REG(AO),
        .setmask = 1 << 22,
        .clrmask = 1 << 19,
    },
    PINMUX_END_ITEM
};

static pinmux_set_t clk32k_pinmux_set = {
    .chip_select = NULL,
    .pinmux = &clk32k_pins[0],
};

static void wifi_clock_enable(void)
{
    //set clk 32k for wifi
    //GPIOAO_6 (CLK_OUT2)  //reg : 108b  sr_sl:9-12  div:0-6    enable:8
    DBG_LINE_INFO();

    gpio_set_status(PAD_GPIOAO_6, gpio_status_out); //set  GPIOAO_6 out
    aml_set_reg32_mask(P_HHI_GEN_CLK_CNTL2, 1<<9); //set clk source
    aml_clr_reg32_mask(P_HHI_GEN_CLK_CNTL2, 0); //set div ==1
    aml_set_reg32_mask(P_HHI_GEN_CLK_CNTL2, 1<<8);//set enable clk
#if 0
    //pinmux_set(&clk32k_pinmux_set);//set mode GPIOAO_6-->CLK_OUT2
#else
    aml_set_reg32_mask(AOBUS_REG_ADDR(0x14), 1<<22);//set mode GPIOAO_6-->CLK_OUT2
    aml_clr_reg32_mask(AOBUS_REG_ADDR(0x14), 1<<19);
#endif
}

void extern_wifi_set_enable(int is_on)
{
    if(is_on){
        printk("WIFI Power Enable!\n");
        gpio_out(PAD_GPIOD_5,0);
        }
    else{
        printk("WIFI Power Disable!\n");
        gpio_out(PAD_GPIOD_5,1);
        }
}
EXPORT_SYMBOL(extern_wifi_set_enable);
#endif

#if defined(CONFIG_AMLOGIC_SPI_NOR)
static struct mtd_partition spi_partition_info[] = {
    {
         .name = "bootloader",
         .offset = 0,
         .size = 0x60000,
    },
    {
        .name = "ubootenv",
        .offset = 0x70000,
        .size = 0x10000,
    },
   
};

static struct flash_platform_data amlogic_spi_platform = {
    .parts = spi_partition_info,
    .nr_parts = ARRAY_SIZE(spi_partition_info),
};

static struct resource amlogic_spi_nor_resources[] = {
    {
        .start = 0xcc000000,
        .end = 0xcfffffff,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device amlogic_spi_nor_device = {
    .name = "AMLOGIC_SPI_NOR",
    .id = -1,
    .num_resources = ARRAY_SIZE(amlogic_spi_nor_resources),
    .resource = amlogic_spi_nor_resources,
    .dev = {
        .platform_data = &amlogic_spi_platform,
    },
};
#endif

#if defined(CONFIG_AML_EMMC_KEY) || defined(CONFIG_AML_NAND_KEY)
static char * secure_device[3]={"nand_key","emmc_key",NULL};
static struct platform_device aml_keys_device = {
    .name   = "aml_keys",
    .id = -1,
    .dev = {
                .platform_data = &secure_device[0],
           },
};
#endif

/***********************************************************************
 * Bluetooth  Section
 **********************************************************************/
#ifdef CONFIG_BT_DEVICE
#include <linux/bt-device.h>

//Low powermode----------------
#define BT2AP_WAKE PAD_GPIOAO_2
#define AP2BT_WAKE PAD_GPIOD_4
#define BT_RST     PAD_GPIOD_6

static struct resource bluesleep_resources[] = {
    {
        .name   = "gpio_host_wake",
        .start  = BT2AP_WAKE,
        .end    = BT2AP_WAKE,
        .flags  = IORESOURCE_IO,
    },
    {
        .name   = "gpio_ext_wake",
        .start  = AP2BT_WAKE,
        .end    = AP2BT_WAKE,
        .flags  = IORESOURCE_IO,
    },
};

static struct platform_device brcm_bluesleep_device = {
    .name = "bluesleep",
    .id     = -1,
    .num_resources  = ARRAY_SIZE(bluesleep_resources),
    .resource   = bluesleep_resources,
};

static struct platform_device bt_device = {
    .name     = "bt-dev",
    .id       = -1,
};

static pinmux_item_t pcm_pins[] = {
    {
        .reg = PINMUX_REG(3),
        .setmask = 0xf << 27
    },
    PINMUX_END_ITEM
};

static pinmux_set_t pcm_pinmux_set = {
    .chip_select = NULL,
    .pinmux = &pcm_pins[0],
};

static void bt_device_init(void)
{
    /* BT_RST_N */
    gpio_out(BT_RST, 1);

    pinmux_set(&pcm_pinmux_set); 

    //set pull-up for BT_WAKE
    aml_clr_reg32_mask(P_PAD_PULL_UP_REG2, 1<<20);
    /* BT_WAKE */
    gpio_out(AP2BT_WAKE, 0);
}

static void bt_device_on(void)
{
    /* BT_RST_N */
    gpio_out(BT_RST, 1);
    msleep(20);
    gpio_out(BT_RST, 0);
    msleep(5);
    /* BT_WAKE */
    gpio_out(AP2BT_WAKE, 1);
}

static void bt_device_off(void)
{
    /* BT_RST_N */
    gpio_out(PAD_GPIOD_6, 1);
    /* BT_WAKE */
    gpio_out(AP2BT_WAKE, 0);
}

static void bt_device_suspend(void)
{
    /* BT_WAKE */
    gpio_out(AP2BT_WAKE, 0);
}

static void bt_device_resume(void)
{
    /* BT_WAKE */
    gpio_out(AP2BT_WAKE, 1);
}

struct bt_dev_data bt_dev = {
    .bt_dev_init    = bt_device_init,
    .bt_dev_on      = bt_device_on,
    .bt_dev_off     = bt_device_off,
    .bt_dev_suspend = bt_device_suspend,
    .bt_dev_resume  = bt_device_resume,
};
#endif

/***********************************************************************
 * Card Reader Section
 **********************************************************************/
#ifdef CONFIG_CARDREADER
static struct resource meson_card_resource[] = {
    [0] = {
        .start = 0x1200230,   //physical address
        .end   = 0x120024c,
        .flags = 0x200,
    }
};
static struct aml_card_info meson_card_info[] = {
    {
        .name           = "sd_card",
        .work_mode      = CARD_HW_MODE,
        .io_pad_type        = SDHC_CARD_0_5,
        .card_ins_en_reg    = CARD_GPIO_ENABLE,
        .card_ins_en_mask   = PREG_IO_29_MASK,
        .card_ins_input_reg = CARD_GPIO_INPUT,
        .card_ins_input_mask    = PREG_IO_29_MASK,
        .card_power_en_reg  = CARD_GPIO_ENABLE,
        .card_power_en_mask = PREG_IO_31_MASK,
        .card_power_output_reg  = CARD_GPIO_OUTPUT,
        .card_power_output_mask = PREG_IO_31_MASK,
        .card_power_en_lev  = 0,
        .card_wp_en_reg     = 0,
        .card_wp_en_mask    = 0,
        .card_wp_input_reg  = 0,
        .card_wp_input_mask = 0,
        .card_extern_init   = 0,
    },
#ifdef CONFIG_AM_WIFI
    {
        .name           = "sdio_card",
        .work_mode      = CARD_HW_MODE,
        .io_pad_type        = SDHC_GPIOX_0_9,
        .card_ins_en_reg    = 0,
        .card_ins_en_mask   = 0,
        .card_ins_input_reg = 0,
        .card_ins_input_mask    = 0,
        .card_power_en_reg  = 0,
        .card_power_en_mask = 0,
        .card_power_output_reg  = 0,
        .card_power_output_mask = 0,
        .card_power_en_lev  = 0,
        .card_wp_en_reg     = 0,
        .card_wp_en_mask    = 0,
        .card_wp_input_reg  = 0,
        .card_wp_input_mask = 0,
        .card_extern_init   = 0,
    },
#endif
#ifdef CONFIG_INAND 
    {
        .name = "inand_card",
        .work_mode = CARD_HW_MODE,
        .io_pad_type = SDHC_BOOT_0_11,
        .card_ins_en_reg = 0,
        .card_ins_en_mask = 0,
        .card_ins_input_reg = 0,
        .card_ins_input_mask = 0,
        .card_power_en_reg = 0,
        .card_power_en_mask = 0,
        .card_power_output_reg = 0,
        .card_power_output_mask = 0,
        .card_power_en_lev = 0,
        .card_wp_en_reg = 0,
        .card_wp_en_mask = 0,
        .card_wp_input_reg = 0,
        .card_wp_input_mask = 0,
        .card_extern_init = 0,
        .partitions = normal_partition_info,
        .nr_partitions = ARRAY_SIZE(normal_partition_info),
    },
#ifdef CONFIG_INAND_LP
    {
        .name = "inand_card_lp",
        .work_mode = CARD_HW_MODE,
        .io_pad_type = SDHC_BOOT_0_11,
        .card_ins_en_reg = 0,
        .card_ins_en_mask = 0,
        .card_ins_input_reg = 0,
        .card_ins_input_mask = 0,
        .card_power_en_reg = 0,
        .card_power_en_mask = 0,
        .card_power_output_reg = 0,
        .card_power_output_mask = 0,
        .card_power_en_lev = 0,
        .card_wp_en_reg = 0,
        .card_wp_en_mask = 0,
        .card_wp_input_reg = 0,
        .card_wp_input_mask = 0,
        .card_extern_init = 0,
        .partitions = normal_partition_info,
        .nr_partitions = ARRAY_SIZE(normal_partition_info),
	},
#endif
#endif
};

static struct aml_card_platform meson_card_platform = {
    .card_num   = ARRAY_SIZE(meson_card_info),
    .card_info  = meson_card_info,
};

static struct platform_device meson_card_device = {
    .name       = "AMLOGIC_CARD",
    .id     = -1,
    .num_resources  = ARRAY_SIZE(meson_card_resource),
    .resource   = meson_card_resource,
    .dev = {
        .platform_data = &meson_card_platform,
    },
};
#endif // CONFIG_CARDREADER

/***********************************************************************
 * IO Mapping
 **********************************************************************/
/*
#define IO_CBUS_BASE        0xf1100000  ///2M
#define IO_AXI_BUS_BASE     0xf1300000  ///1M
#define IO_PL310_BASE       0xf2200000  ///4k
#define IO_PERIPH_BASE      0xf2300000  ///4k
#define IO_APB_BUS_BASE     0xf3000000  ///8k
#define IO_DOS_BUS_BASE     0xf3010000  ///64k
#define IO_AOBUS_BASE       0xf3100000  ///1M
#define IO_USB_A_BASE       0xf3240000  ///256k
#define IO_USB_B_BASE       0xf32C0000  ///256k
#define IO_WIFI_BASE        0xf3300000  ///1M
#define IO_SATA_BASE        0xf3400000  ///64k
#define IO_ETH_BASE         0xf3410000  ///64k

#define IO_SPIMEM_BASE      0xf4000000  ///64M
#define IO_A9_APB_BASE      0xf8000000  ///256k
#define IO_DEMOD_APB_BASE   0xf8044000  ///112k
#define IO_MALI_APB_BASE    0xf8060000  ///128k
#define IO_APB2_BUS_BASE    0xf8000000
#define IO_AHB_BASE         0xf9000000  ///128k
#define IO_BOOTROM_BASE     0xf9040000  ///64k
#define IO_SECBUS_BASE      0xfa000000
#define IO_EFUSE_BASE       0xfa000000  ///4k
*/
static __initdata struct map_desc meson_io_desc[] = {
    {
        .virtual    = IO_CBUS_BASE,
        .pfn        = __phys_to_pfn(IO_CBUS_PHY_BASE),
        .length     = SZ_2M,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_AXI_BUS_BASE,
        .pfn        = __phys_to_pfn(IO_AXI_BUS_PHY_BASE),
        .length     = SZ_1M,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_PL310_BASE,
        .pfn        = __phys_to_pfn(IO_PL310_PHY_BASE),
        .length     = SZ_4K,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_PERIPH_BASE,
        .pfn        = __phys_to_pfn(IO_PERIPH_PHY_BASE),
        .length     = SZ_1M,
        .type       = MT_DEVICE,
    } , {
           .virtual    = IO_APB_BUS_BASE,
           .pfn        = __phys_to_pfn(IO_APB_BUS_PHY_BASE),
           .length     = SZ_1M,
           .type       = MT_DEVICE,
       } , /*{

           .virtual    = IO_DOS_BUS_BASE,
           .pfn        = __phys_to_pfn(IO_DOS_BUS_PHY_BASE),
           .length     = SZ_64K,
           .type       = MT_DEVICE,
       } , */{
           .virtual    = IO_AOBUS_BASE,
        .pfn        = __phys_to_pfn(IO_AOBUS_PHY_BASE),
        .length     = SZ_1M,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_AHB_BUS_BASE,
        .pfn        = __phys_to_pfn(IO_AHB_BUS_PHY_BASE),
        .length     = SZ_8M,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_SPIMEM_BASE,
        .pfn        = __phys_to_pfn(IO_SPIMEM_PHY_BASE),
        .length     = SZ_64M,
        .type       = MT_ROM,
    } , {
        .virtual    = IO_APB2_BUS_BASE,
        .pfn        = __phys_to_pfn(IO_APB2_BUS_PHY_BASE),
        .length     = SZ_512K,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_AHB_BASE,
        .pfn        = __phys_to_pfn(IO_AHB_PHY_BASE),
        .length     = SZ_128K,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_BOOTROM_BASE,
        .pfn        = __phys_to_pfn(IO_BOOTROM_PHY_BASE),
        .length     = SZ_64K,
        .type       = MT_DEVICE,
    } , {
        .virtual    = IO_SECBUS_BASE,
        .pfn        = __phys_to_pfn(IO_SECBUS_PHY_BASE),
        .length     = SZ_4K,
        .type       = MT_DEVICE,
    }, {
        .virtual    = IO_SECURE_BASE,
        .pfn        = __phys_to_pfn(IO_SECURE_PHY_BASE),
        .length     = SZ_16K,
        .type       = MT_DEVICE,
    }, {
        .virtual    = PAGE_ALIGN(__phys_to_virt(RESERVED_MEM_START)),
        .pfn        = __phys_to_pfn(RESERVED_MEM_START),
        .length     = RESERVED_MEM_END - RESERVED_MEM_START + 1,
        .type       = MT_MEMORY_NONCACHED,
    },
#ifdef CONFIG_MESON_SUSPEND
        {
        .virtual    = PAGE_ALIGN(__phys_to_virt(0x9ff00000)),
        .pfn        = __phys_to_pfn(0x9ff00000),
        .length     = SZ_1M,
        .type       = MT_MEMORY_NONCACHED,
        },
#endif

};

static void __init meson_map_io(void)
{
    iotable_init(meson_io_desc, ARRAY_SIZE(meson_io_desc));
}

static void __init meson_fixup(struct machine_desc *mach, struct tag *tag, char **cmdline, struct meminfo *m)
{
    struct membank *pbank;
    mach->video_start    = RESERVED_MEM_START;
    mach->video_end      = RESERVED_MEM_END;

    m->nr_banks = 0;
    pbank = &m->bank[m->nr_banks];
    pbank->start = PAGE_ALIGN(PHYS_MEM_START);
    pbank->size  = SZ_64M & PAGE_MASK;
    m->nr_banks++;
    pbank = &m->bank[m->nr_banks];
    pbank->start = PAGE_ALIGN(RESERVED_MEM_END + 1);
#ifdef CONFIG_MESON_SUSPEND
    pbank->size  = (PHYS_MEM_END-RESERVED_MEM_END-SZ_1M) & PAGE_MASK;
#else
    pbank->size  = (PHYS_MEM_END-RESERVED_MEM_END) & PAGE_MASK;
#endif
    m->nr_banks++;
}

/***********************************************************************
 *USB Setting section
 **********************************************************************/
static void set_usb_a_vbus_power(char is_power_on)
{
    if(is_power_on)
        gpio_out(PAD_GPIOD_7, 1);
    else
        gpio_out(PAD_GPIOD_7, 0);
}

static  int __init setup_usb_devices(void)
{
    struct lm_device * usb_ld_a, *usb_ld_b;
    usb_ld_a = alloc_usb_lm_device(USB_PORT_IDX_A);
    usb_ld_b = alloc_usb_lm_device (USB_PORT_IDX_B);
    usb_ld_a->param.usb.set_vbus_power = set_usb_a_vbus_power;
    usb_ld_b->param.usb.port_type = USB_PORT_TYPE_HOST;
    lm_device_register(usb_ld_a);
    lm_device_register(usb_ld_b);

    //Set VBUSA_OC pin
    if(board_ver == 2)
        gpio_set_status(PAD_GPIOD_8, gpio_status_in);
    else
        gpio_set_status(PAD_GPIOE_9, gpio_status_in);
    return 0;
}

/***********************************************************************/
#ifdef CONFIG_EFUSE
static bool efuse_data_verify(unsigned char *usid)
{  int len;

    len = strlen(usid);    
    if((len > 0)&&(len<48) )
        return true;
    else
        return false;
}

static struct efuse_platform_data aml_efuse_plat = {
    .pos = 454,
    .count = 48,
    .data_verify = efuse_data_verify,
};

static struct platform_device aml_efuse_device = {
    .name   = "efuse",
    .id = -1,
    .dev = {
                .platform_data = &aml_efuse_plat,
           },
};

// BSP EFUSE layout setting
static  efuseinfo_item_t aml_efuse_setting[] = {
	// usid layout can be defined by customer
	{
		.title = "usid",   
		.id = EFUSE_USID_ID,
		.offset = 454,      // customer can modify the offset which must >= 454
		.enc_len = 48,     // customer can modify the encode length by self must <=58
		.data_len = 48,	// customer can modify the data length by self must <=58
		.bch_en = 0,		// customer can modify do bch or not
		.bch_reverse = 0,
	},	
	// customer also can add new EFUSE item to expand, but must be correct and bo conflict
};

static int aml_efuse_getinfoex_byID(unsigned param, efuseinfo_item_t *info)
{
	unsigned num = sizeof(aml_efuse_setting)/sizeof(efuseinfo_item_t);
	int i=0;
	int ret = -1;
	for(i=0; i<num; i++){
		if(aml_efuse_setting[i].id == param){
				strcpy(info->title, aml_efuse_setting[i].title);				
				info->offset = aml_efuse_setting[i].offset;
				info->id = aml_efuse_setting[i].id;				
				info->data_len = aml_efuse_setting[i].data_len;			
				info->enc_len = aml_efuse_setting[i].enc_len;
				info->bch_en = aml_efuse_setting[i].bch_en;
				info->bch_reverse = aml_efuse_setting[i].bch_reverse;					
				ret = 0;
				break;
		}
	}
	return ret;
}

static int aml_efuse_getinfoex_byPos(unsigned param, efuseinfo_item_t *info)
{
	unsigned num = sizeof(aml_efuse_setting)/sizeof(efuseinfo_item_t);
	int i=0;
	int ret = -1;
	for(i=0; i<num; i++){
		if(aml_efuse_setting[i].offset == param){
				strcpy(info->title, aml_efuse_setting[i].title);				
				info->offset = aml_efuse_setting[i].offset;
				info->id = aml_efuse_setting[i].id;				
				info->data_len = aml_efuse_setting[i].data_len;			
				info->enc_len = aml_efuse_setting[i].enc_len;
				info->bch_en = aml_efuse_setting[i].bch_en;
				info->bch_reverse = aml_efuse_setting[i].bch_reverse;					
				ret = 0;
				break;
		}
	}
	return ret;	
}

	extern pfn efuse_getinfoex;
	extern pfn efuse_getinfoex_byPos;
static void setup_aml_efuse(void)
{
	efuse_getinfoex = aml_efuse_getinfoex_byID;
	efuse_getinfoex_byPos = aml_efuse_getinfoex_byPos;
}	
#endif

#if defined(CONFIG_AML_RTC)
static  struct platform_device aml_rtc_device = {
            .name            = "aml_rtc",
            .id               = -1,
    };
#endif
#if defined(CONFIG_SUSPEND)
static void m6ref_set_vccx2(int power_on)
{
    if (power_on) {
        //restore_pinmux();
        set_usb_a_vbus_power(1);
        printk(KERN_INFO "%s() Power ON\n", __FUNCTION__);
    }
    else {
        printk(KERN_INFO "%s() Power OFF\n", __FUNCTION__);
        //save_pinmux();
        set_usb_a_vbus_power(0);
    }
}

static struct meson_pm_config aml_pm_pdata = {
    .pctl_reg_base = (void *)IO_APB_BUS_BASE,
    .mmc_reg_base = (void *)APB_REG_ADDR(0x1000),
    .hiu_reg_base = (void *)CBUS_REG_ADDR(0x1000),
    .power_key = (1<<8),
    .ddr_clk = 0x00110820,
    .sleepcount = 128,
    .set_vccx2 = m6ref_set_vccx2,
    .core_voltage_adjust = 7,  //5,8
};

static struct platform_device aml_pm_device = {
    .name           = "pm-meson",
    .dev = {
        .platform_data  = &aml_pm_pdata,
    },
    .id             = -1,
};
#endif

#if defined (CONFIG_AML_HDMI_TX)
static void hdmi_5v_ctrl(unsigned int pwr)
{
    if(pwr){
        printk("HDMI 5V Power On\n");
        gpio_out(PAD_GPIOC_14, 1);
    }
    else{
        printk("HDMI 5V Power Off\n");
        gpio_out(PAD_GPIOC_14, 0);
    }
}

static struct hdmi_phy_set_data brd_phy_data[] = {
    {27, 0x16, 0x30},   // 480i/p 576i/p
    {74, 0x16, 0x40},   // 720p 1080i
    {148, 0x16, 0x40},  // 1080p
    {-1,   -1},         //end of phy setting
};

static struct vendor_info_data vendor_data = {
    .vendor_name = "XiaoMi",               // Max Chars: 8
    .vendor_id = 0x000000,                  // Refer to http://standards.ieee.org/develop/regauth/oui/oui.txt
    .product_desc = "MDZ-06-AA",         // Max Chars: 16
    .cec_osd_string = "XiaoMi Box 1S",                 // Max Chars: 14
};

static struct hdmi_config_platform_data aml_hdmi_pdata = {
    .hdmi_5v_ctrl = hdmi_5v_ctrl,
    .hdmi_3v3_ctrl = NULL,
    .hdmi_pll_vdd_ctrl = NULL,
    .phy_data = brd_phy_data,
    .vend_data = &vendor_data,
};
#endif
/***********************************************************************
 * Meson CS DCDC section
 **********************************************************************/
#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
#include <linux/regulator/meson_cs_dcdc_regulator.h>
#include <linux/regulator/machine.h>
static struct regulator_consumer_supply vcck_data[] = {
    {
        .supply = "vcck-armcore",
    },
};

static struct regulator_init_data vcck_init_data = {
    .constraints = { /* VCCK default 1.2V */
        .name = "vcck",
        .min_uV =  1000000,
        .max_uV =  1400000,
        .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
    },
    .num_consumer_supplies = ARRAY_SIZE(vcck_data),
    .consumer_supplies = vcck_data,
};

// pwm duty for vcck voltage
static unsigned int vcck_pwm_table[MESON_CS_MAX_STEPS] = {
	0x040018, 0x050017, 0x060016, 0x070015, 
	0x080014, 0x090013, 0x0a0012, 0x0b0011, 
	0x0c0010, 0x0d000f, 0x0e000e, 0x0f000d, 
	0x10000c, 0x11000b, 0x12000a, 0x130009,  
};
static int get_voltage(void) {
//    printk("***vcck: get_voltage");
    int i;
    unsigned int reg = aml_read_reg32(P_PWM_PWM_C);
    for(i=0; i<MESON_CS_MAX_STEPS; i++) {
        if(reg == vcck_pwm_table[i])
	     break;
    }
    if(i >= MESON_CS_MAX_STEPS)
        return -1;
    else 
        return i;
}

static int set_voltage(unsigned int level) {
//    printk("***vcck: set_voltage");
    aml_write_reg32(P_PWM_PWM_C, vcck_pwm_table[level]);
    return 0;
}

static pinmux_item_t vcck_pwm_pins[] ={
    {
        .reg = PINMUX_REG(2),
        .setmask = 1<<2,
    },
    PINMUX_END_ITEM
};

static pinmux_set_t vcck_pwm_set = {
    .chip_select = NULL,
    .pinmux = &vcck_pwm_pins[0]
};

static void vcck_pwm_init(void) {
    printk("***vcck: vcck_pwm_init");
    //enable pwm clk & pwm output
    aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f << 8)) | ((1 << 15) | (PWM_PRE_DIV << 8) | (1 << 0)));
    aml_write_reg32(P_PWM_PWM_C, vcck_pwm_table[0]);
#if 0
    //enable pwm_C pinmux    1<<3 pwm_D
    aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1 << 2));
#endif
    pinmux_set(&vcck_pwm_set);
}
//1.0V   <800MHz
//1.1V   800MHz~9xxMHz
//1.2V   1GHz~1.1xGHz
//1.3V   1.2GHz~1.37GHz
//1.38V  1.39~1.5GHz
static struct meson_cs_pdata_t vcck_pdata = {
    .meson_cs_init_data = &vcck_init_data,
    .voltage_step_table = {
        1320000, 1300000, 1280000, 1270000,
        1240000, 1220000, 1210000, 1190000,
        1170000, 1150000, 1130000, 1110000,
        1090000, 1070000, 1050000, 1010000,
    },
    .default_uV = 1280000,
    .get_voltage = get_voltage,
    .set_voltage = set_voltage,
};

struct meson_opp vcck_opp_table[] = {
    /* freq must be in descending order */
    {
        .freq   = 1500000,
        .min_uV = 1320000,
        .max_uV = 1320000,
    },
    {
        .freq   = 1320000,
        .min_uV = 1320000,
        .max_uV = 1320000,
    },
    {
        .freq   = 1200000,
        .min_uV = 1240000,
        .max_uV = 1240000,
    },
    {
        .freq   = 1080000,
        .min_uV = 1220000,
        .max_uV = 1220000,
    },
    {
        .freq   = 1000000,
        .min_uV = 1110000,
        .max_uV = 1110000,
    },
    {
        .freq   = 984000,
        .min_uV = 1110000,
        .max_uV = 1110000,
    },
    {
        .freq   = 840000,
        .min_uV = 1110000,
        .max_uV = 1110000,
    },
    {
        .freq   = 816000,
        .min_uV = 1110000,
        .max_uV = 1110000,
    },
    {
        .freq   = 792000,
        .min_uV = 1010000,
        .max_uV = 1010000,
    },
    {
        .freq   = 600000,
        .min_uV = 1010000,
        .max_uV = 1010000,
    },
    {
        .freq   = 200000,
        .min_uV = 1010000,
      	.max_uV = 1010000,
    }
 };

static struct platform_device meson_cs_dcdc_regulator_device = {
    .name = "meson-cs-regulator",
    .dev = {
        .platform_data = &vcck_pdata,
    }
};
#endif

/***********************************************************************
 * Meson AO/EE DCDC section
 **********************************************************************/
#ifdef CONFIG_AOEE_DCDC
struct aoee_dcdc_pdata_t {
	int voltage_step_table[MESON_CS_MAX_STEPS];
	int default_uV;
	int min_uV;
	int max_uV;
};

static struct aoee_dcdc_pdata_t aoee_pdata = {
    .voltage_step_table = {
        1251530, 1237414, 1223297, 1209181,
        1195064, 1180948, 1166831, 1152715,
        1138598, 1124482, 1110365, 1096249,
        1082132, 1068016, 1053899, 1039783,
    },
    .default_uV = 1195064,
    .min_uV = 1000000,
    .max_uV = 1260000,
};

static struct platform_device aoee_dcdc_device = {
    .name = "aoee-dcdc",
    .dev = {
        .platform_data = &aoee_pdata,
    }
};
#endif

/***********************************************************************
 * Meson CPUFREQ section
 **********************************************************************/
#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#include <plat/cpufreq.h>

#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
static struct regulator *vcck;
static struct meson_cpufreq_config cpufreq_info;

static unsigned int vcck_cur_max_freq(void)
{
    return meson_vcck_cur_max_freq(vcck, vcck_opp_table, ARRAY_SIZE(vcck_opp_table));
}

static int vcck_scale(unsigned int frequency)
{
    return meson_vcck_scale(vcck, vcck_opp_table, ARRAY_SIZE(vcck_opp_table), frequency);
}

static int vcck_regulator_init(void)
{
    vcck = regulator_get(NULL, vcck_data[0].supply);
    if (WARN(IS_ERR(vcck), "Unable to obtain voltage regulator for vcck;"
                    " voltage scaling unsupported\n")) {
        return PTR_ERR(vcck);
    }

    return 0;
}

static struct meson_cpufreq_config cpufreq_info = {
    .freq_table = NULL,
    .init = vcck_regulator_init,
    .cur_volt_max_freq = vcck_cur_max_freq,
    .voltage_scale = vcck_scale,
};
#endif //CONFIG_MESON_CS_DCDC_REGULATOR


static struct platform_device meson_cpufreq_device = {
    .name   = "cpufreq-meson",
    .dev = {
#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
        .platform_data = &cpufreq_info,
#else
        .platform_data = NULL,
#endif
    },
    .id = -1,
};
#endif //CONFIG_CPU_FREQ

#ifdef CONFIG_SARADC_AM
#include <linux/saradc.h>
static struct platform_device saradc_device = {
    .name = "saradc",
    .id = 0,
    .dev = {
        .platform_data = NULL,
    },
};
#endif

/***********************************************************************
 * Power Key Section
 **********************************************************************/


#if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
#include <linux/input.h>
#include <linux/input/key_input.h>

static int _key_code_list[] = {KEY_POWER};

static inline int key_input_init_func(void)
{
    WRITE_AOBUS_REG(AO_RTC_ADDR0, (READ_AOBUS_REG(AO_RTC_ADDR0) &~(1<<11)));
    WRITE_AOBUS_REG(AO_RTC_ADDR1, (READ_AOBUS_REG(AO_RTC_ADDR1) &~(1<<3)));
    return 0;
}
static inline int key_scan(void* data)
{
    int *key_state_list = (int*)data;
    int ret = 0;
    key_state_list[0] = ((READ_AOBUS_REG(AO_RTC_ADDR1) >> 2) & 1) ? 0 : 1;
    return ret;
}

static  struct key_input_platform_data  key_input_pdata = {
    .scan_period = 20,
    .fuzz_time = 60,
    .key_code_list = &_key_code_list[0],
    .key_num = ARRAY_SIZE(_key_code_list),
    .scan_func = key_scan,
    .init_func = key_input_init_func,
    .config = 0,
};

static struct platform_device input_device_key = {
    .name = "meson-keyinput",
    .id = 0,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &key_input_pdata,
    }
};
#endif



/***********************************************************************
 * Audio section
 **********************************************************************/
static struct resource aml_m6_audio_resource[] = {
    [0] =   {
        .start      =   0,
        .end        =   0,
        .flags      =   IORESOURCE_MEM,
    },
};

static struct platform_device aml_audio = {
    .name           = "aml-audio",
    .id             = 0,
};

static struct platform_device aml_audio_dai = {
    .name           = "aml-dai",
    .id             = 0,
};
#ifdef CONFIG_SND_AML_M6_PCM2BT
static struct platform_device aml_pcm2bt = {
    .name           = "pcm2bt",
    .id             = 0,
};
#endif

#if  defined(CONFIG_AM_TV_OUTPUT2)
static struct resource vout2_device_resources[] = {
    [0] = {
        .start = 0,
        .end   = 0,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device vout2_device = {
    .name       = "mesonvout2",
    .id         = 0,
    .num_resources = ARRAY_SIZE(vout2_device_resources),
    .resource      = vout2_device_resources,
};
#endif

#if defined(CONFIG_SND_SOC_DUMMY_CODEC)
static pinmux_item_t dummy_codec_pinmux[] = {
    /* I2S_MCLK I2S_BCLK I2S_LRCLK I2S_DOUT */
    {
        .reg = PINMUX_REG(9),
        .setmask = (1 << 7) | (1 << 5) | (1 << 9) | (1 << 4),
        .clrmask = (7 << 19) | (7 << 1) | (3 << 10) | (1 << 6),
    },
    {
        .reg = PINMUX_REG(8),
        .clrmask = (0x7f << 24),
    },
    /* spdif out from GPIOC_9 */
    { 
	.reg = PINMUX_REG(3),
	.setmask = (1<<24),
    },
    /* mask spdif out from GPIOE_8 */
    {
	.reg = PINMUX_REG(9),
	.clrmask = (1<<0),
    },
    PINMUX_END_ITEM
};

static pinmux_set_t dummy_codec_pinmux_set = {
    .chip_select = NULL,
    .pinmux = &dummy_codec_pinmux[0],
};

static void dummy_codec_device_init(void)
{
    /* audio pinmux */
    pinmux_set(&dummy_codec_pinmux_set);
    /* set mute pin */
    gpio_out(PAD_GPIOD_9, 1); //set un mute as init
    /* set DEMP on */
    if(board_ver == 2)
        gpio_out(PAD_GPIOX_10, 1);
    else
        gpio_out(PAD_GPIOD_8, 1);
}

static void dummy_codec_device_deinit(void)
{
    pinmux_clr(&dummy_codec_pinmux_set);
}

#if 0
/* mute external amplifier through GPIO */
static void dummy_codec_mute_speaker(int mute)
{
    printk("@@%s###(mute=%d)\n", __func__, mute);
    if(mute){
        gpio_out(PAD_GPIOD_9, 0); //mute
    }else{
        gpio_out(PAD_GPIOD_9, 1); //unmute
    }
}
#endif

static struct dummy_codec_platform_data dummy_codec_pdata = {
    .device_init    = dummy_codec_device_init,
    .device_uninit  = dummy_codec_device_deinit,
    .mute_spk       = NULL,
    //.mute_spk       = dummy_codec_mute_speaker,
};

static struct platform_device aml_dummy_codec_audio = {
    .name           = "aml_dummy_codec_audio",
    .id             = 0,
    .resource       = aml_m6_audio_resource,
    .num_resources  = ARRAY_SIZE(aml_m6_audio_resource),
    .dev = {
        .platform_data = &dummy_codec_pdata,
    },
};

static struct platform_device aml_dummy_codec = {
    .name           = "dummy_codec",
    .id             = 0,
};
#endif

#if defined(CONFIG_AM_DEINTERLACE) || defined (CONFIG_DEINTERLACE)
static struct resource deinterlace_resources[] = {
    [0] = {
        .start =  DI_ADDR_START,
        .end   = DI_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device deinterlace_device = {
    .name       = "deinterlace",
    .id         = 0,
    .num_resources = ARRAY_SIZE(deinterlace_resources),
    .resource      = deinterlace_resources,
};
#endif

/***********************************************************************
 * Device Register Section
 **********************************************************************/
static struct platform_device  *platform_devs[] = {
    &aml_uart_device,
#ifdef CONFIG_AM_ETHERNET
    &meson_device_eth,
#endif
    &meson_device_fb,
    &meson_device_vout,
#ifdef CONFIG_AM_STREAMING
    &meson_device_codec,
#endif
#if defined(CONFIG_AM_NAND)
    &aml_nand_device,
#endif
#if defined(CONFIG_TVIN_VDIN)
    &vdin_device,
#endif
#ifdef CONFIG_SARADC_AM
        &saradc_device,
#endif
#if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
    &input_device_key,
#endif
#if defined(CONFIG_CARDREADER)
    &meson_card_device,
#endif // CONFIG_CARDREADER
#if defined(CONFIG_SUSPEND)
    &aml_pm_device,
#endif
#ifdef CONFIG_EFUSE
    &aml_efuse_device,
#endif
#if defined(CONFIG_AML_RTC)
    &aml_rtc_device,
#endif
    &aml_audio,
    &aml_audio_dai,
#ifdef CONFIG_SND_AML_M6_PCM2BT
    &aml_pcm2bt,
#endif
#if defined(CONFIG_SND_SOC_DUMMY_CODEC)
    &aml_dummy_codec_audio,
    &aml_dummy_codec,
#endif
#ifdef CONFIG_AM_REMOTE
    &meson_device_remote,
#endif
#if defined(CONFIG_AMLOGIC_SPI_NOR)
    &amlogic_spi_nor_device,
#endif
#ifdef CONFIG_POST_PROCESS_MANAGER
	&ppmgr_device,
#endif
#ifdef CONFIG_FREE_SCALE
        &freescale_device,
#endif

#if defined(CONFIG_AM_DEINTERLACE) || defined (CONFIG_DEINTERLACE)
    &deinterlace_device,
#endif

#if defined(CONFIG_AM_TV_OUTPUT2)
    &vout2_device,   
#endif

#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
	&meson_cs_dcdc_regulator_device,
#endif
#ifdef CONFIG_AOEE_DCDC
	&aoee_dcdc_device,
#endif
#ifdef CONFIG_CPU_FREQ
	&meson_cpufreq_device,
#endif
#if defined(CONFIG_AML_EMMC_KEY) || defined(CONFIG_AML_NAND_KEY)
	&aml_keys_device,
#endif
#ifdef CONFIG_BT_DEVICE
    &bt_device,
    &brcm_bluesleep_device,
#endif
#ifdef CONFIG_USB_G_ANDROID
    &android_usb_device,
#endif
};


#ifdef CONFIG_AML_HDMI_TX
extern int setup_hdmi_dev_platdata(void* platform_data);
#endif
static __init void meson_init_machine(void)
{
    if(gpio_get_val(PAD_GPIOA_3))
        board_ver = 3;
    printk("---------------board_ver = %d -------------\n", board_ver);

#ifdef CONFIG_BT_SLEEP
   if(aml_uart_plat.rts_control_pin)
        gpio_out(aml_uart_plat.rts_control_pin, 1);
   gpio_out(PAD_GPIOA_11, 1);
#endif
#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
    vcck_pwm_init();
#endif
#ifdef CONFIG_AM_WIFI
    wifi_clock_enable();
    wifi_gpio_init();
#endif
#ifdef CONFIG_AM_ETHERNET
    setup_eth_device();
#endif
#ifdef CONFIG_AML_HDMI_TX
    setup_hdmi_dev_platdata(&aml_hdmi_pdata);
#endif
#ifdef CONFIG_AM_REMOTE
	setup_remote_device();
#endif
#ifdef CONFIG_EFUSE
	setup_aml_efuse();	
#endif
    setup_usb_devices();
    setup_devices_resource();
    platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));
#if defined(CONFIG_SUSPEND)
    {//todo: remove it after verified. need set it in uboot environment variable.
    extern  int console_suspend_enabled;
    console_suspend_enabled = 0;
    }
#endif

}
static __init void meson_init_early(void)
{///boot seq 1

}

MACHINE_START(MESON6_G19, "Amlogic Meson6 g19 customer platform")
    .boot_params    = BOOT_PARAMS_OFFSET,
    .map_io         = meson_map_io,///2
    .init_early     = meson_init_early,///3
    .init_irq       = meson_init_irq,///0
    .timer          = &meson_sys_timer,
    .init_machine   = meson_init_machine,
    .fixup          = meson_fixup,///1
MACHINE_END
