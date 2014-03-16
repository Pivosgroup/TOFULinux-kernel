/*++
 
 Copyright (c) 2012-2022 ChipOne Technology (Beijing) Co., Ltd. All Rights Reserved.
 This PROPRIETARY SOFTWARE is the property of ChipOne Technology (Beijing) Co., Ltd. 
 and may contains trade secrets and/or other confidential information of ChipOne 
 Technology (Beijing) Co., Ltd. This file shall not be disclosed to any third party,
 in whole or in part, without prior written consent of ChipOne.  
 THIS PROPRIETARY SOFTWARE & ANY RELATED DOCUMENTATION ARE PROVIDED AS IS, 
 WITH ALL FAULTS, & WITHOUT WARRANTY OF ANY KIND. CHIPONE DISCLAIMS ALL EXPRESS OR 
 IMPLIED WARRANTIES.  
 
 File Name:    icn83xx.c
 Abstract:
               input driver.
 Author:       Zhimin Tian
 Date :        10 30,2012
 Version:      0.1[.revision]
 History :
     Change logs.  
 --*/
#include <linux/i2c.h>
#include <linux/input.h>
#include "icn83xx.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
    #include <linux/pm.h>
    #include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/input/mt.h>

#include <linux/init.h>  
#include <linux/kernel.h>  
#include <linux/fs.h>  
 #include <linux/semaphore.h>  
#include <linux/cdev.h>  
#include <linux/device.h>  
#include <linux/ioctl.h>  
#include <linux/slab.h>  
#include <linux/errno.h>  
#include <linux/string.h>  
#include <linux/spinlock_types.h> 
#include <linux/irq.h>

#include <mach/irqs.h>
//#include <mach/system.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/gpio_data.h>
//#include <../../boards/board-m6g04-mtc848.h>

#define INCLUDE_LIB
//#undef INCLUDE_LIB

#ifdef INCLUDE_LIB
//#include "allinc.h"
#endif

//#define TOUCH_VIRTUAL_KEYS
#define SUPPORT_APK_OP
#define SUPPORT_ATTR_OP

//#define PRINT_APP_INFO
//#define PRINT_INT_INFO
//#define PRINT_POINT_INFO
//#define DEBUG


#define ICN83XX_NAME	"chipone-ts"    //"chipone-ts"
#define ICN83XX_PROG_IIC_ADDR    (0x60>>1)
static struct i2c_client *this_client;

static probe_flag = 0;

#ifdef PRINT_POINT_INFO 
#define print_point_info(fmt, args...)   \
        do{                              \
                pr_info(fmt, ##args);     \
        }while(0)
#else
#define print_point_info(fmt, args...)   //
#endif

#ifdef PRINT_INT_INFO 
#define print_int_info(fmt, args...)     \
        do{                              \
                pr_info(fmt, ##args);     \
        }while(0)
#else
#define print_int_info(fmt, args...)   //
#endif

#ifdef PRINT_APP_INFO 
#define print_app_info(fmt, args...)     \
        do{                              \
                pr_info(fmt, ##args);     \
        }while(0)
#else
#define print_app_info(fmt, args...)   //
#endif

///////////////////////////////////////////////


static char revert_x_flag = 0;
static char revert_y_flag = 1;
static int work_mode = 0;
static void chipone_timer_func(struct icn83xx_ts_data *icn83xx_ts);
//static char  *firmware = "/system/vendor/modules/fw.bin";
static char  *firmware = "/system/etc/firmware/fw.bin";
	
#ifdef SUPPORT_APK_OP

#define IN_BUF_LEN 128  
#define OUT_BUF_LEN 128  

#define CMD_GET_ICTYPE   1000
#define CMD_GET_NBX      1001
#define CMD_GET_NBY      1002
#define CMD_GET_RESX     1003
#define CMD_GET_RESY     1004
#define CMD_GET_RAWDATA  1005

#define CMD_SET_ROWNUM   1006
#define CMD_GET_ROWNUM   1007
#define CMD_SET_CALIB    1008
#define CMD_SET_UPDATE   1009
#define CMD_GET_UPDATE   1010
#define CMD_SET_IIC      1011
#define CMD_GET_IIC      1012

#define CMD_FRESH_BASE   1020
#define CMD_ZERO_BASE    1021

static struct class * icn83xx_class;  
static struct cdev icn83xx_cdev;  
static dev_t devnum = 0;  
static char * modname = "icn83xx_mod";  
static char * devicename = "icn83xx";  
static char * classname = "icn83xx_class";  
  

static struct semaphore sem;  
static char * inbuffer = NULL;  
static char * outbuffer = NULL;  
static int command;
static int rownum;
static int iicaddr = 0;
static int iicvalue = 0;
static int updata_status = 0;

static int icn83xx_mod_open(struct inode *, struct file *);  
static int icn83xx_mod_release(struct inode *, struct file *);  
static ssize_t icn83xx_mod_read(struct file *, char *, size_t, loff_t *);  
static ssize_t icn83xx_mod_write(struct file *, const char *, size_t, loff_t *);  
static int icn83xx_mod_ioctl(struct file *, unsigned int, unsigned long);  

#endif

#ifdef SUPPORT_ATTR_OP

static int processbar=0;
static ssize_t icn83xx_show_version(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn83xx_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len);
static ssize_t icn83xx_show_process(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn83xx_store_process(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);



static DEVICE_ATTR(update, S_IRUGO | S_IWUSR, icn83xx_show_version, icn83xx_update);
static DEVICE_ATTR(process, S_IRUGO | S_IWUSR, icn83xx_show_process, icn83xx_store_process);


static int icn83xx_chip_init(int irq_flg)
{
	//chip reset
    //gpio_set_status(PAD_GPIOC_3, gpio_status_out);
    //gpio_out(PAD_GPIOC_3, 0);
    gpio_set_status(PAD_GPIOD_2, gpio_status_out);
    gpio_out(PAD_GPIOD_2, 0);
	mdelay(20);  //
	
    //gpio_out(PAD_GPIOC_3, 1);
    gpio_out(PAD_GPIOD_2, 1);
	mdelay(100); // 

#if 0	
	//设定分辨率为800x480
	icn83xx_write_reg(177, 0x01);        //set X_RESOLUTION_H
	mdelay(1); 
	icn83xx_write_reg(178, 0xE0);        //set X_RESOLUTION_L
	mdelay(1); 
	icn83xx_write_reg(179, 0x03);         //set Y_RESOLUTION_H
	mdelay(1); 
	icn83xx_write_reg(180, 0x20);         //set Y_RESOLUTION_L
	mdelay(1); 
#endif

	//irq init
	if(1 == irq_flg){
		gpio_set_status(PAD_GPIOA_16, gpio_status_in);
		//gpio_irq_set(PAD_GPIOA_16, GPIO_IRQ(INT_GPIO_0-INT_GPIO_0, GPIO_IRQ_RISING));
		gpio_irq_set(PAD_GPIOA_16, GPIO_IRQ(INT_GPIO_0-INT_GPIO_0, GPIO_IRQ_FALLING));
	}  	
	return 0;
}

static ssize_t icn83xx_show_process(struct device* cd,struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;

	sprintf(buf, "ICN83xx process %d\n",processbar);
	ret = strlen(buf) + 1;
	return ret;

}

static ssize_t icn83xx_store_process(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	struct icn83xx_ts_data *data = i2c_get_clientdata(this_client);
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
#ifdef INCLUDE_LIB	
	
	if(on_off == 0)
	{
		work_mode = on_off;
	}
	else if((on_off == 1) || (on_off == 2))
	{
		if(work_mode == 0)
		{
			init_timer(&(data->_timer));
			data->_timer.data = (unsigned long)data;
			data->_timer.function  =	(void (*)(unsigned long))chipone_timer_func;	
		//	add_timer(&(data->_timer));
			mod_timer(&(data->_timer), jiffies + 50);
		}
		work_mode = on_off;
	}
	
#endif
	return len;
}


static ssize_t icn83xx_show_version(struct device* cd,
				     struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;
	short fwVersion = 0;
	unsigned char uc_reg_value[4]; 

#ifdef INCLUDE_LIB
    fwVersion = icn83xx_read_fw_Ver(firmware);   
    printk("fw firmware version is 0x%x\n", fwVersion);

	fwVersion = icn83xx_readVersion();
	printk("current version: 0x%x\n", fwVersion);   

	icn83xx_readInfo(0x0, &uc_reg_value[0]);
	icn83xx_readInfo(0x1, &uc_reg_value[1]);
	icn83xx_readInfo(0x10, &uc_reg_value[2]);
	icn83xx_readInfo(0x11, &uc_reg_value[3]);
	printk("read info: 0x%2x 0x%2x 0x%2x 0x%2x\n", uc_reg_value[0], uc_reg_value[1], uc_reg_value[2], uc_reg_value[3]);
#endif	    
	sprintf(buf, "icn83xx firmware id is 0x%x\n", fwVersion);
	ret = strlen(buf) + 1;
	return ret;
}

static ssize_t icn83xx_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	int err=0;
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	unsigned char uc_reg_value[4]; 
	short fwVersion;
   
	if(on_off==1)
	{
		processbar = 1;
#ifdef INCLUDE_LIB		
		fwVersion = icn83xx_read_fw_Ver(firmware);
		printk("icn83xx update, fw firmware version is 0x%x\n", fwVersion);
		err = kernel_thread(icn83xx_fw_update,firmware,CLONE_KERNEL);
#endif		
		printk("the kernel_thread result is:%d\n", err);			
	}
	else if(on_off == 2)
	{
		processbar = 1;
		printk("icn83xx calibrate\n");

#ifdef INCLUDE_LIB		
		fwVersion = icn83xx_readVersion();
		printk("current version: 0x%x\n", fwVersion);
		
		icn83xx_readInfo(0x0, &uc_reg_value[0]);
		icn83xx_readInfo(0x1, &uc_reg_value[1]);
		icn83xx_readInfo(0x10, &uc_reg_value[2]);
		icn83xx_readInfo(0x11, &uc_reg_value[3]);
		printk("read info: 0x%2x 0x%2x 0x%2x 0x%2x\n", uc_reg_value[0], uc_reg_value[1], uc_reg_value[2], uc_reg_value[3]);
				
		//err = kernel_thread(icn83xx_calib, 0,CLONE_KERNEL);
#endif
		
		printk("the kernel_thread result is:%d\n", err);			
	}
	return len;
}

static int icn83xx_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	printk("%s: \n",__func__);
	
	err = device_create_file(dev, &dev_attr_update);
	err = device_create_file(dev, &dev_attr_process);
	
	return err;
}

#endif



#ifdef TOUCH_VIRTUAL_KEYS
#define TS_KEY_HOME	102
#define TS_KEY_MENU	229
#define TS_KEY_BACK	158
#define TS_KEY_SEARCH  158

static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
	 __stringify(EV_KEY) ":" __stringify(TS_KEY_HOME) ":900:420:60:50"
	 ":" __stringify(EV_KEY) ":" __stringify(TS_KEY_MENU) ":900:300:60:50"
	 ":" __stringify(EV_KEY) ":" __stringify(TS_KEY_BACK) ":900:180:60:50"
	 ":" __stringify(EV_KEY) ":" __stringify(TS_KEY_SEARCH) ":900:30:60:50"
	 "\n");
}

static struct kobj_attribute virtual_keys_attr = {
    .attr = {
        .name = "virtualkeys.chipone-ts",
        .mode = S_IRUGO,
    },
    .show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
    &virtual_keys_attr.attr,
    NULL
};

static struct attribute_group properties_attr_group = {
    .attrs = properties_attrs,
};

static void icn83xx_ts_virtual_keys_init(void)
{
    int ret;
    struct kobject *properties_kobj;	
	
    properties_kobj = kobject_create_and_add("board_properties", NULL);
    if (properties_kobj)
        ret = sysfs_create_group(properties_kobj,
                     &properties_attr_group);
    if (!properties_kobj || ret)
        pr_err("failed to create board_properties\n");    
}
#endif


#ifdef SUPPORT_APK_OP

struct file_operations icn83xx_mod_fops =   
{  
    .owner = THIS_MODULE,  
    .open = icn83xx_mod_open,  
    .read = icn83xx_mod_read,  
    .write = icn83xx_mod_write,  
//    .ioctl = icn83xx_mod_ioctl, 
    .unlocked_ioctl = icn83xx_mod_ioctl,
    .release = icn83xx_mod_release,  
};  
  
  

static int icn83xx_mod_open(struct inode *inode, struct file *pfile)  
{  
    print_app_info("mod_open()!\n");  
    return 0;  
}  
static int icn83xx_mod_release(struct inode *inode, struct file *pfile)  
{  
    print_app_info("mod_release()!\n");
    return 0;  
}  

static ssize_t icn83xx_mod_read(struct file *pfile, char *user_buf, size_t len, loff_t *off)  
{  
	int err = 0;  
	int i = 0;
    print_app_info("mod_read(), len: %d!\n", len);  
  
    if(down_interruptible(&sem))  
    {  
        return -ERESTARTSYS;   
    } 
	memset(outbuffer, 0 , len);
	switch(command)
	{
		case CMD_GET_ICTYPE:
			*(int *)&outbuffer[0] = 83;
			break;
		case CMD_GET_NBX:
			err = icn83xx_i2c_rxdata(161, outbuffer, 1);
			if (err < 0) {
				pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, err);				
			}
			break;
		case CMD_GET_NBY:
			err = icn83xx_i2c_rxdata(160, outbuffer, 1);
			if (err < 0) {
				pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, err);				
			}
			break;
		case CMD_GET_RESX:
			err = icn83xx_i2c_rxdata(177, outbuffer, 2);
			if (err < 0) {
				pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, err);				
			}
			swap_ab(outbuffer[0], outbuffer[1]);
			break;
		case CMD_GET_RESY:
			err = icn83xx_i2c_rxdata(179, outbuffer, 2);
			if (err < 0) {
				pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, err);				
			}
			swap_ab(outbuffer[0], outbuffer[1]);
			break;
		case CMD_GET_RAWDATA:
			icn83xx_write_reg(3, rownum);
			mdelay(1);
			err = icn83xx_i2c_rxdata(128, outbuffer, len);
			if (err < 0) {
				pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, err);				
			}	
			for(i=0; i<len; i=i+2)
			{
				swap_ab(outbuffer[i], outbuffer[i+1]);
			}	
			break;
		case CMD_GET_UPDATE:
#ifdef INCLUDE_LIB			
			*(int *)&outbuffer[0] = updata_status = icn83xx_get_status();
#endif			
			break;
		case CMD_SET_IIC:
			break;
		case CMD_GET_IIC:
			err = icn83xx_i2c_rxdata(iicaddr, outbuffer, 1);
			if (err < 0) {
				pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, err);				
			} 
		default:
			break;		
	}	
//	printk("read: %d\n", *(int *)&outbuffer[0]);
    if(copy_to_user(user_buf, outbuffer, len))  
    {  
        up(&sem);  
        return -EFAULT;  
    }  
    up(&sem);  

    return 0;  
}  
static ssize_t icn83xx_mod_write(struct file *pfile, const char *user_buf, size_t len, loff_t *off)  
{  
    print_app_info("mod_write()!\n");  
    if(down_interruptible(&sem))  
    {  
        return -ERESTARTSYS;  
    }  
    if(len > IN_BUF_LEN)  
    {  
        print_app_info("Out of input buffer\n");  
        return -ERESTARTSYS;  
    }  
    if(copy_from_user(inbuffer, user_buf, len))  
    {  
        up(&sem);  
        return -EFAULT;  
    }  
    
    switch(command)
    {
    	case CMD_SET_IIC:
    		iicvalue = *(int *)&inbuffer[0];   		
    		icn83xx_write_reg(iicaddr, iicvalue);     		
    		break;
    	default:
    		break;
    }
    
    up(&sem);     

    return 0;  
}  
static int icn83xx_mod_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg) 
{  
    int err = 0; 
    char retvalue = 0; 
    print_app_info("mod_ioctl()!\n");  
    print_app_info("cmd: %d, arg: %d\n", cmd, arg);  
    if(down_interruptible(&sem))  
    {  
        return -ERESTARTSYS;   
    } 
    command = cmd;    	
    switch(command)
    {
		case CMD_GET_ICTYPE:
			break;
		case CMD_GET_NBX:

			break;
		case CMD_GET_NBY:

			break;
		case CMD_GET_RESX:

			break;
		case CMD_GET_RESY:

			break;
		case CMD_GET_RAWDATA:

			break;
		case CMD_GET_UPDATE:
			break;    	
    	case CMD_SET_ROWNUM:
    		rownum = arg;
    		if(rownum == 0)
    		{
    			icn83xx_write_reg(4, 0x20); 
    			mdelay(1);
    			icn83xx_read_reg(2, &retvalue);
    			while(retvalue != 1)
    			{
    				mdelay(1);
    				icn83xx_read_reg(2, &retvalue);
    			}
    			
    		}
    		break;
    	case CMD_SET_UPDATE:
    		updata_status = arg;
    		if(updata_status > 0)
    		{    
#ifdef INCLUDE_LIB    						
    			err = kernel_thread(icn83xx_fw_update,firmware,CLONE_KERNEL);
#endif    			
				printk("the kernel_thread result is:%d\n", err);	
    		}
    		break;
    	case CMD_SET_IIC:
    		iicaddr = arg;
    		break;
    	case CMD_GET_IIC:
    		iicaddr = arg;    		  		
    		break;
    			
    	default:
    		break;	
    }
	up(&sem); 
    return err;  
}  
#endif



/* ---------------------------------------------------------------------
*
*   Chipone panel upgrade related driver
*
*
----------------------------------------------------------------------*/

/***********************************************************************************************
Name	:	icn83xx_prog_i2c_rxdata 
Input	:	addr
            *rxdata
            length
Output	:	ret
function	: read data from icn83xx, prog mode	
***********************************************************************************************/
int icn83xx_prog_i2c_rxdata(unsigned short addr, char *rxdata, int length)
{
	int ret;
#if 0	
	struct i2c_msg msgs[] = {	
		{
			.addr	= ICN83XX_PROG_IIC_ADDR,//this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
			.scl_rate = ICN83XX_I2C_SCL,

		},
	};
		
	icn83xx_prog_i2c_txdata(addr, NULL, 0);

 	ret = i2c_transfer(this_client->adapter, msgs, 1);
	if (ret < 0)
		pr_info("msg1 %s i2c read error: %d\n", __func__, ret);	
#else
	unsigned char tmp_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr	= ICN83XX_PROG_IIC_ADDR,//this_client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= tmp_buf,
		//	.scl_rate = ICN83XX_I2C_SCL,
		},
		{
			.addr	= ICN83XX_PROG_IIC_ADDR,//this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		//	.scl_rate = ICN83XX_I2C_SCL,
		},
	};
	tmp_buf[0] = U16HIBYTE(addr);
	tmp_buf[1] = U16LOBYTE(addr);		
 	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_info("msg1 %s i2c read error: %d\n", __func__, ret);	
#endif		
	return ret;
}
/***********************************************************************************************
Name	:	icn83xx_prog_i2c_txdata 
Input	:	addr
            *rxdata
            length
Output	:	ret
function	: send data to icn83xx , prog mode
***********************************************************************************************/
int icn83xx_prog_i2c_txdata(unsigned short addr, char *txdata, int length)
{
	int ret;
	char tmp_buf[128];

	struct i2c_msg msg[] = {
		{
			.addr	= ICN83XX_PROG_IIC_ADDR,//this_client->addr,
			.flags	= 0,
			.len	= length + 2,
			.buf	= tmp_buf,
		//	.scl_rate = ICN83XX_I2C_SCL,
		},
	};
	
	if (length > 125)
	{
		printk("%s too big datalen = %d!\n", __func__, length);
		return -1;
	}
	
	tmp_buf[0] = U16HIBYTE(addr);
	tmp_buf[1] = U16LOBYTE(addr);

	if (length != 0 && txdata != NULL)
	{
		memcpy(&tmp_buf[2], txdata, length);
	}	
	
   	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}
/***********************************************************************************************
Name	:	icn83xx_prog_write_reg
Input	:	addr -- address
            para -- parameter
Output	:	
function	:	write register of icn83xx, prog mode
***********************************************************************************************/
int icn83xx_prog_write_reg(unsigned short addr, char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn83xx_prog_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        pr_err("write reg failed! %#x ret: %d\n", buf[0], ret);
        return -1;
    }
    
    return 0;
}


/***********************************************************************************************
Name	:	icn83xx_prog_read_reg 
Input	:	addr
            pdata
Output	:	
function	:	read register of icn83xx, prog mode
***********************************************************************************************/
int icn83xx_prog_read_reg(unsigned short addr, char *pdata)
{
	int ret;
	ret = icn83xx_prog_i2c_rxdata(addr, pdata, 1);  
	return ret;  
  
}




/***********************************************************************************************
Name	:	icn83xx_i2c_rxdata 
Input	:	addr
            *rxdata
            length
Output	:	ret
function	: read data from icn83xx, normal mode	
***********************************************************************************************/
int icn83xx_i2c_rxdata(unsigned char addr, char *rxdata, int length)
{
	int ret;
#if 0
	struct i2c_msg msgs[] = {	
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
			.scl_rate = ICN83XX_I2C_SCL,
		},
	};
		
	icn83xx_i2c_txdata(addr, NULL, 0);

 	ret = i2c_transfer(this_client->adapter, msgs, 1);
	if (ret < 0)
		pr_info("msg1 %s i2c read error: %d\n", __func__, ret);	

#else
	unsigned char tmp_buf[1];
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= tmp_buf,
			//.scl_rate = ICN83XX_I2C_SCL,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		//	.scl_rate = ICN83XX_I2C_SCL,
		},
	};
	tmp_buf[0] = addr;	
 	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_info("msg1 %s i2c read error: %d\n", __func__, ret);	
#endif
//	printk("icn83xx_i2c_rxdata: %d\n", length);
	
	return ret;
}
/***********************************************************************************************
Name	:	icn83xx_i2c_txdata 
Input	:	addr
            *rxdata
            length
Output	:	ret
function	: send data to icn83xx , normal mode
***********************************************************************************************/
int icn83xx_i2c_txdata(unsigned char addr, char *txdata, int length)
{
	int ret;
	unsigned char tmp_buf[128];

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length + 1,
			.buf	= tmp_buf,
			//.scl_rate = ICN83XX_I2C_SCL,
		},
	};
	
	if (length > 125)
	{
		printk("%s too big datalen = %d!\n", __func__, length);
		return -1;
	}
	
	tmp_buf[0] = addr;

	if (length != 0 && txdata != NULL)
	{
		memcpy(&tmp_buf[1], txdata, length);
	}	
	
   	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}

/***********************************************************************************************
Name	:	icn83xx_write_reg
Input	:	addr -- address
            para -- parameter
Output	:	
function	:	write register of icn83xx, normal mode
***********************************************************************************************/
int icn83xx_write_reg(unsigned char addr, char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn83xx_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        pr_err("write reg failed! %#x ret: %d\n", buf[0], ret);
        return -1;
    }
    
    return 0;
}


/***********************************************************************************************
Name	:	icn83xx_read_reg 
Input	:	addr
            pdata
Output	:	
function	:	read register of icn83xx, normal mode
***********************************************************************************************/
int icn83xx_read_reg(unsigned char addr, char *pdata)
{
	int ret;
	ret = icn83xx_i2c_rxdata(addr, pdata, 1);  
	return ret;  
  
}


static void icn83xx_irq_enable(struct icn83xx_ts_data *ts)
{
	unsigned long irqflags;
	//pr_info("==icn83xx_irq_enable ==\n");
	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (ts->irq_is_disable)
	{
		enable_irq(ts->irq);
		ts->irq_is_disable = 0;
		//printk("enable ts->irq = %d\n",ts->irq);
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

static void icn83xx_irq_disable(struct icn83xx_ts_data *ts)
{
	unsigned long irqflags;
	//pr_info("==icn83xx_irq_disable ==\n");
	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (!ts->irq_is_disable)
	{
		disable_irq_nosync(ts->irq);
		ts->irq_is_disable = 1;
	//	printk("disable ts->irq = %d\n",ts->irq);
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}


static void icn83xx_ts_release(void)
{
	struct icn83xx_ts_data *data = i2c_get_clientdata(this_client);
	pr_info("==icn83xx_ts_release ==\n");
#ifdef CONFIG_ICN83XX_MULTITOUCH	
	input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
#else
	input_report_abs(data->input_dev, ABS_PRESSURE, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
#endif
	
	input_sync(data->input_dev);
	return;

}

static int icn83xx_read_data(void)
{
	struct icn83xx_ts_data *data = i2c_get_clientdata(this_client);
	char buf[POINT_NUM*POINT_SIZE+3]={0};
	int ret = -1;
	int i;
//	pr_info("==icn83xx_read_data ==\n");
	ret = icn83xx_i2c_rxdata(16, buf, POINT_NUM*POINT_SIZE+2);
	if (ret < 0) {
		pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	data->point_num = buf[1];
	
	if (data->point_num == 0) {
		icn83xx_ts_release();
		return 1; 
	}	
	for(i=0;i<data->point_num;i++){
		if(buf[8 + POINT_SIZE*i]  != 4) break ;
	}
	
	if(i == data->point_num) {
		icn83xx_ts_release();
		return 1; 
	}
	

	switch(data->point_num)
	{
		case 5:
			data->point_info[4].u8ID = buf[2 + POINT_SIZE*4];
			data->point_info[4].u16PosX = (buf[3 + POINT_SIZE*4]<<8) + buf[4 + POINT_SIZE*4];
			data->point_info[4].u16PosY = (buf[5 + POINT_SIZE*4]<<8) + buf[6 + POINT_SIZE*4];
			data->point_info[4].u8Pressure = 200;//buf[7 + POINT_SIZE*4];
			data->point_info[4].u8EventId = buf[8 + POINT_SIZE*4];	
			
			if(1 == revert_x_flag)
			{
				data->point_info[4].u16PosX = SCREEN_MAX_X - data->point_info[4].u16PosX;
			}
			if(1 == revert_y_flag)
			{
				data->point_info[4].u16PosY = SCREEN_MAX_Y - data->point_info[4].u16PosY;
			}
			print_int_info("u8ID %d\n", data->point_info[4].u8ID);
			print_int_info("u16PosX %d\n", data->point_info[4].u16PosX);
			print_int_info("u16PosY %d\n", data->point_info[4].u16PosY);
			print_int_info("u8Pressure %d\n", data->point_info[4].u8Pressure);
			print_int_info("u8EventId %d\n", data->point_info[4].u8EventId);	
						
		case 4:
			data->point_info[3].u8ID = buf[2 + POINT_SIZE*3];
			data->point_info[3].u16PosX = (buf[3 + POINT_SIZE*3]<<8) + buf[4 + POINT_SIZE*3];
			data->point_info[3].u16PosY = (buf[5 + POINT_SIZE*3]<<8) + buf[6 + POINT_SIZE*3];
			data->point_info[3].u8Pressure = 200;//buf[7 + POINT_SIZE*3];
			data->point_info[3].u8EventId = buf[8 + POINT_SIZE*3];
			if(1 == revert_x_flag)
			{
				data->point_info[3].u16PosX = SCREEN_MAX_X - data->point_info[3].u16PosX;
			}
			if(1 == revert_y_flag)
			{
				data->point_info[3].u16PosY = SCREEN_MAX_Y - data->point_info[3].u16PosY;
			}			
			print_int_info("u8ID %d\n", data->point_info[3].u8ID);
			print_int_info("u16PosX %d\n", data->point_info[3].u16PosX);
			print_int_info("u16PosY %d\n", data->point_info[3].u16PosY);
			print_int_info("u8Pressure %d\n", data->point_info[3].u8Pressure);
			print_int_info("u8EventId %d\n", data->point_info[3].u8EventId);								
		case 3:
			data->point_info[2].u8ID = buf[2 + POINT_SIZE*2];
			data->point_info[2].u16PosX = (buf[3 + POINT_SIZE*2]<<8) + buf[4 + POINT_SIZE*2];
			data->point_info[2].u16PosY = (buf[5 + POINT_SIZE*2]<<8) + buf[6 + POINT_SIZE*2];
			data->point_info[2].u8Pressure = 200;//buf[7 + POINT_SIZE*2];
			data->point_info[2].u8EventId = buf[8 + POINT_SIZE*2];		
			if(1 == revert_x_flag)
			{
				data->point_info[2].u16PosX = SCREEN_MAX_X - data->point_info[2].u16PosX;
			}
			if(1 == revert_y_flag)
			{
				data->point_info[2].u16PosY = SCREEN_MAX_Y - data->point_info[2].u16PosY;
			}			
			print_int_info("u8ID %d\n", data->point_info[2].u8ID);
			print_int_info("u16PosX %d\n", data->point_info[2].u16PosX);
			print_int_info("u16PosY %d\n", data->point_info[2].u16PosY);
			print_int_info("u8Pressure %d\n", data->point_info[2].u8Pressure);
			print_int_info("u8EventId %d\n", data->point_info[2].u8EventId);						
		case 2:
			data->point_info[1].u8ID = buf[2 + POINT_SIZE];
			data->point_info[1].u16PosX = (buf[3 + POINT_SIZE]<<8) + buf[4 + POINT_SIZE];
			data->point_info[1].u16PosY = (buf[5 + POINT_SIZE]<<8) + buf[6 + POINT_SIZE];
			data->point_info[1].u8Pressure = 200;//buf[7 + POINT_SIZE];
			data->point_info[1].u8EventId = buf[8 + POINT_SIZE];	
			if(1 == revert_x_flag)
			{
				data->point_info[1].u16PosX = SCREEN_MAX_X - data->point_info[1].u16PosX;
			}
			if(1 == revert_y_flag)
			{
				data->point_info[1].u16PosY = SCREEN_MAX_Y - data->point_info[1].u16PosY;
			}
			
			print_int_info("u8ID %d\n", data->point_info[1].u8ID);
			print_int_info("u16PosX %d\n", data->point_info[1].u16PosX);
			print_int_info("u16PosY %d\n", data->point_info[1].u16PosY);
			print_int_info("u8Pressure %d\n", data->point_info[1].u8Pressure);
			print_int_info("u8EventId %d\n", data->point_info[1].u8EventId);					
		case 1:
			data->point_info[0].u8ID = buf[2];
			data->point_info[0].u16PosX = (buf[3]<<8) + buf[4];
			data->point_info[0].u16PosY = (buf[5]<<8) + buf[6];
			data->point_info[0].u8Pressure = 200;//buf[7];
			data->point_info[0].u8EventId = buf[8];
			if(1 == revert_x_flag)
			{
				data->point_info[0].u16PosX = SCREEN_MAX_X - data->point_info[0].u16PosX;
			}
			if(1 == revert_y_flag)
			{
				data->point_info[0].u16PosY = SCREEN_MAX_Y - data->point_info[0].u16PosY;
			}			
			print_int_info("u8ID %d\n", data->point_info[0].u8ID);
			print_int_info("u16PosX %d\n", data->point_info[0].u16PosX);
			print_int_info("u16PosY %d\n", data->point_info[0].u16PosY);
			print_int_info("u8Pressure %d\n", data->point_info[0].u8Pressure);
			print_int_info("u8EventId %d\n", data->point_info[0].u8EventId);
		
	}

    return 0;
}


static void icn83xx_report_multitouch(void)
{
	struct icn83xx_ts_data *data = i2c_get_clientdata(this_client);
//	struct ts_event *event = &data->event;
//	pr_info("==icn83xx_report_multitouch =\n");

	switch(data->point_num) {
	case 5:
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, data->point_info[4].u8ID);	
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->point_info[4].u8Pressure);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->point_info[4].u16PosX);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->point_info[4].u16PosY);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);
		print_point_info("===x4 = %d,y4 = %d, press = %d ====\n",data->point_info[4].u16PosX,data->point_info[4].u16PosY, data->point_info[4].u8Pressure);
	case 4:
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, data->point_info[3].u8ID);	
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->point_info[3].u8Pressure);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->point_info[3].u16PosX);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->point_info[3].u16PosY);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);
		print_point_info("===x3 = %d,y3 = %d, press = %d ====\n",data->point_info[3].u16PosX,data->point_info[3].u16PosY, data->point_info[3].u8Pressure);
	case 3:
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, data->point_info[2].u8ID);	
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->point_info[2].u8Pressure);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->point_info[2].u16PosX);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->point_info[2].u16PosY);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);
		print_point_info("===x2 = %d,y2 = %d, press = %d ====\n",data->point_info[2].u16PosX,data->point_info[2].u16PosY, data->point_info[2].u8Pressure);
	case 2:
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, data->point_info[1].u8ID);	
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->point_info[1].u8Pressure);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->point_info[1].u16PosX);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->point_info[1].u16PosY);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);
		print_point_info("===x1 = %d,y1 = %d, press = %d ====\n",data->point_info[1].u16PosX,data->point_info[1].u16PosY, data->point_info[1].u8Pressure);
	case 1:
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, data->point_info[0].u8ID);	
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->point_info[0].u8Pressure);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->point_info[0].u16PosX);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->point_info[0].u16PosY);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
		input_mt_sync(data->input_dev);
		print_point_info("===x0 = %d,y0 = %d, press = %d ====\n",data->point_info[0].u16PosX,data->point_info[0].u16PosY, data->point_info[0].u8Pressure);
		break;
	default:
		print_point_info("==touch_point default =\n");
		break;
	}

	input_sync(data->input_dev);
	

	return;
}

#ifndef CONFIG_ICN83XX_MULTITOUCH
static void icn83xx_report_singletouch(void)
{
	return;
}
#endif

static void icn83xx_report_value(void)
{
//	pr_info("==icn83xx_report_value =\n");
#ifdef CONFIG_ICN83XX_MULTITOUCH
	icn83xx_report_multitouch();
#else	/* CONFIG_ICN83XX_MULTITOUCH*/
	icn83xx_report_singletouch();
#endif	/* CONFIG_ICN83XX_MULTITOUCH*/
	return;
}	/*end icn83xx_report_value*/

static void icn83xx_report_value_B(void)
{
	struct icn83xx_ts_data *data = i2c_get_clientdata(this_client);
	char buf[POINT_NUM*POINT_SIZE+3]={0};
	static u8 finger_last[POINT_NUM + 1]={0};
	u8  finger_current[POINT_NUM + 1] = {0};
//	u8  coor_data[POINT_SIZE*POINT_NUM] = {0};
	unsigned int position = 0;
	int temp = 0;
	int ret = -1;
//	pr_info("==icn83xx_report_value_B ==\n");
	ret = icn83xx_i2c_rxdata(16, buf, POINT_NUM*POINT_SIZE+2);
	if (ret < 0) {
		pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}
 
	data->point_num = buf[1];
	
	if (data->point_num > 5)
		data->point_num = 5;
		
	if(data->point_num > 0)
	{
		for(position = 0; position<data->point_num; position++)
		{		
			temp = buf[2 + POINT_SIZE*position] + 1;
			finger_current[temp] = 1;
			data->point_info[temp].u8ID = buf[2 + POINT_SIZE*position];
			data->point_info[temp].u16PosX = (buf[3 + POINT_SIZE*position]<<8) + buf[4 + POINT_SIZE*position];
			data->point_info[temp].u16PosY = (buf[5 + POINT_SIZE*position]<<8) + buf[6 + POINT_SIZE*position];
			data->point_info[temp].u8Pressure = buf[7 + POINT_SIZE*position];
			data->point_info[temp].u8EventId = buf[8 + POINT_SIZE*position];
			if(data->point_info[temp].u8EventId == 4)
				finger_current[temp] = 0;				
			if(1 == revert_x_flag)
			{
				data->point_info[temp].u16PosX = SCREEN_MAX_X - data->point_info[temp].u16PosX;
			}
			if(1 == revert_y_flag)
			{
				data->point_info[temp].u16PosY = SCREEN_MAX_Y - data->point_info[temp].u16PosY;
			}
			print_int_info("temp %d\n", temp);
			print_int_info("u8ID %d\n", data->point_info[temp].u8ID);
			print_int_info("u16PosX %d\n", data->point_info[temp].u16PosX);
			print_int_info("u16PosY %d\n", data->point_info[temp].u16PosY);
			print_int_info("u8Pressure %d\n", data->point_info[temp].u8Pressure);
			print_int_info("u8EventId %d\n", data->point_info[temp].u8EventId);				
			//printk("u8Pressure %d\n", data->point_info[temp].u8Pressure*16);
		}
	}	
	else
	{
		for(position = 1; position < POINT_NUM+1; position++)
		{
			finger_current[position] = 0;
		}
		print_int_info("no touch\n");
	}

	for(position = 1; position < POINT_NUM + 1; position++)
	{
		if((finger_current[position] == 0) && (finger_last[position] != 0))
		{
			input_mt_slot(data->input_dev, position - 1);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			print_point_info("one touch up: %d\n", position);
		}
		else if(finger_current[position])
		{
			input_mt_slot(data->input_dev, position - 1);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 1);
			//input_report_abs(data->input_dev, ABS_MT_PRESSURE, data->point_info[position].u8Pressure);
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, 200);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->point_info[position].u16PosX);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,data->point_info[position].u16PosY);
			print_point_info("===position: %d, x = %d,y = %d, press = %d ====\n", position, data->point_info[position].u16PosX,data->point_info[position].u16PosY, data->point_info[position].u8Pressure);
		}

	}
	input_sync(data->input_dev);

	for(position = 1; position < POINT_NUM + 1; position++)
	{
		finger_last[position] = finger_current[position];
	}
	
}

static void icn83xx_ts_pen_irq_work(struct work_struct *work)
{
	int ret = -1;
	struct icn83xx_ts_data *data = i2c_get_clientdata(this_client);	
	
	if(work_mode == 0)
	{	
	//A类
	/*
		ret = icn83xx_read_data();
		if (ret == 0) {
			icn83xx_report_value();
		}
	*/
	
	//B类 天启rockchip
		icn83xx_report_value_B();
	}
	else if(work_mode == 1)
	{
		printk("log raw data\n");
		//dzy icn83xx_log(0);   //raw data
	}
	else if(work_mode == 2)
	{
		printk("log diff data\n");
		// dzyicn83xx_log(1);   //diff data
	}	
	
#ifndef STOP_IRQ_TYPE
	icn83xx_irq_enable(data); 
#endif		
}

static void chipone_timer_func(struct icn83xx_ts_data *icn83xx_ts)
{
	print_int_info("Enter chipone_timer_func\n");
	if (!work_pending(&icn83xx_ts->pen_event_work)) 
	{		
		queue_work(icn83xx_ts->ts_workqueue, &icn83xx_ts->pen_event_work);
	}
	
	if((work_mode == 1) || (work_mode == 2))
		mod_timer(&(icn83xx_ts->_timer), jiffies + 50);
		
}

static irqreturn_t icn83xx_ts_interrupt(int irq, void *dev_id)
{
	
	struct icn83xx_ts_data *icn83xx_ts = dev_id;
		
	print_int_info("\n==========------icn83xx_ts TS Interrupt-----============\n"); 
  if (probe_flag) {
  #ifndef STOP_IRQ_TYPE
	  icn83xx_irq_disable(icn83xx_ts);
  #endif
	  if (!work_pending(&icn83xx_ts->pen_event_work)) 
	  {
	  	print_int_info("Enter work\n");
	  	queue_work(icn83xx_ts->ts_workqueue, &icn83xx_ts->pen_event_work);
	  }
	}
	return IRQ_HANDLED;
}




#ifdef CONFIG_HAS_EARLYSUSPEND

static void icn83xx_ts_suspend(struct early_suspend *handler)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);	
    pr_info("icn83xx_ts_suspend: write ICN83XX_REG_PMODE .\n");
    icn83xx_irq_disable(icn83xx_ts);
    icn83xx_write_reg(ICN83XX_REG_PMODE, PMODE_HIBERNATE); 
    //gpio_out(PAD_GPIOC_3, 0);
	  mdelay(20); 
   
}

static void icn83xx_ts_resume(struct early_suspend *handler)
{
	struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);	
	pr_info("==icn83xx_ts_resume== \n");
#if 0
	//reset
	//wakeup
  	mdelay(20);  
	gpio_out(PAD_GPIOC_3, 1);
	mdelay(100); 
#endif

	gpio_set_status(PAD_GPIOD_2, gpio_status_out);
    gpio_out(PAD_GPIOD_2, 0);
	mdelay(20);  //
	
    //gpio_out(PAD_GPIOC_3, 1);
    gpio_out(PAD_GPIOD_2, 1);
	mdelay(100); // 

#if 0	
	//设定分辨率为800x480
	icn83xx_write_reg(177, 0x01);        //set X_RESOLUTION_H
	mdelay(1); 
	icn83xx_write_reg(178, 0xE0);        //set X_RESOLUTION_L
	mdelay(1); 
	icn83xx_write_reg(179, 0x03);         //set Y_RESOLUTION_H
	mdelay(1); 
	icn83xx_write_reg(180, 0x20);         //set Y_RESOLUTION_L
	mdelay(1); 
#endif

	icn83xx_irq_enable(icn83xx_ts);
}
#endif  //CONFIG_HAS_EARLYSUSPEND

char FbCap[4][16]={
			{0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14},
			{0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12},
			{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},			
			{0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08},
			};	
static int icn83xx_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct icn83xx_ts_data *icn83xx_ts;
	struct input_dev *input_dev;
	short fwVersion = 0;
	short curVersion = 0; 
	int average;
	int err = 0;
	char value;

#ifdef TOUCH_KEY_SUPPORT
	int i = 0;
#endif

	pr_info("====%s begin=====.  \n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	icn83xx_ts = kzalloc(sizeof(struct icn83xx_ts_data), GFP_KERNEL);
	if (!icn83xx_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

#ifdef TOUCH_VIRTUAL_KEYS
	icn83xx_ts_virtual_keys_init();
#endif

	//pr_info("touch panel gpio addr: = 0x%x", gpio_addr);
	this_client = client;
	this_client->addr = client->addr;
	i2c_set_clientdata(client, icn83xx_ts);
	
	/****************** device detected *******************/
	icn83xx_chip_init(0);
	err = icn83xx_readVersion();
	if(err < 0){
		printk("\n ################################### chipone ctp not exist ###################################\n");
		goto exit_create_singlethread;
	}
	
	/****************** device detected end *******************/


//	pr_info("==INIT_WORK=\n");
	INIT_WORK(&icn83xx_ts->pen_event_work, icn83xx_ts_pen_irq_work);
	icn83xx_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!icn83xx_ts->ts_workqueue) {
		err = -ESRCH;
		
		
		goto exit_create_singlethread;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		
		goto exit_input_dev_alloc_failed;
	}
	
	icn83xx_ts->input_dev = input_dev;

#ifdef CONFIG_ICN83XX_MULTITOUCH

//A类
/*
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);	

	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TRACKING_ID, 0, 4, 0, 0);
*/

//B类	rockchip	     
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	__set_bit(EV_ABS, input_dev->evbit);
	input_mt_init_slots(input_dev, POINT_NUM);
	//input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
			     
#ifdef TOUCH_KEY_SUPPORT
	key_tp = 0;
	input_dev->evbit[0] = BIT_MASK(EV_KEY);
	for (i = 1; i < TOUCH_KEY_NUMBER; i++)
		set_bit(i, input_dev->keybit);
#endif
#else
	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	input_set_abs_params(input_dev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_PRESSURE, 0, PRESS_MAX, 0 , 0);
#endif

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->name		= CTP_NAME;		//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"icn83xx_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	pr_info("==register_early_suspend =\n");
	icn83xx_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	icn83xx_ts->early_suspend.suspend = icn83xx_ts_suspend;
	icn83xx_ts->early_suspend.resume	= icn83xx_ts_resume;
	register_early_suspend(&icn83xx_ts->early_suspend);
#endif

#ifdef CONFIG_ICN83XX_MULTITOUCH
	pr_info("CONFIG_ICN83XX_MULTITOUCH is defined. \n");
#endif

#ifdef INT_PORT	
    icn83xx_chip_init(1);
	if (client->irq)
	{
		//err = gpio_request(client->irq, "TS_INT"); //Request IO
		//if (err < 0)
		//{
		//	dev_err(&client->dev, "Failed to request GPIO:%d, ERRNO:%d\n",
		//			(int) INT_PORT, err);
		//	goto exit_set_irq_mode;
	//	}
	//	gpio_direction_input(client->irq);

#ifndef STOP_IRQ_TYPE
		icn83xx_ts->irq = client->irq; 
		icn83xx_ts->irq_is_disable = 0;
#endif
	}
	
    err = request_irq(client->irq,  icn83xx_ts_interrupt, IRQF_DISABLED, client->name, icn83xx_ts);	
//	err = request_irq(client->irq, icn83xx_ts_interrupt,
//			IRQ_TYPE_EDGE_FALLING, client->name, icn83xx_ts);
    //printk( "icn83xx_ts_probe: request irq %d\n",client->irq);
	  if (err < 0) {
		printk( "icn83xx_ts_probe: request irq %d failed\n",client->irq);
		goto exit_irq_request_failed;
	}
/*	if (err != 0)
	{
		dev_err(&client->dev, "Cannot allocate ts INT!ERRNO:%d\n", err);
		gpio_direction_input(INT_PORT);
		gpio_free(INT_PORT);
		goto exit_irq_request_failed;
	}*/
	else
	{

#ifndef STOP_IRQ_TYPE
		icn83xx_irq_enable(icn83xx_ts);
#else
		enable_irq(client->irq);
#endif
	}
	
	
	
#endif	


/*
//add timer
	init_timer(&(icn83xx_ts->_timer));
	icn83xx_ts->_timer.data = (unsigned long)icn83xx_ts;
	icn83xx_ts->_timer.function  =	(void (*)(unsigned long))chipone_timer_func;	
//	add_timer(&(icn83xx_ts->_timer));
	mod_timer(&(icn83xx_ts->_timer), jiffies + 5);
*/

#ifdef SUPPORT_APK_OP
//create cdev
    print_app_info("+create cdev()!\n");     
	alloc_chrdev_region(&devnum, 0, 1, modname);

    cdev_init(&icn83xx_cdev, &icn83xx_mod_fops);  
    icn83xx_cdev.owner = THIS_MODULE;  
    icn83xx_cdev.ops = &icn83xx_mod_fops;  
    err = cdev_add(&icn83xx_cdev, devnum, 1);  
    if(err)  
        print_app_info("Failed at cdev_add()");  
    icn83xx_class = class_create(THIS_MODULE, classname);  
    if(IS_ERR(icn83xx_class))  
    {  
        print_app_info("Failed at class_create().Please exec [mknod] before operate the device\n");  
    }  
    else  
    {  
        device_create(icn83xx_class, NULL, devnum,NULL, devicename);  
    }  
  
 
    inbuffer = (char *)kmalloc(IN_BUF_LEN, GFP_KERNEL);  
    outbuffer = (char *)kmalloc(OUT_BUF_LEN, GFP_KERNEL);  
 
    sema_init(&sem, 1);
    print_app_info("-create cdev ok()!\n");  
#endif

#ifdef SUPPORT_ATTR_OP
	icn83xx_create_sysfs(client);
#endif

#ifdef INCLUDE_LIB
	icn83xx_irq_disable(icn83xx_ts);
	
//update fw
	fwVersion = icn83xx_read_fw_Ver(firmware);
	curVersion = icn83xx_readVersion();
	printk("fwVersion : 0x%x\n", fwVersion); 
	printk("current version: 0x%x\n", curVersion);   	
	if(fwVersion != -1)
	{		
		if(icn83xx_check_progmod() == 0)
		{
			printk("in prog mode\n");	
			icn83xx_fw_update(firmware);
		}
		else if(fwVersion > curVersion)
		{
			icn83xx_fw_update(firmware);
		}
	}
/*
//auto calib fw
	average = icn83xx_calib(0, NULL);
//fix FbCap
//	average = icn83xx_calib(0, FbCap[1]);
	printk("average : %d\n", average); 
	icn83xx_setPeakGroup(250, 150);
	icn83xx_setDownUp(400, 300);

	icn83xx_readReg(0x001a, &value);
	printk("osc 1a: 0x%x\n", value);
	icn83xx_readReg(0x001b, &value);
	printk("osc 1b: 0x%x\n", value);
	icn83xx_readReg(0x005, &value);
	printk("05: 0x%x\n", value);
*/

	icn83xx_irq_enable(icn83xx_ts);
#endif
  probe_flag = 1;
	pr_info("==%s over =\n", __func__);
	 
	return 0;

exit_irq_request_failed:
    free_irq(icn83xx_ts->irq, icn83xx_ts);	
exit_set_irq_mode:
	cancel_work_sync(&icn83xx_ts->pen_event_work);
	destroy_workqueue(icn83xx_ts->ts_workqueue);

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:

exit_create_singlethread:
	pr_info("==singlethread error =\n");
	i2c_set_clientdata(client, NULL);
	kfree(icn83xx_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}
/*
 flash.c
*/
struct file  *fp; 
int g_status;

void icn83xx_memdump(char *mem, int size)
{
	int i;
	for(i=0;i<size; i++)
	{
		if(i%16 == 0)
			printk("\n");
		printk(" 0x%2x", mem[i]);
	}
	_flash_info_("\n");	
} 



int  icn83xx_checksum(int sum, char *buf, unsigned int size)
{
	int i;
	for(i=0; i<size; i++)
	{
		sum = sum + buf[i];
	}
	return sum;
}


int icn83xx_update_status(int status)
{
	/*
//	_flash_info_("icn83xx_update_status: %d\n", status);
#ifdef SUPPORT_APK_OP
	updata_status = status;
#endif

#ifdef SUPPORT_ATTR_OP
	processbar = status;
#endif
*/
	g_status = status;
	return 0;
}

int icn83xx_get_status(void)
{
	return 	g_status;
}

/***********************************************************************************************
Name	:	icn83xx_open_fw 
Input	:	*fw
            
Output	:	file size
function	:	open the fw file, and return total size
***********************************************************************************************/
int  icn83xx_open_fw( char *fw)
{
	loff_t file_size;
	mm_segment_t fs; 
	struct inode *inode = NULL;	
	fp = filp_open(fw, O_RDONLY, 0); 
	//_flash_info_("icn83xx_open_fw FILE:%s\n", fw);
 // printk("%s=%x\n",__func__,fp); 
    if (IS_ERR(fp)) { 
    	_flash_info_("read fw file error\n"); 
		return -1; 
	} 
	else
		_flash_info_("open fw file ok\n"); 
		
	inode = fp->f_dentry->d_inode;
	file_size = inode->i_size;	
	//file_size = 0x7999;
	//_flash_info_("file size: %ld\n", file_size); 
	//_flash_info_("file i size: %ld\n", inode->i_size); 
	

	fs = get_fs(); 
	set_fs(KERNEL_DS); 
	
	return 	file_size;
	
}

/***********************************************************************************************
Name	:	icn83xx_read_fw 
Input	:	offset
            length, read length
            buf, return buffer
Output	:	
function	:	read data to buffer
***********************************************************************************************/
int  icn83xx_read_fw(int offset, int length, char *buf)
{
	loff_t  pos = offset;          		
	vfs_read(fp, buf, length, &pos); 
//	icn83xx_memdump(buf, length);
	return 0;		
}


/***********************************************************************************************
Name	:	icn83xx_close_fw 
Input	:	
Output	:	
function	:	close file
***********************************************************************************************/
int  icn83xx_close_fw(void)
{	
	filp_close(fp, NULL); 
	return 0;
}

/***********************************************************************************************
Name	:	icn83xx_goto_progmode 
Input	:	
Output	:	
function	:	change MCU to progmod
***********************************************************************************************/
int icn83xx_goto_progmode(void)
{
	int ret = -1;
	int i;
	char value[64];
	char regValue = 0;
//	unsigned short addr = 0;
//	char temp_buf[3];
	
	_flash_info_("icn83xx_goto_progmode\n");
	
	ret = icn83xx_readReg(0x009, &regValue);
	if(ret != 0)
    	return ret;	
	_flash_info_("[0x009]: 0x%x\n", regValue);	
		
// open clock
	if(regValue != 0xDF)
	{
		icn83xx_changemode(2);
		ret = icn83xx_writeReg(0x002, 0x00);
	    if(ret != 0)
	    	return ret;			
		ret = icn83xx_writeReg(0x009, 0xDF);
	    if(ret != 0)
	    	return ret;	
		ret = icn83xx_writeReg(0x010, 0x00);
	    if(ret != 0)
	    	return ret;		
		
	}
	
/*	
	addr = 0x0;
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);			
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }

	temp_buf[0] = 0xff;
    ret = icn83xx_i2c_txdata(232, temp_buf, 1);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }	
*/
    ret = icn83xx_writeInfo(0x0, 0xff);
    if(ret != 0)
    	return ret;

/*    
	addr = 0x1;
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);			
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }

	temp_buf[0] = 0xff;
    ret = icn83xx_i2c_txdata(232, temp_buf, 1);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }	    
*/
    ret = icn83xx_writeInfo(0x1, 0xff);
    if(ret != 0)
    	return ret;   
    	
    ret = icn83xx_writeInfo(0x10, 0xff);
    if(ret != 0)
    	return ret;   
    	
    ret = icn83xx_writeInfo(0x11, 0xff);
    if(ret != 0)
    	return ret;       	    	
/*    
	addr = 0xf00;
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);			
    ret = icn83xx_i2c_txdata(224, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
	temp_buf[0] = 0x1;
    ret = icn83xx_i2c_txdata(226, temp_buf, 1);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }	    
*/
	ret = icn83xx_writeReg(0xf00, 1);
    if(ret != 0)
    	return ret;
    		
	return 0;	
}

/***********************************************************************************************
Name	:	icn83xx_check_progmod 
Input	:	
Output	:	
function	:	check if MCU at progmode or not
***********************************************************************************************/
int icn83xx_check_progmod(void)
{
	int ret;
	unsigned char ucTemp = 0x0;
	ret = icn83xx_prog_i2c_rxdata(0x0, &ucTemp, 1);
	_flash_info_("icn83xx_check_progmod: 0x%x\n", ucTemp);
	return ret;
}


/***********************************************************************************************
Name	:	icn83xx_uu 
Input	:	
Output	:	
function	:	unlock flash
***********************************************************************************************/
int icn83xx_uu(void)
{
	unsigned char ucTemp = 0x0;
	ucTemp = 0x1e; 				        
	icn83xx_prog_i2c_txdata(0x050a, &ucTemp, 1);
	ucTemp = 0x10; 				        
	icn83xx_prog_i2c_txdata(0x050b, &ucTemp, 1);
	return 0;	
}
/***********************************************************************************************
Name	:	icn83xx_ll 
Input	:	
Output	:	
function	:	lock flash
***********************************************************************************************/
int icn83xx_ll(void)
{
	unsigned char ucTemp = 0x0;
	ucTemp = 0xcc; 				        
	icn83xx_prog_i2c_txdata(0x050a, &ucTemp, 1);
	ucTemp = 0xcc; 				        
	icn83xx_prog_i2c_txdata(0x050b, &ucTemp, 1);		
}

/***********************************************************************************************
Name	:	icn83xx_op1 
Input	:	
Output	:	
function	:	erase flash
***********************************************************************************************/

int  icn83xx_op1(char info, unsigned short offset, unsigned int size)
{
	int count = 0;
	unsigned char ucTemp = 0x0;
	unsigned short uiAddress = 0x0;	
	unsigned int i;
	
	//_flash_info_("file size: 0x%x, offset=%x\n", size, offset);
	
	icn83xx_uu();
	for(i=0; i<size; )
	{		
		count = 0;
		uiAddress = offset + i;
//		_flash_info_("uiAddress: 0x%x\n", uiAddress);
		ucTemp = D_U16LOBYTE(uiAddress); 				        
		icn83xx_prog_i2c_txdata(0x0502, &ucTemp, 1);
		ucTemp = D_U16HIBYTE(uiAddress); 				       
		icn83xx_prog_i2c_txdata(0x0503, &ucTemp, 1);
		
		ucTemp = 0x02;									    
		icn83xx_prog_i2c_txdata(0x0500, &ucTemp, 1);
		ucTemp = 0x01;
		//_flash_info_("block:%x\n", i / 1024);	
		while(ucTemp)									    
		{
			icn83xx_prog_i2c_rxdata(0x0501, &ucTemp, 1);
			count++;
			if(count > 5000)
			{
				_flash_info_("____op1 ucTemp: 0x%x\n", ucTemp);
				return 1;
			}
		}
		i = i+1024;
	}
	icn83xx_ll();
	return 0;
}

/***********************************************************************************************
Name	:	icn83xx_op2 
Input	:	
Output	:	
function	:	progm flash
***********************************************************************************************/
int  icn83xx_op2(char info, unsigned short offset, unsigned char * buffer, unsigned int size)
{
	int count = 0;
	unsigned int flash_size;
	unsigned char ucTemp;
	unsigned short uiAddress;
	ucTemp = 0x00;
	uiAddress = 0x1000;	
	
	icn83xx_prog_i2c_txdata(uiAddress, buffer, size);
	
	icn83xx_uu();
	
	ucTemp = D_U16LOBYTE(offset); 				        	
	icn83xx_prog_i2c_txdata(0x0502, &ucTemp, 1);
	ucTemp = D_U16HIBYTE(offset); 
	icn83xx_prog_i2c_txdata(0x0503, &ucTemp, 1);
	
	icn83xx_prog_i2c_txdata(0x0504, &uiAddress, 2);
//ensure size is even
	if(size%2 != 0)
	{
		_flash_info_("write op size: %d\n", size);
		flash_size = size+1;
	}
	else
		flash_size = size;
	
	ucTemp = D_U16LOBYTE(flash_size); 						    	
	icn83xx_prog_i2c_txdata(0x0506, &ucTemp, 1);
	ucTemp = D_U16HIBYTE(flash_size); 						    	
	icn83xx_prog_i2c_txdata(0x0507, &ucTemp, 1);
	ucTemp = 0x01;

	if(info > 0)
	   ucTemp = 0x01 | (1<<3);							

	icn83xx_prog_i2c_txdata(0x0500, &ucTemp, 1);	//
	while(ucTemp)										
	{
		icn83xx_prog_i2c_rxdata(0x0501, &ucTemp, 1);
		count++;
		if(count > 5000)
		{
			_flash_info_("op2 ucTemp: 0x%x\n", ucTemp);
			return 1;
		}		
		
	}
	icn83xx_ll();
	return 0;	
}

/***********************************************************************************************
Name	:	icn83xx_op3 
Input	:	
Output	:	
function	:	read flash
***********************************************************************************************/
int  icn83xx_op3(char info, unsigned short offset, unsigned char * buffer, unsigned int size)
{
	int count = 0;
	unsigned int flash_size;
	unsigned char ucTemp;
	unsigned short uiAddress;
	ucTemp = 0x00;
	uiAddress = 0x1000;	
	icn83xx_uu();
	ucTemp = D_U16LOBYTE(offset); 				       
	icn83xx_prog_i2c_txdata(0x0502, &ucTemp, 1);
	ucTemp = D_U16HIBYTE(offset); 				        
	icn83xx_prog_i2c_txdata(0x0503, &ucTemp, 1);

	icn83xx_prog_i2c_txdata(0x0504, (unsigned char*)&uiAddress, 2);
//ensure size is even
	if(size%2 != 0)
	{
		_flash_info_("read op size: %d\n", size);
		flash_size = size+1;
	}
	else
		flash_size = size;
	
	ucTemp = D_U16LOBYTE(flash_size);						    
	icn83xx_prog_i2c_txdata(0x0506, &ucTemp, 1);
	
	ucTemp = D_U16HIBYTE(flash_size);						    
	icn83xx_prog_i2c_txdata(0x0507, &ucTemp, 1);
	ucTemp = 0x40;

	if(info > 0)
		ucTemp = 0x40 | (1<<3);                   

	icn83xx_prog_i2c_txdata(0x0500, &ucTemp, 1);
	ucTemp = 0x01;
	while(ucTemp)									
	{
		icn83xx_prog_i2c_rxdata(0x0501, &ucTemp, 1);
		count++;
		if(count > 5000)
		{
			_flash_info_("op3 ucTemp: 0x%x\n", ucTemp);
			return 1;
		}		
				
	}
	icn83xx_ll();
	icn83xx_prog_i2c_rxdata(uiAddress, buffer, size);
	return 0;	
}


/***********************************************************************************************
Name	:	icn83xx_goto_nomalmode 
Input	:	
Output	:	
function	:	when prog flash ok, change flash info flag
***********************************************************************************************/
int icn83xx_goto_nomalmode(void)
{
	int ret = -1;
	unsigned short addr = 0;
	char temp_buf[3];

	_flash_info_("icn83xx_goto_nomalmode\n");
	temp_buf[0] = 0x03;	
	icn83xx_prog_i2c_txdata(0x0f00, temp_buf, 1);
	
	msleep(100);
/*	
	addr = 0;
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);	
	temp_buf[2] = 0;		
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    
	icn83xx_i2c_rxdata(232, &temp_buf[2], 1);	
	_flash_info_("temp_buf[2]: 0x%x\n", temp_buf[2]);
*/
	ret = icn83xx_readInfo(0, &temp_buf[2]);
	if(ret != 0)
		return ret;
	_flash_info_("temp_buf[2]: 0x%x\n", temp_buf[2]);
	if(temp_buf[2] == 0xff)
	{
/*		
		addr = 0;
		temp_buf[0] = D_U16HIBYTE(addr);
		temp_buf[1] = D_U16LOBYTE(addr);	
	    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
	    if (ret < 0) {
	        pr_err("write reg failed! ret: %d\n", ret);
	        return -1;
	    }			
		temp_buf[0] = 0x11;
	    ret = icn83xx_i2c_txdata(232, temp_buf, 1);
	    if (ret < 0) {
	        pr_err("write reg failed! ret: %d\n", ret);
	        return -1;
	    }
*/	    
		ret = icn83xx_writeInfo(0, 0x11);
		if(ret != 0)
			return ret;		
	    	
	}	
	return 0;
}

/***********************************************************************************************
Name	:	icn83xx_read_fw_Ver 
Input	:	fw
Output	:	
function	:	read fw version
***********************************************************************************************/

short  icn83xx_read_fw_Ver(char *fw)
{
	short FWversion;
	char tmp[2];
	int file_size;
	
	//_flash_info_("icn83xx_read_fw_Ver FILE:%s\n", fw);
	file_size = icn83xx_open_fw(fw);
	if(file_size < 0)
	{
		_flash_info_("no fw file\n");
		return -1;	
	}	
	icn83xx_read_fw(0x4000, 2, &tmp[0]);
	
	icn83xx_close_fw();
	FWversion = (tmp[0]<<8)|tmp[1];
//	_flash_info_("FWversion: 0x%x\n", FWversion);
	return FWversion;
}




/***********************************************************************************************
Name	:	icn83xx_fw_update 
Input	:	fw
Output	:	
function	:	upgrade fw
***********************************************************************************************/

E_UPGRADE_ERR_TYPE  icn83xx_fw_update(char *fw)
{
	int file_size, last_length;
	int i, j, num;
	int checksum_bak = 0;
	int checksum = 0;
	char temp_buf[B_SIZE];
#ifdef ENABLE_BYTE_CHECK	
	char temp_buf1[B_SIZE];
#endif	
	if(icn83xx_goto_progmode() != 0)
	{
		if(icn83xx_check_progmod() < 0)
		{
			icn83xx_update_status(R_STATE_ERR);
			return R_STATE_ERR;
		}	
	}
//	msleep(50);
	file_size = icn83xx_open_fw(fw);
	if(file_size < 0)
	{
		icn83xx_update_status(R_FILE_ERR);
		return R_FILE_ERR;	
	}
	if(icn83xx_op1(0, 0, file_size) != 0)
	{
		_flash_info_("icn83xx_op1 error\n");
		icn83xx_update_status(R_ERASE_ERR);
		return R_ERASE_ERR;
	}
	icn83xx_update_status(5);
	
	num = file_size/B_SIZE;
	for(j=0; j < num; j++)
	{
		icn83xx_read_fw(j*B_SIZE, B_SIZE, temp_buf);
		
//		icn83xx_op3(0, j*B_SIZE, temp_buf1, B_SIZE);
//		icn83xx_memdump(temp_buf1, B_SIZE);
		
		if(icn83xx_op2(0, j*B_SIZE, temp_buf, B_SIZE) != 0)
		{
			icn83xx_update_status(R_PROGRAM_ERR);
			return R_PROGRAM_ERR;
		}
		checksum_bak = icn83xx_checksum(checksum_bak, temp_buf, B_SIZE);
		
		icn83xx_update_status(5+(int)(60*j/num));
	}
	last_length = file_size - B_SIZE*j;
	if(last_length > 0)
	{
		icn83xx_read_fw(j*B_SIZE, last_length, temp_buf);
		
//		icn83xx_op3(0, j*B_SIZE, temp_buf1, B_SIZE);
//		icn83xx_memdump(temp_buf1, B_SIZE);		
		
		if(icn83xx_op2(0, j*B_SIZE, temp_buf, last_length) != 0)
		{
			icn83xx_update_status(R_PROGRAM_ERR);
			return R_PROGRAM_ERR;
		}
		checksum_bak = icn83xx_checksum(checksum_bak, temp_buf, last_length);
	}
	
	icn83xx_close_fw();	
	icn83xx_update_status(65);
	
#ifdef ENABLE_BYTE_CHECK
	file_size = icn83xx_open_fw(fw);
	num = file_size/B_SIZE;
#endif
	
	for(j=0; j < num; j++)
	{

#ifdef ENABLE_BYTE_CHECK		
		icn83xx_read_fw(j*B_SIZE, B_SIZE, temp_buf1);
#endif		
		icn83xx_op3(0, j*B_SIZE, temp_buf, B_SIZE);
		checksum = icn83xx_checksum(checksum, temp_buf, B_SIZE);

#ifdef ENABLE_BYTE_CHECK		
		if(memcmp(temp_buf1, temp_buf, B_SIZE) != 0)
		{
			_flash_info_("cmp error, %d\n", j);
			icn83xx_memdump(temp_buf1, B_SIZE);
			icn83xx_memdump(temp_buf, B_SIZE);	
			icn83xx_update_status(R_VERIFY_ERR);
			return R_VERIFY_ERR;				
			//while(1);
		}
#endif		
		icn83xx_update_status(65+(int)(30*j/num));
	}

#ifdef ENABLE_BYTE_CHECK	
	last_length = file_size - B_SIZE*j;
#endif	
	if(last_length > 0)
	{
#ifdef ENABLE_BYTE_CHECK		
		icn83xx_read_fw(j*B_SIZE, last_length, temp_buf1);
#endif		
		icn83xx_op3(0, j*B_SIZE, temp_buf, last_length);
		checksum = icn83xx_checksum(checksum, temp_buf, last_length);

#ifdef ENABLE_BYTE_CHECK
		if(memcmp(temp_buf1, temp_buf, last_length) != 0)
		{
			_flash_info_("cmp error, %d\n", j);
			icn83xx_memdump(temp_buf1, last_length);	
			icn83xx_memdump(temp_buf, last_length);	
			icn83xx_update_status(R_VERIFY_ERR);
			return R_VERIFY_ERR;							
			//while(1);
		}
#endif		

	}

#ifdef ENABLE_BYTE_CHECK	
	icn83xx_close_fw();
#endif		
	
	_flash_info_("checksum_bak: 0x%x, checksum: 0x%x\n", checksum_bak, checksum);
	if(checksum_bak != checksum)
	{
		_flash_info_("upgrade checksum error\n");
		icn83xx_update_status(R_VERIFY_ERR);
		return R_VERIFY_ERR;
	}

	if(icn83xx_goto_nomalmode() != 0)
	{
		_flash_info_("icn83xx_goto_nomalmode error\n");
		icn83xx_update_status(R_STATE_ERR);
		return R_STATE_ERR;
	}
	
	icn83xx_update_status(R_OK);
	_flash_info_("upgrade ok\n");
	return R_OK;
}

/*
 iic_op.c
*/
/***********************************************************************************************
Name	:	icn83xx_writeInfo 
Input	:	addr, value
Output	:	
function	:	write Flash Info
***********************************************************************************************/

int icn83xx_writeInfo(unsigned short addr, char value)
{
	int ret = -1;
	char temp_buf[3];
	
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);			
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(2);
	temp_buf[0] = value;
    ret = icn83xx_i2c_txdata(232, temp_buf, 1);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(5);
	return 0;   
}
/***********************************************************************************************
Name	:	icn83xx_readInfo 
Input	:	
Output	:	
function	:	read Flash info
***********************************************************************************************/

int icn83xx_readInfo(unsigned short addr, char *value)
{
	int ret = -1;
	char temp_buf[3];
	
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);			
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(2);
	//temp_buf[0] = value;	 
    //ret = icn83xx_i2c_txdata(226, temp_buf, 1);
    ret = icn83xx_i2c_rxdata(232, value, 1); 
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(2);
	return 0;   
}

/***********************************************************************************************
Name	:	icn83xx_writeReg 
Input	:	addr, value
Output	:	
function	:	write MCU xdata and reg
***********************************************************************************************/

int icn83xx_writeReg(unsigned short addr, char value)
{
	int ret = -1;
	char temp_buf[3];
//	printk("icn83xx_writeReg: 0x%x, 0x%x\n", addr, value);
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);			
    ret = icn83xx_i2c_txdata(224, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(2);
	temp_buf[0] = value;
    ret = icn83xx_i2c_txdata(226, temp_buf, 1);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(5);
	return 0;   
}
/***********************************************************************************************
Name	:	icn83xx_readReg 
Input	:	
Output	:	
function	:	read MCU xdata and reg
***********************************************************************************************/

int icn83xx_readReg(unsigned short addr, char *value)
{
	int ret = -1;
	char temp_buf[3];
	
	temp_buf[0] = D_U16HIBYTE(addr);
	temp_buf[1] = D_U16LOBYTE(addr);			
    ret = icn83xx_i2c_txdata(224, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(2);
	//temp_buf[0] = value;	 
    //ret = icn83xx_i2c_txdata(226, temp_buf, 1);
    ret = icn83xx_i2c_rxdata(226, value, 1); 
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    mdelay(2);
	return 0;   
}
/***********************************************************************************************
Name	:	icn83xx_setVol 
Input	:	
Output	:	
function	:	set tx volume
***********************************************************************************************/

int icn83xx_setVol(char vol)
{
	icn83xx_writeReg(0x671, vol);
	return 0;   
}

/***********************************************************************************************
Name	:	icn83xx_readVersion
Input	:	void
Output	:	
function	:	return version
***********************************************************************************************/
int icn83xx_readVersion(void)
{
	int err = 0;
	char tmp[2];	
	short CurVersion;
	err = icn83xx_i2c_rxdata(12, tmp, 2);
	if (err < 0) {
		pr_info("%s read_data i2c_rxdata failed: %d\n", __func__, err);	
		return err;
	}		
	CurVersion = (tmp[0]<<8) | tmp[1];
    return CurVersion;	
}

/***********************************************************************************************
Name	:	icn83xx_changemode 
Input	:	normal/factory/config
Output	:	
function	:	change work mode
***********************************************************************************************/
int icn83xx_changemode(char mode)
{
	char value = 0x0;
	icn83xx_write_reg(0, mode); 
	mdelay(1);
	icn83xx_read_reg(1, &value);
	while(value != 0)
	{
		mdelay(1);
		icn83xx_read_reg(1, &value);
	}	
//	_calib_info_("icn83xx_changemode ok\n");
    return 0;	
}

static int __devexit icn83xx_ts_remove(struct i2c_client *client)
{

	struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(client);
	
	pr_info("==icn83xx_ts_remove=\n");
#ifdef INT_PORT
		gpio_direction_input(INT_PORT);
		gpio_free(INT_PORT);
#endif
		free_irq(client->irq, icn83xx_ts);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&icn83xx_ts->early_suspend);
#endif
	input_unregister_device(icn83xx_ts->input_dev);
	input_free_device(icn83xx_ts->input_dev);
	cancel_work_sync(&icn83xx_ts->pen_event_work);
	destroy_workqueue(icn83xx_ts->ts_workqueue);
	kfree(icn83xx_ts);
    
	i2c_set_clientdata(client, NULL);

	return 0;

}

/***********************************************************************************************
Name	:	icn83xx_log
Input	:	0: rawdata, 1: diff data
Output	:	err type
function	:	calibrate param
***********************************************************************************************/
/* dzy
int  icn83xx_log(char diff)
{
	char row = 0;
	char column = 0;
	int i, j;
	icn83xx_read_reg(160, &row);
	icn83xx_read_reg(161, &column);

	if(diff == 1)
	{
		icn83xx_readTP(row, column, &diffdata[0][0]);

		for(i=0; i<row; i++)
		{		
			for(j=0; j<column; j++)
			{
				diffdata[i][j] = diffdata[i][j] - rawdata[i][j];
			}
		}	
		icn83xx_rawdatadump(&diffdata[0][0], row*16, 16);
	}
	else
	{
		icn83xx_readTP(row, column, &rawdata[0][0]);	
		icn83xx_rawdatadump(&rawdata[0][0], row*16, 16);
	}
}
*/
static const struct i2c_device_id icn83xx_ts_id[] = {
	{ CTP_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, icn83xx_ts_id);

static struct i2c_driver icn83xx_ts_driver = {

	.probe		= icn83xx_ts_probe,
	.remove		= __devexit_p(icn83xx_ts_remove),

 	.id_table	= icn83xx_ts_id,
	.driver	= {
		.name	= CTP_NAME,
		.owner	= THIS_MODULE,
	},
};


static int __init icn83xx_ts_init(void)
{ 
	int ret = -1;
	pr_info("===========================%s=====================\n", __func__);
	ret = i2c_add_driver(&icn83xx_ts_driver);
	return ret;
}

static void __exit icn83xx_ts_exit(void)
{
	pr_info("==icn83xx_ts_exit==\n");
	i2c_del_driver(&icn83xx_ts_driver);
}


/***********************************************************************************************
Name	:	icn83xx_readTP 
Input	:	rownum and columnnum
Output	:	
function	:	read one frame rawdata
***********************************************************************************************/
/* dzy
int icn83xx_readTP(char row_num, char column_num, char *buffer)
{
	int err = 0;
	int i;
//	_calib_info_("icn83xx_readTP\n");
	icn83xx_changemode(1);	
	icn83xx_scanTP();
	for(i=0; i<row_num; i++)
	{
		icn83xx_readrawdata(&buffer[i*16*2], i, column_num*2);
	}
	icn83xx_changemode(0);	
	return err;	
}
*/
/*
short rawdata[28][16] = {0,};
short diffdata[28][16] = {0,};
char phase_delay[28][16] = {0,};
char TX[28] = {0,};
char RX[16] = {0,};
char FB[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
char DC[16] = {0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06};
char reg[0x100] = {0,};

void icn83xx_rawdatadump(short *mem, int size, char br)
{
	int i;
	for(i=0;i<size; i++)
	{
		if((i!=0)&&(i%br == 0))
			printk("\n");
		printk(" %5d", mem[i]);
	}
	_flash_info_("\n");	
} 
*/

//late_initcall(icn83xx_ts_init);
module_init(icn83xx_ts_init);
module_exit(icn83xx_ts_exit);

MODULE_AUTHOR("<zmtian@chiponeic.com>");
MODULE_DESCRIPTION("Chipone icn83xx TouchScreen driver");
MODULE_LICENSE("GPL");
