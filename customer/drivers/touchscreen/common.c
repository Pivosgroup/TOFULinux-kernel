/*
 * drivers/amlogic/input/touchscreen/common.c
 *
 * 
 *	
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include "common.h"

static struct touch_pdata common = {
	.owner = NULL,
};
struct touch_pdata *ts_com = &common;
static struct file  *fp;
/***********************************************************************************************
Name	:	touch_open_fw
Input	:	*fw

Output	:	file size
function	:	open the firware file, and return total size
***********************************************************************************************/
int touch_open_fw(char *fw)
{
	loff_t file_size;
	mm_segment_t fs;
	struct inode *inode = NULL;
	fp = filp_open(fw, O_RDONLY, 0);
    if (IS_ERR(fp)) {
			//printk("read fw file error\n");
		return -1;
	}

	inode = fp->f_dentry->d_inode;
	file_size = inode->i_size;


	fs = get_fs();
	set_fs(KERNEL_DS);

	return 	file_size;

}

/***********************************************************************************************
Name	:	touch_read_fw
Input	:	offset
            length, read length
            buf, return buffer
Output	:
function	:	read data to buffer
***********************************************************************************************/
int touch_read_fw(int offset, int length, char *buf)
{
	loff_t  pos = offset;
	vfs_read(fp, buf, length, &pos);
	return 0;
}

/***********************************************************************************************
Name	:	touch_close_fw
Input	:
Output	:
function	:	close file
***********************************************************************************************/
int touch_close_fw(void)
{
	filp_close(fp, NULL);
	return 0;
}

ssize_t touch_read(struct device *dev, struct device_attribute *attr, char *buf)
{

    if (!strcmp(attr->attr.name, "PrintkFlag")) {
        memcpy(buf, &ts_com->printk_enable_flag,sizeof(ts_com->printk_enable_flag));
        printk("buf[0]=%d, buf[1]=%d\n", buf[0], buf[1]);
        return sizeof(ts_com->printk_enable_flag);
    }
    return 0;
}

ssize_t touch_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	printk("buf[0]=%d, buf[1]=%d\n", buf[0], buf[1]);
	if (!strcmp(attr->attr.name, "PrintkFlag")) {
		if (buf[0] == '0') ts_com->printk_enable_flag = 0;
		if (buf[0] == '1') ts_com->printk_enable_flag = 1;
  }	else if(!strcmp(attr->attr.name, "HardwareReset")) {
  		if(ts_com->hardware_reset)
				ts_com->hardware_reset(ts_com);
  } else if (!strcmp(attr->attr.name, "SoftwareReset")) {
  	  if(ts_com->software_reset)
				ts_com->software_reset(ts_com);
  } else if (!strcmp(attr->attr.name, "EnableIrq")) {
	  	if (buf[0] == '0') {
	  		printk("%s: disable irq %d\n", ts_com->owner, ts_com->irq);
	  		disable_irq_nosync(ts_com->irq); 
	  	}else if (buf[0] == '1') {
	  		printk("%s: enable irq %d\n", ts_com->owner, ts_com->irq);
	  		enable_irq(ts_com->irq);
	  	}
	}  		
	else if (!strcmp(attr->attr.name, "upgrade")) {
			if(ts_com->upgrade_touch)
				ts_com->upgrade_touch();
	}
	return count;
}

DEVICE_ATTR(PrintkFlag, S_IRUGO | S_IWUSR, touch_read, touch_write);
DEVICE_ATTR(HardwareReset, S_IRUGO | S_IWUSR, touch_read, touch_write);
DEVICE_ATTR(SoftwareReset, S_IRUGO | S_IWUSR, touch_read, touch_write);
DEVICE_ATTR(EnableIrq, S_IRUGO | S_IWUSR, touch_read, touch_write);
DEVICE_ATTR(upgrade, S_IRUGO | S_IWUSR, touch_read, touch_write);
struct attribute *touch_attr[] = {
    &dev_attr_PrintkFlag.attr,
    &dev_attr_HardwareReset.attr,
    &dev_attr_SoftwareReset.attr,
    &dev_attr_EnableIrq.attr,
    &dev_attr_upgrade.attr,
    NULL
};
struct attribute_group touch_attr_group = {
    .name = NULL,
    .attrs = touch_attr,
};

int upgrade_open(struct inode * inode, struct file * filp)
{
	return 0;
}
int upgrade_close(struct inode *inode, struct file *file)
{
	return 0;
}

int upgrade_ioctl(struct file *filp,
                     unsigned int cmd, unsigned long args)
{
	unsigned char uc_reg_value;
	switch (cmd) {			
		case 1:
		if(ts_com->upgrade_touch)
			ts_com->upgrade_touch();
			break;
		default:
			printk("%s Warning: Wrong command code\n", __func__);
			return 0;
	}
}
struct file_operations upgrade_fops = {
    .owner	= THIS_MODULE,
    .open	= upgrade_open,
    .release	= upgrade_close,
    .unlocked_ioctl	= upgrade_ioctl,
};

int upgrade_suspend(struct device *dev, pm_message_t state)
{
		return 0;
}

int upgrade_resume(struct device *dev)
{
	return 0;
}

struct class upgrade_class = {
    
	.name = UPGRADE_TOUCH,
	.owner = THIS_MODULE,
	.suspend = upgrade_suspend,
	.resume = upgrade_resume,
};

int create_init(struct device dev, struct touch_pdata *pdata)
{
	int err;
  err = sysfs_create_group(&dev.kobj, &touch_attr_group);
  err = alloc_chrdev_region(&pdata->upgrade_no, 0, 1, UPGRADE_TOUCH);
  if (err < 0) {
      printk("Can't register major for upgrade_touch device\n");
  }
	/* connect the file operations with cdev */
	cdev_init(&pdata->upgrade_cdev, &upgrade_fops);
	pdata->upgrade_cdev.owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	err = cdev_add(&pdata->upgrade_cdev, pdata->upgrade_no, 1);
	if (err) {
		printk("upgrade touch: failed to add device. \n");
	}
	err = class_register(&upgrade_class);
	if (err < 0) {
		printk("class_register(&upgrade_class) failed!\n");
	}
	pdata->dev = device_create(&upgrade_class, NULL, pdata->upgrade_no, NULL, UPGRADE_TOUCH);
	if (IS_ERR(pdata->dev)) {
	 	printk(KERN_ERR "upgrade_cdev: failed to create device node\n");
	  class_destroy(&upgrade_class);
    return -EEXIST;
	}
	
	return 0;
}

void destroy_remove(struct device dev, struct touch_pdata *pdata)
{
	sysfs_remove_group(&dev.kobj, &touch_attr_group);
	cdev_del(&pdata->upgrade_cdev);
	device_destroy(&upgrade_class, pdata->upgrade_no);
	class_destroy(&upgrade_class);
}