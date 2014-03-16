/* 
 * drivers/input/touchscreen/ft5x02_ts.c
 *
 * FocalTech ft5x02 TouchScreen driver. 
 *
 * Copyright (c) 2012  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>   
#include <mach/irqs.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>


#include <linux/proc_fs.h>

#include <linux/ft5x02_ts.h>

//#define FT5X02_CONFIG_INI
//#define CONFIG_PM

struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];	/*x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];	/*y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];	/*touch event:
					0 -- down; 1-- contact; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/*touch ID */
	u16 pressure;
	u8 touch_point;
};

struct ft5x02_ts_data {
	unsigned int irq;
	unsigned int x_max;
	unsigned int y_max;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	struct ft5x02_platform_data *pdata;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;	
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

#ifdef CONFIG_PM
#define RESET_GPIO_PIN	S3C64XX_GPQ(4)
#endif
static struct i2c_client *this_client;
#ifdef CFG_SUPPORT_TOUCH_KEY
int tsp_keycodes[CFG_NUMOFKEYS] ={
        KEY_MENU,
        KEY_HOME,
        KEY_BACK,
        KEY_SEARCH
};
char *tsp_keyname[CFG_NUMOFKEYS] ={
        "Menu",
        "Home",
        "Back",
        "Search"
};
static bool tsp_keystatus[CFG_NUMOFKEYS];
#endif

//#define FT5X02_UPGRADE
static u8 CTPM_FW[] = 
{
	//#include "ft5x02_app.i"
};

#define FT5X02_CONFIG_NAME "fttpconfig_5x02public.ini"

extern int ft5x02_Init_IC_Param(struct i2c_client * client);
extern int ft5x02_get_ic_param(struct i2c_client * client);
extern int ft5x02_Get_Param_From_Ini(char *config_name);

#define SYSFS_DEBUG
#ifdef SYSFS_DEBUG
static struct mutex g_device_mutex;
static int ft5x02_create_sysfs_debug(struct i2c_client *client);
#endif

#define FTS_APK_DEBUG
#ifdef FTS_APK_DEBUG
int ft5x02_create_apk_debug_channel(struct i2c_client *client);
void ft5x02_release_apk_debug_channel(void);
#endif


/*
*ft5x02_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int ft5x02_i2c_Read(struct i2c_client *client,  char * writebuf, int writelen, 
							char *readbuf, int readlen)
{
	int ret;

	if(writelen > 0)
	{
		struct i2c_msg msgs[] = {
			{
				.addr	= client->addr,
				.flags	= 0,
				.len	= writelen,
				.buf	= writebuf,
			},
			{
				.addr	= client->addr,
				.flags	= I2C_M_RD,
				.len	= readlen,
				.buf	= readbuf,
			},
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			pr_err("function:%s. i2c read error: %d\n", __func__, ret);
	}
	else
	{
		struct i2c_msg msgs[] = {
			{
				.addr	= client->addr,
				.flags	= I2C_M_RD,
				.len	= readlen,
				.buf	= readbuf,
			},
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			pr_err("function:%s. i2c read error: %d\n", __func__, ret);
	}
	return ret;
}
/*
*write data by i2c 
*/
int ft5x02_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= writelen,
			.buf	= writebuf,
		},
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}

void delay_qt_ms(unsigned long  w_ms)
{
	unsigned long i;
	unsigned long j;

	for (i = 0; i < w_ms; i++)
	{
		for (j = 0; j < 1000; j++)
		{
			 udelay(1);
		}
	}
}

/*release the point*/
static void ft5x02_ts_release(struct ft5x02_ts_data *data)
{
	input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_sync(data->input_dev);
}

int ft5x02_write_reg(struct i2c_client * client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = {0};
	buf[0] = regaddr;
	buf[1] = regvalue;

	return ft5x02_i2c_Write(client, buf, sizeof(buf));
}

int ft5x02_read_reg(struct i2c_client * client, u8 regaddr, u8 * regvalue)
{
	return ft5x02_i2c_Read(client, &regaddr, 1, regvalue, 1);
}

void ft5x02_upgrade_send_head(struct i2c_client * client)
{
	u8 ret = 0;
	u8 headbuf[2];
	headbuf[0] = 0xFA;
	headbuf[1] = 0xFA;

	ret = ft5x02_i2c_Write(client, headbuf, 2);
	if(ret < 0)
		dev_err(&client->dev, "[FTS]--upgrading, send head error\n");
}
/*
*/
#define FTS_PACKET_LENGTH 128
static int  ft5x02_ctpm_fw_upgrade(struct i2c_client * client, u8* pbt_buf, u32 dw_lenth)
{
	
	u8 reg_val[2] = {0};
	u32 i = 0;

	u32  packet_number;
	u32  j;
	u32  temp;
	u32  lenght;
	u8	packet_buf[FTS_PACKET_LENGTH + 6];
	u8	auc_i2c_write_buf[10];
	u8	bt_ecc;

	struct timeval begin_tv, end_tv;
	do_gettimeofday(&begin_tv);

	for (i=0; i<16; i++) {
		/*********Step 1:Reset	CTPM *****/
		/*write 0xaa to register 0xfc*/
		ft5x02_write_reg(client, 0xfc, 0xaa);
		msleep(30);
		 /*write 0x55 to register 0xfc*/
		ft5x02_write_reg(client, 0xfc, 0x55);
		//delay_qt_ms(18);
		delay_qt_ms(25);
		/*********Step 2:Enter upgrade mode *****/
		#if 0
		auc_i2c_write_buf[0] = 0x55;
		auc_i2c_write_buf[1] = 0xaa;
		do
		{
			i ++;
			i_ret = ft5x02_i2c_Write(client, auc_i2c_write_buf, 2);
			delay_qt_ms(5);
		}while(i_ret <= 0 && i < 5 );
		#else
		auc_i2c_write_buf[0] = 0x55;
		ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
		delay_qt_ms(1);
		auc_i2c_write_buf[0] = 0xaa;
		ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
		#endif

		/*********Step 3:check READ-ID***********************/	 
		delay_qt_ms(1);
	
		ft5x02_upgrade_send_head(client);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;

		ft5x02_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);
		
		if (reg_val[0] == 0x79
			&& reg_val[1] == 0x02) {
			//dev_dbg(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
			pr_info("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
			break;
		} else {
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
			//delay_qt_ms(1);
		}
	}
	if (i >= 6)
		return -EIO;
	/********Step 4:enable write function*/
	ft5x02_upgrade_send_head(client);
	auc_i2c_write_buf[0] = 0x06;
	ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);

	/*********Step 5:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;
	
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;

	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;
	for (j=0; j<packet_number; j++) {
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8)(temp>>8);
		packet_buf[3] = (u8)temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8)(lenght>>8);
		packet_buf[5] = (u8)lenght;
		if(temp>=0x4c00 && temp <(0x4c00+512))
			continue;

		for (i=0; i<FTS_PACKET_LENGTH; i++) {
			packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
			bt_ecc ^= packet_buf[6+i];
		}
		ft5x02_upgrade_send_head(client);
		ft5x02_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH+6);
		delay_qt_ms(2);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8)(temp>>8);
		packet_buf[3] = (u8)temp;

		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8)(temp>>8);
		packet_buf[5] = (u8)temp;

		for (i=0; i<temp; i++) {
			packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
			bt_ecc ^= packet_buf[6+i];
		}
		ft5x02_upgrade_send_head(client);
		ft5x02_i2c_Write(client, packet_buf, temp+6);
		delay_qt_ms(2);
	}

	/********Disable write function*/
	ft5x02_upgrade_send_head(client);
	auc_i2c_write_buf[0] = 0x04;
	ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
	delay_qt_ms(1);
	/*********Step 6: read out checksum***********************/
	ft5x02_upgrade_send_head(client);
	auc_i2c_write_buf[0] = 0xcc;
	ft5x02_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1); 

	if (reg_val[0] != bt_ecc) {
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n", reg_val[0], bt_ecc);
		return -EIO;
	}

	/*********Step 7: reset the new FW***********************/
	ft5x02_upgrade_send_head(client);
	auc_i2c_write_buf[0] = 0x07;
	ft5x02_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(200);  /*make sure CTP startup normally*/
	//DBG("-------upgrade successful-----\n");

	do_gettimeofday(&end_tv);
	DBG("cost time=%lu.%lu\n", end_tv.tv_sec-begin_tv.tv_sec, 
			end_tv.tv_usec-begin_tv.tv_usec);
	
	return 0;
}

/*
*get firmware size

@firmware_name:firmware name
*note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int ft5x02_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s", firmware_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

/*
*read firmware buf for .bin file.

@firmware_name: fireware name
@firmware_buf: data buf of fireware

note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int ft5x02_ReadFirmware(char *firmware_name,
			       unsigned char *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s", firmware_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}

int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
				       char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret;
	int fwsize = ft5x02_GetFirmwareSize(firmware_name);

	if (fwsize <= 0) {
		dev_err(&client->dev, "%s ERROR:Get firmware size failed\n",
					__func__);
		return -EIO;
	}

	if (fwsize < 8 || fwsize > 32 * 1024) {
		dev_dbg(&client->dev, "%s:FW length error\n", __func__);
		return -EIO;
	}

	/*=========FW upgrade========================*/
	pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

	if (ft5x02_ReadFirmware(firmware_name, pbt_buf)) {
		dev_err(&client->dev, "%s() - ERROR: request_firmware failed\n",
					__func__);
		kfree(pbt_buf);
		return -EIO;
	}

	/*call the upgrade function */
	i_ret = ft5x02_ctpm_fw_upgrade(client, pbt_buf, fwsize);
	if (i_ret != 0)
		dev_dbg(&client->dev, "%s() - ERROR:[FTS] upgrade failed..\n",
					__func__);
	kfree(pbt_buf);

	return i_ret;
}


/*
upgrade with *.i file
*/
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client * client)
{
	u8 * pbt_buf = NULL;
	int i_ret;
	int fw_len = sizeof(CTPM_FW);

	/*judge the fw that will be upgraded
	 * if illegal, then stop upgrade and return.
	*/
	if (fw_len<8 || fw_len>32*1024) {
		dev_err(&client->dev, "[FTS]----FW length error\n");
		return -EIO;
	}	
//	if((CTPM_FW[fw_len-8]^CTPM_FW[fw_len-6])==0xFF
//		&& (CTPM_FW[fw_len-7]^CTPM_FW[fw_len-5])==0xFF
//		&& (CTPM_FW[fw_len-3]^CTPM_FW[fw_len-4])==0xFF)
	{
		/*FW upgrade*/
		pbt_buf = CTPM_FW;
		/*call the upgrade function*/
		i_ret =  ft5x02_ctpm_fw_upgrade(client, pbt_buf, sizeof(CTPM_FW));
		if (i_ret != 0)
			dev_err(&client->dev, "[FTS]---- upgrade failed. err=%d.\n", i_ret);
		else
			dev_dbg(&client->dev, "[FTS]----upgrade successful\n");
	}
//	else
//	{
//		dev_err(&client->dev, "[FTS]----FW format error\n");
//		return -EBADFD;
//	}
	return i_ret;
}



/* 
*Read touch point information when the interrupt  is asserted.
*/
static int ft5x02_read_Touchdata(struct ft5x02_ts_data *data)
{
	struct ts_event *event = &data->event;
	u8 buf[POINT_READ_BUF] = { 0 };
	int ret = -1;
	int i = 0;
	int touch_point = 0;
	u8 pointid = FT_MAX_ID;

	ret = ft5x02_i2c_Read(data->client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		dev_err(&data->client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}
	memset(event, 0, sizeof(struct ts_event));

	//event->touch_point = 0;
	event->touch_point = (buf[2] & 0x07);
	for (i = 0; i < event->touch_point; i++) {
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= FT_MAX_ID)
			break;
		else
		    touch_point ++;
			//event->touch_point++;
		event->au16_x[i] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[i] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];
		event->au8_touch_event[i] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
	}
    
	event->pressure = FT_PRESS;
	
    if(event->touch_point != touch_point){
        printk("event->touch_point = %d,touch_point= %d\n",event->touch_point,touch_point);        
        return -1;
    }  

	return 0;
}

#ifdef CFG_SUPPORT_TOUCH_KEY
/* 
*Processes the key message when the CFG_SUPPORT_TOUCH_KEY has defined
*/
int ft5x02_touch_key_process(struct input_dev *dev, int x, int y, int touch_event)
{
	int i;
	int key_id;

	if ( y < 517&&y > 497)
	{
		key_id = 1;
	}
	else if ( y < 367&&y > 347)
	{
		key_id = 0;
	}

	else if ( y < 217&&y > 197)
	{
		key_id = 2;
	}  
	else if (y < 67&&y > 47)
	{
		key_id = 3;
	}
	else
	{
		key_id = 0xf;
	}
    
	for(i = 0; i <CFG_NUMOFKEYS; i++ )
	{
		if(tsp_keystatus[i])
		{
			if(touch_event == 1)
			{
				input_report_key(dev, tsp_keycodes[i], 0);
				tsp_keystatus[i] = KEY_RELEASE;
			}
		}
		else if( key_id == i )
		{
			if( touch_event == 0)                                  // detect
			{
				input_report_key(dev, tsp_keycodes[i], 1);
				tsp_keystatus[i] = KEY_PRESS;
			}
		}
	}
	return 0;
    
}    
#endif

/*
*report the point information
*/
static void ft5x02_report_value(struct ft5x02_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i;

    for (i  = 0; i < event->touch_point; i++)
    {   
        
        //printk("ft5x02_report_value x=%d,y=%d,au8_touch_event=%d,id=%d\n",event->au16_x[i],event->au16_y[i],event->au8_touch_event[i],event->au8_finger_id[i]);             
        if (event->au16_x[i] > data->x_max){
                     //printk("event->au16_x[i] = %d\n",event->au16_x[i]);           
        event->au16_x[i] = data->x_max - 1;

        }

        if (event->au16_y[i] > data->y_max){
                                //printk("event->au16_y[i] = %d\n",event->au16_y[i]);  
            event->au16_y[i] = data->y_max - 1;

            }
         if(event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2) 
        {
        
        
                //printk("ft5x02_report_value111 x=%d,y=%d,au8_touch_event=%d,id=%d\n",event->au16_x[i],event->au16_y[i],event->au8_touch_event[i],event->au8_finger_id[i]);             

            input_report_key(data->input_dev, BTN_TOUCH, 1);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
            input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->au8_finger_id[i]);
            input_mt_sync(data->input_dev);            
        }
        else if (event->au8_touch_event[i]== 1) //ft ic report a swaped xy value when finger release,fix it by re swap 
        {
            swap(event->au16_y[i],event->au16_x[i]);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
            input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->au8_finger_id[i]);
            input_report_key(data->input_dev, BTN_TOUCH, 0);
            input_mt_sync(data->input_dev);
            //printk("event->au8_touch_event[i] = %d\n",event->au8_touch_event[i]);
        }    
                    
        
    }   
    if(event->touch_point == 0){
            input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
            input_report_key(data->input_dev, BTN_TOUCH, 0);
            input_mt_sync(data->input_dev);
        printk("event->touch_point = %d\n",event->touch_point);  
    }    
	input_sync(data->input_dev);
}	/*end ft5x02_report_value*/

static void ft5x02_ts_pen_irq_work(struct work_struct *work)
{
    struct ft5x02_ts_data *ft5x02_ts;
	ft5x02_ts = i2c_get_clientdata(this_client);
	
    int ret = 0;
    ret = ft5x02_read_Touchdata(ft5x02_ts);
	if (ret == 0)
		ft5x02_report_value(ft5x02_ts);
	enable_irq(this_client->irq);
}
/*
 * The ft5x02 device will signal the host about TRIGGER_FALLING. 
 */

static irqreturn_t ft5x02_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x02_ts_data *ft5x02_ts = dev_id;
	int ret = 0;
	disable_irq_nosync(ft5x02_ts->irq);
    //printk("ft5x02_ts_interrupt\n");
    
	if (!work_pending(&ft5x02_ts->pen_event_work)) {
		queue_work(ft5x02_ts->ts_workqueue, &ft5x02_ts->pen_event_work);
	}
	
	return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x02_ts_suspend(struct early_suspend *handler)
{
	struct ft5x02_ts_data *ts = container_of(handler, struct ft5x02_ts_data,
						early_suspend);

	dev_dbg(&ts->client->dev, "[FTS]ft5x02 suspend\n");
	disable_irq(ts->pdata->irq);
}

static void ft5x02_ts_resume(struct early_suspend *handler)
{
	struct ft5x02_ts_data *ts = container_of(handler, struct ft5x02_ts_data,
						early_suspend);

	printk("[FTS]ft5x02 resume.\n");
	ts->pdata->power(0);
	msleep(10);
	ts->pdata->power(1);
	msleep(200);
	ft5x02_Init_IC_Param(ts->client);

	enable_irq(ts->pdata->irq);
}
#else
#define ft5x02_ts_suspend	NULL
#define ft5x02_ts_resume		NULL
#endif

static int ft5x02_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ft5x02_platform_data *pdata =
	    (struct ft5x02_platform_data *)client->dev.platform_data;
	struct ft5x02_ts_data *ft5x02_ts;
	struct input_dev *input_dev;
	int err = 0;
    u8 ver;

#ifdef CFG_SUPPORT_TOUCH_KEY
    	int i;
#endif
  	if(pdata->power){
		pdata->power(0);
		mdelay(10);
		pdata->power(1);
		mdelay(150);
	}
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
	client->irq =pdata->irq;
	ft5x02_ts = kzalloc(sizeof(struct ft5x02_ts_data), GFP_KERNEL);

	if (!ft5x02_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	i2c_set_clientdata(client, ft5x02_ts);
	ft5x02_ts->irq = client->irq;
	ft5x02_ts->client = client;
	ft5x02_ts->pdata = pdata;
	ft5x02_ts->x_max = pdata->x_max - 1;
	ft5x02_ts->y_max = pdata->y_max - 1;
  printk("ft5x02_ts->x_max = %d",ft5x02_ts->x_max);
    printk("ft5x02_ts->y_max = %d",ft5x02_ts->y_max);
  
    printk("pdata->x_max = %d",pdata->x_max);
    printk("pdata->y_max = %d",pdata->y_max);
  
	err = ft5x02_read_reg(client,0xa6, &ver);
	if(err < 0){
        printk("ft5x02_ts_probe no device found\n");
        goto exit_no_dev;
    }
    
	this_client = client;
	    
	INIT_WORK(&ft5x02_ts->pen_event_work, ft5x02_ts_pen_irq_work);
	ft5x02_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x02_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_no_dev;
	}
    
//	err = request_threaded_irq(client->irq, NULL, ft5x02_ts_interrupt,
//				   IRQF_TRIGGER_FALLING, client->dev.driver->name,
//				   ft5x02_ts);
	
printk("==enable Irq=\n");
    if (pdata->init_irq) {
        pdata->init_irq();
    }
	disable_irq_nosync(client->irq);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	ft5x02_ts->input_dev = input_dev;
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, ft5x02_ts->x_max, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, ft5x02_ts->y_max, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
    	input_set_abs_params(input_dev,
			     ABS_MT_TRACKING_ID, 0, CFG_MAX_TOUCH_POINTS, 0, 0);


    	set_bit(EV_KEY, input_dev->evbit);
    	set_bit(EV_ABS, input_dev->evbit);

#ifdef CFG_SUPPORT_TOUCH_KEY
    	/*setup key code area*/
    	set_bit(EV_SYN, input_dev->evbit);
    	set_bit(BTN_TOUCH, input_dev->keybit);
    	input_dev->keycode = tsp_keycodes;
    	for(i = 0; i < CFG_NUMOFKEYS; i++)
    	{
        	input_set_capability(input_dev, EV_KEY, ((int*)input_dev->keycode)[i]);
        	tsp_keystatus[i] = KEY_RELEASE;
    	}
#endif

	input_dev->name		= FT5X02_NAME;
	err = input_register_device(input_dev);
	if (err) 
	{
		dev_err(&client->dev,
				"ft5x02_ts_probe: failed to register input device: %s\n",
				dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

   	 msleep(150);  /*make sure CTP already finish startup process*/
#ifdef FT5X02_UPGRADE
	/*upgrade for program the app to RAM*/
	dev_dbg(&client->dev, "[FTS]----ready for upgrading---\n");
	if (fts_ctpm_fw_upgrade_with_i_file(client) < 0) {
		dev_err(&client->dev, "[FTS]-----upgrade failed!----\n");
	}
	else
		dev_dbg(&client->dev, "[FTS]-----upgrade successful!----\n");
#endif

#ifdef FT5X02_CONFIG_INI
	if (ft5x02_Get_Param_From_Ini(FT5X02_CONFIG_NAME) >= 0)
		ft5x02_Init_IC_Param(client);
	else
		dev_err(&client->dev, "[FTS]-------Get ft5x02 param from INI file failed\n");
#else
	msleep(50);	/*wait...*/
	while(1){
		DBG("-----------------------------------------Init ic param\r\n");
		if (ft5x02_Init_IC_Param(client) >=0 )
		{
			DBG("---------------------------------------get ic param\r\n");
			if(ft5x02_get_ic_param(client) >=0)
				break;
		}
		msleep(50);
		ft5x02_write_reg(client,0x00,0x40);
		msleep(150);
        ft5x02_write_reg(client,0x00,0x00);		
        msleep(150);
	}
#endif

    err = request_irq(client->irq, ft5x02_ts_interrupt, IRQF_DISABLED, client->dev.driver->name, ft5x02_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x02_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}
	

#ifdef SYSFS_DEBUG
	ft5x02_create_sysfs_debug(client);
#endif

#ifdef FTS_APK_DEBUG
	ft5x02_create_apk_debug_channel(client);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	printk("==register_early_suspend =\n");
	ft5x02_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ft5x02_ts->early_suspend.suspend = ft5x02_ts_suspend;
	ft5x02_ts->early_suspend.resume	= ft5x02_ts_resume;
	register_early_suspend(&ft5x02_ts->early_suspend);
#endif

	//enable_irq(client->irq);
    	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
	
exit_input_dev_alloc_failed:
	free_irq(client->irq, ft5x02_ts);
#ifdef CONFIG_PM
exit_request_reset:
	gpio_free(ft5x02_ts->pdata->reset);
#endif	
exit_irq_request_failed:
	cancel_work_sync(&ft5x02_ts->pen_event_work);
	destroy_workqueue(ft5x02_ts->ts_workqueue);
exit_no_dev:
	i2c_set_clientdata(client, NULL);
	kfree(ft5x02_ts);

exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}



#ifdef SYSFS_DEBUG
/*
*init ic param
*for example: cat ft5x02initparam
*/
static ssize_t ft5x02_initparam_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t num_read_chars = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);
	if (ft5x02_Get_Param_From_Ini(FT5X02_CONFIG_NAME) >= 0) {
		if (ft5x02_Init_IC_Param(client) >= 0)
			num_read_chars = sprintf(buf, "%s",
				"ft5x02 init param successful\r\n");
		else
			num_read_chars = sprintf(buf, "%s",
				"ft5x02 init param failed!\r\n");
	} else {
		num_read_chars = sprintf(buf, "%s",
				"get ft5x02 config ini failed!\r\n");
	}
	mutex_unlock(&g_device_mutex);
	
	return num_read_chars;
}


static ssize_t ft5x02_initparam_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}

/*
*get ic param
*for example: cat ft5x02getparam
*/
static ssize_t ft5x02_getparam_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t num_read_chars = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);
	
	ft5x02_get_ic_param(client);
	
	mutex_unlock(&g_device_mutex);
	
	return num_read_chars;
}

static ssize_t ft5x02_getparam_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}


static ssize_t ft5x02_rwreg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}

/*
*read and write register
*for example:
*read register: echo "88" > ft5x02rwreg
*write register: echo "8808" > ft5x02rwreg
*/
static ssize_t ft5x02_rwreg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	ssize_t num_read_chars = 0;
	int retval;
	long unsigned int wmreg = 0;
	u8 regaddr = 0xff, regvalue = 0xff;
	u8 valbuf[5] = {0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&g_device_mutex);
	num_read_chars = count - 1;

	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			pr_info("please input 2 or 4 character\n");
			goto error_return;
		}
	}

	memcpy(valbuf, buf, num_read_chars);
	retval = strict_strtoul(valbuf, 16, &wmreg);

	if (0 != retval) {
		dev_err(&client->dev, "%s() - ERROR: Could not convert the "\
						"given input to a number." \
						"The given input was: \"%s\"\n",
						__func__, buf);
		goto error_return;
	}

	if (2 == num_read_chars) {
		/*read register*/
		regaddr = wmreg;
		if (ft5x02_read_reg(client, regaddr, &regvalue) < 0)
			dev_err(&client->dev, "Could not read the register(0x%02x)\n",
						regaddr);
		else
			pr_info("the register(0x%02x) is 0x%02x\n",
					regaddr, regvalue);
	} else {
		regaddr = wmreg >> 8;
		regvalue = wmreg;
		if (ft5x02_write_reg(client, regaddr, regvalue) < 0)
			dev_err(&client->dev, "Could not write the register(0x%02x)\n",
							regaddr);
		else
			dev_err(&client->dev, "Write 0x%02x into register(0x%02x) successful\n",
							regvalue, regaddr);
	}

error_return:
	mutex_unlock(&g_device_mutex);

	return count;
}

static DEVICE_ATTR(ft5x02initparam, S_IRUGO | S_IWUSR, ft5x02_initparam_show,
		   ft5x02_initparam_store);

static DEVICE_ATTR(ft5x02getparam, S_IRUGO | S_IWUSR, ft5x02_getparam_show,
		   ft5x02_getparam_store);

static DEVICE_ATTR(ft5x02rwreg, S_IRUGO | S_IWUSR, ft5x02_rwreg_show,
		   ft5x02_rwreg_store);


/*add your attr in here*/
static struct attribute *ft5x02_attributes[] = {
	&dev_attr_ft5x02initparam.attr,
	&dev_attr_ft5x02getparam.attr,
	&dev_attr_ft5x02rwreg.attr,
	NULL
};
static struct attribute_group ft5x02_attribute_group = {
	.attrs = ft5x02_attributes
};
static int ft5x02_create_sysfs_debug(struct i2c_client *client)
{
	int err = 0;
	err = sysfs_create_group(&client->dev.kobj, &ft5x02_attribute_group);
	if (0 != err) {
		dev_err(&client->dev,
					 "%s() - ERROR: sysfs_create_group() failed.\n",
					 __func__);
		sysfs_remove_group(&client->dev.kobj, &ft5x02_attribute_group);
		return -EIO;
	} else {
		mutex_init(&g_device_mutex);
		pr_info("ft5x0x:%s() - sysfs_create_group() succeeded.\n",
				__func__);
	}
	return err;
}
#endif

#ifdef FTS_APK_DEBUG
/*create apk debug channel*/

/*please don't modify these macro*/
#define PROC_UPGRADE			0
#define PROC_READ_REGISTER		1
#define PROC_WRITE_REGISTER	2
#define PROC_RESET_PARAM		3
#define PROC_NAME	"ft5x02-debug"

static unsigned char proc_operate_mode = PROC_UPGRADE;
static struct proc_dir_entry *ft5x02_proc_entry;
/*interface of write proc*/
static int ft5x02_debug_write(struct file *filp, 
	const char __user *buff, unsigned long len, void *data)
{
	struct i2c_client *client = (struct i2c_client *)ft5x02_proc_entry->data;
	unsigned char writebuf[FTS_PACKET_LENGTH];
	int buflen = len;
	int writelen = 0;
	int ret = 0;
	
	if (copy_from_user(&writebuf, buff, buflen)) {
		dev_err(&client->dev, "%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];
	
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			DBG("%s\n", upgrade_file_path);
			disable_irq(client->irq);

			ret = fts_ctpm_fw_upgrade_with_app_file(client, upgrade_file_path);

			enable_irq(client->irq);
			if (ret < 0) {
				dev_err(&client->dev, "%s:upgrade failed.\n", __func__);
				return ret;
			}
		}
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		DBG("%s:register addr=0x%02x\n", __func__, writebuf[1]);
		ret = ft5x02_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		DBG("%s:register addr=0x%02x register value=0x%02x\n", __func__, writebuf[1], writebuf[2]);
		ret = ft5x02_write_reg(client, writebuf[1], writebuf[2]);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_RESET_PARAM:
#ifdef FT5X02_CONFIG_INI	
		if (ft5x02_Get_Param_From_Ini(FT5X02_CONFIG_NAME) >= 0) {
			if (ft5x02_Init_IC_Param(client) >= 0)
				DBG("%s:ft5x02 init param successful\r\n", __func__);
			else
				dev_err(&client->dev, "%s:ft5x02 init param failed!\r\n", __func__);
		} else
			dev_err(&client->dev, "%s:get ft5x02 config ini failed!\r\n", __func__);
#else
		if (ft5x02_Init_IC_Param(client) >= 0)
			DBG("%s:ft5x02 init param successful\r\n", __func__);
		else
			dev_err(&client->dev, "%s:ft5x02 init param failed!\r\n", __func__);
#endif
		break;
	default:
		break;
	}
	
	return len;
}

/*interface of read proc*/
static int ft5x02_debug_read( char *page, char **start,
	off_t off, int count, int *eof, void *data )
{
	struct i2c_client *client = (struct i2c_client *)ft5x02_proc_entry->data;
	int ret = 0;
	unsigned char buf[PAGE_SIZE];
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		/*after calling ft5x0x_debug_write to upgrade*/
		regaddr = 0xA6;
		ret = ft5x02_read_reg(client, regaddr, &regvalue);
		if (ret < 0)
			num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
		else
			num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = ft5x02_i2c_Read(client, NULL, 0, buf, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:read iic error\n", __func__);
			return ret;
		} else
			DBG("%s:value=0x%02x\n", __func__, buf[0]);
		num_read_chars = 1;
		break;
	default:
		break;
	}
	
	memcpy(page, buf, num_read_chars);

	return num_read_chars;
}
int ft5x02_create_apk_debug_channel(struct i2c_client * client)
{
	ft5x02_proc_entry = create_proc_entry(PROC_NAME, 0777, NULL);
	if (NULL == ft5x02_proc_entry) {
		dev_err(&client->dev, "Couldn't create proc entry!\n");
		return -ENOMEM;
	} else {
		dev_info(&client->dev, "Create proc entry success!\n");
		ft5x02_proc_entry->data = client;
		ft5x02_proc_entry->write_proc = ft5x02_debug_write;
		ft5x02_proc_entry->read_proc = ft5x02_debug_read;
	}
	return 0;
}

void ft5x02_release_apk_debug_channel(void)
{
	if (ft5x02_proc_entry)
		remove_proc_entry(PROC_NAME, NULL);
}
#endif

static int __devexit ft5x02_ts_remove(struct i2c_client *client)
{
	struct ft5x02_ts_data *ft5x02_ts;
	ft5x02_ts = i2c_get_clientdata(client);
	input_unregister_device(ft5x02_ts->input_dev);
	#ifdef CONFIG_PM
	gpio_free(ft5x02_ts->pdata->reset);
	#endif
	#ifdef SYSFS_DEBUG
	mutex_destroy(&g_device_mutex);
	sysfs_remove_group(&client->dev.kobj, &ft5x02_attribute_group);
	#endif
	#ifdef FTS_APK_DEBUG
	ft5x02_release_apk_debug_channel();
	#endif
	cancel_work_sync(&ft5x02_ts->pen_event_work);
	destroy_workqueue(ft5x02_ts->ts_workqueue);
	free_irq(client->irq, ft5x02_ts);
	kfree(ft5x02_ts);
	i2c_set_clientdata(client, NULL); 	
	return 0;
}


static const struct i2c_device_id ft5x02_ts_id[] = {
	{ FT5X02_NAME, 0 },{ }
};

MODULE_DEVICE_TABLE(i2c, ft5x02_ts_id);

static struct i2c_driver ft5x02_ts_driver = {
	.probe		= ft5x02_ts_probe,
	.remove		= __devexit_p(ft5x02_ts_remove),
	.id_table	= ft5x02_ts_id,
//	.suspend = ft5x02_ts_suspend,
//	.resume = ft5x02_ts_resume,
	.driver	= {
		.name	= FT5X02_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init ft5x02_ts_init(void)
{
	int ret;
	ret = i2c_add_driver(&ft5x02_ts_driver);
	if (ret) {
		printk(KERN_WARNING "Adding ft5x02 driver failed "
		       "(errno = %d)\n", ret);
	} else {
		pr_info("Successfully added driver %s\n",
			  ft5x02_ts_driver.driver.name);
	}
	return ret;
}

static void __exit ft5x02_ts_exit(void)
{
	i2c_del_driver(&ft5x02_ts_driver);
}

module_init(ft5x02_ts_init);
module_exit(ft5x02_ts_exit);

MODULE_AUTHOR("<luowj>");
MODULE_DESCRIPTION("FocalTech ft5x02 TouchScreen driver");
MODULE_LICENSE("GPL");
