/*****************************************************************
**
**  Copyright (C) 2009 Amlogic,Inc.
**  All rights reserved
**        Filename : itefrontend.c
**
**  comment:
**        Driver for ITE9173 demodulator
**  author :
**	    Shijie.Rong@amlogic
**  version :
**	    v1.0	 13/10/30
*****************************************************************/

/*
    Driver for Ite9173 demodulator
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
//#include <linux/gpio.h>
#include <asm/gpio.h>

#include "IT9173.h"
#include "itefrontend.h"

#include "../aml_fe.h"

#if 1
#define pr_dbg(args...) printk("ITE: " args)
#else
#define pr_dbg(args...)
#endif

#define pr_error(args...) printk("ITE: " args)


extern StreamType streamType;
extern DefaultDemodulator demod;

static struct mutex ite_lock;
static struct aml_fe ite9173_fe[FE_DEV_COUNT];
static Demodulator *pdemod = &demod;


static int ite9173_read_status(struct dvb_frontend *fe, fe_status_t * status)
{

	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	Dword ret;
	Bool locked = 0;

//	pr_dbg("ite9173_read_status\n");

	mutex_lock(&ite_lock);
	ret = Demodulator_isLocked(pdemod,&locked);
	printk("DVB: lock status is %d\n",locked);
	mutex_unlock(&ite_lock);

	if(locked==1) {
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	} else {
		*status = FE_TIMEDOUT;
	}

//	pr_dbg("ite9173_read_status--\n");
	return  0;

}

static int ite9173_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	Dword ret = 0;
	OUT Dword postErrorCount = 0;  /** 24 bits */
	OUT Dword postBitCount = 0;    /** 16 bits */
	OUT Word abortCount = 0;
	pr_dbg("ite9173_read_ber\n");

	mutex_lock(&ite_lock);
	ret = Demodulator_getPostVitBer(pdemod,&postErrorCount,&postBitCount,&abortCount);
	mutex_unlock(&ite_lock);

	*ber = (postErrorCount /postBitCount );

	if(Error_NO_ERROR != ret)
		return -1;

	pr_dbg("ite9173_read_ber--\n");


	return 0;
}

static int ite9173_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;
	Dword ret = 0;
	pr_dbg("ite9173_read_signal_strength\n");

	*strength = 0;
	mutex_lock(&ite_lock);
	ret = Demodulator_getSignalStrengthDbm(pdemod,(Long*)strength); 
	mutex_unlock(&ite_lock);

	if(Error_NO_ERROR != ret)
		return -1;

	pr_dbg("ite9173_read_signal_strength--\n");

	return 0;

}

static int ite9173_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;
	Dword ret = 0;

	pr_dbg("ite9173_read_snr\n");

	*snr = 0;
	mutex_lock(&ite_lock);
	ret = Demodulator_getSNR(pdemod,(Byte*)snr);
	mutex_unlock(&ite_lock);

	if(Error_NO_ERROR != ret)
		return -1;

	pr_dbg("ite9173_read_snr--\n");

	return 0;
}

static int ite9173_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	*ucblocks=0;
	return 0;
}

static int ite9173_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	pr_dbg("ite9173_set_frontend\n");
	Dword ret;
	Word bandwidth=8;
	int freq;

	bandwidth=p->u.ofdm.bandwidth;
	if(bandwidth==0)
		bandwidth=8;
	else if(bandwidth==1)
		bandwidth=7;
	else if(bandwidth==2)
		bandwidth=6;
	else
		bandwidth=8;	

	freq=p->frequency/1000;

	pr_dbg("p->frequency ==== %d \n", p->frequency);
	if(freq>0&&freq!=-1) {
		mutex_lock(&ite_lock);
		ret = Demodulator_acquireChannel(pdemod, bandwidth*1000,freq);
		mutex_unlock(&ite_lock);
	}else
		printk("\n--[xsw]: Invalidate Fre!!!!!!!!!!!!!--\n");
	afe->params = *p;
	pr_dbg("ite9173_set_frontend--\n");
	return  0;
}

static int ite9173_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	struct aml_fe *afe = fe->demodulator_priv;
	
	*p = afe->params;
	return 0;
}


#if 1

static int ite9173_set_property(struct dvb_frontend *fe, struct dtv_property *p)
{
	struct aml_fe *afe = fe->demodulator_priv;
	unsigned long value = 0;
	unsigned char layer_id=Layer_A;
	layer_id=p->u.data;
	pr_dbg("[ite9173_set_property]plp_id is %d,cmd is %d\n",layer_id,p->cmd);
	switch (p->cmd) {
		case DTV_ISDBT_LAYER_ENABLED:
			if(layer_id==Layer_A_B_C){
				value = Standard_setOneSeg(pdemod,Layer_A_B_C);
					if(Error_NO_ERROR != value){
					    printk("Standard_setOneSeg error = %d!!!\r\n",value);
						value = -EINVAL;
					}
						msleep(500);
						
				printk("[ite9173]layerA_B_C in\n");
			}
			else if(layer_id==Layer_A){
				value = Standard_setLayerFilter (pdemod,1,0,0);			
					if(Error_NO_ERROR != value){
					    printk("Standard_setOneSeg error = %d!!!\r\n",value);
						value = -EINVAL;
					}
						msleep(500);
						
				printk("[ite9173]layerA in\n");
			}
			else if(layer_id==Layer_B){
					value = Standard_setLayerFilter (pdemod,0,1,0);			
					if(Error_NO_ERROR != value){
					    printk("Standard_setOneSeg error = %d!!!\r\n",value);
						value = -EINVAL;
					}
						msleep(500);
						
				printk("[ite9173]layerB in\n");

			}
			else if(layer_id==Layer_C){
					value = Standard_setLayerFilter (pdemod,0,0,1);			
					if(Error_NO_ERROR != value){
					    printk("Standard_setOneSeg error = %d!!!\r\n",value);
						value = -EINVAL;
					}
						msleep(500);
						
				printk("[ite9173]layerC in\n");

			}
			else{
				printk("[ite9173]no that layer \n");
			}
		
			break;
		default:
			value = EOPNOTSUPP;
			break;
	}

	return value;

}

static int ite9173_get_property(struct dvb_frontend *fe, struct dtv_property *p)
{
	pr_dbg("[ite9173_get_property]cmd is %d\n",p->cmd);

}
#endif


static int ite9173_fe_get_ops(struct aml_fe_dev *dev, int mode, void *ops)
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
			FE_CAN_MUTE_TS |
			FE_CAN_3_LAYER;

	fe_ops->set_frontend = ite9173_set_frontend;
	fe_ops->get_frontend = ite9173_get_frontend;	
	fe_ops->read_status = ite9173_read_status;
	fe_ops->read_ber = ite9173_read_ber;
	fe_ops->read_signal_strength = ite9173_read_signal_strength;
	fe_ops->read_snr = ite9173_read_snr;
	fe_ops->read_ucblocks = ite9173_read_ucblocks;
	fe_ops->set_property = ite9173_set_property,
	fe_ops->get_property = ite9173_get_property,

	fe_ops->asyncinfo.set_frontend_asyncenable = 1;
	
	return 0;
}

static int ite9173_fe_enter_mode(struct aml_fe *fe, int mode)
{
	struct aml_fe_dev *dev = fe->dtv_demod;

	pr_dbg("=========================demod init\r\n");
	gpio_out(dev->reset_gpio, 0);
	msleep(300);
	gpio_out(dev->reset_gpio, 1);
	msleep(200);

	gpio_out(dev->tuner_power_gpio, 1);
	msleep(200);
	
	if(Error_NO_ERROR != Demodulator_initialize (pdemod, streamType))
		return -1;

	return 0;
}

static int ite9173_fe_resume(struct aml_fe_dev *dev)
{
	printk("ite9173_fe_resume\n");
	gpio_out(dev->reset_gpio, 0);
	msleep(300);
	gpio_out(dev->reset_gpio, 1);
	msleep(200);

	gpio_out(dev->tuner_power_gpio, 1);
	msleep(200);
	
	if(Error_NO_ERROR != Demodulator_initialize (pdemod, streamType))
			return -1;
	
	return 0;

}

static int ite9173_fe_suspend(struct aml_fe_dev *dev)
{
	return 0;
}

static struct aml_fe_drv ite9173_dtv_demod_drv = {
.id         = AM_DTV_DEMOD_ITE9173,
.name       = "Ite9173",
.capability = AM_FE_ISDBT,
.get_ops    = ite9173_fe_get_ops,
.enter_mode = ite9173_fe_enter_mode,
.suspend    = ite9173_fe_suspend,
.resume     = ite9173_fe_resume
};

static int __init itefrontend_init(void)
{
	pr_dbg("register ite9173 demod driver\n");
	mutex_init(&ite_lock);
	return aml_register_fe_drv(AM_DEV_DTV_DEMOD, &ite9173_dtv_demod_drv);
}


static void __exit itefrontend_exit(void)
{
	pr_dbg("unregister ite9173 demod driver\n");
	mutex_destroy(&ite_lock);
	aml_unregister_fe_drv(AM_DEV_DTV_DEMOD, &ite9173_dtv_demod_drv);
}

fs_initcall(itefrontend_init);
module_exit(itefrontend_exit);


MODULE_DESCRIPTION("ite9173 ISDBT Demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");


