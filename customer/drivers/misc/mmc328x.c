/*
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 * Initial Code:
 *	Robbie Cao
 * 	Dale Hou
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/workqueue.h>

#include <linux/mmc328x.h>

#define DEBUG			0
#define MAX_FAILURE_COUNT	3
#define READMD			0

#define MMC328X_DEF_DELAY		200
#define MMC328X_DELAY_TM	10	/* ms */
#define MMC328X_DELAY_RM	10	/* ms */
#define MMC328X_DELAY_STDN	1	/* ms */
#define MMC328X_DELAY_RRM       1       /* ms */

#define MMC328X_RETRY_COUNT	3
#define MMC328X_RRM_INTV	100

#define MMC328X_DEV_NAME	"mmc328x"

struct mmc328x_data {
	atomic_t delay;
	atomic_t enable;
	struct input_dev *input;
	struct delayed_work work;
};

static s32 dbglevel;
static u32 read_idx = 0;

static struct i2c_client *this_client;

static int mmc328x_i2c_rx_data(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	for (i = 0; i < MMC328X_RETRY_COUNT; i++) {
		if (i2c_transfer(this_client->adapter, msgs, 2) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= MMC328X_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, MMC328X_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int mmc328x_i2c_tx_data(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buf,
		}
	};
	
	for (i = 0; i < MMC328X_RETRY_COUNT; i++) {
		if (i2c_transfer(this_client->adapter, msg, 1) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= MMC328X_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, MMC328X_RETRY_COUNT);
		return -EIO;
	}
	return 0;
}

static void mmc328x_start(struct mmc328x_data *mmc328x)
{
    if(atomic_read(&mmc328x->enable) == 0)
    {
        schedule_delayed_work(&mmc328x->work,
                msecs_to_jiffies(atomic_read(&mmc328x->delay)));
        atomic_set(&mmc328x->enable, 1);
    }
}

static void mmc328x_stop(struct mmc328x_data *mmc328x)
{
    if(atomic_read(&mmc328x->enable) == 1)
    {
        cancel_delayed_work_sync(&mmc328x->work);
        atomic_set(&mmc328x->enable, 0);
    }
}

static ssize_t mmc328x_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct mmc328x_data *mmc328x = i2c_get_clientdata(this_client);

    error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	atomic_set(&mmc328x->delay, (unsigned int) data);
    return count;
}

static ssize_t mmc328x_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc328x_data *data = i2c_get_clientdata(this_client);

	return sprintf(buf, "%d\n", atomic_read(&data->delay));
}

static ssize_t mmc328x_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mmc328x_data *mmc328x = i2c_get_clientdata(this_client);
	unsigned long data;
	int error;

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if ((data == 0))
        mmc328x_stop(mmc328x); 
    else if (data == 1)
        mmc328x_start(mmc328x);

	return count;
}

static ssize_t mmc328x_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc328x_data *data = i2c_get_clientdata(this_client);

	return sprintf(buf, "%d\n", atomic_read(&data->enable));

}

static ssize_t mmc328x_debug_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;


	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	
	dbglevel = data;

	return count;
}


static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
		mmc328x_delay_show, mmc328x_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
		mmc328x_enable_show, mmc328x_enable_store);
static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR|S_IWGRP,
		0, mmc328x_debug_store);

static struct attribute *mmc328x_attributes[] = {
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_debug.attr,
	NULL
};

static struct attribute_group mmc328x_attribute_group = {
	.attrs = mmc328x_attributes
};

static int mmc328x_read_xyz(int *vec)
{
    unsigned char data[16] = {0};
	int MD_times = 0;

		/* do RM every MMC328X_RESET_INTV times read */
		if (!(read_idx %  MMC328X_RRM_INTV)) {
#ifdef CONFIG_SENSORS_MMC328xMA
	            data[0] = MMC328X_REG_CTRL;
	            data[1] = MMC328X_CTRL_RRM;
	            mmc328x_i2c_tx_data(data, 2);
	            msleep(MMC328X_DELAY_RRM);
#endif
			/* RM */
			data[0] = MMC328X_REG_CTRL;
			data[1] = MMC328X_CTRL_RM;
			/* not check return value here, assume it always OK */
			mmc328x_i2c_tx_data(data, 2);
			/* wait external capacitor charging done for next RM */
			msleep(MMC328X_DELAY_RM);
		}
		read_idx++;
		/* send TM cmd before read */
		data[0] = MMC328X_REG_CTRL;
		data[1] = MMC328X_CTRL_TM;
		/* not check return value here, assume it always OK */
		mmc328x_i2c_tx_data(data, 2);
		/* wait TM done for coming data read */
		msleep(MMC328X_DELAY_TM);
#if READMD
		/* Read MD */
		data[0] = MMC328X_REG_DS;
		if (mmc328x_i2c_rx_data(data, 1) < 0) {
			return -EFAULT;
		}
		while (!(data[0] & 0x01)) {
			msleep(1);
			/* Read MD again*/
			data[0] = MMC328X_REG_DS;
			if (mmc328x_i2c_rx_data(data, 1) < 0) {
				return -EFAULT;
			}
			if (data[0] & 0x01) break;
			MD_times++;
			if (MD_times > 2) {
		#if DEBUG
				printk("TM not work!!");
		#endif
				return -EFAULT;
			}
		}
#endif		
		/* read xyz raw data */
		data[0] = MMC328X_REG_DATA;
		if (mmc328x_i2c_rx_data(data, 6) < 0) {
			return -EFAULT;
		}
		vec[0] = data[1] << 8 | data[0];
		vec[1] = data[3] << 8 | data[2];
		vec[2] = data[5] << 8 | data[4];
		if(dbglevel > 0)
			printk("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
    return 0;
}


static void mmc328x_work_func(struct work_struct *work)
{
	struct mmc328x_data *mmc328x = container_of((struct delayed_work *)work, struct mmc328x_data, work);
	unsigned long delay = msecs_to_jiffies(atomic_read(&mmc328x->delay));
	int vec[3] = {0};

    if(mmc328x_read_xyz(vec) < 0)
    {
		pr_err("%s: failed to read xyz\n", __FUNCTION__);
        return; 
    }
    input_report_abs(mmc328x->input, ABS_X, vec[0]);
    input_report_abs(mmc328x->input, ABS_Y, vec[1]);
    input_report_abs(mmc328x->input, ABS_Z, vec[2]);
    input_sync(mmc328x->input);

	schedule_delayed_work(&mmc328x->work, delay);
}



static int mmc328x_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int mmc328x_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmc328x_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	unsigned char data[16] = {0};
	int vec[3] = {0};
	int MD_times = 0;

	switch (cmd) {
	case MMC328X_IOC_TM:
		data[0] = MMC328X_REG_CTRL;
		data[1] = MMC328X_CTRL_TM;
		if (mmc328x_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		/* wait TM done for coming data read */
		msleep(MMC328X_DELAY_TM);
		break;
#ifdef CONFIG_SENSORS_MMC328xMA
	case MMC328X_IOC_RM:
		data[0] = MMC328X_REG_CTRL;
		data[1] = MMC328X_CTRL_RM;
		if (mmc328x_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		/* wait external capacitor charging done for next SET/RESET */
		msleep(MMC328X_DELAY_RM);
		break;
	case MMC328X_IOC_RRM:
		data[0] = MMC328X_REG_CTRL;
		data[1] = MMC328X_CTRL_RRM;
		if (mmc328x_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		msleep(MMC328X_DELAY_RM);
		break;
#endif
	case MMC328X_IOC_READ:
		data[0] = MMC328X_REG_DATA;
		if (mmc328x_i2c_rx_data(data, 6) < 0) {
			return -EFAULT;
		}
		vec[0] = data[1] << 8 | data[0];
		vec[1] = data[3] << 8 | data[2];
		vec[2] = data[5] << 8 | data[4];
	#if DEBUG
		printk("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
	#endif
		if (copy_to_user(pa, vec, sizeof(vec))) {
			return -EFAULT;
		}
		break;
	case MMC328X_IOC_READXYZ:
		/* do RM every MMC328X_RESET_INTV times read */
		if (!(read_idx %  MMC328X_RRM_INTV)) {
#ifdef CONFIG_SENSORS_MMC328xMA
	            data[0] = MMC328X_REG_CTRL;
	            data[1] = MMC328X_CTRL_RRM;
	            mmc328x_i2c_tx_data(data, 2);
	            msleep(MMC328X_DELAY_RRM);
#endif
			/* RM */
			data[0] = MMC328X_REG_CTRL;
			data[1] = MMC328X_CTRL_RM;
			/* not check return value here, assume it always OK */
			mmc328x_i2c_tx_data(data, 2);
			/* wait external capacitor charging done for next RM */
			msleep(MMC328X_DELAY_RM);
		}
		read_idx++;
		/* send TM cmd before read */
		data[0] = MMC328X_REG_CTRL;
		data[1] = MMC328X_CTRL_TM;
		/* not check return value here, assume it always OK */
		mmc328x_i2c_tx_data(data, 2);
		/* wait TM done for coming data read */
		msleep(MMC328X_DELAY_TM);
#if READMD
		/* Read MD */
		data[0] = MMC328X_REG_DS;
		if (mmc328x_i2c_rx_data(data, 1) < 0) {
			return -EFAULT;
		}
		while (!(data[0] & 0x01)) {
			msleep(1);
			/* Read MD again*/
			data[0] = MMC328X_REG_DS;
			if (mmc328x_i2c_rx_data(data, 1) < 0) {
				return -EFAULT;
			}
			if (data[0] & 0x01) break;
			MD_times++;
			if (MD_times > 2) {
		#if DEBUG
				printk("TM not work!!");
		#endif
				return -EFAULT;
			}
		}
#endif		
		/* read xyz raw data */
		data[0] = MMC328X_REG_DATA;
		if (mmc328x_i2c_rx_data(data, 6) < 0) {
			return -EFAULT;
		}
		vec[0] = data[1] << 8 | data[0];
		vec[1] = data[3] << 8 | data[2];
		vec[2] = data[5] << 8 | data[4];
	#if DEBUG
		printk("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
	#endif
		if (copy_to_user(pa, vec, sizeof(vec))) {
			return -EFAULT;
		}

		break;
	default:
		break;
	}

	return 0;
}

static ssize_t mmc328x_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "MMC328X");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(mmc328x, S_IRUGO, mmc328x_show, NULL);

static struct file_operations mmc328x_fops = {
	.owner          = THIS_MODULE,
	.open           = mmc328x_open,
	.release        = mmc328x_release,
	.unlocked_ioctl = mmc328x_ioctl,
};

static struct miscdevice mmc328x_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MMC328X_DEV_NAME,
	.fops = &mmc328x_fops,
};

static int mmc328x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	unsigned char data[16] = {0};
	int err = 0;

	struct input_dev *dev = 0;
    struct mmc328x_data *mmc328x = 0; 
    
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		err = -ENODEV;
		goto out;
	}
	this_client = client;

	mmc328x = kzalloc(sizeof(struct mmc328x_data), GFP_KERNEL);
	if (!mmc328x) {
		pr_err("%s [mmc328x]:alloc data failed.\n", __FUNCTION__);
		err = -ENOMEM;
		goto out;
	}
	
	INIT_DELAYED_WORK(&mmc328x->work, mmc328x_work_func);
	atomic_set(&mmc328x->delay, MMC328X_DEF_DELAY);
	atomic_set(&mmc328x->enable, 0);

	dev = input_allocate_device();
	if (!dev) {
		goto kfree_exit;
	}

	dev->name = MMC328X_DEV_NAME;
	dev->id.bustype = BUS_I2C;

#if 0
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);
#endif
	input_set_drvdata(dev, mmc328x);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto kfree_exit;
	}

	mmc328x->input = dev;

	err = sysfs_create_group(&mmc328x->input->dev.kobj,
			&mmc328x_attribute_group);
	if (err < 0)
		goto error_sysfs;

	i2c_set_clientdata(client, mmc328x);

	err = misc_register(&mmc328x_device);
	if (err) {
		pr_err("%s: mmc328x_device register failed\n", __FUNCTION__);
		goto out;
	}
	err = device_create_file(&client->dev, &dev_attr_mmc328x);
	if (err) {
		pr_err("%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister;
	}

	/* send ST cmd to mag sensor first of all */
#ifdef CONFIG_SENSORS_MMC328xMA
	data[0] = MMC328X_REG_CTRL;
	data[1] = MMC328X_CTRL_RRM;
	if (mmc328x_i2c_tx_data(data, 2) < 0) {
	}
	msleep(MMC328X_DELAY_RRM);
	data[0] = MMC328X_REG_CTRL;
	data[1] = MMC328X_CTRL_TM;
	if (mmc328x_i2c_tx_data(data, 2) < 0) {
	}
	msleep(5*MMC328X_DELAY_TM);
#endif
	data[0] = MMC328X_REG_CTRL;
	data[1] = MMC328X_CTRL_RM;
	if (mmc328x_i2c_tx_data(data, 2) < 0) {
		/* assume RM always success */
	}
#ifndef CONFIG_SENSORS_MMC328xMA 
	/* wait external capacitor charging done for next RM */
	msleep(MMC328X_DELAY_RM);
#else
	msleep(10*MMC328X_DELAY_RM);
	data[0] = MMC328X_REG_CTRL;
	data[1] = MMC328X_CTRL_TM;
	if (mmc328x_i2c_tx_data(data, 2) < 0) {
	}
#endif

	return 0;

error_sysfs:
	input_unregister_device(mmc328x->input);

kfree_exit:
	kfree(mmc328x);

out_deregister:
	misc_deregister(&mmc328x_device);
out:
	return err;
}

static int mmc328x_remove(struct i2c_client *client)
{
	struct mmc328x_data *data = i2c_get_clientdata(client);
	device_remove_file(&client->dev, &dev_attr_mmc328x);
	misc_deregister(&mmc328x_device);

	sysfs_remove_group(&data->input->dev.kobj, &mmc328x_attribute_group);
	input_unregister_device(data->input);
	kfree(data);

	return 0;
}

static const struct i2c_device_id mmc328x_id[] = {
	{ MMC328X_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver mmc328x_driver = {
	.probe 		= mmc328x_probe,
	.remove 	= mmc328x_remove,
	.id_table	= mmc328x_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= MMC328X_I2C_NAME,
	},
};


static int __init mmc328x_init(void)
{
	pr_info("mmc328x driver: init\n");
	return i2c_add_driver(&mmc328x_driver);
}

static void __exit mmc328x_exit(void)
{
	pr_info("mmc328x driver: exit\n");
	i2c_del_driver(&mmc328x_driver);
}

module_init(mmc328x_init);
module_exit(mmc328x_exit);

MODULE_DESCRIPTION("MEMSIC MMC328X Magnetic Sensor Driver");
MODULE_LICENSE("GPL");

