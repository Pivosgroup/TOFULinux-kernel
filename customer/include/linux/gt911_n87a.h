/* drivers/input/touchscreen/gt813_827_828.h
 *
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version:1.0
 *      V1.0:2012/08/31,first release.
 */

#ifndef      _LINUX_GOODIX_TOUCH_H
#define      _LINUX_GOODIX_TOUCH_H

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <mach/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <mach/gpio_data.h>
#include <linux/ctp.h>

struct goodix_ts_data {
    spinlock_t irq_lock;
    struct i2c_client *client;
    struct input_dev  *input_dev;
    struct hrtimer timer;
    struct work_struct  work;
    struct early_suspend early_suspend;
    s32 irq_is_disable;
    s32 use_irq;
    u16 abs_x_max;
    u16 abs_y_max;
    u8  max_touch_num;
    u8  int_trigger_type;
    u8  green_wake_mode;
    u8  chip_type;
    u8  enter_update;
    u8  gtp_is_suspend;
    u8  gtp_rawdiff_mode;
    u8  gtp_cfg_len;
};

//***************************PART1:ON/OFF define*******************************
#define GTP_DRIVER_SEND_CFG   1
#define GTP_POWER_CTRL_SLEEP  1
#define GTP_AUTO_UPDATE       1
#define GTP_CHANGE_X2Y        0
#define GTP_ESD_PROTECT       0
#define GTP_ICS_SLOT_REPORT   0
#define GTP_DEBUG_ON          0
#define GTP_MAX_HEIGHT   4096
#define GTP_MAX_WIDTH    4096
#define GTP_INT_TRIGGER  1
//***************************PART2:TODO define**********************************
//STEP_1(REQUIRED):Change config table.
/*TODO: puts the config info corresponded to your TP here, the following is just
a sample config, send this config should cause the chip cannot work normally*/
//default or float
#define CTP_CFG_GROUP1 {0x50,0x00,0x04,0x00,0x03,0x05,0x35,0x00,0x00,0x3F,0x14,0x0F,0x64,0x50,0x03,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x17,0x19,0x1C,0x14,0x8C,0x27,0x0E,0x18,0x1B,0x9E,0x0F,0x00,0x00,0x00,0x02,0x03,0x35,0x00,0x00,0x00,0x00,0x00,0x03,0x64,0x32,0x00,0x00,0x00,0x32,0xFA,0x01,0x00,0x00,0x08,0x00,0x00,0x7B,0x15,0x15,0x7B,0x15,0x1D,0x4B,0x1D,0x23,0x4B,0x1D,0x29,0x33,0x21,0x39,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0F,0x10,0x12,0x13,0x14,0x16,0x18,0x1C,0x1D,0x1E,0x1F,0x20,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x5C,0x01}
//TODO puts your group2 config info here,if need.
//VDDIO
#define CTP_CFG_GROUP2 {\
    }
//TODO puts your group3 config info here,if need.
//GND
#define CTP_CFG_GROUP3 {\
    }

//STEP_2(REQUIRED):Change I/O define & I/O operation mode.
#define GTP_RST_PORT    PAD_GPIOC_3
#define GTP_INT_PORT    PAD_GPIOA_16
#define GTP_INT_IRQ     INT_GPIO_0
//#define GTP_INT_CFG     S3C_GPIO_SFN(0xF)

#define GTP_GPIO_AS_INPUT(pin)          do{\
                                              gpio_set_status(pin, gpio_status_in); \
                                        }while(0)
#define GTP_GPIO_AS_INT(pin)            do{\
                                              gpio_set_status(pin, gpio_status_in);\
                                              gpio_irq_set(170, GPIO_IRQ( (GTP_INT_IRQ -INT_GPIO_0), GPIO_IRQ_FALLING)); \
                                        }while(0)
#define GTP_GPIO_OUTPUT(pin,level)        do{\
                                              gpio_set_status(pin,gpio_status_out); \
                                              gpio_out(pin,level); \
                                          }while(0)
#define GTP_IRQ_TAB                     {IRQ_TYPE_EDGE_RISING, IRQ_TYPE_EDGE_FALLING, IRQ_TYPE_LEVEL_LOW, IRQ_TYPE_LEVEL_HIGH}

//STEP_3(optional):Custom set some config by themself,if need.
#define GTP_MAX_TOUCH         5
#define GTP_ESD_CHECK_CIRCLE  2000

//STEP_4(optional):If this project have touch key,Set touch key config.

//***************************PART3:OTHER define*********************************
#define GTP_DRIVER_VERSION    "V1.2<2013/01/29>"
#define GTP_I2C_NAME          "Goodix_GT911"
#define GTP_POLL_TIME         10
#define GTP_ADDR_LENGTH       2
#define GTP_CONFIG_MAX_LENGTH 240

//Register define
#define GTP_READ_COOR_ADDR    0x814E
#define GTP_REG_SLEEP         0x8040
#define GTP_REG_SENSOR_ID     0x814A
#define GTP_REG_CONFIG_DATA   0x8047
#define GTP_REG_VERSION       0x8140

#define RESOLUTION_LOC        3
#define TRIGGER_LOC           8

//Log define
#define GTP_INFO(fmt,arg...)           printk("<<-GTP-INFO->> "fmt"\n",##arg)
#define GTP_ERROR(fmt,arg...)          printk("<<-GTP-ERROR->> "fmt"\n",##arg)
#define GTP_DEBUG(fmt,arg...)          do{\
                                           if(GTP_DEBUG_ON)\
                                           printk("<<-GTP-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg);\
                                       }while(0)
#define GTP_SWAP(x, y)                 do{\
                                           typeof(x) z = x;\
                                           x = y;\
                                           y = z;\
                                       }while (0)
//*****************************End of Part III********************************

#endif /* _LINUX_GOODIX_TOUCH_H */
