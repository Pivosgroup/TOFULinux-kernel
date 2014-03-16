/*****************************************************************
**
**  Copyright (C) 2009 Amlogic,Inc.
**  All rights reserved
**        Filename : avlfrontend.c
**
**  comment:
**        Driver for Si2168 demodulator
**  author :
**	    Shijie.Rong@amlogic
**  version :
**	    v1.0	 12/3/30
*****************************************************************/

/*
    Driver for Si2168 demodulator
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
#include "sifrontend.h"
#include "../aml_fe.h"
	
#if 1
#define pr_dbg(args...) printk("SI: " args)
#else
#define pr_dbg(args...)
#endif
	
#define pr_error(args...) printk("SI: " args)

/* define how many front-ends will be used */
#ifndef   FRONT_END_COUNT
  #define FRONT_END_COUNT  1
#endif /* FRONT_END_COUNT */

SILABS_FE_Context   FrontEnd_Table[FRONT_END_COUNT];
int   fe1;
SILABS_FE_Context    front_end;
CUSTOM_Status_Struct  FE_Status;
CUSTOM_Status_Struct custom_status;
#ifndef   TUNER_ADDRESS_SAT
 #define  TUNER_ADDRESS_SAT 0
#endif /* TUNER_ADDRESS_SAT */

#ifndef   TUNER_ADDRESS_TER
 #define  TUNER_ADDRESS_TER 0
#endif /* TUNER_ADDRESS_TER */


int si2168_get_fe_config(struct si2168_fe_config *cfg){
	cfg->demod_addr=cfg->demod_addr;
	cfg->i2c_adapter=cfg->i2c_adapter;
	printk("cfg->demod_addr is %x,cfg->i2c_adapter is %x\n",cfg->demod_addr,cfg->i2c_adapter);
}

	
static int si2168_read_status(struct dvb_frontend *fe, fe_status_t * status)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;
	unsigned char s=0;
	SiLabs_API_Demod_status(&front_end, &custom_status);
	printk("lock status is %x\n",custom_status.fec_lock);
	s=custom_status.fec_lock;
	if(s==1)
	{
		*status = FE_HAS_LOCK|FE_HAS_SIGNAL|FE_HAS_CARRIER|FE_HAS_VITERBI|FE_HAS_SYNC;
	}
	else
	{
		*status = FE_TIMEDOUT;
	}
	
	return  0;
}

static int si2168_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	SiLabs_API_Demod_status(&front_end, &custom_status);
	*ber=custom_status.ber;
	return 0;
}

static int si2168_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	*strength=NMI120_GetRSSI(1);
	return 0;
}

static int si2168_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	SiLabs_API_Demod_status(&front_end, &custom_status);
	*snr=custom_status.c_n;
	return 0;
}

static int si2168_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	*ucblocks=0;
	return 0;
}

static int si2168_set_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{
	struct aml_fe *afe = fe->demodulator_priv;
	struct aml_fe_dev *dev = afe->dtv_demod;

	pr_dbg("Si2168_set_frontend\n");
	int                  lock;
    int                  freq;
    int                  bandwidth_Hz;
    unsigned int         symbol_rate_bps;
    CUSTOM_Constel_Enum  constellation;
    CUSTOM_Stream_Enum   stream;
    int                  standard;
    int                  polarization;
    int                  band;
    int                  plp_id;
	int					 i;
	freq=p->frequency;
	unsigned char bandwidth=8;
	bandwidth=p->u.ofdm.bandwidth;
	if(bandwidth==0)
		bandwidth=8;
	else if(bandwidth==1)
		bandwidth=7;
	else if(bandwidth==2)
		bandwidth=6;
	else
		bandwidth=8;	
	if((50000000>p->frequency)||(p->frequency>900000000))
	{
			p->frequency =474000000;
			pr_dbg("freq is out of range,force to set 474000khz\n");
	}

//	MDrv_Tuner_SetTuner(freq,8);
	printk("tuner set ok\n");
	if(p->u.ofdm.ofdm_mode==OFDM_DVBT){
		standard=SILABS_DVB_T;
		printk("SILABS_DVB_T\n");
	}else if(p->u.ofdm.ofdm_mode==OFDM_DVBT2){
		standard=SILABS_DVB_T2;
		printk("SILABS_DVB_T2\n");
	}else{
		standard=SILABS_DVB_T2;
		printk("SILABS_DVB_T2\n");
	}
	bandwidth_Hz=bandwidth*1000000;
	stream=0;
	symbol_rate_bps=0;
	constellation=0;
	polarization=0;
	band=0;
	plp_id=0;
	 /* Call SiLabs_API_switch_to_standard, in case the standard is different or the init has not been done yet */
	// if (SiLabs_API_switch_to_standard		(&front_end, standard, 1) ==0) return 0;
	SiLabs_API_Tuner_I2C_Enable(&front_end);
	MDrv_Tuner_SetTuner(freq,bandwidth);
	NMI120_GetRSSI(1);	
	SiLabs_API_Tuner_I2C_Disable(&front_end);
  	printk("now to lock carrier\n");
	SiLabs_API_lock_to_carrier (&front_end, standard, freq, bandwidth_Hz, stream, symbol_rate_bps, constellation, polarization, band, plp_id);
	afe->params = *p;//these data will be writed to eeprom
	for(i=0;i<50;i++){
		SiLabs_API_Demod_status(&front_end, &custom_status);
		if(1==custom_status.fec_lock){
			printk("si2168 lock success\n");
			break;
		}
		msleep(20);
	}
	
	pr_dbg("per is %d,c_n is %d,fec_lock is %d\n",custom_status.per,custom_status.c_n,custom_status.fec_lock);
	pr_dbg("si2168=>frequency=%d\r\n",p->frequency);
	return  0;

}

static int si2168_get_frontend(struct dvb_frontend *fe, struct dvb_frontend_parameters *p)
{//these content will be writed into eeprom .

	struct aml_fe *afe = fe->demodulator_priv;
	*p = afe->params;
	return 0;
}
#if 1
static int si2168_set_property(struct dvb_frontend *fe, struct dtv_property *p)
{
	struct aml_fe *afe = fe->demodulator_priv;
	int r = 0;
	int plp_id=0;
	plp_id=p->u.data;
	pr_dbg("[SI2168_set_property]plp_id is %d,cmd is %d\n",plp_id,p->cmd);
	switch (p->cmd) {
		case DTV_DVBT2_PLP_ID:
			SiLabs_API_Select_PLP(&front_end,plp_id);
			if (SiLabs_API_Select_PLP(&front_end,plp_id) != 0) {
				r = -EINVAL;
			}
			break;
		default:
			r = EOPNOTSUPP;
			break;
	}

	return r;

}

static int si2168_get_property(struct dvb_frontend *fe, struct dtv_property *p)
{
	struct aml_fe *afe = fe->demodulator_priv;
	int r = 0;
	int lock;
	  int standard;
	  int freq;
	  int bandwidth_Hz;
	  int stream;
	  unsigned int symbol_rate_bps;
	  int constellation;
	  int polarization;
	  int band;
	  int num_plp;
	  int carrier_index;
	  int i;
	  int plp_id;
	  int plp_type;
	  i = 0;
	  L1_Si2168_Context *api;

    api = front_end.Si2168_FE->demod;

	Si2168_L1_DVBT2_STATUS (api, Si2168_DVBT_STATUS_CMD_INTACK_CLEAR);
    num_plp = api->rsp->dvbt2_status.num_plp;
	pr_dbg("num_plp is %d\n",num_plp);
  //  lock = SiLabs_API_Channel_Seek_Next(&front_end, &standard, &freq,  &bandwidth_Hz, &stream, &symbol_rate_bps, &constellation, &polarization, &band, &num_plp);
	switch (p->cmd) {
		case DTV_DVBT2_PLP_ID:
			{
				//if (demod_get_active_data_plp(state, &plp_info) != 0) {
				if (num_plp < 1) {
					p->u.buffer.len = 0;
					r = -EINVAL;
				} else {
					p->u.buffer.len = 2;
					p->u.buffer.data[0] = plp_id;
					p->u.buffer.data[1] = plp_type;
				}
			}
			break;
		case DTV_DVBT2_DATA_PLPS:
			{
				uint8_t plpids[256];
				uint8_t plpnum = 0;
				plpnum=num_plp;
				for (i=0; i<num_plp; i++) {
			        SiLabs_API_Get_PLP_ID_and_TYPE   (&front_end, i, &plp_id, &plp_type);
					plpids[i]=(uint8_t)plp_id;
					pr_dbg("plp_id[%d] is %d\n",i,plp_id);
			        if (plp_id == -1) {
			          printf ("ERROR retrieving PLP info for plp index %d\n", i);
			          SiERROR("ERROR retrieving PLP info\n");
			        } else {
			          if (plp_type != SILABS_PLP_TYPE_COMMON) {
			           // carrier_index = SiLabs_Scan_Table_AddOneCarrier (standard, freq, bandwidth_Hz, stream, symbol_rate_bps, constellation, polarization, band, plp_id);
			          }
			        }
			      }
				
				p->u.buffer.len = 0;
				p->u.buffer.reserved1[0] = 0;
				if (p->u.buffer.reserved2 != NULL) {
				//	demod_get_data_plps(state, plpids, &plpnum);
					/* As linux dvb has property_dump, buffer.len cannot be used in this case, 
					 * it must < 32 , we use u.buffer.resvered1[0] to save plp num instead */
					p->u.buffer.reserved1[0] = plpnum;
					if (plpnum > 0 && 
						copy_to_user(p->u.buffer.reserved2, plpids, plpnum * sizeof(uint8_t))) {
						p->u.buffer.reserved1[0] = 0;
					}
				}
			}
			break;
		default:
			r = EOPNOTSUPP;
			break;
	}

	return r;

}
#endif

static int si2168_fe_get_ops(struct aml_fe_dev *dev, int mode, void *ops)
{
	struct dvb_frontend_ops *fe_ops = (struct dvb_frontend_ops*)ops;
	struct si2168_fe_config cfg;

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

	fe_ops->set_frontend = si2168_set_frontend;
	fe_ops->get_frontend = si2168_get_frontend; 
	fe_ops->read_status = si2168_read_status;
	fe_ops->read_ber = si2168_read_ber;
	fe_ops->read_signal_strength = si2168_read_signal_strength;
	fe_ops->read_snr = si2168_read_snr;
	fe_ops->read_ucblocks = si2168_read_ucblocks;
	fe_ops->set_property = si2168_set_property,
	fe_ops->get_property = si2168_get_property,
	cfg.demod_addr=dev->i2c_addr;
	cfg.i2c_adapter=dev->i2c_adap;
	si2168_get_fe_config(&cfg);
	printk("i2c_adap_id is %d,i2c_addr is %x\n",dev->i2c_adap_id,dev->i2c_addr);
	
	return 0;
}

static int si2168_fe_enter_mode(struct aml_fe *fe, int mode)
{
	struct aml_fe_dev *dev = fe->dtv_demod;

	pr_dbg("=========================si2168 demod init\r\n");
	gpio_direction_output(dev->reset_gpio, dev->reset_value);
	msleep(300);
	gpio_direction_output(dev->reset_gpio, !dev->reset_value); //enable tuner power
	msleep(200);
	//reset
	int standard;
	standard=SILABS_DVB_T2;
	//Initialize LSI
	printf("Initializing LSI .. ");
	SiLabs_API_SW_Init(&front_end, DEMOD_ADDRESS+(fe1*2), TUNER_ADDRESS_TER+(fe1*2), TUNER_ADDRESS_SAT+(fe1*2));
	front_end.Si2168_FE->demod->i2c=DEMOD_ADDRESS;
	SiLabs_API_switch_to_standard		(&front_end, standard, 1);
	printk("chip id is %d, i2c is %x\n",front_end.chip,front_end.Si2168_FE->demod->i2c);
	SiLabs_API_Tuner_I2C_Enable(&front_end);
	if(MDrv_Tuner_Init()==1)
		pr_dbg("TUNER OK\n");
	else
		pr_dbg("TUNER NG\n");	
	SiLabs_API_Tuner_I2C_Disable(&front_end);
	return 0;

}

static int si2168_fe_resume(struct aml_fe_dev *dev)
{
	printk("mxl101_fe_resume\n");
	gpio_direction_output(dev->reset_gpio, dev->reset_value);
	msleep(300);
	gpio_direction_output(dev->reset_gpio, !dev->reset_value); //enable tuner power
	msleep(200);
	int standard;
	standard=SILABS_DVB_T2;
	//Initialize LSI
	printf("Initializing LSI .. ");
	SiLabs_API_SW_Init(&front_end, DEMOD_ADDRESS+(fe1*2), TUNER_ADDRESS_TER+(fe1*2), TUNER_ADDRESS_SAT+(fe1*2));
	front_end.Si2168_FE->demod->i2c=DEMOD_ADDRESS;
	SiLabs_API_switch_to_standard		(&front_end, standard, 1);
	printk("chip id is %d, i2c is %x\n",front_end.chip,front_end.Si2168_FE->demod->i2c);
	if(MDrv_Tuner_Init()==1)
		pr_dbg("TUNER OK\n");
	else
		pr_dbg("TUNER NG\n");	
	return 0;

}

static int si2168_fe_suspend(struct aml_fe_dev *dev)
{
	return 0;
}

static struct aml_fe_drv si2168_dtv_demod_drv = {
.id 		= AM_DTV_DEMOD_SI2168,
.name		= "Si2168",
.capability = AM_FE_OFDM,
.get_ops	= si2168_fe_get_ops,
.enter_mode = si2168_fe_enter_mode,
.suspend	= si2168_fe_suspend,
.resume 	= si2168_fe_resume
};

static int __init sifrontend_init(void)
{
	pr_dbg("register si2168 demod driver\n");
	return aml_register_fe_drv(AM_DEV_DTV_DEMOD, &si2168_dtv_demod_drv);
}


static void __exit sifrontend_exit(void)
{
	pr_dbg("unregister si2168 demod driver\n");
	aml_unregister_fe_drv(AM_DEV_DTV_DEMOD, &si2168_dtv_demod_drv);
}

fs_initcall(sifrontend_init);
module_exit(sifrontend_exit);


MODULE_DESCRIPTION("si2168 DVB-T2 Demodulator driver");
MODULE_AUTHOR("RSJ");
MODULE_LICENSE("GPL");






