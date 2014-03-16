/* 
 * drivers/input/touchscreen/zforce0x_ts.c
 *
 * FocalTech zforce TouchScreen driver. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
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
 *
 *	note: only support mulititouch	Wenfs 2010-10-01
 *  for this touchscreen to work, it's slave addr must be set to 0x7e | 0x70
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include "zforce_ts.h"
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

#include <linux/irq.h>

#include <mach/gpio_data.h>
#include <mach/gpio.h>

#if GTP_ICS_SLOT_REPORT
#include <linux/input/mt.h>
#endif

//modify by emdoor jim.kuang 2013-05-09 for Syste First Start ,Tp can't work
#define ZFORCE_USE_DELAYWORK	1
#if defined(ZFORCE_USE_DELAYWORK)
#include <linux/workqueue.h>
static struct workqueue_struct *zforce_queue = NULL;
static struct delayed_work zforce_work;
static int debug_flag=0;
struct zforce_ts_priv *zforce_priv = NULL;
#define debug() if(debug_flag) printk("################%s %d ##############",__func__,__LINE__)
#endif
//Add the flag for TP work or not
u8 zforce_work_flag=0;
//#include <mach/irqs.h>
//#include <mach/system.h>
//#include <mach/hardware.h>
//#include <mach/sys_config.h>
//#include "ctp_platform_ops.h"

#define FOR_TSLIB_TEST
//#define PRINT_INT_INFO
//#define PRINT_POINT_INFO
//#define DEBUG
//#define TOUCH_KEY_SUPPORT
#ifdef TOUCH_KEY_SUPPORT
#define TOUCH_KEY_LIGHT_SUPPORT
#define TOUCH_KEY_FOR_EVB13
#define TOUCH_KEY_FOR_ANGDA
#ifdef TOUCH_KEY_FOR_ANGDA
#define TOUCH_KEY_X_LIMIT	(60000)
//#define TOUCH_KEY_NUMBER	(4)
#endif
#ifdef TOUCH_KEY_FOR_EVB13
#define TOUCH_KEY_LOWER_X_LIMIT	(848)
#define TOUCH_KEY_HIGHER_X_LIMIT	(852)
//#define TOUCH_KEY_NUMBER	(5)
#endif
#endif

//#define CONFIG_SUPPORT_FTS_CTP_UPG


struct i2c_dev{
struct list_head list;	
struct i2c_adapter *adap;
struct device *dev;
};

static struct class *i2c_dev_class;
static LIST_HEAD (i2c_dev_list);
static DEFINE_SPINLOCK(i2c_dev_list_lock);

static int ctp_vendor;
static int ctp_a730_id = 0;
static int key_value = 0;

static struct i2c_client *this_client;
#ifdef TOUCH_KEY_LIGHT_SUPPORT
static int gpio_light_hdle = 0;
#endif
#ifdef TOUCH_KEY_SUPPORT
static int key_tp  = 0;
static int key_val = 0;
#endif

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
///////////////////////////////////////////////
//specific tp related macro: need be configured for specific tp
#ifdef CONFIG_ARCH_SUN4I
#define CTP_IRQ_NO			(IRQ_EINT21)
#elif defined CONFIG_ARCH_SUN5I
#define CTP_IRQ_NO			(IRQ_EINT9)
#endif

#define CTP_IRQ_MODE			(LOW_LEVEL)
#define CTP_NAME		"zforce-ts"	
#define TS_RESET_LOW_PERIOD		(1)
#define TS_INITIAL_HIGH_PERIOD		(30)
#define TS_WAKEUP_LOW_PERIOD	(20)
#define TS_WAKEUP_HIGH_PERIOD	(20)
#define TS_POLL_DELAY			(10)	/* ms delay between samples */
#define TS_POLL_PERIOD			(10)	/* ms delay between samples */
#define SCREEN_MAX_X			(screen_max_x)
#define SCREEN_MAX_Y			(screen_max_y)
#define PRESS_MAX			(255)


#ifndef CONFIG_HAS_EARLYSUSPEND
#define CONFIG_HAS_EARLYSUSPEND
#endif


static void* __iomem gpio_addr = NULL;
static int gpio_int_hdle = 0;
static int gpio_wakeup_hdle = 0;
static int gpio_reset_hdle = 0;
static int gpio_wakeup_enable = 1;
static int gpio_reset_enable = 1;


static int screen_max_x = 0;
static int screen_max_y = 0;
static int revert_x_flag = 0;
static int revert_y_flag = 0;
static int exchange_x_y_flag = 0;

static int user_dbg_flag = 0;
/* Addresses to scan */
static union{
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
}u_i2c_addr = {{0x00},};
static __u32 twi_id = 0;

struct ts_event {
	u16	x1;
	u16	y1;
	u16	x2;
	u16	y2;
	u16	x3;
	u16	y3;
	u16	x4;
	u16	y4;
	u16	x5;
	u16	y5;
	u16	pressure;
	s16 touch_ID1;
	s16 touch_ID2;
	s16 touch_ID3;
	s16 touch_ID4;
	s16 touch_ID5;
    u8  touch_point;
    u8  gest_id;
};

struct zforce_ts_data {
	struct input_dev	*input_dev;
	struct ts_event		event;
	struct work_struct 	pen_event_work;
	struct workqueue_struct *ts_workqueue;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
};

static void zforce_get_reg_key(u8 gest_id)
{
	//broncho xiangyu
	if(ctp_vendor == 1)
	{
		switch(gest_id)
		{
			case 1:
				key_value = KEY_MENU;
				break;
			case 2:
				key_value = KEY_HOME;
				break;
			case 4:
				key_value = KEY_BACK;
				break;
			case 8:
				key_value = KEY_SEARCH;
				break;
			default:
				break;
		}
	}
	else if(ctp_vendor == 2) //a723b
	{
		switch(gest_id)
		{
			case 1:
				key_value = KEY_BACK;
				break;
			case 2:
				key_value = KEY_HOME;
				break;
			case 4:
				key_value = KEY_MENU;
				break;
			case 8:
				key_value = KEY_SEARCH;
				break;
			default:
				break;
		}
	}
	else
	{
	}

	return ;//broncho xiangyu
}

static void zforce_report_key(u16 x, u16 y)
{
	return ;
}


#ifdef TOUCH_KEY_LIGHT_SUPPORT
static void zforce_lighting(void)
{
	if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_light_hdle, 1, "ctp_light")){
		pr_info("zforce_ts_light: err when operate gpio. \n");
	}    
	msleep(15);
	if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_light_hdle, 0, "ctp_light")){
		pr_info("zforce_ts_light: err when operate gpio. \n");
	}         

	return;
}
#endif

#ifdef TOUCH_KEY_SUPPORT
static void zforce_report_touchkey(void)
{
	struct zforce_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
	//print_point_info("x=%d===Y=%d\n",event->x1,event->y1);

	if(event->gest_id != 0) //report key
	{
#ifdef PRINT_POINT_INFO
		printk("gest_id key_value %d\n", key_value);
#endif
		input_report_key(data->input_dev, key_value, 1);
		input_sync(data->input_dev);  
	}
	else //report x y
	{
#ifdef TOUCH_KEY_FOR_ANGDA
		if((1==event->touch_point)&&(event->x1 > TOUCH_KEY_X_LIMIT)){
			key_tp = 1;
			if(event->y1 < 40){
				key_val = 1;
				input_report_key(data->input_dev, key_val, 1);
				input_sync(data->input_dev);  
				print_point_info("===KEY 1====\n");
			}else if(event->y1 < 90){
				key_val = 2;
				input_report_key(data->input_dev, key_val, 1);
				input_sync(data->input_dev);     
				print_point_info("===KEY 2 ====\n");
			}else{
				key_val = 3;
				input_report_key(data->input_dev, key_val, 1);
				input_sync(data->input_dev);     
				print_point_info("===KEY 3====\n");	
			}
		} else{
			key_tp = 0;
		}
#endif
#ifdef TOUCH_KEY_FOR_EVB13
		if((1==event->touch_point)&&((event->x1 > TOUCH_KEY_LOWER_X_LIMIT)&&(event->x1<TOUCH_KEY_HIGHER_X_LIMIT))){
			key_tp = 1;
			if(event->y1 < 5){
				key_val = 1;
				input_report_key(data->input_dev, key_val, 1);
				input_sync(data->input_dev);  
				print_point_info("===KEY 1====\n");     
			}else if((event->y1 < 45)&&(event->y1>35)){
				key_val = 2;
				input_report_key(data->input_dev, key_val, 1);
				input_sync(data->input_dev);     
				print_point_info("===KEY 2 ====\n");
			}else if((event->y1 < 75)&&(event->y1>65)){
				key_val = 3;
				input_report_key(data->input_dev, key_val, 1);
				input_sync(data->input_dev);     
				print_point_info("===KEY 3====\n");
			}else if ((event->y1 < 105)&&(event->y1>95))	{
				key_val = 4;
				input_report_key(data->input_dev, key_val, 1);
				input_sync(data->input_dev);     
				print_point_info("===KEY 4====\n");	
			}
		}else{
			key_tp = 0;
		}
#else
		if((1==event->touch_point)&&((event->x1 > SCREEN_MAX_X + 10) || (event->y1 > SCREEN_MAX_Y + 10)))
		{
			print_point_info("event->y1 %d\n", event->y1);
			zforce_report_key(event->x1, event->y1);
			print_point_info("key_value =%d\n", key_value);
			input_report_key(data->input_dev, key_value, 1);
			input_sync(data->input_dev);     
		}
#endif
	}
#ifdef TOUCH_KEY_LIGHT_SUPPORT
	zforce_lighting();
#endif
	return;
}
#endif

static int detect_device(void)
{
	return 0;
} 

#define ZF_DEACTIVATE 0x00
#define ZF_ACTIVATE 0x01
#define ZF_SET_RESOLUTION 0x02
#define ZF_CONFIGURE 0x03
#define ZF_REQ_COORDINATES 0x04
#define ZF_SCANNINGFREN	   0x08
#define ZF_AREA_SIZE 0x09
#define ZF_REQ_VERSION 0x1E//0x0A
#define ZF_CALIBRATION 0x1A
// Bit flags for the startup state machinery. Response flags are set at response from the
// ZForce, but ignored by the program logic

#define   ZF_VERSION_REQUESTED       0x0001
#define   ZF_VERSION_RESPONDED       0x0002
#define   ZF_DEACTIVATE_REQUESTED    0x0004
#define   ZF_DEACTIVATE_RESPONDED    0x0008
#define   ZF_ACTIVATE_REQUESTED  	 0x0010
#define   ZF_ACTIVATE_RESPONDED   	 0x0020
#define   ZF_RESOLUTION_REQUESTED	 0x0040
#define   ZF_RESOLUTION_RESPONDED	 0x0080
#define   ZF_CONFIGURE_REQUESTED 	 0x0100
#define   ZF_CONFIGURE_RESPONDED 	 0x0200
#define   ZF_FIRSTTOUCH_REQUESTED	 0x0400
#define   ZF_FIRSTTOUCH_RESPONDED	 0x0800

//add by Ethan
#define   ZF_TOUCHAREA_REQUESTED 	 0X1000
#define   ZF_TOUCHAREA_RESPONDED 	 0x2000
#define   ZF_CALIBRATION_REQUESTED	 0x4000
#define   ZF_CALIBRATION_RESPONDED   0x8000
//end add


#define ZF_MAX_X 800//1024//800
#define ZF_MAX_Y 480//600//480

// ZForce-device data
struct zforce_ts_priv {
	struct i2c_client *client;  // I2C client to communicate with the ZF chip
	struct input_dev *input;    // The registered input device to which events are reported

	struct delayed_work delayed_work;   // Scheduler entry for ISR "bottom half"
	struct work_struct  work;
	
	struct early_suspend early_suspend;  //zforce chip sleep/wake up

	int irq;   // IRQ on which ZF signals data available
	int startup_state; // Bit flags according to ZF_xx_REQUESTED/RESPONDED definitions

	struct hrtimer timer;
	int use_irq;
	int is_irq_enabled;
	int is_suspended;
	struct workqueue_struct *ts_workqueue;
};


static void zforce_ts_handle_deactivate(u_int8_t *data, int len);
static void zforce_ts_handle_activate(u_int8_t *data, int len);
static void zforce_ts_handle_configure(u_int8_t *data, int len);
static void zforce_ts_handle_setres(u_int8_t *data, int len);
static void zforce_ts_handle_touchdata(struct zforce_ts_priv *priv,
		u_int8_t *data, int len);
static void zforce_ts_handle_version(u_int8_t *data, int len);
static int zforce_ts_send_deactivate(struct zforce_ts_priv *priv);
static int zforce_ts_send_activate(struct zforce_ts_priv *priv);
static int zforce_ts_send_configure(struct zforce_ts_priv *priv, int dual_touch);
static int zforce_ts_send_setres(struct zforce_ts_priv *priv, int width,
		int height);
static int zforce_ts_send_touchdata_req(struct zforce_ts_priv *priv);
static int zforce_ts_send_version_req(struct zforce_ts_priv *priv);


//add by Ethan

static int zforce_ts_send_calibration(struct zforce_ts_priv *priv);
static void zforce_ts_handle_calibration(u_int8_t *data, int len);
static void zforce_ts_handle_toucharea(u_int8_t *data, int len);
static int zforce_ts_send_areasize(struct zforce_ts_priv *priv);

//end add



#if 1
struct zforce_ts_priv * g_priv = NULL;
//int one_more_work_done = 0;
int ext_work_loop = 0;
int ext_handle_data_done = 0;
int is_ts_suspended = 1;
void zforce_send_restart_cmd(void)
{
	//msleep(500);
	struct zforce_ts_priv * priv= g_priv;
	if(!priv)
	{
		printk("***** priv is NULL, func %s\n",__func__);
		return;
	}
	zforce_ts_send_deactivate(priv);
	zforce_ts_send_activate(priv);
	zforce_ts_send_setres(priv, ZF_MAX_X, ZF_MAX_Y);
	zforce_ts_send_configure(priv, 1);
	zforce_ts_send_areasize(priv);
	zforce_ts_send_calibration(priv);
	zforce_ts_send_touchdata_req(priv);
	printk("***** Do zforce tp restart cmd manual\n");
}

EXPORT_SYMBOL(zforce_send_restart_cmd);

void zforce_pwr_enable(int enable)
{
	if(enable)
	{
		GPIO_DIRECTION_OUTPUT(RESET_PORT,1);
		GPIO_DIRECTION_OUTPUT(TOUCH_PWR_EN,1);
		udelay(100);
		GPIO_DIRECTION_OUTPUT(RESET_PORT,1);
		udelay(900);
		//LCD CS open
		gpio_out(PAD_GPIOC_0, 1);		
		msleep(200);
	}
	else
	{
		GPIO_DIRECTION_OUTPUT(RESET_PORT,0);
		udelay(400);
		//LCD CS
		gpio_out(PAD_GPIOC_0, 0);
		udelay(20);
		GPIO_DIRECTION_OUTPUT(TOUCH_PWR_EN,0);
	}

}
#endif
EXPORT_SYMBOL(zforce_pwr_enable);


#ifdef CONFIG_HAS_EARLYSUSPEND
static void zforce_ts_suspend(struct early_suspend *handler)
{
    struct zforce_ts_priv  *priv; 
    priv =  container_of(handler, struct zforce_ts_priv, early_suspend);
    pr_info("==zforce_ts_suspend== \n");
    //zforce_ts_send_deactivate(priv);

	is_ts_suspended = 1;
	if(priv->use_irq)
	{
		if(priv->is_irq_enabled)
		{
			disable_irq_nosync(TS_INT);
			priv->is_irq_enabled = 0;
		}
		cancel_work_sync(&priv->work);
		ext_work_loop = 1;
		ext_handle_data_done = 1;
		#if 1
		#endif
	}
	else
		hrtimer_cancel(&priv->timer);
	
#if 1
	zforce_pwr_enable(0);
#endif

	return;


}


//设置充电电流，与tp 驱动无关,2013-06-26
#if 1//defined(USB_CHARGING_ONLY)
#ifdef CONFIG_AW_AXP
#include <linux/power_supply.h>
#include <linux/apm_bios.h>
#include <linux/apm-emulation.h>
#include <linux/regulator/machine.h>
#include <mach/irqs.h>
#include "../../../drivers/amlogic/power/axp_power/axp-gpio.h"
#include "../../../drivers/amlogic/power/axp_power/axp-mfd.h"
#include "../../../drivers/amlogic/power/axp_power/axp-sply.h"
#endif
extern struct axp_charger *gcharger;
#define BIT(X)  (1 << (X))
#endif

static void zforce_ts_resume(struct early_suspend *handler)
{
    struct zforce_ts_priv *priv;
	priv = container_of(handler, struct zforce_ts_priv, early_suspend);
	pr_info("==zforce_ts_resume== \n");

	is_ts_suspended = 0;
	
#if 1
	zforce_pwr_enable(1);
#endif
		
	if(priv->use_irq)
	{
		ext_work_loop = 0;
		ext_handle_data_done = 0;
		if(!priv->is_irq_enabled)
		{
			priv->is_irq_enabled = 1;
			enable_irq(TS_INT);
		}
			
	}
	else
		hrtimer_start(&priv->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
		//hrtimer_start(&priv->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	//设置充电电流，与tp 驱动无关,2013-06-26
	#ifdef CONFIG_AW_AXP20
	#if 1//defined(USB_CHARGING_ONLY)
	if(gcharger)
	{
		axp_write(gcharger->master,0x30,0xe3);
		axp_write(gcharger->master,0x33,0xc2);
		printk("***** Set charging cur to 500ma,in func zforce_ts_resume()\n");
	}
	#endif
	#endif
	
	return;
	
}
#endif  //CONFIG_HAS_EARLYSUSPEND

static void zforce_ts_handle_data(struct work_struct *work) {

	// The container struct of the work_struct passed to the schedule call happens to be
	// the private data associated with this driver, use the container_of
	// macro to acquire it

	//printk("***** zforce_ts_handle_data\n");

#if 1
	//struct zforce_ts_priv *priv = container_of(to_delayed_work(work), struct zforce_ts_priv,work);// work.work);
	struct zforce_ts_priv *priv = container_of(work, struct zforce_ts_priv,work);// work.work);

	//if(is_ts_suspended)
	//{
	//	printk("***** is_suspended is true\n");
	//	return;
	//}
	// Current command received
	int zforce_command;
	// Length of payload part of received block
	int payload_length;
	int j;
	int reg_val;
	// Data buffer (large enough to hold biggest expected block)
	u_int8_t tmp_start[2];
	u_int8_t tmp_buf[256];//tmp_buf[20];
	int more_verbose;
	int i;

	memset(tmp_buf, 0, sizeof(tmp_buf));

	// Be more verbose until one touch coordinate was handled
	more_verbose = priv->startup_state & ZF_FIRSTTOUCH_RESPONDED ? 0 : 1;

	// Read the first three bytes to get the command id and the size of the rest of the message
	if (i2c_master_recv(priv->client, tmp_start, 2) != 2) {
		dev_err(&priv->client->dev, "Unable to read ZForce data header\n");
		goto out;
	}

	//printk("*****tmp_start[0] = %x,tmp_start[1] = %x,\n",tmp_start[0],tmp_start[1]);
	// Check the start byte
	if (tmp_start[0] != 0xee) {
		//dev_err(&priv->client->dev,
		//		"Invalid initial byte of ZForce data block\n");
		goto out;
	}

	// Get the length of the payload
	payload_length = tmp_start[1] ;

	// Get the command
	//  zforce_command = tmp_buf[2];	
	// The block is too long to handle
	if (payload_length > sizeof(tmp_buf)) {

		dev_err(&priv->client->dev, "Block from Zforce was too long,payload_length = %d\n",payload_length);

		// Read byte by byte to flush the buffer
		//TODO: Read bigger blocks, set a max limit, maybe try to resync after this,
		//  at multiple failure, the ISR shoud be switched off

		for (i = 0; i < payload_length; i++) {

			i2c_master_recv(priv->client, tmp_buf, 1);
		}
		//modify
		//schedule_delayed_work(&priv->delayed_work, HZ / 20);
		//schedule_work(&priv->work);
		queue_work(priv->ts_workqueue,&priv->work);
		printk("***** zforce data is too long\n");
		goto out;
		//return;
	}

	// Reuse the data buffer and read the payload part of the block
	if (i2c_master_recv(priv->client, tmp_buf, payload_length)
			!= payload_length) {
		dev_err(&priv->client->dev, "Unable to get ZForce data header\n");
		goto out;
	}

	zforce_command = tmp_buf[0];
	/***************/
	//printk("Data from ZForce, command=%d, length=%d\n", zforce_command, payload_length);
	//printk("recieved:  0x%x ,  0x%x",tmp_start[0], tmp_start[1]);
	//for(j = 0; j < payload_length; j++){

	//printk("  0x%x, \t",tmp_buf[j]);
	//}
	//printk("\n");
	/***************************/
	switch (zforce_command) { // Attend to the command

		case 0x07:
			printk("ZForce complete cmd received, date is:",payload_length);
			for(i =0;i<payload_length;i++)
			{
				printk("0x%02x,",tmp_buf[i]);
			}
			printk("\n");
			
			priv->startup_state = 0;
			break;

		case ZF_DEACTIVATE: // Got response from deactivate request
			priv->startup_state |= ZF_DEACTIVATE_RESPONDED;
			zforce_ts_handle_deactivate(tmp_buf, payload_length);
			break;

		case ZF_ACTIVATE: // Got response from activate request
			priv->startup_state |= ZF_ACTIVATE_RESPONDED;
			zforce_ts_handle_activate(tmp_buf, payload_length);
			break;

		case ZF_CONFIGURE: // Got response from configuration request
			priv->startup_state |= ZF_CONFIGURE_RESPONDED;
			zforce_ts_handle_configure(tmp_buf, payload_length);
			break;

		case ZF_SET_RESOLUTION: // Got response from resolution setting request
			priv->startup_state |= ZF_RESOLUTION_RESPONDED;
			zforce_ts_handle_setres(tmp_buf, payload_length);
			break;

		case ZF_REQ_COORDINATES: // Got touch event
			//		printk("----------------------enter coordinates-------------------");
			zforce_ts_handle_touchdata(priv, tmp_buf +1, payload_length - 1);
			priv->startup_state |= ZF_FIRSTTOUCH_RESPONDED;
			if(ext_work_loop)
			{
				//printk("==================== ext_handle_data_done\n");
				ext_handle_data_done = 1;
			}
				
			break;

		case ZF_REQ_VERSION: // Got version
			priv->startup_state |= ZF_VERSION_RESPONDED;
			zforce_ts_handle_version(tmp_buf, payload_length);
			break;
		
		case ZF_AREA_SIZE: //set touch area size
			priv->startup_state |=  ZF_TOUCHAREA_RESPONDED;
			zforce_ts_handle_toucharea(tmp_buf, payload_length);
			break;
		
		case ZF_CALIBRATION:
			priv->startup_state |= ZF_CALIBRATION_RESPONDED;
			zforce_ts_handle_calibration(tmp_buf, payload_length);
			break;
	}

	if (!(priv->startup_state & ZF_DEACTIVATE_REQUESTED)) {
		// Still not deactivated, send a deactivation request, this is
		// necessary as the ZForce will potentially stop sending coordinates
		// if an activation is issued without prior deactivation
		priv->startup_state |= ZF_DEACTIVATE_REQUESTED;
		zforce_ts_send_deactivate(priv);
	} else if (!(priv->startup_state & ZF_ACTIVATE_REQUESTED)) {
		// Still not activated, send an activation request
		priv->startup_state |= ZF_ACTIVATE_REQUESTED;
		zforce_ts_send_activate(priv);
	} else if (!(priv->startup_state & ZF_RESOLUTION_REQUESTED)) {
		// Still no resolution set, send a resolution setting request
		priv->startup_state |= ZF_RESOLUTION_REQUESTED;
		zforce_ts_send_setres(priv, ZF_MAX_X, ZF_MAX_Y);
	} else if (!(priv->startup_state & ZF_CONFIGURE_REQUESTED)) {
		// Still not configured, send a configuration request
		priv->startup_state |= ZF_CONFIGURE_REQUESTED;
		zforce_ts_send_configure(priv, 1);
	} else if (!(priv->startup_state & ZF_TOUCHAREA_REQUESTED)){
		priv->startup_state |= ZF_TOUCHAREA_REQUESTED;
		zforce_ts_send_areasize(priv);
	} else if (!(priv->startup_state & ZF_CALIBRATION_REQUESTED)){
		priv->startup_state |= ZF_CALIBRATION_REQUESTED;
		zforce_ts_send_calibration(priv);
	} else if (!(priv->startup_state & ZF_FIRSTTOUCH_REQUESTED)){
		// All setup done, request some coordinates
		priv->startup_state |= ZF_FIRSTTOUCH_REQUESTED;
		zforce_ts_send_touchdata_req(priv);
	}

out:
#if 0//TODO
	//printk("-----------------------------%d -----------------\n",__LINE__);
	//	schedule_delayed_work(&priv->work, HZ / 20);
	reg_val = readl(gpio_addr + PIO_INT_STAT_OFFSET);
	writel(reg_val&(1<<(IRQ_EINT21)),gpio_addr + PIO_INT_STAT_OFFSET);
	reg_val = readl(gpio_addr + PIO_INT_CTRL_OFFSET);
	reg_val |=(1<<IRQ_EINT21);
	writel(reg_val,gpio_addr + PIO_INT_CTRL_OFFSET);
	// Re-enable IRQ so that we can handle the next ZForce event
	//	enable_irq(priv->irq);
	while ((reg_val = readl(gpio_addr + PIO_INT_STAT_OFFSET))&(1<<IRQ_EINT21)) {
		//      printk("============== Clear EINT21 ================\n");
		writel(reg_val&(1<<(IRQ_EINT21)),gpio_addr + PIO_INT_STAT_OFFSET);
	}
	enable_irq(SW_INT_IRQNO_PIO);
#endif

	if(priv->use_irq)
	{
#if 1
	if(!gpio_get_val(INT_PORT) && is_ts_suspended == 0)
	{
		ext_work_loop = 0;
		ext_handle_data_done = 0;
		//schedule_work(&priv->work);
		queue_work(priv->ts_workqueue,&priv->work);
		//printk("********** a new  work \n");
		return;
	}

	#if 1
	else if(ext_handle_data_done == 0 && is_ts_suspended == 0)
	{
		//schedule_work(&priv->work);
		ext_work_loop = 1;
		hrtimer_start(&priv->timer, ktime_set(0, (20)*1000000), HRTIMER_MODE_REL);
		//printk("********** one more  work \n");
		if(priv->is_irq_enabled == 0)
		{
			priv->is_irq_enabled = 1;
			enable_irq(TS_INT);
		}
		
		return;
	}
	#endif
#endif

	#if 1
	//if(priv->use_irq)
	//{
		ext_work_loop = 0;
		ext_handle_data_done = 0;

		if(priv->is_irq_enabled == 0)
		{
			priv->is_irq_enabled = 1;
			enable_irq(TS_INT);
		}
	}
	#endif
#endif
	//enable_irq(TS_INT);
	return;
}

static void zforce_ts_handle_deactivate(u_int8_t *data, int len) {

	printk("ZForce deactivation response, status=%d\n", data[0]);
}

static void zforce_ts_handle_activate(u_int8_t *data, int len) {

	printk("ZForce activation response, status=%d\n", data[0]);
}

static void zforce_ts_handle_configure(u_int8_t *data, int len) {

	printk("ZForce configuration response, status=%d\n", data[0]);
}

static void zforce_ts_handle_setres(u_int8_t *data, int len) {

	printk("ZForce resolution setting response, status=%d\n", data[0]);
}

#if 1
int zforce_two_point_complete = 1;
#endif
// Handle touch coordinate events form the ZF
static void zforce_ts_handle_touchdata(struct zforce_ts_priv *priv, u_int8_t *payload, int len) 
{
	// Be more verbose for the very first touch coordinate we get
	int more_verbose = priv->startup_state & ZF_FIRSTTOUCH_RESPONDED ? 0 : 1;
	// The number of touch coordinates reported by the ZForce
	int coordinate_count = payload[0];
	// Used to automatically determine protocol version
	int protocol_variant = 0;
	// Index within the coordinate sub-data blocks
	int coord_index;
	int point = 0;
	int x ,y;
	int x1;
	int y1;
	int x2;
	int y2;
	int status;
	int i;
	int tmp;

	zforce_priv = priv;
	// This is a QaD trick to determine which of the two protocol flavors we got (one uses 5 bytes
	//  per coordinate, the other uses 7 bytes per coordinate). The "protocol_variant" is set to the
	//  coordinate block length
	if (len <= 2){
		#if 0
		ctp_wakeup();
		#endif
		printk("[zforce ts]data len <=2,send active cmd now\n");
		zforce_ts_send_activate(priv);	
	
	} else if (len == coordinate_count * 5 + 1) {
		if (more_verbose) {
			printk("Using 5 bytes per coordinate protocol\n");
		}
		// Older ZForce with 5 bytes per coordinate
		protocol_variant = 5;
	} else if (len == coordinate_count * 9 + 1) {
		if (more_verbose) {
			//printk("Using 5 bytes per coordinate protocol\n");
		}
		// Newer ZForce with 7 bytes per coordinate
		protocol_variant = 9;
	}
	//printk("\n+++++++++++++++++++++++++++++++++++++++Touch_X_Y_TS+++++++++++++++++++++++++++\n");
	// Did we figure out the protocol ?
	if (!protocol_variant) {
		// Not the expected packet length
		dev_err(&priv->client->dev,
				"Could not match ZForce block length to any protocol\n");
		return;
	}

	for (i = 1; i < len; i++) 
	{ // Scan the buffer (skip the coordinate count)
		int data = payload[i];
		//dual touch event
		// The index within the multi-touch coordinate we are currently parsing
		
		if (coordinate_count == 1) //single touch event
		{
				coord_index = (i - 1) % (protocol_variant );
				if (more_verbose && !coord_index) 
				{
					//printk("Parsing ZForce coordinate %d\n", i / protocol_variant);
				}
				switch (coord_index) 
				{
					case 0: // X LSB
						x1 = data;
						break;

					case 1: // X MSB
						x1 |= (data << 8);
						break;

					case 2: // Y LSB
						y1 = data;
						break;

					case 3: // Y MSB
						y1 |= (data << 8);
						break;

					case 4: // Status
						status = data;
						if (more_verbose) {
							//	printk("Status=0x0%x x=%d y=%d\n", status, x, y);
						}
						//printk("Status1=0x0%x x1=%d y1=%d\n", status, x1, y1);
						
						//tmp = x1;
						//x1 = y1;
						//y1=tmp;
						
						x1 = (ZF_MAX_X - x1) ;
						y1 = (ZF_MAX_Y - y1) ;
						if(user_dbg_flag)
							printk("+++Status1=0x0%x __x1=%d  __y1=%d\n", status, x1, y1);
						// The status bits differ between the two "protocol variants"
						switch (status) {
							case 0x04: // Touch down
							#if GTP_ICS_SLOT_REPORT
								if(!zforce_two_point_complete)
								{
									printk("***** zforce_two_point_complete = 0\n");
									input_mt_slot(priv->input, 0);
									//input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
									input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, true);
								
									input_mt_slot(priv->input, 1);
									//input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
									input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, true);
									input_sync(priv->input);
									
									zforce_two_point_complete = 1;
								}
								input_mt_slot(priv->input, 0);
								//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, true);
    							input_report_abs(priv->input, ABS_MT_TRACKING_ID, 0);
    							input_report_abs(priv->input, ABS_MT_POSITION_X, x1);
    							input_report_abs(priv->input, ABS_MT_POSITION_Y, y1);
    							//input_report_abs(priv->input, ABS_MT_PRESSURE, 4);
								input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
							#else
								input_report_key(priv->input, BTN_TOUCH, 1);
								input_report_abs(priv->input, ABS_MT_POSITION_X, x1);
								input_report_abs(priv->input, ABS_MT_POSITION_Y, y1);
								input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
								input_mt_sync(priv->input);
							#endif
							case 0x05: // Move
							#if GTP_ICS_SLOT_REPORT
								input_mt_slot(priv->input, 0);
								//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, true);
    							input_report_abs(priv->input, ABS_MT_TRACKING_ID, 0);
								input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
    							input_report_abs(priv->input, ABS_MT_POSITION_X, x1);
    							input_report_abs(priv->input, ABS_MT_POSITION_Y, y1);
    							//input_report_abs(priv->input, ABS_MT_PRESSURE, 4);
							
							#else
								input_report_key(priv->input, BTN_TOUCH, 1);
								input_report_abs(priv->input, ABS_MT_POSITION_X, x1);
								input_report_abs(priv->input, ABS_MT_POSITION_Y, y1);
								input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
								input_mt_sync(priv->input);
							#endif
								break;
							
							case 0x06: // Up
							#if GTP_ICS_SLOT_REPORT
								input_mt_slot(priv->input, 0);
    							input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
								//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, false);
								
								
								#if 1
								input_mt_slot(priv->input, 1);
								input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
								//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, false);
								#endif
							#else
								input_report_key(priv->input, BTN_TOUCH, 0);
								input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 0);
								input_mt_sync(priv->input);
							#endif
								break;
						}
						break;

					case 5: // Reserved byte
					case 6: // Probability, disregarded for the moment
					case 7:
					case 8:
						break;

				}

		} else {
			coord_index = (i - 1) % (protocol_variant );
			if (more_verbose && !coord_index) 
			{
				//printk("Parsing ZForce coordinate %d\n", i / protocol_variant);
			}
			switch (coord_index) 
			{
				case 0: // X LSB
					point ++;
					x = data;
					break;

				case 1: // X MSB
					x |= (data << 8);
					break;

				case 2: // Y LSB
					y = data;
					break;

				case 3: // Y MSB
					y |= (data << 8);
					break;

				case 4: // Status
					status = data;
					break;

				case 5: // Reserved byte
				case 6: // Probability, disregarded for the moment
				case 7:
				case 8:
					break;
			}

			if (more_verbose) 
			{
				//	printk("Status=0x0%x x=%d y=%d\n", status, x, y);
			}
			//printk("Status1=0x0%x x1=%d y1=%d\n", status, x1, y1);
			if (point == 1)
			{
				x1 = x;
				y1 = y;
				
				x1 = (ZF_MAX_X - x1) ;
				y1 = (ZF_MAX_Y - y1) ;
			}else if (point == 2)
			{
				x2 = x;
				y2 = y;
				
				x2 = (ZF_MAX_X - x2) ;
				y2 = (ZF_MAX_Y - y2) ;
			}
			if(user_dbg_flag)
			{
				printk("=====Status1=0x0%x __x1=%d  __y1=%d\n", status, x1, y1);
				printk("=====Status1=0x0%x __x2=%d  __y2=%d\n", status, x2, y2);
			}
			// The status bits differ between the two "protocol variants"

			switch (status) 
			{
				case 0x08: // Touch down
				case 0x09:
				#if GTP_ICS_SLOT_REPORT
				input_mt_slot(priv->input, 0);
				//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, true);
    			input_report_abs(priv->input, ABS_MT_TRACKING_ID, 0);
				input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
    			input_report_abs(priv->input, ABS_MT_POSITION_X, x1);
    			input_report_abs(priv->input, ABS_MT_POSITION_Y, y1);
    			//input_report_abs(priv->input, ABS_MT_PRESSURE, 4);

				input_mt_slot(priv->input, 1);
				//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, true);
    			input_report_abs(priv->input, ABS_MT_TRACKING_ID, 1);
				input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
    			input_report_abs(priv->input, ABS_MT_POSITION_X, x2);
    			input_report_abs(priv->input, ABS_MT_POSITION_Y, y2);
    			//input_report_abs(priv->input, ABS_MT_PRESSURE, 4);
				
				zforce_two_point_complete = 0;
				#else

					input_report_key(priv->input, BTN_TOUCH, 1);
					//input_report_key(priv->input, ABS_MT_TRACKING_ID, 0); 
					input_report_abs(priv->input, ABS_MT_POSITION_X, x1);
					input_report_abs(priv->input, ABS_MT_POSITION_Y, y1);
					input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
					input_mt_sync(priv->input);

					input_report_key(priv->input, BTN_TOUCH, 1);
					//input_report_key(priv->input, ABS_MT_TRACKING_ID, 1); 
					input_report_abs(priv->input, ABS_MT_POSITION_X, x2);
					input_report_abs(priv->input, ABS_MT_POSITION_Y, y2);
					input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 1);
					input_mt_sync(priv->input);
				#endif
					break;

				case 0x0a: // Up
				#if GTP_ICS_SLOT_REPORT
    				input_mt_slot(priv->input, 0);
    				input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
					//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, false);
					input_mt_slot(priv->input, 1);
    				input_report_abs(priv->input, ABS_MT_TRACKING_ID, -1);
					//input_mt_report_slot_state(priv->input, MT_TOOL_FINGER, false);
					zforce_two_point_complete = 1;
				#else
					input_report_key(priv->input, BTN_TOUCH, 0);
					input_report_abs(priv->input, ABS_MT_TOUCH_MAJOR, 0);
					input_mt_sync(priv->input);
				#endif
					break;
				}
			}
			
		}

		#if 0
		if (!(i % protocol_variant)) {
			// This is the last byte of the coordinate, so we can fire off the MT sync here
			input_mt_sync(priv->input);
			if (more_verbose) {
				printk("Syncing touch coordinate\n");
			}
		}
		#endif
		// Final sync
		input_sync(priv->input);
		if (more_verbose) {
			printk("Syncing multi touch event\n");
		}
}

static void zforce_ts_handle_version(u_int8_t *data, int len) {

	u_int16_t major = data[0] + (data[1] << 8);
	u_int16_t minor = data[2] + (data[3] << 8);
	u_int16_t build = data[4] + (data[5] << 8);
	u_int16_t revision = data[6] + (data[7] << 8);

	printk("ZForce version %d.%d, build %d, rev %d\n", major, minor, build,
			revision);
}

static int zforce_ts_send_deactivate(struct zforce_ts_priv *priv) {

	u_int8_t data[] = { 0xee, 0x01,  ZF_DEACTIVATE };

	printk("send:  Deactivating zForce command:  0xee, 0x01, 0x00 \n");

	return i2c_master_send(priv->client, data, sizeof(data));
}

static int zforce_ts_send_activate(struct zforce_ts_priv *priv) {

	u_int8_t data[] = { 0xee, 0x01, ZF_ACTIVATE };

	printk("send: Activating zForce: 0xee, 0x01, 0x01 \n");

	return i2c_master_send(priv->client, data, sizeof(data));
}

static int zforce_ts_send_configure(struct zforce_ts_priv *priv, int dual_touch) {

	u_int8_t data[] = {0xee, 0x05, ZF_CONFIGURE, 0, 0, 0, 0 };
	printk("send: 0xee, 0x05, 0x03, 0,0,0 0");
	printk("Configuring zForce, using %s touch\n",
			dual_touch ? "dual" : "single");

	data[3] = dual_touch ? 0x01 : 0x00;

	return i2c_master_send(priv->client, data, sizeof(data));
}


//add by Ethan
static int zforce_ts_send_areasize(struct zforce_ts_priv *priv)
{
	u_int8_t data[] = {0xee, 0x05, ZF_AREA_SIZE,  0x01, 0xff, 0x01, 0xff};

	printk("send: touch Area zForce:  0xee, 0x05, 0x09, 0x01, 0xff, 0x01, 0xff \n");

	return i2c_master_send(priv->client, data, sizeof(data));

}

static int zforce_ts_send_calibration(struct zforce_ts_priv *priv)
{

	u_int8_t data[] = {0xee, 0x01,ZF_CALIBRATION };

	printk("send: calibraion of zForce: 0xee, 0x01, 0x1a  \n");

	return i2c_master_send(priv->client, data, sizeof(data));
}


static void zforce_ts_handle_toucharea(u_int8_t *data, int len)
{
	printk("==zforce_ts_handle_toucharea==\n ");
}


static void zforce_ts_handle_calibration(u_int8_t *data, int len)
{
	printk("==zforce_ts_handle_calibration ==\n");
}
//end add

static int zforce_ts_send_setres(struct zforce_ts_priv *priv, int width,
		int height) {

	u_int8_t data[] = {0xee, 0x05,  ZF_SET_RESOLUTION, 0, 0, 0, 0 };

	printk("Setting ZForce resolution, width=%d, height=%d\n", width, height);

	data[3] = width & 0xff;
	data[4] = width >> 8;
	data[5] = height & 0xff;
	data[6] = height >> 8;
	
	printk("send: resolution command: 0xee, 0x05, 0x02,%x, %x, %x, %x\n ",width & 0xff, width >>8 , height & 0xff, height >> 8);

	return i2c_master_send(priv->client, data, sizeof(data));
}

static int zforce_ts_send_touchdata_req(struct zforce_ts_priv *priv) {
	u_int8_t data[] = {0xee, 0x01,  ZF_REQ_COORDINATES };
	u_int8_t tmp_buf[10] = {0};
	int i = 0,j = 0;
	if (!(priv->startup_state & ZF_FIRSTTOUCH_RESPONDED)) {

		printk("Requesting touch coordinates\n");
	}
	
	printk("send: TouchDataRequest: 0xee, 0x01, 0x04\n");
		
	 i2c_master_send(priv->client, data, sizeof(data));
	for(;i < 10; i++){
		 i2c_master_recv(priv->client, tmp_buf, 10);
		for(;j < 10; j++){
			printk("%2x \t", tmp_buf[j]);
		}
		printk("\n");
	}
}

static int zforce_ts_send_version_req(struct zforce_ts_priv *priv) {

	u_int8_t data[] = { 0xee, 0x01, ZF_REQ_VERSION };

	printk("send: Getting ZForce version: 0xee , 0x01, 0x0a\n");

	return i2c_master_send(priv->client, data, sizeof(data));
}

static irqreturn_t zforce_ts_isr(int irq, void *dev_id) {
	struct zforce_ts_priv *priv = dev_id;

	if(is_ts_suspended)
	{
		return IRQ_HANDLED;
	}
	int reg_val, tmp;
	//if(priv->is_irq_enabled)
	//{
		disable_irq_nosync(TS_INT);
		priv->is_irq_enabled = 0;
	//}
	
	//printk("==interrupt generate==\n");
	queue_work(priv->ts_workqueue,&priv->work);
	return IRQ_HANDLED;
}

static int zforce_ts_open(struct input_dev *dev) {
	return 0;
}

static void zforce_ts_close(struct input_dev *dev) {

}


static enum hrtimer_restart zforce_ts_timer_func(struct hrtimer *timer)
{
	//printk("***** zforce_ts_timer_func\n");
    struct zforce_ts_priv *priv = container_of(timer, struct zforce_ts_priv,timer);// work.work);

    schedule_work(&priv->work);
    hrtimer_start(&priv->timer, ktime_set(0, (POLL_TIME)*1000000), HRTIMER_MODE_REL);

    return HRTIMER_NORESTART;
}

static enum hrtimer_restart zforce_ts_timer_func2(struct hrtimer *timer)
{
	//printk("***** zforce_ts_timer_func\n");
    struct zforce_ts_priv *priv = container_of(timer, struct zforce_ts_priv,timer);// work.work);
    schedule_work(&priv->work);

    return HRTIMER_NORESTART;
}

static ssize_t zdebug_store(struct class *cla, struct class_attribute* attr, const char* buf,size_t count)
{
	
    int ret = 0;
	u_int8_t tmp_buf[20];
	int i = 0;
	
	struct zforce_ts_priv * priv = g_priv;

	printk("input buf = %s,count = %d\n",buf,count);
	
	if(!g_priv)
	{
		printk("***** g_priv init err\n");
		return count;
	}
		
    if(!strncmp(buf,"d",count-1))
    {
		ret = zforce_ts_send_deactivate(priv);
		printk("**** deactive cmd,ret = %d\n",ret);
    }
	else if(!strncmp(buf,"a",count-1))
	{
		ret = zforce_ts_send_activate(priv);
		printk("**** active cmd,ret = %d\n",ret);
	}
	else if(!strncmp(buf,"r",count-1))
	{
		ret = i2c_master_recv(priv->client, tmp_buf, 20);
		printk("**** active cmd,ret = %d\n",ret);

		if(ret > 0)
		{
			for(i = 0;i < 20;i++)
			{
				printk(" 0x%2x ",tmp_buf[i]);
			}
			printk("\n");
		}
	}
	else if(!strncmp(buf,"wq",count-1))
	{
		ret = schedule_work(&priv->work);
		printk("**** schedule_work,ret = %d\n",ret);
	}
	else if(!strncmp(buf,"pwoff",count-1))
	{
		zforce_pwr_enable(0);
		printk("**** pwoff,ret = %d\n",ret);
	}
	else if(!strncmp(buf,"pwon",count-1))
	{
		zforce_pwr_enable(1);
		printk("**** pwoff,ret = %d\n",ret);
	}
	else if(!strncmp(buf,"pwreset",count-1))
	{
		zforce_pwr_enable(0);
		zforce_pwr_enable(1);
		printk("**** pw reset,ret = %d\n",ret);
	}
	else if(!strncmp(buf,"restart",count-1))
	{
		ret = zforce_ts_send_deactivate(priv);
		ret = zforce_ts_send_activate(priv);
		ret = zforce_ts_send_setres(priv, ZF_MAX_X, ZF_MAX_Y);
		ret = zforce_ts_send_configure(priv, 1);
		ret = zforce_ts_send_areasize(priv);
		ret = zforce_ts_send_calibration(priv);
		ret = zforce_ts_send_touchdata_req(priv);
		printk("**** restart cmd,ret = %d\n",ret);
	}
	else if(!strncmp(buf,"v",count-1))
	{


		u_int16_t major;
		u_int16_t minor;
		u_int16_t build;
		u_int16_t revision;
			
		u_int8_t data[] = { 0xee, 0x01, 0x1e };

		printk("send: Getting ZForce version: 0xee , 0x01, 0x1e\n");

		i2c_master_send(priv->client, data, sizeof(data));
		msleep(50);
		
		ret = i2c_master_recv(priv->client, tmp_buf, 20);

		major = tmp_buf[3] + (tmp_buf[4] << 8);
		minor = tmp_buf[5] + (tmp_buf[6] << 8);
		build = tmp_buf[7] + (tmp_buf[8] << 8);
		revision = tmp_buf[9] + (tmp_buf[10]<< 8);

		printk("data len = %d,ZForce version %d.%d, build %d, rev %d\n",tmp_buf[2], major, minor, build,
			revision);
	}
	else if(!strncmp(buf,"rstlow",count-1))
	{
		GPIO_DIRECTION_OUTPUT(RESET_PORT,0);
		printk("rstlow cmd done\n");
	}
	else if(!strncmp(buf,"rsthigh",count-1))
	{
		GPIO_DIRECTION_OUTPUT(RESET_PORT,1);
		printk("rsthigh cmd done\n");
	}
	else if(!strncmp(buf,"pwlow",count-1))
	{
		GPIO_DIRECTION_OUTPUT(TOUCH_PWR_EN,0);
		printk("pwlow cmd done\n");
	}
	else if(!strncmp(buf,"rstlow",count-1))
	{
		GPIO_DIRECTION_OUTPUT(TOUCH_PWR_EN,1);
		printk("pwhigh cmd done\n");
	}
	else if(!strncmp(buf,"intlow",count-1))
	{
		GPIO_DIRECTION_OUTPUT(INT_PORT,0);
		printk("intlow cmd done\n");
	}
	else if(!strncmp(buf,"inthigh",count-1))
	{
		GPIO_DIRECTION_OUTPUT(INT_PORT,1);
		printk("inthigh cmd done\n");
	}
	else if(!strncmp(buf,"intinput",count-1))
	{
		GPIO_DIRECTION_INPUT(INT_PORT);
    	GPIO_CFG_PIN(INT_PORT_IRQ_IDX, TS_INT, INT_CFG);
		printk("intinput cmd done\n");
	}else if(!strncmp(buf,"1",count-1))
	{
		//Add the debug interface for debug tp echo 1 > /sys/class/zforce-ts/zforce 2013-05-09
		debug_flag=1;
	}
	else if(!strncmp(buf,"releasedoubl",count-1))
	{
		if(!zforce_priv)
		{
			printk("err,zforce_priv is NULL\n");
		}
		else
		{
			if(zforce_priv->input)
			{
				#if defined(GTP_ICS_SLOT_REPORT)
				input_mt_slot(zforce_priv->input, 0);
				input_report_abs(zforce_priv->input, ABS_MT_TRACKING_ID, -1);
								
			    input_mt_slot(zforce_priv->input, 1);
				input_report_abs(zforce_priv->input, ABS_MT_TRACKING_ID, -1);
				input_sync(zforce_priv->input);
				printk("zforce_priv 2 poonts be released\n");
				#endif
			}
			else
				printk("err,zforce_priv->input is NULL\n");
		}
	}
	else if(!strncmp(buf,"release1st",count-1))
	{
		if(!zforce_priv)
		{
			printk("err,zforce_priv is NULL\n");
		}
		else
		{
			if(zforce_priv->input)
			{
				#if defined(GTP_ICS_SLOT_REPORT)
				input_mt_slot(zforce_priv->input, 0);
				input_report_abs(zforce_priv->input, ABS_MT_TRACKING_ID, -1);
								
			    input_mt_slot(zforce_priv->input, 1);
				input_report_abs(zforce_priv->input, ABS_MT_TRACKING_ID, -1);
				input_sync(zforce_priv->input);
				printk("zforce_priv 1st poont be released\n");
				#endif
			}
			else
				printk("err,zforce_priv->input is NULL\n");
		}
	}
	else if(!strncmp(buf,"release2nd",count-1))
	{
		if(!zforce_priv)
		{
			printk("err,zforce_priv is NULL\n");
		}
		else
		{
			if(zforce_priv->input)
			{
				#if defined(GTP_ICS_SLOT_REPORT)
				input_mt_slot(zforce_priv->input, 0);
				input_report_abs(zforce_priv->input, ABS_MT_TRACKING_ID, -1);
								
			    input_mt_slot(zforce_priv->input, 1);
				input_report_abs(zforce_priv->input, ABS_MT_TRACKING_ID, -1);
				input_sync(zforce_priv->input);
				printk("zforce_priv 2nd poont be released\n");
				#endif
			}
			else
				printk("err,zforce_priv->input is NULL\n");
		}
	}
	else if(!strncmp(buf,"user_dbg1",count-1))
	{
		user_dbg_flag = 1;
		printk("***** user_dbg =1\n");
	}
	else if(!strncmp(buf,"user_dbg0",count-1))
	{
		user_dbg_flag = 0;
		printk("***** user_dbg =0\n");
	}
	else
		printk("***** err command\n");
	
    return count;
}

static struct class_attribute zforce_tp_class_attrs[] = {
	__ATTR(zdebug,0666,NULL,zdebug_store),
    __ATTR_NULL
};

static struct class zforce_tp_class = {
        .name = CTP_NAME,
        .class_attrs = zforce_tp_class_attrs,
#ifdef CONFIG_PM
#endif
    };
//modify by emdoor jim.kuang 2013-05-09 for System First Start ,Tp can't work
#if defined(ZFORCE_USE_DELAYWORK)
void zforce_work_handle(void *data)
{
	is_ts_suspended=0;		
	hrtimer_start(&zforce_priv->timer, ktime_set(15, 0), HRTIMER_MODE_REL);
	if(!zforce_priv->is_irq_enabled)
	{
		zforce_priv->is_irq_enabled = 1;
		enable_irq(TS_INT);
	}	
}
#endif

static int zforce_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct zforce_ts_priv *priv = NULL;
	struct input_dev *input = NULL;
	int error;
	u_int8_t tmp_start[2],tmp_buf[256];
	int tmp_length,i;

	printk("********** zforce_ts_probe =***=2013-06-22=***=\n",INT_CFG);

#if 1
	zforce_pwr_enable(1);
#endif

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate driver data\n");
		error = -ENOMEM;
		goto err0;
	}

	dev_set_drvdata(&client->dev, priv);

	input = input_allocate_device();
	if (!input) {
		dev_err(&client->dev, "Failed to allocate input device.\n");
		error = -ENOMEM;
		goto err1;
	}

	input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

#if GTP_ICS_SLOT_REPORT
    __set_bit(INPUT_PROP_DIRECT, input->propbit);
    input_mt_init_slots(input, 255);
	printk("********** SUPPORT SLOT REPORTING MODE\n");
#else
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif
    input->absbit[0] = BIT(ABS_X) | BIT(ABS_Y);// | BIT(ABS_PRESSURE);

	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR,	0, 15, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_X, 0, ZF_MAX_X, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y, 0, ZF_MAX_Y, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
	//input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);	

	input->name = client->name;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	input->open = zforce_ts_open;

	input_set_drvdata(input, priv);

	priv->client = client;
	priv->input = input;

	error = input_register_device(input);
	if (error)
		goto err1;

#if CONFIG_HAS_EARLYSUSPEND
	priv->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	priv->early_suspend.suspend = zforce_ts_suspend;
	priv->early_suspend.resume	= zforce_ts_resume;
	register_early_suspend(&priv->early_suspend);
#endif


	INIT_WORK(&priv->work, zforce_ts_handle_data);
	priv->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	priv->irq = client->irq;

	GPIO_DIRECTION_INPUT(INT_PORT);
    GPIO_CFG_PIN(INT_PORT_IRQ_IDX, TS_INT, INT_CFG);        //Set IO port function    
	priv->use_irq = 1;
	if(priv->use_irq)
	{
		error = request_irq(TS_INT, zforce_ts_isr, IRQF_DISABLED,
			client->name, priv);
		if (error) {
			dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
			goto err2;
		}
		printk("****************** zforce Use irq!\n");

		hrtimer_init(&priv->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		priv->timer.function = zforce_ts_timer_func2;
		printk("****************** zforce_ts_timer_func2\n");
	}
	else
	{
		hrtimer_init(&priv->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		priv->timer.function = zforce_ts_timer_func;
		hrtimer_start(&priv->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		printk("****************** zforce Use timer!\n");
	}

	g_priv = priv;
	class_register(&zforce_tp_class);

	//modify by emdoor jim.kuang 2013-05-09 for Syste First Start ,Tp can't work
	#if defined(ZFORCE_USE_DELAYWORK)
	zforce_queue = create_workqueue("zforce_queue_events");	
	INIT_DELAYED_WORK(&zforce_work, zforce_work_handle);
	queue_delayed_work(zforce_queue,&zforce_work,HZ*1);	
	zforce_priv=priv;
	#endif
	
	return 0;

err2: input_unregister_device(input);
	  input = NULL; /* so we dont try to free it below */
err1: input_free_device(input);
	  kfree(priv);
err0: dev_set_drvdata(&client->dev, NULL);
	  return error;

}

static int __devexit zforce_ts_remove(struct i2c_client *client)
{
	struct zforce_ts_priv *priv = i2c_get_clientdata(client);
	
	pr_info("==zforce_ts_remove=\n");
	class_unregister(&zforce_tp_class);
	free_irq(TS_INT, priv);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&priv->early_suspend);
#endif
	input_unregister_device(priv->input);
	input_free_device(priv->input);
	cancel_work_sync(&priv->work);
	destroy_workqueue(priv->ts_workqueue);
	kfree(priv);
    
	i2c_set_clientdata(client, NULL);
	#if 0
	ctp_ops.free_platform_resource();
	#endif

	//add by emdoor jim.kuang 2013-05-09
	#if defined(ZFORCE_USE_DELAYWORK)
	flush_workqueue(zforce_queue);
	cancel_delayed_work(&zforce_work);
	destroy_workqueue(zforce_queue);
	#endif


	return 0;

}

static const struct i2c_device_id zforce_ts_id[] = {
	{ CTP_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, zforce_ts_id);

static struct i2c_driver zforce_ts_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		= zforce_ts_probe,
	.remove		= __devexit_p(zforce_ts_remove),
	.id_table	= zforce_ts_id,
	.driver	= {
		.name	= CTP_NAME,
		.owner	= THIS_MODULE,
	},
	.address_list	= u_i2c_addr.normal_i2c,
};

static int __init zforce_ts_init(void)
{ 
	int ret = -1;
	int err = -1;

	pr_info("=====================%s=====================\n", __func__);
	ret = i2c_add_driver(&zforce_ts_driver);
	return ret;
}

static void __exit zforce_ts_exit(void)
{
	pr_info("==zforce_ts_exit==\n");
	i2c_del_driver(&zforce_ts_driver);
}

late_initcall(zforce_ts_init);
module_exit(zforce_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech zforce TouchScreen driver");
MODULE_LICENSE("GPL");

