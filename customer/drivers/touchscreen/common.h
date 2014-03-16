#ifndef _TOUCH_H_
#define _TOUCH_H_

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <mach/am_regs.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/input/mt.h>
#include <linux/vmalloc.h>
#include <linux/hrtimer.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_OF
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of.h>
#else
#include <mach/gpio.h>
#include <mach/gpio_data.h>
#endif

#define touch_dbg(fmt, args...)  { if(ts_com->printk_enable_flag) \
					printk("[%s]: " fmt, ts_com->owner, ## args); }
					
#define UPGRADE_TOUCH "upgrade_touch"

struct touch_pdata {
  int ic_type; /* see Focaltech IC type */
  int irq;
  int gpio_interrupt;
  int gpio_reset;
  int gpio_power;
  int xres;
  int yres;
  int pol; 
  int irq_edge;
	int max_num;
  unsigned bus_type;
  unsigned reg;
	char *owner;
	const char *fw_file;
	const char *config_file;
	
	int printk_enable_flag;
	void(*hardware_reset)(struct touch_pdata *);
	void(*software_reset)(struct touch_pdata *);
	int(*upgrade_touch)(void);
	
	struct cdev upgrade_cdev;
	dev_t upgrade_no;
	struct device *dev;
	struct task_struct *upgrade_task;

	struct tp_key *tp_key;
	int tp_key_num;
};

typedef enum
{
    ERR_NO,
    ERR_NO_NODE,
    ERR_GET_DATA,
    ERR_GPIO_REQ
}GET_DT_ERR_TYPE;

int touch_open_fw(char *fw);
int touch_read_fw(int offset, int length, char *buf);
int touch_close_fw(void);
int create_init(struct device dev, struct touch_pdata *pdata);
void destroy_remove(struct device dev, struct touch_pdata *pdata);

#endif
