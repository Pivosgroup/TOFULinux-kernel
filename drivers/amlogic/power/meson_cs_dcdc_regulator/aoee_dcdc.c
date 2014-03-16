#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <plat/io.h>

#define AOEE_DCDC_MAX_STEPS 15

static struct aoee_dcdc_pdata_t *g_aoee_dcdc_pdata;

static int cur_voltage;

struct aoee_dcdc_pdata_t {
	int voltage_step_table[AOEE_DCDC_MAX_STEPS+1];
	int default_uV;
	int min_uV;
	int max_uV;
};

static DEFINE_MUTEX(aoee_dcdc_lock);

static int set_voltage(int from, int to)
{
	
	if (to < 0 || to > AOEE_DCDC_MAX_STEPS) {
		printk(KERN_ERR "%s: to(%d) out of range!\n", __FUNCTION__, to);
		return -EINVAL;
	}
	if (from < 0 || from > AOEE_DCDC_MAX_STEPS) {	
		aml_set_reg32_bits(P_VGHL_PWM_REG0, to, 0, 4);
		udelay(200);
	} else if(to < from) {
		if (from - to > 3) {
			aml_set_reg32_bits(P_VGHL_PWM_REG0, to + 3, 0, 4);
			udelay(100);
		} if (from - to > 1) {
			aml_set_reg32_bits(P_VGHL_PWM_REG0, to + 1, 0, 4);
			udelay(100);
		}
		aml_set_reg32_bits(P_VGHL_PWM_REG0, to, 0, 4);
		udelay(100);
	} else if(to > from) {
		// going to lower voltage
		if (to - from > 3) {
			aml_set_reg32_bits(P_VGHL_PWM_REG0, to - 3, 0, 4);
			udelay(100);
		}
		if (to - from > 1) {
			aml_set_reg32_bits(P_VGHL_PWM_REG0, to - 1, 0, 4);
			udelay(100);
		}
		aml_set_reg32_bits(P_VGHL_PWM_REG0, to, 0, 4);
		udelay(100);
	}
	return 0;
}


static int get_voltage(void)
{
	u32 reg_val;
	int data;
	mutex_lock(&aoee_dcdc_lock);

	reg_val = aml_read_reg32(P_VGHL_PWM_REG0);

	if ((reg_val>>12&3) != 1) {
		printk("Error getting voltage\n");
		data = -1;
		goto out;
	}

	/* Convert the data from table & step to microvolts */
	data = g_aoee_dcdc_pdata->voltage_step_table[reg_val & 0xf];

out:
	mutex_unlock(&aoee_dcdc_lock);
	return data;
}

static ssize_t show_cur_voltage(struct class* class,
			   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", get_voltage());
}

static int store_cur_voltage(struct class* class,
			   struct device_attribute *attr, char *buf)
{
	int tar_vol = 0;
	size_t ret;
	ret = sscanf(buf, "%d\n", &tar_vol);
	int cur_idx, last_idx;
	
	if (tar_vol < g_aoee_dcdc_pdata->voltage_step_table[AOEE_DCDC_MAX_STEPS] 
			|| tar_vol > g_aoee_dcdc_pdata->voltage_step_table[0])
		return -EINVAL;

	for (last_idx=0; last_idx<=AOEE_DCDC_MAX_STEPS; last_idx++) {
		if(cur_voltage >= g_aoee_dcdc_pdata->voltage_step_table[last_idx]) {
			break;
		}
	}

	for (cur_idx=0; cur_idx<=AOEE_DCDC_MAX_STEPS; cur_idx++) {
		if(tar_vol >= g_aoee_dcdc_pdata->voltage_step_table[cur_idx]) {
			break;
		}
	}

	mutex_lock(&aoee_dcdc_lock);

	set_voltage(last_idx, cur_idx);

	cur_voltage = g_aoee_dcdc_pdata->voltage_step_table[cur_idx];
	mutex_unlock(&aoee_dcdc_lock);
	return ret;
}

static ssize_t min_vol_show(struct class* class,
			   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_aoee_dcdc_pdata->min_uV);
}

static ssize_t max_vol_show(struct class* class,
			   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_aoee_dcdc_pdata->max_uV);
}

static struct class_attribute aoee_dcdc_attrs[]={
    __ATTR(cur_voltage,  S_IRUGO | S_IWUSR,
    		show_cur_voltage, store_cur_voltage),
    //__ATTR_RO(vol_step),
    __ATTR_RO(max_vol),
    __ATTR_RO(min_vol),
    __ATTR_NULL,
};

static struct class* aoee_dcdc_clsp;

static __devinit int aoee_dcdc_probe(struct platform_device *pdev)
{
	int ret = -1;
	int i;
	g_aoee_dcdc_pdata  = pdev->dev.platform_data;
	
	aoee_dcdc_clsp = class_create(THIS_MODULE, "aoee_dcdc");
	if(IS_ERR(aoee_dcdc_clsp)){
		ret = PTR_ERR(aoee_dcdc_clsp);
		return ret;
	}
	for(i = 0; aoee_dcdc_attrs[i].attr.name; i++){
		if(class_create_file(aoee_dcdc_clsp, &aoee_dcdc_attrs[i]) < 0)
			goto err;
	}
	cur_voltage = g_aoee_dcdc_pdata->voltage_step_table[0];
	return 0;
err:
	for(i=0; aoee_dcdc_attrs[i].attr.name; i++){
		class_remove_file(aoee_dcdc_clsp, &aoee_dcdc_attrs[i]);
	}
	class_destroy(aoee_dcdc_clsp); 
	return -1;  
}

static int __devexit aoee_dcdc_remove(struct platform_device *pdev)
{
	int i;
	for(i=0; aoee_dcdc_attrs[i].attr.name; i++){
		class_remove_file(aoee_dcdc_clsp, &aoee_dcdc_attrs[i]);
	}
	class_destroy(aoee_dcdc_clsp); 
	return 0;
}



static struct platform_driver aoee_dcdc_driver = {
	.driver = {
		.name = "aoee-dcdc",
		.owner = THIS_MODULE,
	},
	.probe = aoee_dcdc_probe,
	.remove = __devexit_p(aoee_dcdc_remove),
};


static int __init aoee_dcdc_init(void)
{
	return platform_driver_register(&aoee_dcdc_driver);
}

static void __exit aoee_dcdc_cleanup(void)
{
	platform_driver_unregister(&aoee_dcdc_driver);
}

subsys_initcall(aoee_dcdc_init);
module_exit(aoee_dcdc_cleanup);
