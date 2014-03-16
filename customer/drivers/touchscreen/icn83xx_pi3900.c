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
 Date :        01,17,2013
 Version:      1.0
 History :
     2012,10,30, V0.1 first version  
 --*/

#include "icn83xx_pi3900.h"

#if COMPILE_FW_WITH_DRIVER
#include "icn83xx_fw.h"
#endif

static struct i2c_client *this_client;
short log_rawdata[28][16] = {0,};
short log_diffdata[28][16] = {0,};

#if SUPPORT_AMLOGIC
static char firmware[128] = {"/fw.bin"};
//if file system not ready,you can use inner array 
//static char firmware[128] = "icn83xx_firmware";
static int probe_flag = 0;
#endif

#if SUPPORT_ALLWINNER_A13
static char firmware[128] = {"/system/vendor/modules/fw.bin"};
//if file system not ready,you can use inner array 
//static char firmware[128] = "icn83xx_firmware";
#endif

#if SUPPORT_ROCKCHIP
static char firmware[128] = {"/system/vendor/modules/fw.bin"};
//if file system not ready,you can use inner array 
//static char firmware[128] = "icn83xx_firmware";
#endif

#if SUPPORT_SPREADTRUM
static char firmware[128] = {"/system/sps/ICN83XX/ko/ICN8303.BIN"};
//if file system not ready,you can use inner array 
//static char firmware[128] = "icn83xx_firmware";
#endif

#if SUPPORT_SYSFS
static enum hrtimer_restart chipone_timer_func(struct hrtimer *timer);

static ssize_t icn83xx_show_update(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn83xx_store_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len);
static ssize_t icn83xx_show_process(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn83xx_store_process(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);

static DEVICE_ATTR(update, S_IRUGO | S_IWUSR, icn83xx_show_update, icn83xx_store_update);
static DEVICE_ATTR(process, S_IRUGO | S_IWUSR, icn83xx_show_process, icn83xx_store_process);

static ssize_t icn83xx_show_process(struct device* cd,struct device_attribute *attr, char* buf)
{
    ssize_t ret = 0;
    sprintf(buf, "icn83xx process\n");
    ret = strlen(buf) + 1;
    return ret;
}

static ssize_t icn83xx_store_process(struct device* cd, struct device_attribute *attr,
               const char* buf, size_t len)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    unsigned long on_off = simple_strtoul(buf, NULL, 10); 
    if(on_off == 0)
    {
        icn83xx_ts->work_mode = on_off;
    }
    else if((on_off == 1) || (on_off == 2))
    {
        if((icn83xx_ts->work_mode == 0) && (icn83xx_ts->use_irq == 1))
        {
            hrtimer_init(&icn83xx_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
            icn83xx_ts->timer.function = chipone_timer_func;
            hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
        }
        icn83xx_ts->work_mode = on_off;
    }
    return len;
}

static ssize_t icn83xx_show_update(struct device* cd,
                     struct device_attribute *attr, char* buf)
{
    ssize_t ret = 0;     
    sprintf(buf, "icn83xx firmware\n");
    ret = strlen(buf) + 1;
    return ret;
}

static ssize_t icn83xx_store_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    int err=0;
    unsigned long on_off = simple_strtoul(buf, NULL, 10); 
    return len;
}

static int icn83xx_create_sysfs(struct i2c_client *client)
{
    int err;
    struct device *dev = &(client->dev);
    icn83xx_trace("%s: \n",__func__);    
    err = device_create_file(dev, &dev_attr_update);
    err = device_create_file(dev, &dev_attr_process);
    return err;
}

#endif

#if SUPPORT_PROC_FS

pack_head cmd_head;
static struct proc_dir_entry *icn83xx_proc_entry;
int  DATA_LENGTH = 0;
static int icn83xx_tool_write(struct file *filp, const char __user *buff, unsigned long len, void *data)
{
    int ret = 0;
    
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    proc_info("%s \n",__func__);  
    if(down_interruptible(&icn83xx_ts->sem))  
    {  
        return -1;   
    }     
    ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);
    if(ret)
    {
        proc_error("copy_from_user failed.\n");
        goto write_out;
    }  
    else
    {
        ret = CMD_HEAD_LENGTH;
    }
    
    proc_info("wr  :0x%02x.\n", cmd_head.wr);
    proc_info("flag:0x%02x.\n", cmd_head.flag);
    proc_info("circle  :%d.\n", (int)cmd_head.circle);
    proc_info("times   :%d.\n", (int)cmd_head.times);
    proc_info("retry   :%d.\n", (int)cmd_head.retry);
    proc_info("data len:%d.\n", (int)cmd_head.data_len);
    proc_info("addr len:%d.\n", (int)cmd_head.addr_len);
    proc_info("addr:0x%02x%02x.\n", cmd_head.addr[0], cmd_head.addr[1]);
    proc_info("len:%d.\n", (int)len);
    proc_info("data:0x%02x%02x.\n", buff[CMD_HEAD_LENGTH], buff[CMD_HEAD_LENGTH+1]);
    if (1 == cmd_head.wr)  // write iic
    {
        if(1 == cmd_head.addr_len)
        {
            ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
            if(ret)
            {
                proc_error("copy_from_user failed.\n");
                goto write_out;
            }
            ret = icn83xx_i2c_txdata(cmd_head.addr[0], &cmd_head.data[0], cmd_head.data_len);
            if (ret < 0) {
                proc_error("write iic failed! ret: %d\n", ret);
                goto write_out;
            }
            ret = cmd_head.data_len + CMD_HEAD_LENGTH;
            goto write_out;
        }
    }
    else if(3 == cmd_head.wr)
    {
        ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
        if(ret)
        {
            proc_error("copy_from_user failed.\n");
            goto write_out;
        }
        ret = cmd_head.data_len + CMD_HEAD_LENGTH;
        memset(firmware, 0, 128);
        memcpy(firmware, &cmd_head.data[0], cmd_head.data_len);
        proc_info("firmware : %s\n", firmware);
    }
    else if(5 == cmd_head.wr)
    {        
        icn83xx_update_status(1);  
        ret = kernel_thread(icn83xx_fw_update,firmware,CLONE_KERNEL);
        icn83xx_trace("the kernel_thread result is:%d\n", ret);    
    }
    else if(7 == cmd_head.wr)  //write reg
    { 
        if(2 == cmd_head.addr_len)
        {
            ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
            if(ret)
            {
                proc_error("copy_from_user failed.\n");
                goto write_out;
            }
            ret = icn83xx_writeReg((cmd_head.addr[0]<<8)|cmd_head.addr[1], cmd_head.data[0]);
            if (ret < 0) {
                proc_error("write reg failed! ret: %d\n", ret);
                goto write_out;
            }
            ret = cmd_head.data_len + CMD_HEAD_LENGTH;
            goto write_out;   

        }
    }

write_out:
    up(&icn83xx_ts->sem); 
    return len;
    
}
static int icn83xx_tool_read( char *page, char **start, off_t off, int count, int *eof, void *data )
{
    int i;
    int ret = 0;
    int data_len = 0;
    int len = 0;
    int loc = 0;
    char retvalue;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    if(down_interruptible(&icn83xx_ts->sem))  
    {  
        return -1;   
    }     
    proc_info("%s: count:%d, off:%d, cmd_head.data_len: %d\n",__func__, count, off, cmd_head.data_len); 
    if (cmd_head.wr % 2)
    {
        ret = 0;
        goto read_out;
    }
    else if (0 == cmd_head.wr)   //read iic
    {
        if(1 == cmd_head.addr_len)
        {
            data_len = cmd_head.data_len;
            if(cmd_head.addr[0] == 0xff)
            {                
                page[0] = 83;
                proc_info("read ic type: %d\n", page[0]);
            }
            else
            {
                while(data_len>0)
                {
                    if (data_len > DATA_LENGTH)
                    {
                        len = DATA_LENGTH;
                    }
                    else
                    {
                        len = data_len;
                    }
                    data_len -= len;   
                    memset(&cmd_head.data[0], 0, len+1);
                    ret = icn83xx_i2c_rxdata(cmd_head.addr[0]+loc, &cmd_head.data[0], len);
                    //proc_info("cmd_head.data[0]: 0x%02x\n", cmd_head.data[0]);
                    //proc_info("cmd_head.data[1]: 0x%02x\n", cmd_head.data[1]);
                    if(ret < 0)
                    {
                        icn83xx_error("read iic failed: %d\n", ret);
                        goto read_out;
                    }
                    else
                    {
                        //proc_info("iic read out %d bytes, loc: %d\n", len, loc);
                        memcpy(&page[loc], &cmd_head.data[0], len);
                    }
                    loc += len;
                }
                proc_info("page[0]: 0x%02x\n", page[0]);
                proc_info("page[1]: 0x%02x\n", page[1]);
            }
        }
    }
    else if(2 == cmd_head.wr)  //read rawdata
    {
        //scan tp rawdata
        icn83xx_write_reg(4, 0x20); 
        mdelay(cmd_head.times);
        icn83xx_read_reg(2, &retvalue);
        while(retvalue != 1)
        {
            mdelay(cmd_head.times);
            icn83xx_read_reg(2, &retvalue);
        }            
  
        if(2 == cmd_head.addr_len)
        {
            for(i=0; i<cmd_head.addr[1]; i++)
            {
                icn83xx_write_reg(3, i);
                mdelay(cmd_head.times);
                ret = icn83xx_i2c_rxdata(128, &cmd_head.data[0], cmd_head.addr[0]*2);
                if (ret < 0) 
                {
                    icn83xx_error("read rawdata failed: %d\n", ret);            
                    goto read_out;           
                }
                else
                {
                    //proc_info("read rawdata out %d bytes, loc: %d\n", cmd_head.addr[0]*2, loc);                    
                    memcpy(&page[loc], &cmd_head.data[0], cmd_head.addr[0]*2);
                }
                loc += cmd_head.addr[0]*2;
            }  
            for(i=0; i<cmd_head.data_len; i=i+2)
            {
                swap_ab(page[i], page[i+1]);
            }
            //icn83xx_rawdatadump(&page[0], cmd_head.data_len/2, cmd_head.addr[0]);
        }

        //finish scan tp rawdata
        icn83xx_write_reg(2, 0x0); 

    }
    else if(4 == cmd_head.wr)  //get update status
    {
        page[0] = icn83xx_get_status();
    }
    else if(6 == cmd_head.wr)  //read reg
    {   
        if(2 == cmd_head.addr_len)
        {
            ret = icn83xx_readReg((cmd_head.addr[0]<<8)|cmd_head.addr[1], &cmd_head.data[0]);
            if (ret < 0) {
                proc_error("reg reg failed! ret: %d\n", ret);
                goto read_out;
            }
            page[0] = cmd_head.data[0];
            goto read_out;   
        }
    }
read_out:
    up(&icn83xx_ts->sem);   
    proc_info("%s out: %d, cmd_head.data_len: %d\n\n",__func__, count, cmd_head.data_len); 
    return cmd_head.data_len;
}

int init_proc_node()
{
    int i;
    memset(&cmd_head, 0, sizeof(cmd_head));
    cmd_head.data = NULL;

    i = 5;
    while ((!cmd_head.data) && i)
    {
        cmd_head.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);
        if (NULL != cmd_head.data)
        {
            break;
        }
        i--;
    }
    if (i)
    {
        //DATA_LENGTH = i * DATA_LENGTH_UINT + GTP_ADDR_LENGTH;
        DATA_LENGTH = i * DATA_LENGTH_UINT;
        icn83xx_trace("alloc memory size:%d.\n", DATA_LENGTH);
    }
    else
    {
        proc_error("alloc for memory failed.\n");
        return 0;
    }

    icn83xx_proc_entry = create_proc_entry(ICN83XX_ENTRY_NAME, 0666, NULL);
    if (icn83xx_proc_entry == NULL)
    {
        proc_error("Couldn't create proc entry!\n");
        return 0;
    }
    else
    {
        icn83xx_trace("Create proc entry success!\n");
        icn83xx_proc_entry->write_proc = icn83xx_tool_write;
        icn83xx_proc_entry->read_proc = icn83xx_tool_read;
    }

    return 1;
}

void uninit_proc_node(void)
{
    kfree(cmd_head.data);
    cmd_head.data = NULL;
    remove_proc_entry(ICN83XX_ENTRY_NAME, NULL);
}
    
#endif


#if TOUCH_VIRTUAL_KEYS
static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf,
     __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":100:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":280:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":470:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":900:1030:50:60"
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


#if SUPPORT_ALLWINNER_A13

#define CTP_IRQ_NO             (gpio_int_info[0].port_num)

static void* __iomem gpio_addr = NULL;
static int gpio_int_hdle = 0;
static int gpio_wakeup_hdle = 0;
static int gpio_reset_hdle = 0;
static int gpio_wakeup_enable = 1;
static int gpio_reset_enable = 1;
static user_gpio_set_t  gpio_int_info[1];
static int screen_max_x = 0;
static int screen_max_y = 0;
static int revert_x_flag = 0;
static int revert_y_flag = 0;
static int exchange_x_y_flag = 0;

static int  int_cfg_addr[]={PIO_INT_CFG0_OFFSET,PIO_INT_CFG1_OFFSET,
            PIO_INT_CFG2_OFFSET, PIO_INT_CFG3_OFFSET};
/* Addresses to scan */
static union{
    unsigned short dirty_addr_buf[2];
    const unsigned short normal_i2c[2];
}u_i2c_addr = {{0x00},};

static __u32 twi_id = 0;

/*
 * ctp_get_pendown_state  : get the int_line data state, 
 * 
 * return value:
 *             return PRESS_DOWN: if down
 *             return FREE_UP: if up,
 *             return 0: do not need process, equal free up.
 */
static int ctp_get_pendown_state(void)
{
    unsigned int reg_val;
    static int state = FREE_UP;

    //get the input port state
    reg_val = readl(gpio_addr + PIOH_DATA);
    //pr_info("reg_val = %x\n",reg_val);
    if(!(reg_val & (1<<CTP_IRQ_NO))){
        state = PRESS_DOWN;
        icn83xx_info("pen down. \n");
    }else{ //touch panel is free up
        state = FREE_UP;
        icn83xx_info("free up. \n");
    }
    return state;
}

/**
 * ctp_clear_penirq - clear int pending
 *
 */
static void ctp_clear_penirq(void)
{
    int reg_val;
    //clear the IRQ_EINT29 interrupt pending
    //pr_info("clear pend irq pending\n");
    reg_val = readl(gpio_addr + PIO_INT_STAT_OFFSET);
    //writel(reg_val,gpio_addr + PIO_INT_STAT_OFFSET);
    //writel(reg_val&(1<<(IRQ_EINT21)),gpio_addr + PIO_INT_STAT_OFFSET);
    if((reg_val = (reg_val&(1<<(CTP_IRQ_NO))))){
        icn83xx_info("==CTP_IRQ_NO=\n");              
        writel(reg_val,gpio_addr + PIO_INT_STAT_OFFSET);
    }
    return;
}

/**
 * ctp_set_irq_mode - according sysconfig's subkey "ctp_int_port" to config int port.
 * 
 * return value: 
 *              0:      success;
 *              others: fail; 
 */
static int ctp_set_irq_mode(char *major_key , char *subkey, ext_int_mode int_mode)
{
    int ret = 0;
    __u32 reg_num = 0;
    __u32 reg_addr = 0;
    __u32 reg_val = 0;
    //config gpio to int mode
    icn83xx_trace("%s: config gpio to int mode. \n", __func__);
#ifndef SYSCONFIG_GPIO_ENABLE
#else
    if(gpio_int_hdle){
        gpio_release(gpio_int_hdle, 2);
    }
    gpio_int_hdle = gpio_request_ex(major_key, subkey);
    if(!gpio_int_hdle){
        icn83xx_error("request tp_int_port failed. \n");
        ret = -1;
        goto request_tp_int_port_failed;
    }
    gpio_get_one_pin_status(gpio_int_hdle, gpio_int_info, subkey, 1);
    icn83xx_trace("%s, %d: gpio_int_info, port = %d, port_num = %d. \n", __func__, __LINE__, \
        gpio_int_info[0].port, gpio_int_info[0].port_num);
#endif

#ifdef AW_GPIO_INT_API_ENABLE

#else
    icn83xx_trace(" INTERRUPT CONFIG\n");
    reg_num = (gpio_int_info[0].port_num)%8;
    reg_addr = (gpio_int_info[0].port_num)/8;
    reg_val = readl(gpio_addr + int_cfg_addr[reg_addr]);
    reg_val &= (~(7 << (reg_num * 4)));
    reg_val |= (int_mode << (reg_num * 4));
    writel(reg_val,gpio_addr+int_cfg_addr[reg_addr]);
                                                               
    ctp_clear_penirq();
                                                               
    reg_val = readl(gpio_addr+PIO_INT_CTRL_OFFSET); 
    reg_val |= (1 << (gpio_int_info[0].port_num));
    writel(reg_val,gpio_addr+PIO_INT_CTRL_OFFSET);

    udelay(1);
#endif

request_tp_int_port_failed:
    return ret;  
}

/**
 * ctp_set_gpio_mode - according sysconfig's subkey "ctp_io_port" to config io port.
 *
 * return value: 
 *              0:      success;
 *              others: fail; 
 */
static int ctp_set_gpio_mode(void)
{
    //int reg_val;
    int ret = 0;
    //config gpio to io mode
    icn83xx_trace("%s: config gpio to io mode. \n", __func__);
#ifndef SYSCONFIG_GPIO_ENABLE
#else
    if(gpio_int_hdle){
        gpio_release(gpio_int_hdle, 2);
    }
    gpio_int_hdle = gpio_request_ex("ctp_para", "ctp_io_port");
    if(!gpio_int_hdle){
        icn83xx_error("request ctp_io_port failed. \n");
        ret = -1;
        goto request_tp_io_port_failed;
    }
#endif
    return ret;

request_tp_io_port_failed:
    return ret;
}

/**
 * ctp_judge_int_occur - whether interrupt occur.
 *
 * return value: 
 *              0:      int occur;
 *              others: no int occur; 
 */
static int ctp_judge_int_occur(void)
{
    //int reg_val[3];
    int reg_val;
    int ret = -1;

    reg_val = readl(gpio_addr + PIO_INT_STAT_OFFSET);
    if(reg_val&(1<<(CTP_IRQ_NO))){
        ret = 0;
    }
    return ret;     
}

/**
 * ctp_free_platform_resource - corresponding with ctp_init_platform_resource
 *
 */
static void ctp_free_platform_resource(void)
{
    if(gpio_addr){
        iounmap(gpio_addr);
    }
    
    if(gpio_int_hdle){
        gpio_release(gpio_int_hdle, 2);
    }
    
    if(gpio_wakeup_hdle){
        gpio_release(gpio_wakeup_hdle, 2);
    }
    
    if(gpio_reset_hdle){
        gpio_release(gpio_reset_hdle, 2);
    }

    return;
}


/**
 * ctp_init_platform_resource - initialize platform related resource
 * return value: 0 : success
 *               -EIO :  i/o err.
 *
 */
static int ctp_init_platform_resource(void)
{
    int ret = 0;

    gpio_addr = ioremap(PIO_BASE_ADDRESS, PIO_RANGE_SIZE);
    //pr_info("%s, gpio_addr = 0x%x. \n", __func__, gpio_addr);
    if(!gpio_addr) {
        ret = -EIO;
        goto exit_ioremap_failed;   
    }
    //    gpio_wakeup_enable = 1;
    gpio_wakeup_hdle = gpio_request_ex("ctp_para", "ctp_wakeup");
    if(!gpio_wakeup_hdle) {
        icn83xx_error("%s: tp_wakeup request gpio fail!\n", __func__);
        gpio_wakeup_enable = 0;
    }

    gpio_reset_hdle = gpio_request_ex("ctp_para", "ctp_reset");
    if(!gpio_reset_hdle) {
        icn83xx_error("%s: tp_reset request gpio fail!\n", __func__);
        gpio_reset_enable = 0;
    }

    return ret;

exit_ioremap_failed:
    ctp_free_platform_resource();
    return ret;
}


/**
 * ctp_fetch_sysconfig_para - get config info from sysconfig.fex file.
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
static int ctp_fetch_sysconfig_para(void)
{
    int ret = -1;
    int ctp_used = -1;
    char name[I2C_NAME_SIZE];
    __u32 twi_addr = 0;
    //__u32 twi_id = 0;
    script_parser_value_type_t type = SCIRPT_PARSER_VALUE_TYPE_STRING;

    icn83xx_trace("%s. \n", __func__);

    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_used", &ctp_used, 1)){
        icn83xx_error("%s: script_parser_fetch err. \n", __func__);
        goto script_parser_fetch_err;
    }
    if(1 != ctp_used){
        icn83xx_error("%s: ctp_unused. \n",  __func__);
        //ret = 1;
        return ret;
    }

    if(SCRIPT_PARSER_OK != script_parser_fetch_ex("ctp_para", "ctp_name", (int *)(&name), &type, sizeof(name)/sizeof(int))){
        icn83xx_error("%s: script_parser_fetch err. \n", __func__);
        goto script_parser_fetch_err;
    }
    if(strcmp(CTP_NAME, name)){
        icn83xx_error("%s: name %s does not match CTP_NAME. \n", __func__, name);
        icn83xx_error(CTP_NAME);
        //ret = 1;
        return ret;
    }

    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_twi_addr", &twi_addr, sizeof(twi_addr)/sizeof(__u32))){
        icn83xx_error("%s: script_parser_fetch err. \n", name);
        goto script_parser_fetch_err;
    }
    //big-endian or small-endian?
    //pr_info("%s: before: ctp_twi_addr is 0x%x, dirty_addr_buf: 0x%hx. dirty_addr_buf[1]: 0x%hx \n", __func__, twi_addr, u_i2c_addr.dirty_addr_buf[0], u_i2c_addr.dirty_addr_buf[1]);
    u_i2c_addr.dirty_addr_buf[0] = twi_addr;
    u_i2c_addr.dirty_addr_buf[1] = I2C_CLIENT_END;
    icn83xx_trace("%s: after: ctp_twi_addr is 0x%x, dirty_addr_buf[0]: 0x%hx. dirty_addr_buf[1]: 0x%hx \n", __func__, twi_addr, u_i2c_addr.dirty_addr_buf[0], u_i2c_addr.dirty_addr_buf[1]);
    //pr_info("%s: after: ctp_twi_addr is 0x%x, u32_dirty_addr_buf: 0x%hx. u32_dirty_addr_buf[1]: 0x%hx \n", __func__, twi_addr, u32_dirty_addr_buf[0],u32_dirty_addr_buf[1]);

    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_twi_id", &twi_id, sizeof(twi_id)/sizeof(__u32))){
        icn83xx_error("%s: script_parser_fetch err. \n", name);
        goto script_parser_fetch_err;
    }
    icn83xx_trace("%s: ctp_twi_id is %d. \n", __func__, twi_id);
    
    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_screen_max_x", &screen_max_x, 1)){
        icn83xx_error("%s: script_parser_fetch err. \n", __func__);
        goto script_parser_fetch_err;
    }
    icn83xx_trace("%s: screen_max_x = %d. \n", __func__, screen_max_x);

    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_screen_max_y", &screen_max_y, 1)){
        icn83xx_error("%s: script_parser_fetch err. \n", __func__);
        goto script_parser_fetch_err;
    }
    icn83xx_trace("%s: screen_max_y = %d. \n", __func__, screen_max_y);

    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_revert_x_flag", &revert_x_flag, 1)){
        icn83xx_error("%s: script_parser_fetch err. \n", __func__);
        goto script_parser_fetch_err;
    }
    icn83xx_trace("%s: revert_x_flag = %d. \n", __func__, revert_x_flag);

    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_revert_y_flag", &revert_y_flag, 1)){
        icn83xx_error("%s: script_parser_fetch err. \n", __func__);
        goto script_parser_fetch_err;
    }
    icn83xx_trace("%s: revert_y_flag = %d. \n", __func__, revert_y_flag);

    if(SCRIPT_PARSER_OK != script_parser_fetch("ctp_para", "ctp_exchange_x_y_flag", &exchange_x_y_flag, 1)){
        icn83xx_error("%s: script_parser_fetch err. \n", __func__);
        goto script_parser_fetch_err;
    }
    icn83xx_trace("%s: exchange_x_y_flag = %d. \n", __func__, exchange_x_y_flag);

    return 0;

script_parser_fetch_err:
    icn83xx_trace("=========script_parser_fetch_err============\n");
    return ret;
}

/**
 * ctp_reset - function
 *
 */
static void ctp_reset(void)
{
    if(gpio_reset_enable){
        icn83xx_trace("%s. \n", __func__);
        if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 0, "ctp_reset")){
            icn83xx_error("%s: err when operate gpio. \n", __func__);
        }
        mdelay(CTP_RESET_LOW_PERIOD);
        if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_reset_hdle, 1, "ctp_reset")){
            icn83xx_error("%s: err when operate gpio. \n", __func__);
        }
        mdelay(CTP_RESET_HIGH_PERIOD);
    }
}

/**
 * ctp_wakeup - function
 *
 */
static void ctp_wakeup(void)
{
    if(1 == gpio_wakeup_enable){  
        icn83xx_trace("%s. \n", __func__);
        if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_wakeup_hdle, 0, "ctp_wakeup")){
            icn83xx_error("%s: err when operate gpio. \n", __func__);
        }
        mdelay(CTP_WAKEUP_LOW_PERIOD);
        if(EGPIO_SUCCESS != gpio_write_one_pin_value(gpio_wakeup_hdle, 1, "ctp_wakeup")){
            icn83xx_error("%s: err when operate gpio. \n", __func__);
        }
        mdelay(CTP_WAKEUP_HIGH_PERIOD);

    }
    return;
}
/**
 * ctp_detect - Device detection callback for automatic device creation
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    struct i2c_adapter *adapter = client->adapter;

    if(twi_id == adapter->nr)
    {
        icn83xx_trace("%s: Detected chip %s at adapter %d, address 0x%02x\n",
             __func__, CTP_NAME, i2c_adapter_id(adapter), client->addr);

        strlcpy(info->type, CTP_NAME, I2C_NAME_SIZE);
        return 0;
    }else{
        return -ENODEV;
    }
}
////////////////////////////////////////////////////////////////

static struct ctp_platform_ops ctp_ops = {
    .get_pendown_state                = ctp_get_pendown_state,
    .clear_penirq                     = ctp_clear_penirq,
    .set_irq_mode                     = ctp_set_irq_mode,
    .set_gpio_mode                    = ctp_set_gpio_mode,  
    .judge_int_occur                  = ctp_judge_int_occur,
    .init_platform_resource           = ctp_init_platform_resource,
    .free_platform_resource           = ctp_free_platform_resource,
    .fetch_sysconfig_para             = ctp_fetch_sysconfig_para,
    .ts_reset                         = ctp_reset,
    .ts_wakeup                        = ctp_wakeup,
    .ts_detect                        = ctp_detect,
};

#endif


/* ---------------------------------------------------------------------
*
*   Chipone panel related driver
*
*
----------------------------------------------------------------------*/
/***********************************************************************************************
Name    :   icn83xx_ts_wakeup 
Input   :   void
Output  :   ret
function    : this function is used to wakeup tp
***********************************************************************************************/
void icn83xx_ts_wakeup(void)
{
#if SUPPORT_ALLWINNER_A13
    ctp_ops.ts_wakeup();
#endif    


}

/***********************************************************************************************
Name    :   icn83xx_ts_reset 
Input   :   void
Output  :   ret
function    : this function is used to reset tp, you should not delete it
***********************************************************************************************/
void icn83xx_ts_reset(void)
{
    //set reset func
#if SUPPORT_ALLWINNER_A13
    ctp_ops.ts_reset();    
#endif

#if SUPPORT_ROCKCHIP

#endif

#if SUPPORT_SPREADTRUM
    gpio_direction_output(sprd_3rdparty_gpio_tp_rst, 1);
    gpio_set_value(sprd_3rdparty_gpio_tp_rst, 0);
    mdelay(CTP_RESET_LOW_PERIOD);
    gpio_set_value(sprd_3rdparty_gpio_tp_rst,1);
    mdelay(CTP_RESET_HIGH_PERIOD);
#endif

#if SUPPORT_AMLOGIC
		gpio_set_status(PAD_GPIOC_3, gpio_status_out);
		gpio_out(PAD_GPIOC_3, 0);
		mdelay(CTP_RESET_LOW_PERIOD);
		gpio_out(PAD_GPIOC_3, 1);
		mdelay(CTP_RESET_HIGH_PERIOD);
#endif
}
#if SUPPORT_AMLOGIC
static int icn83xx_chip_init(void)
{
		icn83xx_ts_reset();
    gpio_set_status(PAD_GPIOA_16, gpio_status_in);
    gpio_irq_set(PAD_GPIOA_16, GPIO_IRQ(INT_GPIO_0-INT_GPIO_0, GPIO_IRQ_FALLING));
      	
	return 0;
}
#endif
/***********************************************************************************************
Name    :   icn83xx_irq_disable 
Input   :   void
Output  :   ret
function    : this function is used to disable irq
***********************************************************************************************/
void icn83xx_irq_disable(void)
{
    unsigned long irqflags;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);

    spin_lock_irqsave(&icn83xx_ts->irq_lock, irqflags);
    if (!icn83xx_ts->irq_is_disable)
    {
        icn83xx_ts->irq_is_disable = 1; 
        disable_irq_nosync(icn83xx_ts->irq);
        //disable_irq(icn83xx_ts->irq);
    }
    spin_unlock_irqrestore(&icn83xx_ts->irq_lock, irqflags);
}

/***********************************************************************************************
Name    :   icn83xx_irq_enable 
Input   :   void
Output  :   ret
function    : this function is used to enable irq
***********************************************************************************************/
void icn83xx_irq_enable(void)
{
    unsigned long irqflags = 0;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);

    spin_lock_irqsave(&icn83xx_ts->irq_lock, irqflags);
    if (icn83xx_ts->irq_is_disable) 
    {
        enable_irq(icn83xx_ts->irq);
        icn83xx_ts->irq_is_disable = 0; 
    }
    spin_unlock_irqrestore(&icn83xx_ts->irq_lock, irqflags);

}

/***********************************************************************************************
Name    :   icn83xx_prog_i2c_rxdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : read data from icn83xx, prog mode 
***********************************************************************************************/
int icn83xx_prog_i2c_rxdata(unsigned short addr, char *rxdata, int length)
{
    int ret = -1;
    int retries = 0;
#if 0   
    struct i2c_msg msgs[] = {   
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN83XX_I2C_SCL,
#endif            
        },
    };
        
    icn83xx_prog_i2c_txdata(addr, NULL, 0);
    while(retries < IIC_RETRY_NUM)
    {    
        ret = i2c_transfer(this_client->adapter, msgs, 1);
        if(ret == 1)break;
        retries++;
    }
    if (retries >= IIC_RETRY_NUM)
    {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }    
#else
    unsigned char tmp_buf[2];
    struct i2c_msg msgs[] = {
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = 0,
            .len    = 2,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN83XX_I2C_SCL,
#endif            
        },
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN83XX_I2C_SCL,
#endif            
        },
    };
    tmp_buf[0] = U16HIBYTE(addr);
    tmp_buf[1] = U16LOBYTE(addr);  

    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msgs, 2);
        if(ret == 2)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }
#endif      
    return ret;
}
/***********************************************************************************************
Name    :   icn83xx_prog_i2c_txdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : send data to icn83xx , prog mode
***********************************************************************************************/
int icn83xx_prog_i2c_txdata(unsigned short addr, char *txdata, int length)
{
    int ret = -1;
    char tmp_buf[128];
    int retries = 0; 
    struct i2c_msg msg[] = {
        {
            .addr   = ICN83XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = 0,
            .len    = length + 2,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN83XX_I2C_SCL,
#endif            
        },
    };
    
    if (length > 125)
    {
        icn83xx_error("%s too big datalen = %d!\n", __func__, length);
        return -1;
    }
    
    tmp_buf[0] = U16HIBYTE(addr);
    tmp_buf[1] = U16LOBYTE(addr);

    if (length != 0 && txdata != NULL)
    {
        memcpy(&tmp_buf[2], txdata, length);
    }   
    
    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msg, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn83xx_error("%s i2c write error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }
    return ret;
}
/***********************************************************************************************
Name    :   icn83xx_prog_write_reg
Input   :   addr -- address
            para -- parameter
Output  :   
function    :   write register of icn83xx, prog mode
***********************************************************************************************/
int icn83xx_prog_write_reg(unsigned short addr, char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn83xx_prog_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        icn83xx_error("write reg failed! %#x ret: %d\n", buf[0], ret);
        return -1;
    }
    
    return ret;
}


/***********************************************************************************************
Name    :   icn83xx_prog_read_reg 
Input   :   addr
            pdata
Output  :   
function    :   read register of icn83xx, prog mode
***********************************************************************************************/
int icn83xx_prog_read_reg(unsigned short addr, char *pdata)
{
    int ret = -1;
    ret = icn83xx_prog_i2c_rxdata(addr, pdata, 1);  
    return ret;    
}

/***********************************************************************************************
Name    :   icn83xx_i2c_rxdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : read data from icn83xx, normal mode   
***********************************************************************************************/
int icn83xx_i2c_rxdata(unsigned char addr, char *rxdata, int length)
{
    int ret = -1;
    int retries = 0;
#if 0
    struct i2c_msg msgs[] = {   
        {
            .addr   = this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN83XX_I2C_SCL,
#endif            
        },
    };
        
    icn83xx_i2c_txdata(addr, NULL, 0);
    while(retries < IIC_RETRY_NUM)
    {

        ret = i2c_transfer(this_client->adapter, msgs, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }

#else
    unsigned char tmp_buf[1];
    struct i2c_msg msgs[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = 1,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN83XX_I2C_SCL,
#endif            
        },
        {
            .addr   = this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN83XX_I2C_SCL,
#endif
        },
    };
    tmp_buf[0] = addr; 
    
    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msgs, 2);
        if(ret == 2)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn83xx_error("%s i2c read error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }    
#endif

    return ret;
}
/***********************************************************************************************
Name    :   icn83xx_i2c_txdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : send data to icn83xx , normal mode
***********************************************************************************************/
int icn83xx_i2c_txdata(unsigned char addr, char *txdata, int length)
{
    int ret = -1;
    unsigned char tmp_buf[128];
    int retries = 0;

    struct i2c_msg msg[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = length + 1,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP             
            .scl_rate = ICN83XX_I2C_SCL,
#endif            
        },
    };
    
    if (length > 125)
    {
        icn83xx_error("%s too big datalen = %d!\n", __func__, length);
        return -1;
    }
    
    tmp_buf[0] = addr;

    if (length != 0 && txdata != NULL)
    {
        memcpy(&tmp_buf[1], txdata, length);
    }   
    
    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msg, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn83xx_error("%s i2c write error: %d\n", __func__, ret); 
        icn83xx_ts_reset();
    }

    return ret;
}

/***********************************************************************************************
Name    :   icn83xx_write_reg
Input   :   addr -- address
            para -- parameter
Output  :   
function    :   write register of icn83xx, normal mode
***********************************************************************************************/
int icn83xx_write_reg(unsigned char addr, char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn83xx_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        icn83xx_error("write reg failed! %#x ret: %d\n", buf[0], ret);
        return -1;
    }
    
    return ret;
}


/***********************************************************************************************
Name    :   icn83xx_read_reg 
Input   :   addr
            pdata
Output  :   
function    :   read register of icn83xx, normal mode
***********************************************************************************************/
int icn83xx_read_reg(unsigned char addr, char *pdata)
{
    int ret = -1;
    ret = icn83xx_i2c_rxdata(addr, pdata, 1);  
    return ret;    
}

#if SUPPORT_FW_UPDATE
/***********************************************************************************************
Name    :   icn83xx_log
Input   :   0: rawdata, 1: diff data
Output  :   err type
function    :   calibrate param
***********************************************************************************************/
int  icn83xx_log(char diff)
{
    char row = 0;
    char column = 0;
    int i, j;
    icn83xx_read_reg(160, &row);
    icn83xx_read_reg(161, &column);

    if(diff == 1)
    {
        icn83xx_readTP(row, column, &log_diffdata[0][0]);

        for(i=0; i<row; i++)
        {       
            for(j=0; j<column; j++)
            {
                log_diffdata[i][j] = log_diffdata[i][j] - log_rawdata[i][j];
            }
        }   
        icn83xx_rawdatadump(&log_diffdata[0][0], row*16, 16);
    }
    else
    {
        icn83xx_readTP(row, column, &log_rawdata[0][0]);    
        icn83xx_rawdatadump(&log_rawdata[0][0], row*16, 16);
    }
}
#endif

/***********************************************************************************************
Name    :   icn83xx_iic_test 
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_iic_test(void)
{
    int  ret = -1;
    char value = 0;
    int  retry = 0;
    while(retry++ < 3)
    {        
        ret = icn83xx_read_reg(0, &value);
        if(ret > 0)
        {
            return ret;
        }
        icn83xx_error("iic test error! %d\n", retry);
        msleep(3);
    }
    return ret;    
}
/***********************************************************************************************
Name    :   icn83xx_ts_release 
Input   :   void
Output  :   
function    : touch release
***********************************************************************************************/
static void icn83xx_ts_release(void)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    icn83xx_info("==icn83xx_ts_release ==\n");
    input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
    input_sync(icn83xx_ts->input_dev);
}

/***********************************************************************************************
Name    :   icn83xx_report_value_A
Input   :   void
Output  :   
function    : reprot touch ponit
***********************************************************************************************/
static void icn83xx_report_value_A(void)
{
    icn83xx_info("==icn83xx_report_value_A ==\n");
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    char buf[POINT_NUM*POINT_SIZE+3]={0};
    int ret = -1;
    int i;
#if TOUCH_VIRTUAL_KEYS
    unsigned char button;
    static unsigned char button_last;
#endif

    ret = icn83xx_i2c_rxdata(16, buf, POINT_NUM*POINT_SIZE+2);
    if (ret < 0) {
        icn83xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
        return ret;
    }
#if TOUCH_VIRTUAL_KEYS    
    button = buf[0];    
    icn83xx_info("%s: button=%d\n",__func__, button);

    if((button_last != 0) && (button == 0))
    {
        icn83xx_ts_release();
        button_last = button;
        return 1;       
    }
    if(button != 0)
    {
        switch(button)
        {
            case ICN_VIRTUAL_BUTTON_HOME:
                icn83xx_info("ICN_VIRTUAL_BUTTON_HOME down\n");
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 280);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
                input_mt_sync(icn83xx_ts->input_dev);
                input_sync(icn83xx_ts->input_dev);            
                break;
            case ICN_VIRTUAL_BUTTON_BACK:
                icn83xx_info("ICN_VIRTUAL_BUTTON_BACK down\n");
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 470);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
                input_mt_sync(icn83xx_ts->input_dev);
                input_sync(icn83xx_ts->input_dev);                
                break;
            case ICN_VIRTUAL_BUTTON_MENU:
                icn83xx_info("ICN_VIRTUAL_BUTTON_MENU down\n");
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 100);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
                input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
                input_mt_sync(icn83xx_ts->input_dev);
                input_sync(icn83xx_ts->input_dev);            
                break;                      
            default:
                icn83xx_info("other gesture\n");
                break;          
        }
        button_last = button;
        return 1;
    }        
#endif
 
    icn83xx_ts->point_num = buf[1];    
    if (icn83xx_ts->point_num == 0) {
        icn83xx_ts_release();
        return 1; 
    }   
    for(i=0;i<icn83xx_ts->point_num;i++){
        if(buf[8 + POINT_SIZE*i]  != 4) break ;
    }
    
    if(i == icn83xx_ts->point_num) {
        icn83xx_ts_release();
        return 1; 
    }   

    for(i=0; i<icn83xx_ts->point_num; i++)
    {
        icn83xx_ts->point_info[i].u8ID = buf[2 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u16PosX = (buf[3 + POINT_SIZE*i]<<8) + buf[4 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u16PosY = (buf[5 + POINT_SIZE*i]<<8) + buf[6 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u8Pressure = 200;//buf[7 + POINT_SIZE*i];
        icn83xx_ts->point_info[i].u8EventId = buf[8 + POINT_SIZE*i];    

        if(1 == icn83xx_ts->revert_x_flag)
        {
            icn83xx_ts->point_info[i].u16PosX = icn83xx_ts->screen_max_x- icn83xx_ts->point_info[i].u16PosX;
        }
        if(1 == icn83xx_ts->revert_y_flag)
        {
            icn83xx_ts->point_info[i].u16PosY = icn83xx_ts->screen_max_y- icn83xx_ts->point_info[i].u16PosY;
        }
        
        icn83xx_info("u8ID %d\n", icn83xx_ts->point_info[i].u8ID);
        icn83xx_info("u16PosX %d\n", icn83xx_ts->point_info[i].u16PosX);
        icn83xx_info("u16PosY %d\n", icn83xx_ts->point_info[i].u16PosY);
        icn83xx_info("u8Pressure %d\n", icn83xx_ts->point_info[i].u8Pressure);
        icn83xx_info("u8EventId %d\n", icn83xx_ts->point_info[i].u8EventId);  


        input_report_abs(icn83xx_ts->input_dev, ABS_MT_TRACKING_ID, icn83xx_ts->point_info[i].u8ID);    
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, icn83xx_ts->point_info[i].u8Pressure);
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, icn83xx_ts->point_info[i].u16PosX);
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, icn83xx_ts->point_info[i].u16PosY);
        input_report_abs(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
        input_mt_sync(icn83xx_ts->input_dev);
        icn83xx_point_info("point: %d ===x = %d,y = %d, press = %d ====\n",i, icn83xx_ts->point_info[i].u16PosX,icn83xx_ts->point_info[i].u16PosY, icn83xx_ts->point_info[i].u8Pressure);
    }

    input_sync(icn83xx_ts->input_dev);
  
}
/***********************************************************************************************
Name    :   icn83xx_report_value_B
Input   :   void
Output  :   
function    : reprot touch ponit
***********************************************************************************************/
#if CTP_REPORT_PROTOCOL
static void icn83xx_report_value_B(void)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    char buf[POINT_NUM*POINT_SIZE+3]={0};
    static unsigned char finger_last[POINT_NUM + 1]={0};
    unsigned char  finger_current[POINT_NUM + 1] = {0};
    unsigned int position = 0;
    int temp = 0;
    int ret = -1;
    icn83xx_info("==icn83xx_report_value_B ==\n");
    
    ret = icn83xx_i2c_rxdata(16, buf, POINT_NUM*POINT_SIZE+2);
    if (ret < 0) {
        icn83xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
        return ret;
    }

    icn83xx_ts->point_num = buf[1];    
    if(icn83xx_ts->point_num > 0)
    {
        for(position = 0; position<icn83xx_ts->point_num; position++)
        {       
            temp = buf[2 + POINT_SIZE*position] + 1;
            finger_current[temp] = 1;
            icn83xx_ts->point_info[temp].u8ID = buf[2 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u16PosX = (buf[3 + POINT_SIZE*position]<<8) + buf[4 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u16PosY = (buf[5 + POINT_SIZE*position]<<8) + buf[6 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u8Pressure = buf[7 + POINT_SIZE*position];
            icn83xx_ts->point_info[temp].u8EventId = buf[8 + POINT_SIZE*position];
            
            if(icn83xx_ts->point_info[temp].u8EventId == 4)
                finger_current[temp] = 0;
                            
            if(1 == icn83xx_ts->revert_x_flag)
            {
                icn83xx_ts->point_info[temp].u16PosX = icn83xx_ts->screen_max_x- icn83xx_ts->point_info[temp].u16PosX;
            }
            if(1 == icn83xx_ts->revert_y_flag)
            {
                icn83xx_ts->point_info[temp].u16PosY = icn83xx_ts->screen_max_y- icn83xx_ts->point_info[temp].u16PosY;
            }
            icn83xx_info("temp %d\n", temp);
            icn83xx_info("u8ID %d\n", icn83xx_ts->point_info[temp].u8ID);
            icn83xx_info("u16PosX %d\n", icn83xx_ts->point_info[temp].u16PosX);
            icn83xx_info("u16PosY %d\n", icn83xx_ts->point_info[temp].u16PosY);
            icn83xx_info("u8Pressure %d\n", icn83xx_ts->point_info[temp].u8Pressure);
            icn83xx_info("u8EventId %d\n", icn83xx_ts->point_info[temp].u8EventId);             
            //icn83xx_info("u8Pressure %d\n", icn83xx_ts->point_info[temp].u8Pressure*16);
            icn83xx_info("++++icn83xx_ts->irq%d\n",icn83xx_ts->irq); 
        }
    }   
    else
    {
        for(position = 1; position < POINT_NUM+1; position++)
        {
            finger_current[position] = 0;
        }
        icn83xx_info("no touch\n");
    }

    for(position = 1; position < POINT_NUM + 1; position++)
    {
        if((finger_current[position] == 0) && (finger_last[position] != 0))
        {
            input_mt_slot(icn83xx_ts->input_dev, position-1);
            input_mt_report_slot_state(icn83xx_ts->input_dev, MT_TOOL_FINGER, false);
            icn83xx_point_info("one touch up: %d\n", position);
        }
        else if(finger_current[position])
        {
            input_mt_slot(icn83xx_ts->input_dev, position-1);
            input_mt_report_slot_state(icn83xx_ts->input_dev, MT_TOOL_FINGER, true);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
            //input_report_abs(icn83xx_ts->input_dev, ABS_MT_PRESSURE, icn83xx_ts->point_info[position].u8Pressure);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_PRESSURE, 200);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_X, icn83xx_ts->point_info[position].u16PosX);
            input_report_abs(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, icn83xx_ts->point_info[position].u16PosY);
            icn83xx_point_info("===position: %d, x = %d,y = %d, press = %d ====\n", position, icn83xx_ts->point_info[position].u16PosX,icn83xx_ts->point_info[position].u16PosY, icn83xx_ts->point_info[position].u8Pressure);
        }

    }
    input_sync(icn83xx_ts->input_dev);

    for(position = 1; position < POINT_NUM + 1; position++)
    {
        finger_last[position] = finger_current[position];
    }
    
}
#endif

/***********************************************************************************************
Name    :   icn83xx_ts_pen_irq_work
Input   :   void
Output  :   
function    : work_struct
***********************************************************************************************/
static void icn83xx_ts_pen_irq_work(struct work_struct *work)
{
    int ret = -1;
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);  
#if SUPPORT_PROC_FS
    if(down_interruptible(&icn83xx_ts->sem))  
    {  
        return -1;   
    }  
#endif
      
    if(icn83xx_ts->work_mode == 0)
    {
#if CTP_REPORT_PROTOCOL
        icn83xx_report_value_B();
#else
        icn83xx_report_value_A();
#endif 

#if (SUPPORT_ALLWINNER_A13 == 0)  
        if(icn83xx_ts->use_irq)
        {
            icn83xx_irq_enable();
        }
#endif
    }
#if SUPPORT_FW_UPDATE    
    else if(icn83xx_ts->work_mode == 1)
    {
        printk("log raw data\n");
        icn83xx_log(0);   //raw data
    }
    else if(icn83xx_ts->work_mode == 2)
    {
        printk("log diff data\n");
        icn83xx_log(1);   //diff data
    }
#endif

#if SUPPORT_PROC_FS
    up(&icn83xx_ts->sem);
#endif


}
/***********************************************************************************************
Name    :   chipone_timer_func
Input   :   void
Output  :   
function    : Timer interrupt service routine.
***********************************************************************************************/
static enum hrtimer_restart chipone_timer_func(struct hrtimer *timer)
{
    struct icn83xx_ts_data *icn83xx_ts = container_of(timer, struct icn83xx_ts_data, timer);
    queue_work(icn83xx_ts->ts_workqueue, &icn83xx_ts->pen_event_work);

    if(icn83xx_ts->use_irq == 1)
    {
        if((icn83xx_ts->work_mode == 1) || (icn83xx_ts->work_mode == 2))
        {
            hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_POLL_TIMER/1000, (CTP_POLL_TIMER%1000)*1000000), HRTIMER_MODE_REL);
        }
    }
    else
    {
        hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_POLL_TIMER/1000, (CTP_POLL_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }
    return HRTIMER_NORESTART;
}
/***********************************************************************************************
Name    :   icn83xx_ts_interrupt
Input   :   void
Output  :   
function    : interrupt service routine
***********************************************************************************************/
static irqreturn_t icn83xx_ts_interrupt(int irq, void *dev_id)
{
    struct icn83xx_ts_data *icn83xx_ts = dev_id;
       
    icn83xx_info("==========------icn83xx_ts TS Interrupt-----============probe_flag=%d\n",probe_flag); 
    
#if SUPPORT_AMLOGIC
    if (!probe_flag)
    {
	icn83xx_info("=========probe_flag=============\n"); 
	probe_flag = 1;
	return IRQ_HANDLED;
    }
#endif

    if(icn83xx_ts->work_mode != 0)
    {
        return IRQ_HANDLED;
    }
#if SUPPORT_ALLWINNER_A13
// irq share, can not disable irq?
    if(!ctp_ops.judge_int_occur()){
        icn83xx_info("==IRQ_EINT21=\n");
        ctp_ops.clear_penirq();
        if (!work_pending(&icn83xx_ts->pen_event_work)) 
        {
            icn83xx_info("Enter work\n");
            queue_work(icn83xx_ts->ts_workqueue, &icn83xx_ts->pen_event_work);
        }
    }else{
        icn83xx_info("Other Interrupt\n");
        return IRQ_NONE;
    }
#else
    icn83xx_irq_disable();
    if (!work_pending(&icn83xx_ts->pen_event_work)) 
    {
        icn83xx_info("Enter work\n");
        queue_work(icn83xx_ts->ts_workqueue, &icn83xx_ts->pen_event_work);
    }
#endif

    return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name    :   icn83xx_ts_suspend
Input   :   void
Output  :   
function    : tp enter sleep mode
***********************************************************************************************/
static void icn83xx_ts_suspend(struct early_suspend *handler)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    icn83xx_trace("icn83xx_ts_suspend: write ICN83XX_REG_PMODE .\n");
    if (icn83xx_ts->use_irq)
    {
        icn83xx_irq_disable();
    }
    else
    {
        hrtimer_cancel(&icn83xx_ts->timer);
    }    
    icn83xx_write_reg(ICN83XX_REG_PMODE, PMODE_HIBERNATE); 

}

/***********************************************************************************************
Name    :   icn83xx_ts_resume
Input   :   void
Output  :   
function    : wakeup tp or reset tp
***********************************************************************************************/
static void icn83xx_ts_resume(struct early_suspend *handler)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(this_client);
    icn83xx_trace("==icn83xx_ts_resume== \n");
    icn83xx_ts_wakeup();
    icn83xx_ts_reset();
    if (icn83xx_ts->use_irq)
    {
        icn83xx_irq_enable();
    }
    else
    {
        hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }
    
}
#endif

/***********************************************************************************************
Name    :   icn83xx_request_io_port
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_request_io_port(struct icn83xx_ts_data *icn83xx_ts)
{
    int err;
#if SUPPORT_ALLWINNER_A13
    icn83xx_ts->screen_max_x = screen_max_x;
    icn83xx_ts->screen_max_y = screen_max_y;
    icn83xx_ts->revert_x_flag = revert_x_flag;
    icn83xx_ts->revert_y_flag = revert_y_flag;
    icn83xx_ts->exchange_x_y_flag = exchange_x_y_flag;
    icn83xx_ts->irq = CTP_IRQ_PORT;
#endif 

#if SUPPORT_ROCKCHIP
    icn83xx_ts->screen_max_x = SCREEN_MAX_X;
    icn83xx_ts->screen_max_y = SCREEN_MAX_Y;
    icn83xx_ts->irq = CTP_IRQ_PORT;
#endif
#if SUPPORT_SPREADTRUM
    icn83xx_ts->screen_max_x = SCREEN_MAX_X;
    icn83xx_ts->screen_max_y = SCREEN_MAX_Y;
    icn83xx_ts->irq = CTP_IRQ_PORT;
#endif

#if SUPPORT_AMLOGIC
    icn83xx_ts->screen_max_x = SCREEN_MAX_X;
    icn83xx_ts->screen_max_y = SCREEN_MAX_Y;
#endif
}

/***********************************************************************************************
Name    :   icn83xx_free_io_port
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_free_io_port(struct icn83xx_ts_data *icn83xx_ts)
{    
#if SUPPORT_ALLWINNER_A13    
    ctp_ops.free_platform_resource();
#endif

#if SUPPORT_ROCKCHIP

#endif

#if SUPPORT_SPREADTRUM

#endif    
}

/***********************************************************************************************
Name    :   icn83xx_request_irq
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_request_irq(struct icn83xx_ts_data *icn83xx_ts)
{
    int err = -1;
#if SUPPORT_ALLWINNER_A13

    err = ctp_ops.set_irq_mode("ctp_para", "ctp_int_port", CTP_IRQ_MODE);
    if(0 != err)
    {
        icn83xx_error("%s:ctp_ops.set_irq_mode err. \n", __func__);
        return err;
    }    
    err = request_irq(icn83xx_ts->irq, icn83xx_ts_interrupt, IRQF_TRIGGER_FALLING | IRQF_SHARED, "icn83xx_ts", icn83xx_ts);
    if (err < 0) 
    {
        icn83xx_error("icn83xx_ts_probe: request irq failed\n");
        return err;
    } 
    else
    {
        icn83xx_irq_disable();
        icn83xx_ts->use_irq = 1;        
    }
#endif

#if SUPPORT_ROCKCHIP

    err = gpio_request(icn83xx_ts->irq, "TS_INT"); //Request IO
    if (err < 0)
    {
        icn83xx_error("Failed to request GPIO:%d, ERRNO:%d\n", (int)icn83xx_ts->irq, err);
        return err;
    }
    gpio_direction_input(icn83xx_ts->irq);
    err = request_irq(icn83xx_ts->irq, icn83xx_ts_interrupt, IRQ_TYPE_EDGE_FALLING, "icn83xx_ts", icn83xx_ts);
    if (err < 0) 
    {
        icn83xx_error("icn83xx_ts_probe: request irq failed\n");
        return err;
    } 
    else
    {
        icn83xx_irq_disable();
        icn83xx_ts->use_irq = 1;        
    } 
#endif

#if SUPPORT_SPREADTRUM    
    eic_ctrl(EIC_ID_2, 1 , 1); 
    icn83xx_ts->irq = sprd_alloc_eic_irq(EIC_ID_2);
    err = request_irq(icn83xx_ts->irq, icn83xx_ts_interrupt, IRQF_TRIGGER_LOW | IRQF_DISABLED, "icn83xx_ts", icn83xx_ts);

    if (err < 0) 
    {
        icn83xx_error("icn83xx_ts_probe: request irq failed\n");
        return err;
    } 
    else
    {
        icn83xx_irq_disable();
        icn83xx_ts->use_irq = 1;        
    }    
#endif

#if	SUPPORT_AMLOGIC
		err = request_irq(icn83xx_ts->irq, icn83xx_ts_interrupt, IRQF_DISABLED, "icn83xx_ts", icn83xx_ts);    
    if (err < 0) 
    {
        icn83xx_error("icn83xx_ts_probe: request irq failed\n");
        return err;
    } 
    else
    {
        icn83xx_irq_disable();
        icn83xx_ts->use_irq = 1;        
    }
#endif
    return 0;
}


/***********************************************************************************************
Name    :   icn83xx_free_irq
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_free_irq(struct icn83xx_ts_data *icn83xx_ts)
{
    if (icn83xx_ts) 
    {
        if (icn83xx_ts->use_irq)
        {
            free_irq(icn83xx_ts->irq, icn83xx_ts);
        }
        else
        {
            hrtimer_cancel(&icn83xx_ts->timer);
        }
    } 
}

/***********************************************************************************************
Name    :   icn83xx_request_input_dev
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn83xx_request_input_dev(struct icn83xx_ts_data *icn83xx_ts)
{
    int ret = -1;    
    struct input_dev *input_dev;

    input_dev = input_allocate_device();
    if (!input_dev) {
        icn83xx_error("failed to allocate input device\n");
        return -ENOMEM;
    }
    icn83xx_ts->input_dev = input_dev;

    icn83xx_ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
#if CTP_REPORT_PROTOCOL
    __set_bit(INPUT_PROP_DIRECT, icn83xx_ts->input_dev->propbit);
    input_mt_init_slots(icn83xx_ts->input_dev, 255);
#else
 
    set_bit(ABS_MT_TOUCH_MAJOR, icn83xx_ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, icn83xx_ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, icn83xx_ts->input_dev->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, icn83xx_ts->input_dev->absbit); 
#endif
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);  
    input_set_abs_params(icn83xx_ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

    __set_bit(KEY_MENU,  input_dev->keybit);
    __set_bit(KEY_BACK,  input_dev->keybit);
    __set_bit(KEY_HOME,  input_dev->keybit);
    __set_bit(KEY_SEARCH,  input_dev->keybit);

    input_dev->name = CTP_NAME;
    ret = input_register_device(input_dev);
    if (ret) {
        icn83xx_error("Register %s input device failed\n", input_dev->name);
        input_free_device(input_dev);
        return -ENODEV;        
    }
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    icn83xx_trace("==register_early_suspend =\n");
    icn83xx_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    icn83xx_ts->early_suspend.suspend = icn83xx_ts_suspend;
    icn83xx_ts->early_suspend.resume  = icn83xx_ts_resume;
    register_early_suspend(&icn83xx_ts->early_suspend);
#endif

    return 0;
}

char FbCap[4][16]={
            {0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14},
            {0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12},
            {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},           
            {0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08},
            };  

static int icn83xx_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct icn83xx_ts_data *icn83xx_ts;
    short fwVersion = 0;
    short curVersion = 0;
    int average;
    int err = 0;
    char value;
    int retry;

    icn83xx_trace("====%s begin=====.  \n", __func__);
    
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
    {
        icn83xx_error("I2C check functionality failed.\n");
        return -ENODEV;
    }

    icn83xx_ts = kzalloc(sizeof(*icn83xx_ts), GFP_KERNEL);
    if (!icn83xx_ts)
    {
        icn83xx_error("Alloc icn83xx_ts memory failed.\n");
        return -ENOMEM;
    }
    memset(icn83xx_ts, 0, sizeof(*icn83xx_ts));

    this_client = client;
    this_client->addr = client->addr;
    i2c_set_clientdata(client, icn83xx_ts);

    icn83xx_ts->work_mode = 0;
    spin_lock_init(&icn83xx_ts->irq_lock);

    err = icn83xx_iic_test();
    if (err < 0)
    {
        icn83xx_error("icn83xx_iic_test  failed.\n");
#if SUPPORT_FW_UPDATE

    #if COMPILE_FW_WITH_DRIVER
        icn83xx_set_fw(sizeof(icn83xx_fw), &icn83xx_fw[0]);
    #endif
        if(icn83xx_check_progmod() == 0)
        {
            retry = 5;
            icn83xx_trace("in prog mode\n");  
            while(retry > 0)
            {
                if(R_OK == icn83xx_fw_update(firmware))
                {
                    break;
                }
                retry--;
                icn83xx_error("icn83xx_fw_update failed.\n");
            }
        }
        else // 
        {
            icn83xx_error("I2C communication failed.\n");
            kfree(icn83xx_ts);
            return -1;
        }
            
#endif        
    }
    else
    {
        icn83xx_trace("iic communication ok\n"); 
    }

//    icn83xx_ts->irq_lock = SPIN_LOCK_UNLOCKED;

#if SUPPORT_SPREADTRUM
    LDO_SetVoltLevel(LDO_LDO_SIM2, LDO_VOLT_LEVEL0);
    LDO_TurnOnLDO(LDO_LDO_SIM2);
    msleep(5);
    icn83xx_ts_reset();
#endif
    INIT_WORK(&icn83xx_ts->pen_event_work, icn83xx_ts_pen_irq_work);
    icn83xx_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!icn83xx_ts->ts_workqueue) {
        icn83xx_error("create_singlethread_workqueue failed.\n");
        kfree(icn83xx_ts);
        return -ESRCH;
    }

    err= icn83xx_request_input_dev(icn83xx_ts);
    if (err < 0)
    {
        icn83xx_error("request input dev failed\n");
        kfree(icn83xx_ts);
        return err;        
    }
#if	SUPPORT_AMLOGIC
		icn83xx_chip_init();
		icn83xx_ts->irq = client->irq;
#endif
    err = icn83xx_request_io_port(icn83xx_ts);
    if (err != 0)
    {
        icn83xx_error("icn83xx_request_io_port failed.\n");
        kfree(icn83xx_ts);
        return err;
    }

#if SUPPORT_FW_UPDATE  
    fwVersion = icn83xx_read_fw_Ver(firmware);
    curVersion = icn83xx_readVersion();
    icn83xx_trace("fwVersion : 0x%x\n", fwVersion); 
    icn83xx_trace("current version: 0x%x\n", curVersion);  


#if FORCE_UPDATA_FW
    retry = 5;
    while(retry > 0)
    {
        if(R_OK == icn83xx_fw_update(firmware))
        {
            break;
        }
        retry--;
        icn83xx_error("icn83xx_fw_update failed.\n");        
    }
#else
    if(fwVersion > curVersion)
    {
        retry = 5;
        while(retry > 0)
        {
            if(R_OK == icn83xx_fw_update(firmware))
            {
                break;
            }
            retry--;
            icn83xx_error("icn83xx_fw_update failed.\n");   
        }
    }
#endif

#endif

#if SUPPORT_FW_CALIB
    err = icn83xx_read_reg(0, &value);
    if(err > 0)
    {
//auto calib fw
        average = icn83xx_calib(0, NULL);
//fix FbCap
//      average = icn83xx_calib(0, FbCap[1]);
        icn83xx_trace("average : %d\n", average); 
        icn83xx_setPeakGroup(250, 150);
        icn83xx_setDownUp(400, 300);
    }
#endif

#if TOUCH_VIRTUAL_KEYS
    icn83xx_ts_virtual_keys_init();
#endif

    err = icn83xx_request_irq(icn83xx_ts);

    if (err != 0)
    {
        icn83xx_error("request irq error, use timer\n");
        icn83xx_ts->use_irq = 0;
        hrtimer_init(&icn83xx_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        icn83xx_ts->timer.function = chipone_timer_func;
        hrtimer_start(&icn83xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }
#if SUPPORT_SYSFS
    icn83xx_create_sysfs(client);
#endif

#if SUPPORT_PROC_FS
    sema_init(&icn83xx_ts->sem, 1);
    init_proc_node();
#endif

    icn83xx_irq_enable();
    icn83xx_trace("==%s over =\n", __func__);
    return 0;
}

static int __devexit icn83xx_ts_remove(struct i2c_client *client)
{
    struct icn83xx_ts_data *icn83xx_ts = i2c_get_clientdata(client);  
    icn83xx_trace("==icn83xx_ts_remove=\n");
    icn83xx_irq_disable();
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&icn83xx_ts->early_suspend);
#endif

#if SUPPORT_PROC_FS
    uninit_proc_node();
#endif

    input_unregister_device(icn83xx_ts->input_dev);
    input_free_device(icn83xx_ts->input_dev);
    cancel_work_sync(&icn83xx_ts->pen_event_work);
    destroy_workqueue(icn83xx_ts->ts_workqueue);
    icn83xx_free_irq(icn83xx_ts);
    icn83xx_free_io_port(icn83xx_ts);
    kfree(icn83xx_ts);    
    i2c_set_clientdata(client, NULL);
    return 0;
}

static const struct i2c_device_id icn83xx_ts_id[] = {
    { CTP_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, icn83xx_ts_id);

static struct i2c_driver icn83xx_ts_driver = {
    .class      = I2C_CLASS_HWMON,
    .probe      = icn83xx_ts_probe,
    .remove     = __devexit_p(icn83xx_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend    = icn83xx_ts_suspend,
    .resume     = icn83xx_ts_resume,
#endif
    .id_table   = icn83xx_ts_id,
    .driver = {
        .name   = CTP_NAME,
        .owner  = THIS_MODULE,
    },
#if SUPPORT_ALLWINNER_A13    
    .address_list   = u_i2c_addr.normal_i2c,
#endif    
};


static int __init icn83xx_ts_init(void)
{ 
    int ret = -1;
    icn83xx_trace("===========================%s=====================\n", __func__);
    
#if SUPPORT_ALLWINNER_A13
    if (ctp_ops.fetch_sysconfig_para)
    {
        if(ctp_ops.fetch_sysconfig_para()){
            pr_info("%s: err.\n", __func__);
            return -1;
        }
    }
    icn83xx_trace("%s: after fetch_sysconfig_para:  normal_i2c: 0x%hx. normal_i2c[1]: 0x%hx \n", \
    __func__, u_i2c_addr.normal_i2c[0], u_i2c_addr.normal_i2c[1]);

    ret = ctp_ops.init_platform_resource();
    if(0 != ret){
        icn83xx_error("%s:ctp_ops.init_platform_resource err. \n", __func__);    
    }
    //reset
    ctp_ops.ts_reset();
    //wakeup
    ctp_ops.ts_wakeup();      
    icn83xx_ts_driver.detect = ctp_ops.ts_detect;
#endif     

    ret = i2c_add_driver(&icn83xx_ts_driver);
    return ret;
}

static void __exit icn83xx_ts_exit(void)
{
    icn83xx_trace("==icn83xx_ts_exit==\n");
    i2c_del_driver(&icn83xx_ts_driver);
}

late_initcall(icn83xx_ts_init);
module_exit(icn83xx_ts_exit);

MODULE_AUTHOR("<zmtian@chiponeic.com>");
MODULE_DESCRIPTION("Chipone icn83xx TouchScreen driver");
MODULE_LICENSE("GPL");


/*++
 
 Copyright (c) 2012-2022 ChipOne Technology (Beijing) Co., Ltd. All Rights Reserved.
 This PROPRIETARY SOFTWARE is the property of ChipOne Technology (Beijing) Co., Ltd. 
 and may contains trade secrets and/or other confidential information of ChipOne 
 Technology (Beijing) Co., Ltd. This file shall not be disclosed to any third party,
 in whole or in part, without prior written consent of ChipOne.  
 THIS PROPRIETARY SOFTWARE & ANY RELATED DOCUMENTATION ARE PROVIDED AS IS, 
 WITH ALL FAULTS, & WITHOUT WARRANTY OF ANY KIND. CHIPONE DISCLAIMS ALL EXPRESS OR 
 IMPLIED WARRANTIES.  
 
 File Name:    flash.c
 Abstract:
               flash operation, read write etc.
 Author:       Zhimin Tian
 Date :        10 30,2012
 Version:      0.1[.revision]
 History :
     Change logs.  
 --*/
//#include "icn83xx.h"

struct file  *fp; 
int g_status = R_OK;
static char fw_mode = 0;
static int fw_size = 0;
static unsigned char *fw_buf;

void icn83xx_rawdatadump(short *mem, int size, char br)
{
    int i;
    for(i=0;i<size; i++)
    {
        if((i!=0)&&(i%br == 0))
            printk("\n");
        printk(" %5d", mem[i]);
    }
    printk("\n"); 
} 

void icn83xx_memdump(char *mem, int size)
{
    int i;
    for(i=0;i<size; i++)
    {
        if(i%16 == 0)
            printk("\n");
        printk(" 0x%2x", mem[i]);
    }
    printk("\n"); 
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
//  flash_info("icn83xx_update_status: %d\n", status);
    g_status = status;
    return 0;
}

int icn83xx_get_status(void)
{
    return  g_status;
}

void icn83xx_set_fw(int size, unsigned char *buf)
{
    fw_size = size;
    fw_buf = buf;
    
}

/***********************************************************************************************
Name    :   icn83xx_writeInfo 
Input   :   addr, value
Output  :   
function    :   write Flash Info
***********************************************************************************************/

int icn83xx_writeInfo(unsigned short addr, char value)
{
    int ret = -1;
    char temp_buf[3];
    
    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);            
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(2);
    temp_buf[0] = value;
    ret = icn83xx_i2c_txdata(232, temp_buf, 1);
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(5);
    return 0;   
}
/***********************************************************************************************
Name    :   icn83xx_readInfo 
Input   :   
Output  :   
function    :   read Flash info
***********************************************************************************************/

int icn83xx_readInfo(unsigned short addr, char *value)
{
    int ret = -1;
    char temp_buf[3];
    
    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);            
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(2);
    ret = icn83xx_i2c_rxdata(232, value, 1); 
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(2);
    return 0;   
}

/***********************************************************************************************
Name    :   icn83xx_writeReg 
Input   :   addr, value
Output  :   
function    :   write MCU xdata and reg
***********************************************************************************************/

int icn83xx_writeReg(unsigned short addr, char value)
{
    int ret = -1;
    char temp_buf[3];

    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);            
    ret = icn83xx_i2c_txdata(224, temp_buf, 2);
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(2);
    temp_buf[0] = value;
    ret = icn83xx_i2c_txdata(226, temp_buf, 1);
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(5);
    return 0;   
}
/***********************************************************************************************
Name    :   icn83xx_readReg 
Input   :   
Output  :   
function    :   read MCU xdata and reg
***********************************************************************************************/

int icn83xx_readReg(unsigned short addr, char *value)
{
    int ret = -1;
    char temp_buf[3];
    
    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);            
    ret = icn83xx_i2c_txdata(224, temp_buf, 2);
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(2);

    ret = icn83xx_i2c_rxdata(226, value, 1); 
    if (ret < 0) {
        op_error("%s failed! ret: %d\n", __func__, ret);
        return -1;
    }
    mdelay(2);
    return 0;   
}

/***********************************************************************************************
Name    :   icn83xx_open_fw 
Input   :   *fw
            
Output  :   file size
function    :   open the fw file, and return total size
***********************************************************************************************/
int  icn83xx_open_fw( char *fw)
{
    int file_size;
    mm_segment_t fs; 
    struct inode *inode = NULL; 
    if(strcmp(fw, "icn83xx_firmware") == 0)
    {
        fw_mode = 1;  //use inner array
        return fw_size;
    }
    else
    {
        fw_mode = 0; //use file in file system
    }
    
    fp = filp_open(fw, O_RDONLY, 0); 
    if (IS_ERR(fp)) { 
        flash_error("read fw file error\n"); 
        return -1; 
    } 
    else
        flash_info("open fw file ok\n"); 
        
    inode = fp->f_dentry->d_inode;
    file_size = inode->i_size;  
    flash_info("file size: %d\n", file_size); 

    fs = get_fs(); 
    set_fs(KERNEL_DS); 
    
    return  file_size;
    
}

/***********************************************************************************************
Name    :   icn83xx_read_fw 
Input   :   offset
            length, read length
            buf, return buffer
Output  :   
function    :   read data to buffer
***********************************************************************************************/
int  icn83xx_read_fw(int offset, int length, char *buf)
{
    loff_t  pos = offset;               
    if(fw_mode == 1)
    {
        memcpy(buf, fw_buf+offset, length);
    }
    else
    {                   
        vfs_read(fp, buf, length, &pos); 
    }
//  icn83xx_memdump(buf, length);
    return 0;       
}


/***********************************************************************************************
Name    :   icn83xx_close_fw 
Input   :   
Output  :   
function    :   close file
***********************************************************************************************/
int  icn83xx_close_fw(void)
{   
    if(fw_mode == 0)
    {
        filp_close(fp, NULL); 
    }
    
    return 0;
}
/***********************************************************************************************
Name    :   icn83xx_readVersion
Input   :   void
Output  :   
function    :   return version
***********************************************************************************************/
int icn83xx_readVersion(void)
{
    int err = 0;
    char tmp[2];    
    short CurVersion;
    err = icn83xx_i2c_rxdata(12, tmp, 2);
    if (err < 0) {
        calib_error("%s failed: %d\n", __func__, err); 
        return err;
    }       
    CurVersion = (tmp[0]<<8) | tmp[1];
    return CurVersion;  
}

/***********************************************************************************************
Name    :   icn83xx_changemode 
Input   :   normal/factory/config
Output  :   
function    :   change work mode
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
//  calib_info("icn83xx_changemode ok\n");
    return 0;   
}


/***********************************************************************************************
Name    :   icn83xx_readrawdata 
Input   :   rownum and length
Output  :   
function    :   read one row rawdata
***********************************************************************************************/

int icn83xx_readrawdata(char *buffer, char row, char length)
{
    int err = 0;
    int i;
//  calib_info("readrawdata: %d, length: %d\n", row, length);
    icn83xx_write_reg(3, row);
    mdelay(1);
    err = icn83xx_i2c_rxdata(160, buffer, length);
    if (err < 0) {
        calib_error("%s failed: %d\n", __func__, err); 
        return err;
    }   

    for(i=0; i<length; i=i+2)
    {
        swap_ab(buffer[i], buffer[i+1]);
    }   
    return err; 
}

/***********************************************************************************************
Name    :   icn83xx_scanTP 
Input   :   
Output  :   
function    :   scan one frame rawdata
***********************************************************************************************/

int icn83xx_scanTP(void)
{
    char value = 0;
    icn83xx_write_reg(2, 0x0); 
    mdelay(1);  
    icn83xx_read_reg(2, &value);
    while(value != 1)
    {
        mdelay(1);
        icn83xx_read_reg(2, &value);
    }
//  calib_info("icn83xx_scanTP ok\n");    
    return 0;
}

/***********************************************************************************************
Name    :   icn83xx_readTP 
Input   :   rownum and columnnum
Output  :   
function    :   read one frame rawdata
***********************************************************************************************/

int icn83xx_readTP(char row_num, char column_num, char *buffer)
{
    int err = 0;
    int i;
//  calib_info("icn83xx_readTP\n");
    icn83xx_changemode(1);  
    icn83xx_scanTP();
    for(i=0; i<row_num; i++)
    {
        icn83xx_readrawdata(&buffer[i*16*2], i, column_num*2);
    }
    icn83xx_changemode(0);  
    return err; 
}


/***********************************************************************************************
Name    :   icn83xx_goto_progmode 
Input   :   
Output  :   
function    :   change MCU to progmod
***********************************************************************************************/
int icn83xx_goto_progmode(void)
{
    int ret = -1;
    int i;
//    char value[64];
    char regValue = 0;
    
    flash_info("icn83xx_goto_progmode\n");
    
    ret = icn83xx_readReg(0x009, &regValue);
    if(ret != 0)
        return ret; 
    flash_info("[0x009]: 0x%x\n", regValue);  
        
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
    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);            
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
    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);            
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
    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);            
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
    icn83xx_ts_reset();     
    mdelay(100);        
    return 0;   
}

/***********************************************************************************************
Name    :   icn83xx_check_progmod 
Input   :   
Output  :   
function    :   check if MCU at progmode or not
***********************************************************************************************/
int icn83xx_check_progmod(void)
{
    int ret;
    unsigned char ucTemp = 0x0;
    ret = icn83xx_prog_i2c_rxdata(0x0, &ucTemp, 1);
    flash_info("icn83xx_check_progmod: 0x%x\n", ucTemp);
    if(ret < 0)
    {
        flash_error("icn83xx_check_progmod error, ret: %d\n", ret);
        return ret;
    }

    return 0;
}


/***********************************************************************************************
Name    :   icn83xx_uu 
Input   :   
Output  :   
function    :   unlock flash
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
Name    :   icn83xx_ll 
Input   :   
Output  :   
function    :   lock flash
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
Name    :   icn83xx_op1 
Input   :   
Output  :   
function    :   erase flash
***********************************************************************************************/

int  icn83xx_op1(char info, unsigned short offset, unsigned int size)
{
    int count = 0;
    unsigned char ucTemp = 0x0;
    unsigned short uiAddress = 0x0; 
    int i;
    
    icn83xx_uu();
    for(i=0; i<size; )
    {       
        uiAddress = offset + i;
//      flash_info("uiAddress: 0x%x\n", uiAddress);
        ucTemp = U16LOBYTE(uiAddress);                        
        icn83xx_prog_i2c_txdata(0x0502, &ucTemp, 1);
        ucTemp = U16HIBYTE(uiAddress);                       
        icn83xx_prog_i2c_txdata(0x0503, &ucTemp, 1);
        
        ucTemp = 0x02;                                      
        icn83xx_prog_i2c_txdata(0x0500, &ucTemp, 1);
        ucTemp = 0x01;
        count = 0;
        while(ucTemp)                                       
        {
            icn83xx_prog_i2c_rxdata(0x0501, &ucTemp, 1);
            count++;
            if(count > 5000)
            {
                flash_error("op1 ucTemp: 0x%x\n", ucTemp);
                return 1;
            }
        }
        i = i+1024;
    }
    icn83xx_ll();
    return 0;
}

/***********************************************************************************************
Name    :   icn83xx_op2 
Input   :   
Output  :   
function    :   progm flash
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
    
    ucTemp = U16LOBYTE(offset);                           
    icn83xx_prog_i2c_txdata(0x0502, &ucTemp, 1);
    ucTemp = U16HIBYTE(offset); 
    icn83xx_prog_i2c_txdata(0x0503, &ucTemp, 1);
    
    icn83xx_prog_i2c_txdata(0x0504, &uiAddress, 2);


//ensure size is even
    if(size%2 != 0)
    {
        flash_info("write op size: %d\n", size);
        flash_size = size+1;
    }
    else
        flash_size = size;
    
    ucTemp = U16LOBYTE(flash_size);                               
    icn83xx_prog_i2c_txdata(0x0506, &ucTemp, 1);
    ucTemp = U16HIBYTE(flash_size);                               
    icn83xx_prog_i2c_txdata(0x0507, &ucTemp, 1);
    ucTemp = 0x01;

    if(info > 0)
       ucTemp = 0x01 | (1<<3);                          

    icn83xx_prog_i2c_txdata(0x0500, &ucTemp, 1);    //
    while(ucTemp)                                       
    {
        icn83xx_prog_i2c_rxdata(0x0501, &ucTemp, 1);
        count++;
        if(count > 5000)
        {
            flash_error("op2 ucTemp: 0x%x\n", ucTemp);
            return 1;
        }       
        
    }
    icn83xx_ll();
    return 0;   
}

/***********************************************************************************************
Name    :   icn83xx_op3 
Input   :   
Output  :   
function    :   read flash
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
    ucTemp = U16LOBYTE(offset);                      
    icn83xx_prog_i2c_txdata(0x0502, &ucTemp, 1);
    ucTemp = U16HIBYTE(offset);                       
    icn83xx_prog_i2c_txdata(0x0503, &ucTemp, 1);

    icn83xx_prog_i2c_txdata(0x0504, (unsigned char*)&uiAddress, 2);

//ensure size is even
    if(size%2 != 0)
    {
        flash_info("read op size: %d\n", size);
        flash_size = size+1;
    }
    else
        flash_size = size;
    
    ucTemp = U16LOBYTE(flash_size);                           
    icn83xx_prog_i2c_txdata(0x0506, &ucTemp, 1);
    
    ucTemp = U16HIBYTE(flash_size);                           
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
            flash_error("op3 ucTemp: 0x%x\n", ucTemp);
            return 1;
        }       
                
    }
    icn83xx_ll();
    icn83xx_prog_i2c_rxdata(uiAddress, buffer, size);
    return 0;   
}


/***********************************************************************************************
Name    :   icn83xx_goto_nomalmode 
Input   :   
Output  :   
function    :   when prog flash ok, change flash info flag
***********************************************************************************************/
int icn83xx_goto_nomalmode(void)
{
    int ret = -1;
    unsigned short addr = 0;
    char temp_buf[3];

    flash_info("icn83xx_goto_nomalmode\n");
    temp_buf[0] = 0x03; 
    icn83xx_prog_i2c_txdata(0x0f00, temp_buf, 1);
    
    msleep(100);
/*  
    addr = 0;
    temp_buf[0] = U16HIBYTE(addr);
    temp_buf[1] = U16LOBYTE(addr);    
    temp_buf[2] = 0;        
    ret = icn83xx_i2c_txdata(230, temp_buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! ret: %d\n", ret);
        return -1;
    }
    
    icn83xx_i2c_rxdata(232, &temp_buf[2], 1);   
    flash_info("temp_buf[2]: 0x%x\n", temp_buf[2]);
*/
    ret = icn83xx_readInfo(0, &temp_buf[2]);
    if(ret != 0)
        return ret;
    flash_info("temp_buf[2]: 0x%x\n", temp_buf[2]);
    if(temp_buf[2] == 0xff)
    {
/*      
        addr = 0;
        temp_buf[0] = U16HIBYTE(addr);
        temp_buf[1] = U16LOBYTE(addr);    
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
Name    :   icn83xx_read_fw_Ver 
Input   :   fw
Output  :   
function    :   read fw version
***********************************************************************************************/

short  icn83xx_read_fw_Ver(char *fw)
{
    short FWversion;
    char tmp[2];
    int file_size;
    file_size = icn83xx_open_fw(fw);
    if(file_size < 0)
    {
        return -1;  
    }   
    icn83xx_read_fw(0x4000, 2, &tmp[0]);
    
    icn83xx_close_fw();
    FWversion = (tmp[0]<<8)|tmp[1];
//  flash_info("FWversion: 0x%x\n", FWversion);
    return FWversion;
}




/***********************************************************************************************
Name    :   icn83xx_fw_update 
Input   :   fw
Output  :   
function    :   upgrade fw
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

    file_size = icn83xx_open_fw(fw);
    if(file_size < 0)
    {
        icn83xx_update_status(R_FILE_ERR);
        return R_FILE_ERR;  
    }
    
    if(icn83xx_goto_progmode() != 0)
    {
        if(icn83xx_check_progmod() < 0)
        {
            icn83xx_update_status(R_STATE_ERR);
            icn83xx_close_fw();
            return R_STATE_ERR;
        }   
    }
//  msleep(50);

    if(icn83xx_op1(0, 0, file_size) != 0)
    {
        flash_error("icn83xx_op1 error\n");
        icn83xx_update_status(R_ERASE_ERR);
        icn83xx_close_fw();
        return R_ERASE_ERR;
    }
    icn83xx_update_status(5);
    
    num = file_size/B_SIZE;
    for(j=0; j < num; j++)
    {
        icn83xx_read_fw(j*B_SIZE, B_SIZE, temp_buf);
        
//      icn83xx_op3(0, j*B_SIZE, temp_buf1, B_SIZE);
//      icn83xx_memdump(temp_buf1, B_SIZE);
        
        if(icn83xx_op2(0, j*B_SIZE, temp_buf, B_SIZE) != 0)
        {
            icn83xx_update_status(R_PROGRAM_ERR);
            icn83xx_close_fw();
            return R_PROGRAM_ERR;
        }
        checksum_bak = icn83xx_checksum(checksum_bak, temp_buf, B_SIZE);
        
        icn83xx_update_status(5+(int)(60*j/num));
    }
    last_length = file_size - B_SIZE*j;
    if(last_length > 0)
    {
        icn83xx_read_fw(j*B_SIZE, last_length, temp_buf);
        
//      icn83xx_op3(0, j*B_SIZE, temp_buf1, B_SIZE);
//      icn83xx_memdump(temp_buf1, B_SIZE);     
        
        if(icn83xx_op2(0, j*B_SIZE, temp_buf, last_length) != 0)
        {
            icn83xx_update_status(R_PROGRAM_ERR);
            icn83xx_close_fw();
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
            flash_error("cmp error, %d\n", j);
            icn83xx_memdump(temp_buf1, B_SIZE);
            icn83xx_memdump(temp_buf, B_SIZE);  
            icn83xx_update_status(R_VERIFY_ERR);
#ifdef ENABLE_BYTE_CHECK            
            icn83xx_close_fw();
#endif          
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
            flash_error("cmp error, %d\n", j);
            icn83xx_memdump(temp_buf1, last_length);    
            icn83xx_memdump(temp_buf, last_length); 
            icn83xx_update_status(R_VERIFY_ERR);
#ifdef ENABLE_BYTE_CHECK            
            icn83xx_close_fw();
#endif              
            return R_VERIFY_ERR;                            
            //while(1);
        }
#endif      

    }

#ifdef ENABLE_BYTE_CHECK    
    icn83xx_close_fw();
#endif      
    
    flash_info("checksum_bak: 0x%x, checksum: 0x%x\n", checksum_bak, checksum);
    if(checksum_bak != checksum)
    {
        flash_error("upgrade checksum error\n");
        icn83xx_update_status(R_VERIFY_ERR);
        return R_VERIFY_ERR;
    }

    if(icn83xx_goto_nomalmode() != 0)
    {
        flash_error("icn83xx_goto_nomalmode error\n");
        icn83xx_update_status(R_STATE_ERR);
        return R_STATE_ERR;
    }
    
    icn83xx_update_status(R_OK);
    flash_info("upgrade ok\n");
    return R_OK;
}
