/*
 *
 * arch/arm/plat-meson/chip_temp_ctrl.c
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * License terms: GNU General Public License (GPL) version 2
 * CPU tempreture management.
 *
 */
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/fs.h>
 
static struct workqueue_struct* temp_ck_wq;
static struct delayed_work temp_check_work;

static DEFINE_MUTEX(monitor_mutex);

#define CHIP_TEMP_DEV_NAME "chip_temp"

#ifdef CONFIG_TEMPERATURE_SENSOR_GOVERNOR
extern int get_chip_temperature(void);
#else
static int get_chip_temperature(void)
{
	return 0;
}
#endif

struct chip_temp_dev {
	const char	*name;
	struct device	*dev;
	dev_t		dev_no;
} temp_dev;

static struct class *chip_temp_ctrl_class;
	
#define OFF_TEMP_COUNT 10
#define MAX_TEMP_COUNT 10
#define MIN_TEMP_COUNT 10

struct chip_temp_ctrl_priv {
	unsigned int enable;
	unsigned int off_temp;
	unsigned int max_temp;
	unsigned int min_temp;
	unsigned int scaling_freq_limit;
	unsigned int normal_scaling_freq;
	unsigned int delay;
	unsigned int hot_flag;
	unsigned int max_temp_flag;
	unsigned int min_temp_flag;
	bool ctrl_flag;
}; 
static struct chip_temp_ctrl_priv temp_ctrl_priv = {
	.off_temp = 700,
	.max_temp = 650,
	.min_temp = 600,
	.scaling_freq_limit = 1000000,
	.normal_scaling_freq = 1200000,
	.delay = 500,
	.ctrl_flag = false,
};

static void temp_check_func(struct work_struct * work)
{
	struct cpufreq_policy new_policy;
	struct cpufreq_policy *policy;
	int ret;
	unsigned int temp = get_chip_temperature();
	
	mutex_lock(&monitor_mutex);
	if (!temp_ctrl_priv.enable) {
		temp_ctrl_priv.hot_flag = 0;
		temp_ctrl_priv.max_temp_flag = 0;
		temp_ctrl_priv.min_temp_flag = 0;
		mutex_unlock(&monitor_mutex);
		goto out;
	}
	mutex_unlock(&monitor_mutex);

	//printk("current temp is %d\n", temp);
	if (temp >= temp_ctrl_priv.off_temp) {
		temp_ctrl_priv.hot_flag++;
		if (temp_ctrl_priv.hot_flag > OFF_TEMP_COUNT) {
			printk("chip is too hot\n");
			kobject_uevent(&temp_dev.dev->kobj, KOBJ_CHANGE);
			temp_ctrl_priv.hot_flag = 0;
		}
	} else if (temp_ctrl_priv.hot_flag > 0) {
		temp_ctrl_priv.hot_flag--;
	}
	if (temp >= temp_ctrl_priv.max_temp ) {
		temp_ctrl_priv.max_temp_flag++;
		if (temp_ctrl_priv.max_temp_flag < MAX_TEMP_COUNT) 
			goto out;
		policy = cpufreq_cpu_get(0);
		if (!policy) {
			printk("ERROR, cpufreq policy not get, ret:%p\n",
				 policy);
			goto out;
		}
		ret = cpufreq_get_policy(&new_policy, policy->cpu);
		if (!ret && new_policy.max > 
				temp_ctrl_priv.scaling_freq_limit) {
			//temp_ctrl_priv.normal_scaling_freq = new_policy.max;
			new_policy.max = temp_ctrl_priv.scaling_freq_limit;
			cpufreq_set_policy(policy, &new_policy);
			temp_ctrl_priv.ctrl_flag = true;
			printk("enable temp ctrl, current temp:%d\n", temp);
		}
		temp_ctrl_priv.max_temp_flag = 0;
	} else if (temp_ctrl_priv.max_temp_flag > 0) {
		temp_ctrl_priv.max_temp_flag--;
	}
	if (temp <= temp_ctrl_priv.min_temp 
			&& temp_ctrl_priv.ctrl_flag) {
		temp_ctrl_priv.min_temp_flag++;
		if (temp_ctrl_priv.min_temp_flag < MIN_TEMP_COUNT)
			goto out;
		policy = cpufreq_cpu_get(0);
		if (!policy) {
			printk("ERROR, cpufreq policy not get, ret:%p\n", 
				policy);
			goto out;
		}
		ret = cpufreq_get_policy(&new_policy, policy->cpu);
		if (!ret && new_policy.max < 
				temp_ctrl_priv.normal_scaling_freq) {
			new_policy.max = temp_ctrl_priv.normal_scaling_freq;
			cpufreq_set_policy(policy, &new_policy);
			temp_ctrl_priv.ctrl_flag = false;
			printk("disable temp ctrl, current temp:%d\n", temp);
		}
		temp_ctrl_priv.min_temp_flag = 0;
	} else if (temp_ctrl_priv.min_temp_flag > 0) {
		temp_ctrl_priv.min_temp_flag--;
	}
out:
	queue_delayed_work(temp_ck_wq, &temp_check_work, 
					temp_ctrl_priv.delay);
}

#define define_one_rw(file_name) \
static ssize_t file_name##_show(struct class *cla, \
		struct class_attribute *attr, char *buf) \
{ \
	return sprintf(buf, "%d\n", temp_ctrl_priv.file_name); \
} \
\
static ssize_t file_name##_store(struct class *cla, \
		struct class_attribute *attr, char *buf, ssize_t count) \
{ \
	sscanf(buf, "%d", (int*)&temp_ctrl_priv.file_name); \
	printk(#file_name "=%d\n", temp_ctrl_priv.file_name); \
	return count; \
}

define_one_rw(enable)
define_one_rw(off_temp)
define_one_rw(max_temp)
define_one_rw(min_temp)
define_one_rw(scaling_freq_limit)
define_one_rw(normal_scaling_freq)

static ssize_t state_show(struct class *cla, 
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", temp_ctrl_priv.ctrl_flag);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);
static DEVICE_ATTR(off_temp, S_IRUGO | S_IWUSR, off_temp_show, off_temp_store);
static DEVICE_ATTR(max_temp, S_IRUGO | S_IWUSR, max_temp_show, max_temp_store);
static DEVICE_ATTR(min_temp, S_IRUGO | S_IWUSR, min_temp_show, min_temp_store);
static DEVICE_ATTR(scaling_freq_limit, S_IRUGO | S_IWUSR, 
			scaling_freq_limit_show, scaling_freq_limit_store);
static DEVICE_ATTR(normal_scaling_freq, S_IRUGO | S_IWUSR, 
			normal_scaling_freq_show, normal_scaling_freq_store);
static DEVICE_ATTR(state, S_IRUGO | S_IWUSR, state_show, NULL);

static int __init chip_temp_ctrl_init(void)
{
	int ret;
	chip_temp_ctrl_class = class_create(THIS_MODULE, "chip_temp");
	if (IS_ERR(chip_temp_ctrl_class)){
		pr_info("chip_temp class create error\n");
		ret = PTR_ERR(chip_temp_ctrl_class);
		goto err_class_reg;
	}
	ret = alloc_chrdev_region(&temp_dev.dev_no, 0, 1, CHIP_TEMP_DEV_NAME);
	if (ret < 0)
	{
		pr_info("temp dev no alloc error\n");
		ret = -ENODEV;
		goto err_dev_no;
	}
	temp_dev.dev = device_create(chip_temp_ctrl_class, NULL, 
				temp_dev.dev_no, NULL, CHIP_TEMP_DEV_NAME);
	if (IS_ERR(temp_dev.dev)) {
		pr_info("temp dev create error\n");
		ret = PTR_ERR(temp_dev.dev);
		goto err_dev_create;
	}
	ret = device_create_file(temp_dev.dev, &dev_attr_enable);
	if (ret < 0)
		goto err_create_file_0;
	ret = device_create_file(temp_dev.dev, &dev_attr_off_temp);
	if (ret < 0)
		goto err_create_file_1;
	ret = device_create_file(temp_dev.dev, &dev_attr_max_temp);
	if (ret < 0)
		goto err_create_file_2;
	ret = device_create_file(temp_dev.dev, &dev_attr_min_temp);
	if (ret < 0)
		goto err_create_file_3;
	ret = device_create_file(temp_dev.dev, &dev_attr_scaling_freq_limit);
	if (ret < 0)
		goto err_create_file_4;
	ret = device_create_file(temp_dev.dev, &dev_attr_normal_scaling_freq);
	if (ret < 0)
		goto err_create_file_5;
	ret = device_create_file(temp_dev.dev, &dev_attr_state);
	if (ret < 0)
		goto err_create_file_6;
	
	temp_ck_wq = create_workqueue("temp_ck_wq");
	INIT_DELAYED_WORK(&temp_check_work, temp_check_func);
	queue_delayed_work(temp_ck_wq, &temp_check_work, 
						temp_ctrl_priv.delay);
	return 0;

err_create_file_6:
	device_remove_file(temp_dev.dev, &dev_attr_normal_scaling_freq);
err_create_file_5:
	device_remove_file(temp_dev.dev, &dev_attr_scaling_freq_limit);
err_create_file_4:
	device_remove_file(temp_dev.dev, &dev_attr_min_temp);
err_create_file_3:
	device_remove_file(temp_dev.dev, &dev_attr_max_temp);
err_create_file_2:
	device_remove_file(temp_dev.dev, &dev_attr_off_temp);	
err_create_file_1:
	device_remove_file(temp_dev.dev, &dev_attr_enable);	
err_create_file_0:
	device_destroy(chip_temp_ctrl_class, temp_dev.dev_no);
err_dev_create:
	unregister_chrdev_region(temp_dev.dev_no, 1);
err_dev_no:
	class_unregister(chip_temp_ctrl_class);
err_class_reg: 
	return ret;
}

static void __exit chip_temp_ctrl_exit(void)
{
	device_remove_file(temp_dev.dev, &dev_attr_state);
	device_remove_file(temp_dev.dev, &dev_attr_normal_scaling_freq);
	device_remove_file(temp_dev.dev, &dev_attr_scaling_freq_limit);
	device_remove_file(temp_dev.dev, &dev_attr_min_temp);
	device_remove_file(temp_dev.dev, &dev_attr_max_temp);
	device_remove_file(temp_dev.dev, &dev_attr_off_temp);	
	device_destroy(chip_temp_ctrl_class, temp_dev.dev_no);
	unregister_chrdev_region(temp_dev.dev_no, 1);
	class_unregister(chip_temp_ctrl_class);
	return;
}

module_init(chip_temp_ctrl_init);
module_exit(chip_temp_ctrl_exit);

MODULE_DESCRIPTION("Amlogic CHIP TEMP CTRL");
MODULE_AUTHOR("Amlogic");
MODULE_LICENSE("GPL");
