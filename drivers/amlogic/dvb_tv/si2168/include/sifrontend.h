


#ifndef _MN88436SF_H_
#define _MN88436SF_H_



#include <linux/dvb/frontend.h>
#include "../../../media/dvb/dvb-core/dvb_frontend.h"
#include "../aml_dvb.h"


#include "SiLabs_API_L3_Console.h"
#include "Si2168_Platform_Definition.h"
#include "Silabs_L0_API.h"

#include "nmituner_api.h"


#define printf printk
#define SiTRACE //printk
#define SiERROR	printk
#define SiTraceConfiguration printk



struct si2168_fe_config {
	int                 reset_pin;
	int                 demod_addr;
	int                 tuner_addr;
	void 			  *i2c_adapter;
};



#endif
