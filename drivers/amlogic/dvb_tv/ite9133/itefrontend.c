/*****************************************************************
**
**  Copyright (C) 2009 Amlogic,Inc.
**  All rights reserved
**        Filename : itefrontend.c
**
**  comment:
**        Driver for ITE9133 demodulator
**  author :
**	    Shijie.Rong@amlogic
**  version :
**	    v1.0	 13/10/25
*****************************************************************/

/*
    Driver for ITE9133 demodulator
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#ifdef ARC_700
#include <asm/arch/am_regs.h>
#else
#include <mach/am_regs.h>
#endif
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "IT9133.h"
#include "itefrontend.h"

#include "../aml_fe.h"

#if 1
#define pr_dbg(args...) printk("MXL: " args)
#else
#define pr_dbg(args...)
#endif

#define pr_error(args...) printk("MXL: " args)

static struct mutex ite_lock;
static struct aml_fe ite9133_fe[FE_DEV_COUNT];


StreamType streamType = StreamType_DVBT_SERIAL;//StreamType_DVBT_SERIAL;//StreamType_DVBT_PARALLEL;//StreamType_DVBT_SERIAL;//Modified by Roan 2012-03-14

DefaultDemodulator demod = {
	NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    12000,
    2025000,
//20480,
//2048000,
    StreamType_DVBT_SERIAL,//StreamType_DVBT_PARALLEL,//StreamType_DVBT_SERIAL,//Modified by Roan 2012-03-14
    8000,
    642000,
    0x00000000,
	{False, False, 0, 0},
    0,
    False,
    False,
	0,
	User_I2C_ADDRESS,
	False
};


static Demodulator *pdemod = &demod;


static int ite9133_read_status(struct dvb_frontend *fe, fe_status_t * status)
{

	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	Dword ret;
	Bool locked = 0;

	pr_dbg("ite9133_read_status\n");

	mutex_lock(&ite_lock);
	ret = Demodulator_isLocked(pdemod,&locked);
	printk("DVB: lock status is %d\n",locked);
	mutex_unlock(&ite_lock);

	if(locked==1) {
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	} else {
		*status = FE_TIMEDOUT;
	}

	pr_dbg("ite9133_read_status--\n");
	return  0;

}

static int ite9133_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	pr_dbg("ite9133_read_ber\n");
	return 0;
}

static int ite9133_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	pr_dbg("ite9133_read_signal_strength\n");
	return 0;
}

static int ite9133_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;
	pr_dbg("ite9133_read_snr\n");

	mutex_lock(&ite_lock);
	if(Error_NO_ERROR != Demodulator_getSNR(pdemod,(Byte*)snr))
;
	mutex_unlock(&ite_lock);
		//return -1;

	pr_dbg("ite9133_read_snr--\n");
	return 0;
}

static int ite9133_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	*ucblocks=0;
	return 0;
}

static int ite9133_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	pr_dbg("ite9133_set_frontend\n");
	Dword ret;
	Word bandwidth=8;

	bandwidth=p->u.ofdm.bandwidth;
	if(bandwidth==0)
		bandwidth=8;
	else if(bandwidth==1)
		bandwidth=7;
	else if(bandwidth==2)
		bandwidth=6;
	else
		bandwidth=8;	

	p->frequency/=1000;

	pr_dbg("p->frequency ==== %d \n", p->frequency);
	if(p->frequency>0&&p->frequency!=-1) {
		mutex_lock(&ite_lock);
		ret = Demodulator_acquireChannel(pdemod, bandwidth*1000,p->frequency);
		mutex_unlock(&ite_lock);
	}else
		printk("\n--[xsw]: Invalidate Fre!!!!!!!!!!!!!--\n");
	afe->params = *p;
	pr_dbg("ite9133_set_frontend--\n");
	return  0;
}

static int ite9133_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	struct aml_fe *afe = fe->demodulator_priv;
	
	*p = afe->params;
	return 0;
}

static int ite9133_fe_get_ops(struct aml_fe_dev *dev, int mode, void *ops)
{
	struct dvb_frontend_ops *fe_ops = (struct dvb_frontend_ops*)ops;

	fe_ops->info.frequency_min = 51000000;
	fe_ops->info.frequency_max = 858000000;
	fe_ops->info.frequency_stepsize = 0;
	fe_ops->info.frequency_tolerance = 0;
	fe_ops->info.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_QAM_16 |
			FE_CAN_QAM_64 | FE_CAN_QAM_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO |
			FE_CAN_HIERARCHY_AUTO |
			FE_CAN_RECOVER |
			FE_CAN_MUTE_TS;

	fe_ops->set_frontend = ite9133_set_frontend;
	fe_ops->get_frontend = ite9133_get_frontend;	
	fe_ops->read_status = ite9133_read_status;
	fe_ops->read_ber = ite9133_read_ber;
	fe_ops->read_signal_strength = ite9133_read_signal_strength;
	fe_ops->read_snr = ite9133_read_snr;
	fe_ops->read_ucblocks = ite9133_read_ucblocks;

	fe_ops->asyncinfo.set_frontend_asyncenable = 1;
	
	return 0;
}

static int ite9133_fe_enter_mode(struct aml_fe *fe, int mode)
{
	struct aml_fe_dev *dev = fe->dtv_demod;

	pr_dbg("=========================demod init\r\n");
	gpio_direction_output(dev->reset_gpio, dev->reset_value);
	msleep(300);
	gpio_direction_output(dev->reset_gpio, !dev->reset_value); //enable tuner power
	msleep(200);
	
	if(Error_NO_ERROR != Demodulator_initialize (pdemod, streamType))
		return -1;

	return 0;
}

static int ite9133_fe_resume(struct aml_fe_dev *dev)
{
	printk("ite9133_fe_resume\n");
	gpio_direction_output(dev->reset_gpio, dev->reset_value);
	msleep(300);
	gpio_direction_output(dev->reset_gpio, !dev->reset_value); //enable tuner power
	msleep(200);
	
	if(Error_NO_ERROR != Demodulator_initialize (pdemod, streamType))
			return -1;
	
	return 0;

}

static int ite9133_fe_suspend(struct aml_fe_dev *dev)
{
	return 0;
}

static struct aml_fe_drv ite9133_dtv_demod_drv = {
.id         = AM_DTV_DEMOD_ITE9133,
.name       = "Ite9133",
.capability = AM_FE_OFDM,
.get_ops    = ite9133_fe_get_ops,
.enter_mode = ite9133_fe_enter_mode,
.suspend    = ite9133_fe_suspend,
.resume     = ite9133_fe_resume
};

static int __init itefrontend_init(void)
{
	pr_dbg("register ite9133 demod driver\n");
	mutex_init(&ite_lock);
	return aml_register_fe_drv(AM_DEV_DTV_DEMOD, &ite9133_dtv_demod_drv);
}


static void __exit itefrontend_exit(void)
{
	pr_dbg("unregister ite9133 demod driver\n");
	mutex_destroy(&ite_lock);
	aml_unregister_fe_drv(AM_DEV_DTV_DEMOD, &ite9133_dtv_demod_drv);
}

fs_initcall(itefrontend_init);
module_exit(itefrontend_exit);


MODULE_DESCRIPTION("ite9133 DVB-T Demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");


