/*++
 
 Copyright (c) 2012-2022 ChipOne Technology (Beijing) Co., Ltd. All Rights Reserved.
 This PROPRIETARY SOFTWARE is the property of ChipOne Technology (Beijing) Co., Ltd. 
 and may contains trade secrets and/or other confidential information of ChipOne 
 Technology (Beijing) Co., Ltd. This file shall not be disclosed to any third party,
 in whole or in part, without prior written consent of ChipOne.  
 THIS PROPRIETARY SOFTWARE & ANY RELATED DOCUMENTATION ARE PROVIDED AS IS, 
 WITH ALL FAULTS, & WITHOUT WARRANTY OF ANY KIND. CHIPONE DISCLAIMS ALL EXPRESS OR 
 IMPLIED WARRANTIES.  
 
 File Name:    icn83xx.h
 Abstract:
               include files.
 Author:       Zhimin Tian
 Date :        10 30,2012
 Version:      0.1[.revision]
 History :
     Change logs.  
 --*/

#ifndef 	_LINUX_ICN83XX_H
#define		_LINUX_ICN83XX_H

#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>



#define CTP_NAME                ICN83XX_NAME

#define SCREEN_MAX_X            (1280)
#define SCREEN_MAX_Y            (768)
#define PRESS_MAX               (255)


#define POINT_NUM           5
#define POINT_SIZE          7
typedef	struct _POINT_INFO
{
	unsigned char  u8ID;
	unsigned short u16PosX;		// coordinate X, plus 4 LSBs for precision extension
	unsigned short u16PosY;		// coordinate Y, plus 4 LSBs for precision extension
	unsigned char  u8Pressure;
	unsigned char  u8EventId;
}POINT_INFO;

struct icn83xx_ts_data {
	struct            input_dev	*input_dev;
	int               point_num;
	POINT_INFO        point_info[POINT_NUM+1];
	struct work_struct 	    pen_event_work;
	struct workqueue_struct *ts_workqueue;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend    early_suspend;
#endif
	struct timer_list       _timer;
	
	spinlock_t	            irq_lock;     
	int                     irq_is_disable; /* 0: irq enable */ 
	int                     irq;

};


#define swap_ab(a,b)       {char temp;temp=a;a=b;b=temp;}
#define U16LOBYTE(var)     (*(unsigned char *) &var) 
#define U16HIBYTE(var)     (*(unsigned char *)((unsigned char *) &var + 1))     


//#define STOP_IRQ_TYPE                     // if define then   no stop irq in irq_handle
#define	INT_PORT	     INT_GPIO_0
#define ICN83XX_I2C_SCL  200*1000


// gpio base address
#define CONFIG_ICN83XX_MULTITOUCH     (1)
                                
#undef  AW_GPIO_INT_API_ENABLE

enum icn83xx_ts_regs {
	ICN83XX_REG_PMODE	= 0x04,	/* Power Consume Mode		*/	
};


//ICN83XX_REG_PMODE
//#define PMODE_ACTIVE        0x00
//#define PMODE_MONITOR       0x01
//#define PMODE_STANDBY       0x02
#define PMODE_HIBERNATE     0x02


#ifndef ABS_MT_TOUCH_MAJOR
    #define ABS_MT_TOUCH_MAJOR	0x30	/* touching ellipse */
    #define ABS_MT_TOUCH_MINOR	0x31	/* (omit if circular) */
    #define ABS_MT_WIDTH_MAJOR	0x32	/* approaching ellipse */
    #define ABS_MT_WIDTH_MINOR	0x33	/* (omit if circular) */
    #define ABS_MT_ORIENTATION	0x34	/* Ellipse orientation */
    #define ABS_MT_POSITION_X	0x35	/* Center X ellipse position */
    #define ABS_MT_POSITION_Y	0x36	/* Center Y ellipse position */
    #define ABS_MT_TOOL_TYPE	0x37	/* Type of touching device */
    #define ABS_MT_BLOB_ID		0x38	/* Group set of pkts as blob */
#endif /* ABS_MT_TOUCH_MAJOR */

int icn83xx_i2c_rxdata(unsigned char addr, char *rxdata, int length);
int icn83xx_i2c_txdata(unsigned char addr, char *txdata, int length);
int icn83xx_write_reg(unsigned char addr, char para);
int icn83xx_read_reg(unsigned char addr, char *pdata);
int icn83xx_prog_i2c_rxdata(unsigned short addr, char *rxdata, int length);
int icn83xx_prog_i2c_txdata(unsigned short addr, char *txdata, int length);
int icn83xx_prog_write_reg(unsigned short addr, char para);
int icn83xx_prog_read_reg(unsigned short addr, char *pdata);


#define D_U16LOBYTE(var)     (*(unsigned char *) &var) 
#define D_U16HIBYTE(var)     (*(unsigned char *)((unsigned char *) &var + 1))    

#define B_SIZE  64
#define ENABLE_BYTE_CHECK

#define DBG_FLASH
#ifdef DBG_FLASH
#define _flash_info_(fmt, args...)   \
        do{                              \
                pr_info(fmt, ##args);     \
        }while(0)
#else
#define _flash_info_(fmt, args...)   //
#endif
//-----------------------------------------------------------------------------
// Struct, Union and Enum DEFINITIONS
//-----------------------------------------------------------------------------
typedef enum
{
	R_OK = 100,
	R_FILE_ERR,
	R_STATE_ERR,
	R_ERASE_ERR,
	R_PROGRAM_ERR,
	R_VERIFY_ERR,
}E_UPGRADE_ERR_TYPE;
//-----------------------------------------------------------------------------
// Global VARIABLES
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Function PROTOTYPES
//-----------------------------------------------------------------------------

void icn83xx_memdump(char *mem, int size);
int  icn83xx_checksum(int sum, char *buf, unsigned int size);
int  icn83xx_update_status(int status);
int  icn83xx_get_status(void);
int  icn83xx_open_fw( char *fw);
int  icn83xx_read_fw(int offset, int length, char *buf);
int  icn83xx_close_fw(void);
int  icn83xx_goto_progmode(void);
int  icn83xx_check_progmod(void);
int  icn83xx_uu(void);
int  icn83xx_ll(void);
int  icn83xx_op1(char info, unsigned short offset, unsigned int size);
int  icn83xx_op2(char info, unsigned short offset, unsigned char * buffer, unsigned int size);
int  icn83xx_op3(char info, unsigned short offset, unsigned char * buffer, unsigned int size);
short  icn83xx_read_fw_Ver(char *fw);
E_UPGRADE_ERR_TYPE  icn83xx_fw_update(char *fw);

#define DBG_OP
#ifdef DBG_OP
#define _op_info_(fmt, args...)   \
        do{                              \
                pr_info(fmt, ##args);     \
        }while(0)
#else
#define _op_info_(fmt, args...)   //
#endif
//-----------------------------------------------------------------------------
// Struct, Union and Enum DEFINITIONS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Global VARIABLES
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Function PROTOTYPES
//-----------------------------------------------------------------------------
int icn83xx_writeInfo(unsigned short addr, char value);
int icn83xx_readInfo(unsigned short addr, char *value);
int icn83xx_writeReg(unsigned short addr, char value);
int icn83xx_readReg(unsigned short addr, char *value);
int icn83xx_setVol(char vol);

int  icn83xx_readVersion(void);
int  icn83xx_changemode(char mode);
int  icn83xx_log(char diff);

int icn83xx_readTP(char row_num, char column_num, char *buffer);
void icn83xx_rawdatadump(short *mem, int size, char br);

#endif /* _LINUX_ICN83XX_H */
