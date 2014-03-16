/*
 * customer/boards/board-m6g35-v131.c
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
#include <mach/gpio_data.h>
#include <mach/pinmux.h>
#include <mach/voltage.h>
#include <linux/uart-aml.h>
#include <linux/i2c-aml.h>
#include <linux/syscore_ops.h>

#include "board-m6g35-v131.h"

#ifdef CONFIG_MMC_AML
#include <mach/mmc.h>
#endif

#ifdef CONFIG_CARDREADER
#include <mach/card_io.h>
#endif // CONFIG_CARDREADER

#include <mach/gpio.h>


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
#ifdef CONFIG_MESON_TRUSTZONE
#include <mach/meson-secure.h>
#endif


/***********************************************************************
 * IO Mapping
 **********************************************************************/
#define PHYS_MEM_START      (0x80000000)
#define PHYS_MEM_SIZE       (1024*SZ_1M)
#define PHYS_MEM_END        (PHYS_MEM_START + PHYS_MEM_SIZE -1 )

#define PWM_PRE_DIV         0   //pwm_freq = 24M / (pre_div + 1) / PWM_MAX

/******** Reserved memory setting ************************/
#define RESERVED_MEM_START  (0x80000000+64*SZ_1M)   /*start at the second 64M*/

/******** CODEC memory setting ************************/
//  Codec need 16M for 1080p decode
//  4M for sd decode;
#define ALIGN_MSK           ((SZ_1M)-1)
#define U_ALIGN(x)          ((x+ALIGN_MSK)&(~ALIGN_MSK))
#define D_ALIGN(x)          ((x)&(~ALIGN_MSK))

/******** AUDIODSP memory setting ************************/
#define AUDIODSP_ADDR_START U_ALIGN(RESERVED_MEM_START)     /*audiodsp memstart*/
#define AUDIODSP_ADDR_END   (AUDIODSP_ADDR_START+SZ_1M-1)   /*audiodsp memend*/

/******** Frame buffer memory configuration ***********/
#define OSD_480_PIX         (640*480)
#define OSD_576_PIX         (768*576)
#define OSD_720_PIX         (1280*720)
#define OSD_1080_PIX        (1920*1080)
#define OSD_PANEL_PIX       (800*480)
#define B16BpP              (2)
#define B32BpP              (4)
#define DOUBLE_BUFFER       (2)

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
    #define CODEC_MEM_SIZE  U_ALIGN(32*SZ_1M)
#else
    #define CODEC_MEM_SIZE  U_ALIGN(16*SZ_1M)
#endif

#define CODEC_ADDR_START    U_ALIGN(OSD2_ADDR_END)
#define CODEC_ADDR_END      (CODEC_ADDR_START+CODEC_MEM_SIZE-1)

/********VDIN memory configuration ***************/
#ifdef CONFIG_TVIN_VIUIN
#define VDIN_MEM_SIZE       (SZ_1M*15)
#define VDIN_ADDR_START     U_ALIGN(CODEC_ADDR_END)
#define VDIN_ADDR_END       (VDIN_ADDR_START +VDIN_MEM_SIZE -1)
#else
#define VDIN_ADDR_START     U_ALIGN(CODEC_ADDR_END)
#define VDIN_ADDR_END       (VDIN_ADDR_START + CODEC_MEM_SIZE - 1)
#endif

#if defined(CONFIG_AMLOGIC_VIDEOIN_MANAGER)
#define VM_SIZE             (SZ_1M*16)
#else
#define VM_SIZE             (1)
#endif /* CONFIG_AMLOGIC_VIDEOIN_MANAGER  */

#define VM_ADDR_START       U_ALIGN(VDIN_ADDR_END)
#define VM_ADDR_END         (VM_SIZE + VM_ADDR_START - 1)

#if defined(CONFIG_AM_DEINTERLACE_SD_ONLY)
    #define DI_MEM_SIZE         (SZ_1M*3)
#else
    #define DI_MEM_SIZE         (SZ_1M*35)
#endif
#define DI_ADDR_START       U_ALIGN(VDIN_ADDR_END)
#define DI_ADDR_END         (DI_ADDR_START+DI_MEM_SIZE-1)

#ifdef CONFIG_POST_PROCESS_MANAGER
    #ifdef CONFIG_POST_PROCESS_MANAGER_PPSCALER
        #define PPMGR_MEM_SIZE      1920 * 1088 * 22
    #else
        #define PPMGR_MEM_SIZE      1920 * 1088 * 15
    #endif
#else
    #define PPMGR_MEM_SIZE      1
#endif /* CONFIG_POST_PROCESS_MANAGER */

#define PPMGR_ADDR_START    U_ALIGN(DI_ADDR_END)
#define PPMGR_ADDR_END      (PPMGR_ADDR_START+PPMGR_MEM_SIZE-1)

#ifdef CONFIG_AM_MEMPROTECT
    #define STREAMBUF_MEM_SIZE          (SZ_1M*3)
#else
    #define STREAMBUF_MEM_SIZE          (SZ_1M*10)
#endif
#define STREAMBUF_ADDR_START    U_ALIGN(PPMGR_ADDR_END)
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

#ifdef CONFIG_AML_VIDEO_RES_MGR
static struct resource resmgr_resources[] = {
    [0] = {
        .start = CODEC_ADDR_START,
        .end   = CODEC_ADDR_END,
        .flags = IORESOURCE_MEM,
    },
};
#endif

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
#endif //CONFIG_POST_PROCESS_MANAGER

#ifdef CONFIG_AML_VIDEO_RES_MGR
static struct platform_device resmgr_device = {
    .name       = "resmgr",
    .id         = 0,
    .num_resources = ARRAY_SIZE(resmgr_resources),
    .resource      = resmgr_resources,
};
#endif //CONFIG_AML_VIDEO_RES_MGR

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
#endif  //CONFIG_FREE_SCALE

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
 * Remote Section
 ***********************************************************/
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
 * I2C Section
 **********************************************************************/
#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
static bool pinmux_dummy_share(bool select)
{
    return select;
}

static pinmux_item_t aml_i2c_a_pinmux_item[] = {
    {
        .reg = 5,
        .setmask = 3<<26
    },
    PINMUX_END_ITEM
};

static struct aml_i2c_platform aml_i2c_plat_a = {
    .wait_count             = 50000,
    .wait_ack_interval   = 5,
    .wait_read_interval  = 5,
    .wait_xfer_interval   = 5,
    .master_no          = AML_I2C_MASTER_A,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K,

    .master_pinmux      = {
        .chip_select    = pinmux_dummy_share,
        .pinmux         = &aml_i2c_a_pinmux_item[0]
    }
};

static pinmux_item_t aml_i2c_b_pinmux_item[]={
    {
        .reg = 5,
        .setmask = 3<<30
    },
    PINMUX_END_ITEM
};

static struct aml_i2c_platform aml_i2c_plat_b = {
    .wait_count         = 50000,
    .wait_ack_interval = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no          = AML_I2C_MASTER_B,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_300K,

    .master_pinmux      = {
        .chip_select    = pinmux_dummy_share,
        .pinmux         = &aml_i2c_b_pinmux_item[0]
    }
};

static pinmux_item_t aml_i2c_ao_pinmux_item[] = {
    {
        .reg = AO,
        .clrmask  = 3<<1,
        .setmask = 3<<5
    },
    PINMUX_END_ITEM
};

static struct aml_i2c_platform aml_i2c_plat_ao = {
    .wait_count         = 50000,
    .wait_ack_interval  = 5,
    .wait_read_interval = 5,
    .wait_xfer_interval = 5,
    .master_no          = AML_I2C_MASTER_AO,
    .use_pio            = 0,
    .master_i2c_speed   = AML_I2C_SPPED_100K,

    .master_pinmux      = {
        .pinmux         = &aml_i2c_ao_pinmux_item[0]
    }
};

static struct resource aml_i2c_resource_a[] = {
    [0] = {
        .start = MESON_I2C_MASTER_A_START,
        .end   = MESON_I2C_MASTER_A_END,
        .flags = IORESOURCE_MEM,
    }
};

static struct resource aml_i2c_resource_b[] = {
    [0] = {
        .start = MESON_I2C_MASTER_B_START,
        .end   = MESON_I2C_MASTER_B_END,
        .flags = IORESOURCE_MEM,
    }
};

static struct resource aml_i2c_resource_ao[] = {
    [0]= {
        .start = MESON_I2C_MASTER_AO_START,
        .end   = MESON_I2C_MASTER_AO_END,
        .flags = IORESOURCE_MEM,
    }
};

static struct platform_device aml_i2c_device_a = {
    .name     = "aml-i2c",
    .id       = 0,
    .num_resources = ARRAY_SIZE(aml_i2c_resource_a),
    .resource = aml_i2c_resource_a,
    .dev = {
        .platform_data = &aml_i2c_plat_a,
    },
};

static struct platform_device aml_i2c_device_b = {
    .name     = "aml-i2c",
    .id       = 1,
    .num_resources = ARRAY_SIZE(aml_i2c_resource_b),
    .resource = aml_i2c_resource_b,
    .dev = {
        .platform_data = &aml_i2c_plat_b,
    },
};

static struct platform_device aml_i2c_device_ao = {
    .name     = "aml-i2c",
    .id       = 2,
    .num_resources = ARRAY_SIZE(aml_i2c_resource_ao),
    .resource = aml_i2c_resource_ao,
    .dev = {
        .platform_data = &aml_i2c_plat_ao,
    },
};

static struct i2c_board_info __initdata aml_i2c_bus_info_a[] = {
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_HI2056
    {
        /*hi2056 i2c address is 0x48/0x49*/
        I2C_BOARD_INFO("hi2056_i2c",  0x48 >> 1),
        .platform_data = (void *)&mipi_hi2056_data,
    },
#endif
};

static struct i2c_board_info __initdata aml_i2c_bus_info_ao[] = {
};


static struct i2c_board_info __initdata aml_i2c_bus_info_b[] = {
};

static int __init aml_i2c_init(void)
{
    i2c_register_board_info(0, aml_i2c_bus_info_a,
        ARRAY_SIZE(aml_i2c_bus_info_a));
    i2c_register_board_info(1, aml_i2c_bus_info_b,
        ARRAY_SIZE(aml_i2c_bus_info_b));
    i2c_register_board_info(2, aml_i2c_bus_info_ao,
        ARRAY_SIZE(aml_i2c_bus_info_ao));
    return 0;
}
#endif // defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)

#if defined(CONFIG_I2C_SW_AML)
static struct aml_sw_i2c_platform aml_sw_i2c_plat_1 = {
    .sw_pins = {
        .scl_reg_out    = P_PREG_PAD_GPIO4_O,
        .scl_reg_in     = P_PREG_PAD_GPIO4_I,
        .scl_bit        = 26,
        .scl_oe         = P_PREG_PAD_GPIO4_EN_N,
        .sda_reg_out    = P_PREG_PAD_GPIO4_O,
        .sda_reg_in     = P_PREG_PAD_GPIO4_I,
        .sda_bit        = 25,
        .sda_oe         = P_PREG_PAD_GPIO4_EN_N,
    },
    .udelay         = 2,
    .timeout        = 100,
};

static struct platform_device aml_sw_i2c_device_1 = {
    .name     = "aml-sw-i2c",
    .id       = -1,
    .dev = {
        .platform_data = &aml_sw_i2c_plat_1,
    },
};
#endif // defined(CONFIG_I2C_SW_AML)

/***********************************************************************
 * UART Section
 **********************************************************************/
static pinmux_item_t uart_pins[] = {
    {
        .reg = PINMUX_REG(AO),
        .setmask = 3 << 11
    },
    {
        .reg = PINMUX_REG(4),
        .setmask = 0xf << 10
    },
    PINMUX_END_ITEM
};

static pinmux_set_t aml_uart_ao = {
    .chip_select = NULL,
    .pinmux = &uart_pins[0]
};

static pinmux_set_t aml_uart_a = {
    .chip_select = NULL,
    .pinmux = &uart_pins[1]
};

static struct aml_uart_platform  __initdata aml_uart_plat = {
    .uart_line[0]   = UART_AO,
    .uart_line[1]   = UART_A,
    .uart_line[2]   = UART_B,
    .uart_line[3]   = UART_C,
    .uart_line[4]   = UART_D,

    .pinmux_uart[0] = (void*)&aml_uart_ao,
    .pinmux_uart[1] = (void*)&aml_uart_a,
    .pinmux_uart[2] = NULL,
    .pinmux_uart[3] = NULL,
    .pinmux_uart[4] = NULL
};

static struct platform_device aml_uart_device = {
    .name     = "mesonuart",
    .id       = -1,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &aml_uart_plat,
    },
};

/***********************************************************************
 * Nand Section
 **********************************************************************/

#ifdef CONFIG_AM_NAND
static struct mtd_partition normal_partition_info[] = {
    #ifdef CONFIG_AML_NAND_ENV
    {
        .name = "ubootenv",
        .offset = 8*SZ_1M,
        .size = 4*SZ_1M,
    },
    #endif
    {
        .name = "logo",
        .offset = 32*SZ_1M+40*SZ_1M,
        .size = 8*SZ_1M,
    },
    {
        .name = "aml_logo",
        .offset = 48*SZ_1M+40*SZ_1M,
        .size = 8*SZ_1M,
    },
    {
        .name = "recovery",
        .offset = 64*SZ_1M+40*SZ_1M,
        .size = 8*SZ_1M,
    },
    {
        .name = "boot",
        .offset = 96*SZ_1M+40*SZ_1M,
        .size = 8*SZ_1M,
    },
    {
        .name = "system",
        .offset = 128*SZ_1M+40*SZ_1M,
        .size = 512*SZ_1M+512*SZ_1M,
    },
    {
        .name = "cache",
        .offset = 640*SZ_1M+512*SZ_1M+40*SZ_1M,
        .size = 512*SZ_1M,
    },
    {
        .name = "backup",
        .offset = 1152*SZ_1M+512*SZ_1M+40*SZ_1M,
        .size = 256*SZ_1M,
    },
    {
        .name = "userdata",
        .offset = MTDPART_OFS_APPEND,
        .size = MTDPART_SIZ_FULL,
    },
};


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
        .T_REA = 20,
        .T_RHOH = 15,
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
#endif // CONFIG_AM_NAND

#if defined(CONFIG_AMLOGIC_SPI_NOR)
static struct mtd_partition spi_partition_info[] = {
    {
        .name = "bootloader",
        .offset = 0,
        .size = 0x100000,
    },
    {
        .name = "ubootenv",
        .offset = 0x100000,
        .size = 0x8000,
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

#if defined(CONFIG_AML_CARD_KEY) || defined(CONFIG_AML_NAND_KEY)
static char * secure_device[2]={"nand_key",NULL};
static struct platform_device aml_keys_device = {
    .name   = "aml_keys",
    .id = -1,
    .dev = {
                .platform_data = &secure_device[0],
           },
};
#endif

#ifdef CONFIG_MESON_TRUSTZONE
static struct platform_device aml_secure_monitor_device = {
    .name = "secure_monitor",
    .id = -1,
};
#endif



/***********************************************************************
 * Card Reader Section
 **********************************************************************/

/***********************************************************************
 * WIFI  Section
 **********************************************************************/
/**
* GPIOX_0          -->WIFI_SD_D0
* GPIOX_1          -->WIFI_SD_D1
* GPIOX_2          -->WIFI_SD_D2
* GPIOX_3          -->WIFI_SD_D3
*
* GPIOX_8          -->WIFI_SD_CMD
* GPIOX_9          -->WIFI_SD_CLK
*
* WIFI_EN          -->GPIOC_8
* WIFI_WAKE        -->GPIOX_11
* 32K_CLOCK_OUT    -->GPIOX_12 (CLK_OUT3)
*/
#ifdef CONFIG_AM_WIFI_SD_MMC
#define DBG_LINE_INFO()  printk(KERN_INFO "[%s] in\n",__func__)

/* WIFI ON Flag */
static int WIFI_ON;
/* BT ON Flag */
static int BT_ON;
static void wifi_gpio_init(void) {
//set status
    //WIFI_EN WIFI_PWREN  WLAN_RST --->out    :0
    gpio_set_status(PAD_GPIOC_6, gpio_status_out);
    //WIFI_WAKE -->1GPIOX_11   in    :
    #ifdef CONFIG_BCM40183_WIFI
        gpio_set_status(PAD_GPIOX_11, gpio_status_in);
    #endif
    #ifdef CONFIG_BCM40181_WIFI
        gpio_set_status(PAD_GPIOX_11, gpio_status_in);
    #endif
    //set pull-up
    aml_clr_reg32_mask(P_PAD_PULL_UP_REG4, 0xf|1<<8|1<<9|1<<11|1<<12);
    aml_clr_reg32_mask(P_PAD_PULL_UP_REG2, 1<<6);
}

static void wifi_clock_enable(int is_on) {
    //使用PWM输出32.768KHz
    //enable pinmux
    //aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<2));   //pwm_C pinmux，D14，GPIOD_0
    aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<3)); //pwm_D pinmux，D13，GPIOD_1
    //enable pwm clk & pwm output
    //aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<8)) | ((1 << 15) | (PWM_PRE_DIV<<8) | (3<<4) | (1<<0)));  //
    aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<16)) | ((1 << 23) | (PWM_PRE_DIV<<16) | (3<<6) | (1<<1)));//
    //aml_set_reg32_bits(P_PWM_PWM_C, 0x27bc-1, 0, 16);   //pwm low
    //aml_set_reg32_bits(P_PWM_PWM_C, 0x27bd-1, 16, 16); //pwm high
    aml_set_reg32_bits(P_PWM_PWM_D, 0x27bc-1, 0, 16);   //pwm low
    aml_set_reg32_bits(P_PWM_PWM_D, 0x27bd-1, 16, 16); //pwm high
}

void extern_wifi_set_power(int is_on) {
    DBG_LINE_INFO();
    gpio_set_status(PAD_GPIOC_6, gpio_status_out);//set wifi_en gpio mode out
    if (is_on) {
        gpio_out(PAD_GPIOC_6, 1);
        printk("WIFI Enable! \n");
    }
    else {
        gpio_out(PAD_GPIOC_6,0);
        printk("WIFI Disable! \n");
    }
}
EXPORT_SYMBOL(extern_wifi_set_power);

void extern_wifi_set_enable(int is_on) {
    DBG_LINE_INFO();
    gpio_set_status(PAD_GPIOC_6, gpio_status_out);//set wifi_en gpio mode out
    if (is_on) {
        gpio_out(PAD_GPIOC_6, 1);
        printk("WIFI Enable! \n");
    }
    else {
        gpio_out(PAD_GPIOC_6, 0);
        printk("WIFI Disable! \n");
    }
}
EXPORT_SYMBOL(extern_wifi_set_enable);

void extern_wifi_reset(int is_on) {
}
EXPORT_SYMBOL(extern_wifi_reset);

void wifi_dev_init(void) {
    DBG_LINE_INFO();
    wifi_clock_enable(1);
    udelay(200);
    wifi_gpio_init();
//#if defined(CONFIG_BCM40181_WIFI) || defined(CONFIG_BCM40183_WIFI)
//  extern_wifi_set_enable(0);
//#endif
}
EXPORT_SYMBOL(wifi_dev_init);

static void extern_wifi_power(int is_power) {
    DBG_LINE_INFO();

    WIFI_ON = is_power;
    mdelay(200);
    extern_wifi_set_enable(is_power);
}
EXPORT_SYMBOL(extern_wifi_power);
#endif // CONFIG_AM_WIFI_SD_MMC


#ifdef CONFIG_CARDREADER
static struct resource meson_card_resource[] = {
    [0] = {
        .start = 0x1200230,   //physical address
        .end   = 0x120024c,
        .flags = 0x200,
    }
};

#ifdef CONFIG_AM_WIFI_SD_MMC
static void sdio_extern_init(void) {
    #if defined(CONFIG_BCM4329_HW_OOB) || defined(CONFIG_BCM4329_OOB_INTR_ONLY)/* Jone add */
    // gpio_set_status(PAD_GPIOX_11,gpio_status_in);
    // gpio_irq_set(PAD_GPIOX_11,GPIO_IRQ(4,GPIO_IRQ_RISING));

    // extern_wifi_power(1);
    #endif

    #ifdef CONFIG_BCM40183_WIFI
        gpio_set_status(PAD_GPIOX_11, gpio_status_in);
        gpio_irq_set(PAD_GPIOX_11,GPIO_IRQ(4, GPIO_IRQ_HIGH));
    #endif
    //5, GPIO_IRQ_RISING
    #ifdef CONFIG_BCM40181_WIFI
        gpio_set_status(PAD_GPIOX_11, gpio_status_in);
        gpio_irq_set(PAD_GPIOX_11, GPIO_IRQ(4, GPIO_IRQ_HIGH));
    #endif


    {
            uint32_t value;
            uint32_t addr = 0xf1100000 + (0x2623 << 2);
            value = aml_read_reg32(addr);   //GPIO_INTR_FILTER_SEL0
            value &= 0xFF0FFFFF;            //set FILTER_SEL5 to 0
            aml_write_reg32(addr, value);
    }

#if defined(CONFIG_BCM40181_WIFI) || defined(CONFIG_BCM40183_WIFI)
    extern_wifi_set_enable(1);
#endif

}
#endif // CONFIG_AM_WIFI_SD_MMC

static struct aml_card_info meson_card_info[] = {
    [0] = {
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
#ifdef CONFIG_AM_WIFI_SD_MMC
    [1] = {
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
        .card_extern_init   = sdio_extern_init,
    },
#endif // CONFIG_AM_WIFI_SD_MMC
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

/**
 *  Some Meson6 socket board has card detect issue.
 *  Force card detect success for socket board.
 */
static int meson_mmc_detect(void) {
    return 0;
}
#endif // CONFIG_CARDREADER

/***********************************************************************
 * MMC SD Card  Section
 **********************************************************************/
#ifdef CONFIG_MMC_AML
struct platform_device;
struct mmc_host;
struct mmc_card;
struct mmc_ios;

//return 1: no inserted  0: inserted
static int aml_sdio_detect(struct aml_sd_host * host) {
    aml_set_reg32_mask(P_PREG_PAD_GPIO5_EN_N, 1<<29);//CARD_6 input mode
    if ((aml_read_reg32(P_PREG_PAD_GPIO5_I)&(1<<29)) == 0)
        return 0;
    else { //for socket card box
        return 0;
    }
    return 1; //no insert.
}

static void cpu_sdio_pwr_prepare(unsigned port) {
    switch (port) {
        case MESON_SDIO_PORT_A:
            aml_clr_reg32_mask(P_PREG_PAD_GPIO4_EN_N,0x30f);
            aml_clr_reg32_mask(P_PREG_PAD_GPIO4_O   ,0x30f);
            aml_clr_reg32_mask(P_PERIPHS_PIN_MUX_8,0x3f);
            break;
        case MESON_SDIO_PORT_B:
            aml_clr_reg32_mask(P_PREG_PAD_GPIO5_EN_N,0x3f<<23);
            aml_clr_reg32_mask(P_PREG_PAD_GPIO5_O   ,0x3f<<23);
            aml_clr_reg32_mask(P_PERIPHS_PIN_MUX_2,0x3f<<10);
            break;
        case MESON_SDIO_PORT_C:
            aml_clr_reg32_mask(P_PREG_PAD_GPIO3_EN_N,0xc0f);
            aml_clr_reg32_mask(P_PREG_PAD_GPIO3_O   ,0xc0f);
            aml_clr_reg32_mask(P_PERIPHS_PIN_MUX_6,(0x3f<<24));
            break;
        case MESON_SDIO_PORT_XC_A:
            break;
        case MESON_SDIO_PORT_XC_B:
            break;
        case MESON_SDIO_PORT_XC_C:
            break;
    }
}

static int cpu_sdio_init(unsigned port) {
    switch (port) {
        case MESON_SDIO_PORT_A:
                aml_set_reg32_mask(P_PERIPHS_PIN_MUX_8,0x3d<<0);
                aml_set_reg32_mask(P_PERIPHS_PIN_MUX_8,0x1<<1);
                break;
        case MESON_SDIO_PORT_B:
                aml_set_reg32_mask(P_PERIPHS_PIN_MUX_2,0x3d<<10);
                aml_set_reg32_mask(P_PERIPHS_PIN_MUX_2,0x1<<11);
                break;
        case MESON_SDIO_PORT_C://SDIOC GPIOB_2~GPIOB_7
            aml_clr_reg32_mask(P_PERIPHS_PIN_MUX_2,(0x1f<<22));
            aml_set_reg32_mask(P_PERIPHS_PIN_MUX_6,(0x1f<<25));
            aml_set_reg32_mask(P_PERIPHS_PIN_MUX_6,(0x1<<24));
            break;
        case MESON_SDIO_PORT_XC_A:
            //sdxc controller can't work
            break;
        case MESON_SDIO_PORT_XC_B:
            //sdxc controller can't work
            break;
        case MESON_SDIO_PORT_XC_C:
            //sdxc controller can't work
            break;
        default:
            return -1;
    }
    return 0;
}

static void aml_sdio_pwr_prepare(unsigned port) {
    /// @todo NOT FINISH
    ///do nothing here
    cpu_sdio_pwr_prepare(port);
}

static void aml_sdio_pwr_on(unsigned port) {
    if ((aml_read_reg32(P_PREG_PAD_GPIO5_O) & (1<<31)) != 0) {
        aml_clr_reg32_mask(P_PREG_PAD_GPIO5_O,(1<<31));
        aml_clr_reg32_mask(P_PREG_PAD_GPIO5_EN_N,(1<<31));
        udelay(1000);
    }
    /// @todo NOT FINISH
}

static void aml_sdio_pwr_off(unsigned port) {
    if ((aml_read_reg32(P_PREG_PAD_GPIO5_O) & (1<<31)) == 0) {
        aml_set_reg32_mask(P_PREG_PAD_GPIO5_O,(1<<31));
        aml_clr_reg32_mask(P_PREG_PAD_GPIO5_EN_N,(1<<31));//GPIOD13
        udelay(1000);
    }
    /// @todo NOT FINISH
}

static int aml_sdio_init(struct aml_sd_host * host) {
    //set pinumx ..
    aml_set_reg32_mask(P_PREG_PAD_GPIO5_EN_N,1<<29);//CARD_6
    cpu_sdio_init(host->sdio_port);
    host->clk = clk_get_sys("clk81",NULL);
    if (!IS_ERR(host->clk))
        host->clk_rate = clk_get_rate(host->clk);
    else
        host->clk_rate = 0;
    return 0;
}

static struct resource aml_mmc_resource[] = {
   [0] = {
        .start = 0x1200230,   //physical address
        .end   = 0x1200248,
        .flags = IORESOURCE_MEM, //0x200
    },
};

static u64 aml_mmc_device_dmamask = 0xffffffffUL;
static struct aml_mmc_platform_data aml_mmc_def_platdata = {
    .no_wprotect = 1,
    .no_detect = 0,
    .wprotect_invert = 0,
    .detect_invert = 0,
    .use_dma = 0,
    .gpio_detect=1,
    .gpio_wprotect=0,
    .ocr_avail = MMC_VDD_33_34,

    .sdio_port = MESON_SDIO_PORT_B,
    .max_width  = 4,
    .host_caps  = (MMC_CAP_4_BIT_DATA |
                            MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED | MMC_CAP_NEEDS_POLL),

    .f_min = 200000,
    .f_max = 40000000,
    .clock = 300000,

    .sdio_init = aml_sdio_init,
    .sdio_detect = aml_sdio_detect,
    .sdio_pwr_prepare = aml_sdio_pwr_prepare,
    .sdio_pwr_on = aml_sdio_pwr_on,
    .sdio_pwr_off = aml_sdio_pwr_off,
};

static struct platform_device aml_mmc_device = {
    .name       = "aml_sd_mmc",
    .id     = 0,
    .num_resources  = ARRAY_SIZE(aml_mmc_resource),
    .resource   = aml_mmc_resource,
    .dev        = {
        .dma_mask       =       &aml_mmc_device_dmamask,
        .coherent_dma_mask  = 0xffffffffUL,
        .platform_data      = &aml_mmc_def_platdata,
    },
};
#endif //CONFIG_MMC_AML

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
    } , */ {
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

static void __init meson_map_io(void) {
    iotable_init(meson_io_desc, ARRAY_SIZE(meson_io_desc));
}

static void __init meson_fixup(struct machine_desc *mach, struct tag *tag, char **cmdline, struct meminfo *m) {
    struct membank *pbank;
    mach->video_start    = RESERVED_MEM_START;
    mach->video_end      = RESERVED_MEM_END;

#ifdef CONFIG_MESON_TRUSTZONE
    meson_trustzone_memconfig();
#endif

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
 * USB Setting section
 **********************************************************************/
static void set_usb_a_vbus_power(char is_power_on) {
#define USB_A_POW_GPIO  PREG_EGPIO
#define USB_A_POW_GPIO_BIT  25
#define USB_A_POW_GPIO_BIT_ON   1
#define USB_A_POW_GPIO_BIT_OFF  0
    if (is_power_on) {
        printk( "set usb port power on (board gpio %d)!\n",USB_A_POW_GPIO_BIT);

        aml_set_reg32_bits(CBUS_REG_ADDR(0x2012),0,USB_A_POW_GPIO_BIT,1);//mode
        aml_set_reg32_bits(CBUS_REG_ADDR(0x2013),1,USB_A_POW_GPIO_BIT,1);//out
        //set_gpio_mode(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,GPIO_OUTPUT_MODE);
        //set_gpio_val(USB_A_POW_GPIO,USB_A_POW_GPIO_BIT,USB_A_POW_GPIO_BIT_ON);
    }
    else {
        printk("set usb port power off (board gpio %d)!\n",USB_A_POW_GPIO_BIT);
        aml_set_reg32_bits(CBUS_REG_ADDR(0x2012),0,USB_A_POW_GPIO_BIT,1);//mode
        aml_set_reg32_bits(CBUS_REG_ADDR(0x2013),0,USB_A_POW_GPIO_BIT,1);//out
    }
}

static  int __init setup_usb_devices(void) {
    struct lm_device * usb_ld_a, *usb_ld_b;
    usb_ld_a = alloc_usb_lm_device(USB_PORT_IDX_A);
    usb_ld_b = alloc_usb_lm_device (USB_PORT_IDX_B);
    usb_ld_a->param.usb.set_vbus_power = set_usb_a_vbus_power;
    usb_ld_a->param.usb.port_type = USB_PORT_TYPE_OTG;
    lm_device_register(usb_ld_a);
    lm_device_register(usb_ld_b);
    return 0;
}

/***********************************************************************
 * WiFi power section
 **********************************************************************/
/* built-in usb wifi power ctrl, usb dongle must register NULL to power_ctrl! 1:power on  0:power off */
#ifdef CONFIG_AM_WIFI
#ifdef CONFIG_AM_WIFI_USB
static void usb_wifi_power(int is_power)
{
    //printk(KERN_INFO "usb_wifi_power %s\n", is_power ? "On" : "Off");
    printk(KERN_INFO "usb_wifi_power %s\n", is_power ? "On" : "Off");
//    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_1,(1<<11));
//    CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_0,(1<<18));
//    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<8));
    if (is_power) {
        aml_set_reg32_bits(CBUS_REG_ADDR(0x2018),0,11,1);//mode
        aml_set_reg32_bits(CBUS_REG_ADDR(0x2019),0,11,1);//out
    }
    else {
        aml_set_reg32_bits(CBUS_REG_ADDR(0x2018),0,11,1);//mode
        aml_set_reg32_bits(CBUS_REG_ADDR(0x2019),0,11,1);//out
    }
    return 0;
}

static struct wifi_power_platform_data wifi_plat_data = {
    .usb_set_power = usb_wifi_power,
};
#elif defined(CONFIG_AM_WIFI_SD_MMC)&&defined(CONFIG_CARDREADER)
static struct wifi_power_platform_data wifi_plat_data = {

};
#endif

static struct platform_device wifi_power_device = {
    .name       = "wifi_power",
    .id     = -1,
    .dev = {
        .platform_data = &wifi_plat_data,
    },
};
#endif // CONFIG_AM_WIFI


/***********************************************************************
* Bluetooth  Section
**********************************************************************/
#ifdef CONFIG_BT_DEVICE
#include <linux/bt-device.h>

static struct platform_device bt_device = {
    .name             = "bt-dev",
    .id               = -1,
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
    gpio_set_status(PAD_GPIOE_10,gpio_status_out);
    /* BT_REG_ON */
    gpio_set_status(PAD_GPIOE_11,gpio_status_out);

    pinmux_set(&pcm_pinmux_set);

    /* BT_WAKE */
    gpio_set_status(PAD_GPIOX_10,gpio_status_out);
    gpio_out(PAD_GPIOX_10,1);
}

static void bt_device_on(void)
{
    gpio_out(PAD_GPIOE_10,0);
    gpio_out(PAD_GPIOE_11,0);
    msleep(20);
    /* BT_RST_N */
    gpio_out(PAD_GPIOE_10,1);
    /* BT_REG_ON */
    gpio_out(PAD_GPIOE_11,1);
    msleep(20);
}

static void bt_device_off(void)
{
    /* BT_RST_N */
    gpio_out(PAD_GPIOE_10,0);
    /* BT_REG_ON */
    gpio_out(PAD_GPIOE_11,0);
    msleep(20);
}

static void bt_device_suspend(void)
{
}

static void bt_device_resume(void)
{

}

struct bt_dev_data bt_dev = {
    .bt_dev_init    = bt_device_init,
    .bt_dev_on      = bt_device_on,
    .bt_dev_off     = bt_device_off,
    .bt_dev_suspend = bt_device_suspend,
    .bt_dev_resume  = bt_device_resume,
    };
#endif


/***********************************************************************/
#ifdef CONFIG_EFUSE
static bool efuse_data_verify(unsigned char *usid) {
    int len;

    len = strlen(usid);
    if ((len > 0)&&(len<58) )
        return true;
    else
        return false;
}

static struct efuse_platform_data aml_efuse_plat = {
    .pos = 454,
    .count = 58,
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
        .enc_len = 58,     // customer can modify the encode length by self must <=58
        .data_len = 58,    // customer can modify the data length by self must <=58
        .bch_en = 0,        // customer can modify do bch or not
        .bch_reverse = 0,
    },
    // customer also can add new EFUSE item to expand, but must be correct and bo conflict
};

static int aml_efuse_getinfoex_byID(unsigned param, efuseinfo_item_t *info)
{
    unsigned num = sizeof(aml_efuse_setting)/sizeof(efuseinfo_item_t);
    int i=0;
    int ret = -1;
    for (i=0; i<num; i++) {
        if (aml_efuse_setting[i].id == param){
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

static int aml_efuse_getinfoex_byPos(unsigned param, efuseinfo_item_t *info) {
    unsigned num = sizeof(aml_efuse_setting)/sizeof(efuseinfo_item_t);
    int i=0;
    int ret = -1;
    for (i=0; i<num; i++){
        if (aml_efuse_setting[i].offset == param) {
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

static setup_aml_efuse() {
    efuse_getinfoex = aml_efuse_getinfoex_byID;
    efuse_getinfoex_byPos = aml_efuse_getinfoex_byPos;
}
#endif // CONFIG_EFUSE

#if defined(CONFIG_AML_RTC)
static void RTC_clock_enable()
{
//没有32K晶振，使用PWM输出32.768KHz
//enable pinmux
//aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<2));   //pwm_C pinmux，D14，GPIOD_0
aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1<<3)); //pwm_D pinmux，D13，GPIOD_1
//enable pwm clk & pwm output
//aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<8)) | ((1 << 15) | (PWM_PRE_DIV<<8) | (3<<4) | (1<<0)));  //
aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f<<16)) | ((1 << 23) | (PWM_PRE_DIV<<16) | (3<<6) | (1<<1)));//
//aml_set_reg32_bits(P_PWM_PWM_C, 0x27bc-1, 0, 16);   //pwm low
//aml_set_reg32_bits(P_PWM_PWM_C, 0x27bd-1, 16, 16); //pwm high
aml_set_reg32_bits(P_PWM_PWM_D, 0x27bc-1, 0, 16);   //pwm low
aml_set_reg32_bits(P_PWM_PWM_D, 0x27bd-1, 16, 16); //pwm high

}

static struct platform_device aml_rtc_device = {
    .name   = "aml_rtc",
    .id     = -1,
};
#endif

#if defined(CONFIG_SUSPEND)
static void m6ref_set_vccx2(int power_on) {
    if (power_on) {
        //restore_pinmux();
        printk(KERN_INFO "%s() Power ON\n", __FUNCTION__);
        aml_clr_reg32_mask(P_PREG_PAD_GPIO0_EN_N,(1<<26));
        aml_clr_reg32_mask(P_PREG_PAD_GPIO0_O,(1<<26));
    }
    else {
        printk(KERN_INFO "%s() Power OFF\n", __FUNCTION__);
        aml_clr_reg32_mask(P_PREG_PAD_GPIO0_EN_N,(1<<26));
        aml_set_reg32_mask(P_PREG_PAD_GPIO0_O,(1<<26));
        //save_pinmux();
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
    .name = "pm-meson",
    .dev = {
        .platform_data  = &aml_pm_pdata,
    },
    .id = -1,
};
#endif // defined(CONFIG_SUSPEND)

#if defined (CONFIG_AML_HDMI_TX)
static void m6ref_hdmi_5v_ctrl(unsigned int pwr) {
    if (pwr) {
        printk("HDMI 5V Power On\n");
        aml_clr_reg32_mask(P_PREG_PAD_GPIO2_O,(1<<21));
        aml_clr_reg32_mask(P_PREG_PAD_GPIO2_EN_N,(1<<21));
    } else {
        printk("HDMI 5V Power Off\n");
        aml_clr_reg32_mask(P_PREG_PAD_GPIO2_O,(0<<21));
        aml_set_reg32_mask(P_PREG_PAD_GPIO2_EN_N,(1<<21));
    }
}

static struct hdmi_phy_set_data brd_phy_data[] = {
//    {27, 0x16, 0x30},   // 480i/p 576i/p
//    {74, 0x16, 0x40},   // 720p 1080i
//    {148, 0x16, 0x40},  // 1080p

    {27, 0x10, 0xf0},
    {74, 0x10, 0xf0},
    {148, 0x10, 0xf0},
    {27, 0x14, 0x2},   // 480i/p 576i/p
    {27, 0x16, 0x0},   // 480i/p 576i/p
    {74, 0x14, 0x2},   // 720p 1080i
    {74, 0x16, 0x0},   // 720p 1080i
    {148, 0x14, 0x2},  // 1080p
    {148, 0x16, 0x10},  // 1080p
    {-1,   -1},         //end of phy setting
};

static struct vendor_info_data vendor_data = {
    .vendor_name = "Amlogic",               // Max Chars: 8
    .vendor_id = 0x000000,                  // Refer to http://standards.ieee.org/develop/regauth/oui/oui.txt
                                            // here for test only
    .product_desc = "MX MBox g35",         // Max Chars: 16
    .cec_osd_string = "AmlogicDongle",                 // Max Chars: 14
};

static struct hdmi_config_platform_data aml_hdmi_pdata = {
    .hdmi_5v_ctrl = m6ref_hdmi_5v_ctrl,
    .hdmi_3v3_ctrl = NULL,
    .hdmi_pll_vdd_ctrl = NULL,
    .phy_data = brd_phy_data,
    .vend_data = &vendor_data,
};
#endif // defined (CONFIG_AML_HDMI_TX)

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
        .min_uV =  1010000,
        .max_uV =  1380000, //1209000,
        .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
    },
    .num_consumer_supplies = ARRAY_SIZE(vcck_data),
    .consumer_supplies = vcck_data,
};

// pwm duty for vcck voltage
static unsigned int vcck_pwm_table[MESON_CS_MAX_STEPS] = {
    0x00001c, 0x01001b, 0x030019, 0x040018,
    0x060016, 0x090013, 0x0b0011, 0x0d000f,
    0x0f000d, 0x11000b, 0x130009, 0x150007,
    0x170005, 0x190003, 0x1b0001, 0x1c0000,
};
static int get_voltage() {
//    printk("***vcck: get_voltage");
    int i;
    unsigned int reg = aml_read_reg32(P_PWM_PWM_C);
    for (i=0; i<MESON_CS_MAX_STEPS; i++) {
        if (reg == vcck_pwm_table[i])
            break;
    }
    if (i >= MESON_CS_MAX_STEPS)
        return -1;
    else
        return i;
}

static int set_voltage(unsigned int level) {
//    printk("***vcck: set_voltage");
    aml_write_reg32(P_PWM_PWM_C, vcck_pwm_table[level]);
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

static void vcck_pwm_init() {
    printk("***vcck: vcck_pwm_init");
    //enable pwm clk & pwm output
    aml_write_reg32(P_PWM_MISC_REG_CD, (aml_read_reg32(P_PWM_MISC_REG_CD) & ~(0x7f << 8)) | ((1 << 15) | (PWM_PRE_DIV << 8) | (1 << 0)));
    aml_write_reg32(P_PWM_PWM_C, vcck_pwm_table[0]);
    //enable pwm_C pinmux    1<<3 pwm_D
    pinmux_set(&vcck_pwm_set);
    //aml_write_reg32(P_PERIPHS_PIN_MUX_2, aml_read_reg32(P_PERIPHS_PIN_MUX_2) | (1 << 2));
}


//1.0V   <800MHz
//1.1V   800MHz~9xxMHz
//1.2V   1GHz~1.1xGHz
//1.3V   1.2GHz~1.37GHz
//1.38V  1.39~1.5GHz
static struct meson_cs_pdata_t vcck_pdata = {
    .meson_cs_init_data = &vcck_init_data,
    .voltage_step_table = {
        1400000, 1380000, 1350000, 1330000,
        1300000, 1240000, 1210000, 1180000,
        1140000, 1110000, 1070000, 1040000,
        1010000,  970000,  940000,  920000,
    },
    .default_uV = 1110000,
    .get_voltage = get_voltage,
    .set_voltage = set_voltage,
};

static struct meson_opp vcck_opp_table[] = {
    /* freq must be in descending order */
    {
        .freq   = 1500000,
        .min_uV = 1380000,
        .max_uV = 1380000,
    },
    {
        .freq   = 1320000,
        .min_uV = 1300000,
        .max_uV = 1300000,
    },
    {
        .freq   = 1200000,
        .min_uV = 1210000,
        .max_uV = 1210000,
    },
    {
        .freq   = 1080000,
        .min_uV = 1210000,
        .max_uV = 1210000,
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
        .min_uV = 1040000,
        .max_uV = 1040000,
    },
    {
        .freq   = 600000,
        .min_uV = 1040000,
        .max_uV = 1040000,
    },
    {
        .freq   = 200000,
        .min_uV = 1040000,
        .max_uV = 1040000,
    }
};


static struct platform_device meson_cs_dcdc_regulator_device = {
    .name = "meson-cs-regulator",
    .dev = {
        .platform_data = &vcck_pdata,
    }
};
#endif // CONFIG_MESON_CS_DCDC_REGULATOR

/***********************************************************************
 * Meson CPUFREQ section
 **********************************************************************/
#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#include <plat/cpufreq.h>

#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
#include <mach/voltage.h>
static struct regulator *vcck;
static struct meson_cpufreq_config cpufreq_info;

static unsigned int vcck_cur_max_freq()
{
   // return meson_vcck_cur_max_freq(vcck);
    return meson_vcck_cur_max_freq(vcck, vcck_opp_table, ARRAY_SIZE(vcck_opp_table));
}

static int vcck_scale(unsigned int frequency)
{
   // return meson_vcck_scale(vcck, frequency);
    return meson_vcck_scale(vcck, vcck_opp_table, ARRAY_SIZE(vcck_opp_table),
                            frequency);
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

#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
#include <linux/input.h>
#include <linux/adc_keypad.h>

static struct adc_key adc_kp_key[] = {
    {KEY_VOLUMEDOWN,    "vol-", CHAN_4, 150, 40},
    {KEY_VOLUMEUP,      "vol+", CHAN_4, 275, 40},
};

static struct adc_kp_platform_data adc_kp_pdata = {
    .key = &adc_kp_key[0],
    .key_num = ARRAY_SIZE(adc_kp_key),
};

static struct platform_device adc_kp_device = {
    .name = "m1-adckp",
    .id = 0,
    .num_resources = 0,
    .resource = NULL,
    .dev = {
        .platform_data = &adc_kp_pdata,
    }
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
}

static void dummy_codec_device_deinit(void)
{
    pinmux_clr(&dummy_codec_pinmux_set);
}


static struct dummy_codec_platform_data dummy_codec_pdata = {
    .device_init    = dummy_codec_device_init,
    .device_uninit  = dummy_codec_device_deinit,
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

#if defined(CONFIG_AM_DVB)
static struct resource amlfe_resource[]  = {
    [0] = {
        .start = 0,                                    //frontend  i2c adapter id
        .end   = 0,
        .flags = IORESOURCE_MEM,
        .name  = "frontend0_i2c"
    },
    [1] = {
        .start = 0xC0,                                 //frontend  tuner address
        .end   = 0xC0,
        .flags = IORESOURCE_MEM,
        .name  = "frontend0_tuner_addr"
    },
    [2] = {
        .start = 4,                   //frontend   mode 0-dvbc 1-dvbt 2-isdbt 3-dtmb,4-atsc
        .end   = 4,
        .flags = IORESOURCE_MEM,
        .name  = "frontend0_mode"
    },
    [3] = {
        .start = 7,                   //frontend  tuner 0-NULL, 1-DCT7070, 2-Maxliner, 3-FJ2207, 4-TD1316
        .end   = 7,
        .flags = IORESOURCE_MEM,
        .name  = "frontend0_tuner"
    },
};

static  struct platform_device amlfe_device = {
    .name             = "amlfe",
    .id               = -1,
    .num_resources    = ARRAY_SIZE(amlfe_resource),
    .resource         = amlfe_resource,
};



static struct resource amlogic_dvb_resource[]  = {
    [0] = {
        .start = INT_DEMUX,                   //demux 0 irq
        .end   = INT_DEMUX,
        .flags = IORESOURCE_IRQ,
        .name  = "demux0_irq"
    },
    [1] = {
        .start = INT_DEMUX_1,                    //demux 1 irq
        .end   = INT_DEMUX_1,
        .flags = IORESOURCE_IRQ,
        .name  = "demux1_irq"
    },
    [2] = {
        .start = INT_DEMUX_2,                    //demux 2 irq
        .end   = INT_DEMUX_2,
        .flags = IORESOURCE_IRQ,
        .name  = "demux2_irq"
    },
    [3] = {
        .start = INT_ASYNC_FIFO_FLUSH,                   //dvr 0 irq
        .end   = INT_ASYNC_FIFO_FLUSH,
        .flags = IORESOURCE_IRQ,
        .name  = "dvr0_irq"
    },
    [4] = {
        .start = INT_ASYNC_FIFO2_FLUSH,          //dvr 1 irq
        .end   = INT_ASYNC_FIFO2_FLUSH,
        .flags = IORESOURCE_IRQ,
        .name  = "dvr1_irq"
    },
};

static  struct platform_device amlogic_dvb_device = {
    .name             = "amlogic-dvb",
    .id               = -1,
    .num_resources    = ARRAY_SIZE(amlogic_dvb_resource),
    .resource         = amlogic_dvb_resource,
};

static int m6skt_dvb_init(void)
{
    {
//#define FEC_B
        static pinmux_item_t fec_pins[] = {
            {/*fec mode*/
                .reg = PINMUX_REG(0),
                .clrmask = 1 << 6
            },
#if FEC_B    /*FEC_B*/
            {
                .reg = PINMUX_REG(6),
                .setmask = 0x1f << 19
            },
            {
                .reg = PINMUX_REG(3),
                .clrmask = 1 << 5
            },
#else        /*FEC_A*/
            {
                .reg = PINMUX_REG(3),
                .setmask = 0x3f
            },
            {
                .reg = PINMUX_REG(6),
                .clrmask = 0x1f << 19
            },
#endif
            PINMUX_END_ITEM
        };
        static pinmux_set_t fec_pinmux_set = {
            .chip_select = NULL,
            .pinmux = &fec_pins[0]
        };
        pinmux_set(&fec_pinmux_set);
    }

    {
        static pinmux_item_t i2c_pins[] = {
            {/*sw i2c*/
                .reg = PINMUX_REG(5),
                .clrmask = 0xf << 28
            },
            PINMUX_END_ITEM
        };

        static pinmux_set_t i2c_pinmux_set = {
            .chip_select = NULL,
            .pinmux = &i2c_pins[0]
        };
        pinmux_set(&i2c_pinmux_set);
    }
    return 0;
}

#ifdef CONFIG_AM_DIB7090P
static struct resource dib7090p_resource[]  = {

    [0] = {
        .start = 0,                                    //frontend  i2c adapter id
        .end   = 0,
        .flags = IORESOURCE_MEM,
        .name  = "frontend0_i2c"
    },
    [1] = {
        .start = 0x10,                                 //frontend 0 demod address
        .end   = 0x10,
        .flags = IORESOURCE_MEM,
        .name  = "frontend0_demod_addr"
    },
};

static  struct platform_device dib7090p_device = {
    .name             = "DiB7090P",
    .id               = -1,
    .num_resources    = ARRAY_SIZE(dib7090p_resource),
    .resource         = dib7090p_resource,
};
#endif
#endif // defined(CONFIG_AM_DVB)

#ifdef CONFIG_AM_SMARTCARD
static int m6skt_smc_init(void)
{
    {
        static pinmux_item_t smc_pins[] = {
            {/*disable i2s_in*/
                .reg = PINMUX_REG(8),
                .clrmask = 0x7f << 24
            },
            {/*disable uart_b*/
                .reg = PINMUX_REG(4),
                .clrmask = 0xf << 6
            },
            {/*enable 7816*/
                .reg = PINMUX_REG(4),
                .setmask = 0xf << 18
            },
            {/*disable pcm*/
                .reg = PINMUX_REG(4),
                .clrmask = 0xf << 22
            },
            PINMUX_END_ITEM
        };

        static pinmux_set_t smc_pinmux_set = {
            .chip_select = NULL,
            .pinmux = &smc_pins[0]
        };
        pinmux_set(&smc_pinmux_set);
    }
    return 0;
}

static struct resource smartcard_resource[] =
{
    [0] = {
           .start = PAD_GPIOX_18,
        .end   = PAD_GPIOX_18,
        .flags = IORESOURCE_MEM,
        .name  = "smc0_reset"
    },
    [1] = {
        .start = INT_SMART_CARD,
        .end   = INT_SMART_CARD,
        .flags = IORESOURCE_IRQ,
        .name  = "smc0_irq"
    },
};

static struct platform_device smartcard_device =
    {

        .name             = "amlogic-smc",
        .id                 = -1,
        .num_resources    = ARRAY_SIZE(smartcard_resource),
        .resource         = smartcard_resource,
};

#endif //def CONFIG_AM_SMARTCARD
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
#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
    &aml_i2c_device_a,
    &aml_i2c_device_b,
    &aml_i2c_device_ao,
#endif
#if defined(CONFIG_I2C_SW_AML)
    &aml_sw_i2c_device_1,
#endif
    &aml_uart_device,
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
#if defined(CONFIG_ADC_KEYPADS_AM)||defined(CONFIG_ADC_KEYPADS_AM_MODULE)
    &adc_kp_device,
#endif
#if defined(CONFIG_KEY_INPUT_CUSTOM_AM) || defined(CONFIG_KEY_INPUT_CUSTOM_AM_MODULE)
    &input_device_key,
#endif
#if defined(CONFIG_CARDREADER)
    &meson_card_device,
#endif // CONFIG_CARDREADER
#if defined(CONFIG_MMC_AML)
    &aml_mmc_device,
#endif
#if defined(CONFIG_SUSPEND)
    &aml_pm_device,
#endif
#ifdef CONFIG_EFUSE
    &aml_efuse_device,
#endif
#if defined(CONFIG_AML_RTC)
    &aml_rtc_device,
#endif
#ifdef CONFIG_MESON_TRUSTZONE
    //&aml_secure_monitor_device,
#endif
    &aml_audio,
    &aml_audio_dai,
#if defined(CONFIG_SND_SOC_DUMMY_CODEC)
    &aml_dummy_codec_audio,
    &aml_dummy_codec,
#endif
#ifdef CONFIG_AM_WIFI
    &wifi_power_device,
#endif
#ifdef CONFIG_BT_DEVICE
    &bt_device,
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
#ifdef CONFIG_AM_DVB
    &amlogic_dvb_device,
    &amlfe_device,
#endif
#ifdef CONFIG_AM_DIB7090P
    &dib7090p_device,
#endif

#ifdef CONFIG_AM_SMARTCARD
    &smartcard_device,
#endif

#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
    &meson_cs_dcdc_regulator_device,
#endif
#ifdef CONFIG_CPU_FREQ
    &meson_cpufreq_device,
#endif
#if defined(CONFIG_AML_CARD_KEY) || defined(CONFIG_AML_NAND_KEY)
    &aml_keys_device,
#endif
#ifdef CONFIG_AML_VIDEO_RES_MGR
    &resmgr_device,
#endif
};

static __init void meson_init_machine(void)
{
//    meson_cache_init();

    /**
     *  Meson6 socket board ONLY
     *  Do *NOT* merge for other BSP
     */
    #ifdef CONFIG_AM_WIFI_SD_MMC
    wifi_dev_init();
    #endif
    #ifdef CONFIG_AML_RTC
    RTC_clock_enable();
    #endif

    aml_set_reg32_bits(AOBUS_REG_ADDR(0x24), 0,  3, 1);
    aml_set_reg32_bits(AOBUS_REG_ADDR(0x24), 0, 19, 1);
    aml_set_reg32_bits(AOBUS_REG_ADDR(0x24), 0,  2, 1);
    aml_set_reg32_bits(AOBUS_REG_ADDR(0x24), 1, 18, 1);
    aml_set_reg32_bits(P_HHI_VIID_PLL_CNTL, 1, 30, 1);//close video2 pll
    aml_write_reg32(P_LED_PWM_REG0, 0x63100b);


#ifdef CONFIG_MESON_CS_DCDC_REGULATOR
    vcck_pwm_init();
#endif

#ifdef CONFIG_AML_HDMI_TX
    extern int setup_hdmi_dev_platdata(void* platform_data);
    setup_hdmi_dev_platdata(&aml_hdmi_pdata);
#endif
#ifdef CONFIG_AM_REMOTE
    setup_remote_device();
#endif
#ifdef CONFIG_EFUSE
//    setup_aml_efuse();
#endif
    setup_usb_devices();
    setup_devices_resource();
    platform_add_devices(platform_devs, ARRAY_SIZE(platform_devs));
#ifdef CONFIG_AM_WIFI_USB
    if(wifi_plat_data.usb_set_power)
        wifi_plat_data.usb_set_power(0);//power off built-in usb wifi
#endif
#if defined(CONFIG_SUSPEND)
    {//todo: remove it after verified. need set it in uboot environment variable.
    extern  int console_suspend_enabled;
    console_suspend_enabled = 0;
    }
#endif

#if defined(CONFIG_I2C_AML) || defined(CONFIG_I2C_HW_AML)
 //   aml_i2c_init();

#if 1
    {
        static pinmux_item_t i2c_pins[] = {
            {/*sw i2c*/
                .reg = PINMUX_REG(5),
                .clrmask = 0xf << 24
            },
            PINMUX_END_ITEM
        };

        static pinmux_set_t i2c_pinmux_set = {
            .chip_select = NULL,
            .pinmux = &i2c_pins[0]
        };
        pinmux_set(&i2c_pinmux_set);
    }
#endif


#endif

//    CLEAR_CBUS_REG_MASK(PREG_PAD_GPIO2_EN_N, (1<<21));
//    SET_CBUS_REG_MASK(PREG_PAD_GPIO2_O, (1<<21));
#ifdef CONFIG_AM_DVB
  // m6skt_dvb_init();
    gpio_out(PAD_GPIOD_6, 0);
    msleep(200);
    gpio_out(PAD_GPIOD_6, 1);//tuner_enable
    printk("gpio D_6,tuner_enable\r\n");
    {
#define FEC_B
        static pinmux_item_t fec_pins[] = {
            {/*fec mode*/
                .reg = PINMUX_REG(0),
                .clrmask = 1 << 6
            },
#ifdef FEC_B    /*FEC_B*/
            {
                .reg = PINMUX_REG(3),
                .setmask = 0x3f << 6
            },
            {
                .reg = PINMUX_REG(0),
                .clrmask = 0xf << 0
            },
            {
                .reg = PINMUX_REG(5),
                .clrmask = 0xff << 16
            },
#else        /*FEC_A*/
            {
                .reg = PINMUX_REG(3),
                .setmask = 0x3f
            },
            {
                .reg = PINMUX_REG(6),
                .clrmask = 0x1f << 19
            },
#endif
            PINMUX_END_ITEM
        };
        static pinmux_set_t fec_pinmux_set = {
            .chip_select = NULL,
            .pinmux = &fec_pins[0]
        };
        pinmux_set(&fec_pinmux_set);
    }

#endif // CONFIG_AM_DVB
#ifdef CONFIG_AM_SMARTCARD
   m6skt_smc_init();
#endif
}
static __init void meson_init_early(void)
{///boot seq 1

}

MACHINE_START(MESON6_G35, "Amlogic Meson6 g35 customer platform")
    .boot_params    = BOOT_PARAMS_OFFSET,
    .map_io         = meson_map_io,///2
    .init_early     = meson_init_early,///3
    .init_irq       = meson_init_irq,///0
    .timer          = &meson_sys_timer,
    .init_machine   = meson_init_machine,
    .fixup          = meson_fixup,///1
MACHINE_END
