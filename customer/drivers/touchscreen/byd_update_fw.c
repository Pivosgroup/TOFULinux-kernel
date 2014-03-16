/*
 * File:         byd_cts12_13_ts.c
 *
 * Created:	     2012-10-07
 * Depend on:    byd_bfxxxx_ts.c
 * Description:  Firmware burnning for BYD TouchScreen IC
 *
 * Copyright (C) 2012 BYD Company Limited 
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
 */
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/types.h> 
#include <asm/unistd.h>
#include <linux/types.h>
#include <asm/types.h>
#include <linux/fs.h>

typedef unsigned char   RRS_BYTE;
#define    RRS_PACKET_LENGTH       128 //128 
#define  bin     0
#define para    1
#define I2C_RATE  200*1000

#ifdef CONFIG_CTS11C
#define PARA_DOWNLOAD_FILE	"system/etc/byd_cts11c_ts.dat"
#endif


#if defined(CONFIG_CTS12)   
#define CTS12_BIN   
#elif defined(CONFIG_CTS13)   
#define CTS13_BIN   
#endif


#if defined(CONFIG_CTS12)   
#define CTS12_PARA   
#elif defined(CONFIG_CTS13)   
#define  CTS13_PARA   
#endif

#ifdef TS_DEBUG_MSG
#define TS_DBG(format, ...)	\
	printk(format, ##__VA_ARGS__)
	//printk(KERN_INFO "BYD_TS *** " format "\n", ## __VA_ARGS__)
#else
#define TS_DBG(format, ...)
#endif

unsigned char  Register[] [10] = 
{
	{0xa1,0x1a,0xc3,0x3c,0xe7,0x7e,0xa5,0x5a,0x24,0x42},  //ONLINE_BEGIN
	{0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa},  //ONLINE_BEGIN
	{0x0a,0x75,0x58},   //ONLINE_BEG_CHECK
	{0xab,0x9d,0x57,0x3b}, //exit online mode check
	{0x81,0x00,0x00,0xa5,0x02,0xd1,0x08}, //flash start address
	{0x81,0x00,0x00,0x5a,0x22,0xc6,0x66}, //write flash
	{0x81,0x00,0x00,0xa5,0xb3,0xf3,0x6e}, //address add+1
	{0x81,0x00,0x00,0xa5,0x68,0xf5,0x86}, //flash end address
	{0x81,0x00,0x00,0xa5,0xbc,0xbc,0x9a}, //address subtract-1
	{0x81,0x00,0x00,0x5a,0xc3,0x58,0xa4}, //erase full flash
	{0x81,0x00,0x00,0xb6,0x3b,0x20,0x6e}, //entry read flash
	{0x81,0x00,0x00,0xb6,0x7b,0xc7,0xe4}, //entry read flash
	{0x08,0xe8,0x0e}, //read buffer
	{0x81,0x00,0x00,0xb6,0x76,0x15,0x46}, //end read flash
	{0x81,0x00,0x00,0xb6,0x36,0xf2,0xcc}, //end read flash
	{0xa0,0xa2,0xa4,0xa6,0xa8,0xc2,0x7e,0x95,0x75,0x55}, //exit online mode
	{0x0a,0x75,0x58},  //flash 0a address
	{0x80,0x00,0x00,0x03,0x33}, //
	{0xbb},
	{0x81,0x00,0x00,0xa5,0x05,0x76,0xf2},//RDN1区首地址
	{0x81,0x00,0x00,0x5a,0xc1,0xc5,0xf2} //进行页擦除
};

/*******************************************************************************
* Function    : i2c master read (no address sent) 
* Description : read data to buffer form i2c bus 
*               the function is only used for checking correctness of data write
* Parameters  : client, buf, len
* Return      : i2c data transfer status
*******************************************************************************/
 int i2c_read_data(struct i2c_client *client, uint8_t *buf, int len)
{
	struct i2c_msg msgs[1];
	
	msgs[0].flags=I2C_M_RD;
	msgs[0].addr=client->addr;
	msgs[0].len=len;
	msgs[0].buf=&buf[0];
	//msgs[0].scl_rate=I2C_RATE;

	return i2c_transfer(client->adapter,msgs, 1);
}



 int i2c_write_bytes(struct i2c_client *client, uint8_t *data, int len)
{
	struct i2c_msg msg;

	msg.flags=!I2C_M_RD;
	msg.addr=client->addr;
	msg.len=len;
	msg.buf=data;
	//msg.scl_rate=I2C_RATE;
	return i2c_transfer(client->adapter,&msg, 1);
}

/*******************************************************************************
* Function    : file_read 
* Description : read data to buffer form file 
* Parameters  : filepath, buffer, len, offset
* Return      : length of data read or -1 in the case of error
*******************************************************************************/
static int file_read(char *filename, char *buf, ssize_t len, int offset)
{	
    struct file *fd;
    int retLen = -2;

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

    fd = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(fd)) {
    	TS_DBG("[BFxxxx_ts][file_open] : failed to open file: %s\n", filename);
    	return -1;
    }
	
    do {
    	if ((fd->f_op == NULL) || (fd->f_op->read == NULL)) {
    		printk("[BFxxxx_ts][file_read] : file can not be read!!\n");
    		break;
    	} 

    	if (fd->f_pos != offset) {
    		if (fd->f_op->llseek) {
    			if (fd->f_op->llseek(fd, offset, 0) != offset) {
    				printk("[BFxxxx_ts][file_read] : failed to seek!!\n");
    				break;
    			}
    		} else {
    			fd->f_pos = offset;
    		}
    	}

    	retLen = fd->f_op->read(fd, buf, len, &fd->f_pos);			

    } while (false);
    
    filp_close(fd, NULL);
	
    set_fs(old_fs);
    TS_DBG(" BFxxxx_ts read file over");
    return retLen;
}

static int GetFirmwareSize(char *filename)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	
	if (NULL == pfile)
		pfile = filp_open(filename, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filename);
		return -EIO;
	}
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	
	return fsize;
}

//#define ONLINE_HANDSHAKE {0xa1,0x1a,0xc3,0x3c,0xe7,0x7e,0xa5,0x5a,0x24,0x42}

/*******************************************************************************
* Function    :  CRC 校验
* Description : 对32 位数据进行数据效验
* Parameters  :  datahigh--地址(0x80/0x81)，datalow--32位数据
* Return      :    crc 值
*******************************************************************************/
unsigned int calc32bit_crc(unsigned char datahigh, unsigned int datalow)
{  
	unsigned int crc = 0;
	unsigned int tempdata = 0;
	unsigned int tempcrc = 0;
	unsigned char i = 0;
    
	for (i = 0; i < 8; i++)
	{
		tempdata = datahigh << 7;
		if ((tempdata & 0x4000) != (crc & 0x4000))
		{
			crc = ((crc << 1) & 0x7fff) ^ 0x4599;
		}
		else
		{
			crc = (crc << 1) & 0x7fff;
		}
		datahigh = datahigh << 1;
	}   

	for (i = 0; i < 32; i++)
	{   
		tempcrc = (unsigned int)crc << 17;
		if ((tempcrc & 0x80000000) != (datalow & 0x80000000))
		{
			crc = ((crc << 1) & 0x7fff) ^ 0x4599;
		}
			else
		{
			crc = (crc << 1) & 0x7fff;
		}
		datalow = datalow << 1;
	}
    return crc;
}

/*******************************************************************************
* Function    :  fts_ctpm_crc_get
* Description : get crc data to buffer
* Parameters  :  datahigh--地址(0x80/0x81)，datalow--32位数据
* Return      :    crc 值
*******************************************************************************/
static RRS_BYTE* raysens_ctpm_crc_get(u8* pbt_buf)
{
	int IICBufOnlineCRC;
	unsigned int tempcrc = 0;

	IICBufOnlineCRC = (pbt_buf[1] << 24) | (pbt_buf[2] << 16) | (pbt_buf[3] << 8) | pbt_buf[4];		          	   
	tempcrc = calc32bit_crc(pbt_buf[0], IICBufOnlineCRC);
	tempcrc = (tempcrc << 1);
	pbt_buf[5] = tempcrc >> 8;
	pbt_buf[6] = tempcrc;    
	return pbt_buf;
}


//read program form flash
/*******************************************************************************
* Function    : static RRS_BYTE*  CTS12_read_flash(struct i2c_client *client,uint8_t *address, uint8_t *direction, int len,RRS_BYTE* pbt_buf,char type)
* Description : read program form flash
* Parameters  :  address : start address
			    direction : +1/-1
			    len:  length
			    pbt_buf: data save in buffer
			    type:  para or bin
* Return      :    0: successful ; error: -1
*******************************************************************************/
static RRS_BYTE*  CTS12_read_flash(struct i2c_client *client, uint8_t *address, uint8_t *direction, int len, RRS_BYTE* pbt_buf, char type)
{
	int  i;
	unsigned char IICBufOnlineR[10];	
	i2c_write_bytes(client, address, 7); 
	TS_DBG("PROG_READ_START  ok \n");
	
#if (defined(CTS13_PARA)|| defined(CTS13_BIN))
	if (type == para)
	{
	       for (i = 0; i < 254; i++) 
		{ 
			i2c_write_bytes(client, Register[8], 7);  //address -1
		}
	}
#endif	   

	i2c_write_bytes(client, Register[10], 7); 
	i2c_write_bytes(client, Register[11], 7); 
	
	for (i = 0; i < len; i++)
	{
		i2c_write_bytes(client, Register[12], 7); 
		i2c_read_data(client,  IICBufOnlineR, 4); 

		pbt_buf[i * 4 + 0] = IICBufOnlineR[0]; 
		pbt_buf[i * 4 + 1] = IICBufOnlineR[1]; 
		pbt_buf[i * 4 + 2] = IICBufOnlineR[2]; 
		pbt_buf[i * 4 + 3] = IICBufOnlineR[3]; 

		pr_info("i == %d ,IICBufOnlineR = %x, %x, %x, %x \n", i, IICBufOnlineR[0], IICBufOnlineR[1], IICBufOnlineR[2], IICBufOnlineR[3]);
		i2c_write_bytes(client, direction, 7); 
	}

	i2c_write_bytes(client, Register[13], 7); 
	i2c_write_bytes(client, Register[14], 7); 
	TS_DBG("PROG_READ_FLSH  ok \n");
	return pbt_buf;
	
}

//进入在线编程模式
/*******************************************************************************
* Function    :  raysens_ctpm_fw_online
* Description : entry online  mode and check 
* Parameters  :  none
* Return      :    0: successful ; error: -1
*******************************************************************************/
static int raysens_ctpm_fw_online(struct i2c_client *client)
{
	int i;
	unsigned char IICBufOnlineR[10];	
	
	//由正常工作切换到在线编程模式
	i2c_write_bytes(client, Register[18], 1);
	//进入在线编程的握手指令
	i2c_write_bytes(client, Register[0], 10);  //4 
       i2c_read_data(client, IICBufOnlineR, 10);
	for (i = 0; i < 10; i++) 
	{
		if (IICBufOnlineR[i] != Register[0][i])
        {
	        TS_DBG("ONLINE_HANDLE error! \n");
	        strcpy(fw_status,"***err: ONLINE_HANDLE error!***\n");
			return -1;
        } 
		TS_DBG("ONLINE_HANDLE ok! \n");

        }   
	
	//开始在线编程的命令	  

	i2c_write_bytes(client, Register[1], 10);  //4 
	TS_DBG("ONLINE_BEGIN data \n");
	msleep(20);
	//判断是否已进入在线编程
	i2c_write_bytes(client,  Register[2], 3); 	
	//i2c_read_interface(I2C_CTPM_ADDRESS, IICBufOnlineR, 10);//
	 i2c_read_data(client,  IICBufOnlineR, 4);
	pr_info("IICBufOnlineR = %x, %x, %x, %x \n", IICBufOnlineR[0], IICBufOnlineR[1], IICBufOnlineR[2], IICBufOnlineR[3]);
		
	 if ((IICBufOnlineR[0] == Register[3][0]) && (IICBufOnlineR[1] == Register[3][1]) && (IICBufOnlineR[2] == Register[3][2]) && (IICBufOnlineR[3] == Register[3][3])) 
	{ //如果读到的数不是 0xab9d573b
		TS_DBG("ONLINE_BEG_CHECK ok \n");
	} 
	else
	{
		TS_DBG("ONLINE_BEG_CHECK error \n");
		strcpy(fw_status,"***err: ONLINE_BEG_CHECK error!***\n");
		return -1;
	}	
	return 1;
}

//写数据到flash. 
/*******************************************************************************
* Function    :  raysens_ctpm_fw_upgrade
* Description : entry online  burst 
* Parameters  :  none
* Return      :    0: successful ; error: -1
*******************************************************************************/
unsigned int  raysens_ctpm_fw_upgrade(struct i2c_client *client, u8* pbt_buf, u16 dw_lenth)
{
	u16  i,j,temp,lenght,packet_number;
	unsigned char IICBufOnlineW[10];
	u8  packet_buf[RRS_PACKET_LENGTH + 4];
	static unsigned char CTPM_FLASHE[10];
	
	TS_DBG("PROG_WRIT_start  ok \n");
	packet_number = (dw_lenth) / RRS_PACKET_LENGTH;
	//0x810000a502d108     //指向Flash起始地址 
	i2c_write_bytes(client,  Register[4], 7); 	
	for (j = 0; j < packet_number; j++)
	{
	        temp = j * RRS_PACKET_LENGTH;
	        packet_buf[0] = (RRS_BYTE)(temp >> 8);
	        packet_buf[1] = (RRS_BYTE)temp;
	        lenght = RRS_PACKET_LENGTH;
	        packet_buf[2] = (RRS_BYTE)(lenght >> 8);
	        packet_buf[3] = (RRS_BYTE)lenght;

		for (i = 0; i < RRS_PACKET_LENGTH; i++)
		{
			packet_buf[4 + i] = pbt_buf[j * RRS_PACKET_LENGTH + i]; 
           	}	

		for (i = 0; i < 32; i++)  ///RRS_PACKET_LENGTH/4
		{
			IICBufOnlineW[0] = 0x80; 
			IICBufOnlineW[1] = packet_buf[4 + 4 * i];//写数?
			IICBufOnlineW[2] = packet_buf[4 + 4 * i + 1];
			IICBufOnlineW[3] = packet_buf[4 + 4 * i + 2];
			IICBufOnlineW[4] = packet_buf[4 + 4 * i + 3];
			raysens_ctpm_crc_get(IICBufOnlineW);
			i2c_write_bytes(client, IICBufOnlineW, 7); 
	#if 0	
			if ((j * RRS_PACKET_LENGTH % 4096) == 0)
			{
				 pr_info("upgrade the 0x%x th byte.\n", ((unsigned int)j) * RRS_PACKET_LENGTH);
				printk("upgrade the 0x%d th byte.\n", j * RRS_PACKET_LENGTH);
				
			}
	#endif
			i2c_write_bytes(client,  Register[5], 7); 
			udelay(30);
			////地址+1  0x810000a5b379b7
			i2c_write_bytes(client,  Register[6], 7); 	
	  	}
        	
	}
	 if ((dw_lenth) % RRS_PACKET_LENGTH > 0)
	{
		temp = packet_number * RRS_PACKET_LENGTH;
		packet_buf[0] = (RRS_BYTE)(temp >> 8);
		packet_buf[1] = (RRS_BYTE)temp;

		temp = (dw_lenth) % RRS_PACKET_LENGTH;
		packet_buf[2] = (RRS_BYTE)(temp >> 8);
		packet_buf[3] = (RRS_BYTE)temp;

		for (i = 0; i < temp; i++)
		{
			packet_buf[4 + i] = pbt_buf[packet_number * RRS_PACKET_LENGTH + i]; 
		}
		for (i = 0; i < temp / 4; i++)
		{
			IICBufOnlineW[0] = 0x80; 
			IICBufOnlineW[1] = packet_buf[4 + 4 * i];//写数据
			IICBufOnlineW[2] = packet_buf[4 + 4 * i + 1];
			IICBufOnlineW[3] = packet_buf[4 + 4 * i + 2];
			IICBufOnlineW[4] = packet_buf[4 + 4 * i + 3];
			raysens_ctpm_crc_get(IICBufOnlineW);
			i2c_write_bytes(client, IICBufOnlineW, 7); 
			// printk("upgrade the last block data:%x \n", IICBufOnlineW[1],IICBufOnlineW[2],IICBufOnlineW[3],IICBufOnlineW[4]);

			//写Flash指令
			i2c_write_bytes(client, Register[5], 7); 
			udelay(30);
			////地址+1  0x810000a5b379b7
			i2c_write_bytes(client, Register[6], 7); 
		}
	}
	CTS12_read_flash(client,Register[4], Register[6], 1,CTPM_FLASHE,bin);  //19
	pr_info("CTPM_FLASHE = %x, %x, %x, %x \n", CTPM_FLASHE[0], CTPM_FLASHE[1], CTPM_FLASHE[2], CTPM_FLASHE[3]);
	
	 if ((CTPM_FLASHE[0] == pbt_buf[0]) && (CTPM_FLASHE[1] == pbt_buf[1]) && (CTPM_FLASHE[2] == pbt_buf[2]) && (CTPM_FLASHE[3] == pbt_buf[3])) 
	{ //如果读到的数不是 0xab9d573b
		TS_DBG("raysens_ctpm_fw_upgrade  ok \n");
		return 1;
	} 
	 else
	 {
		TS_DBG("raysens_ctpm_fw_upgrade error \n");
		strcpy(fw_status,"err: ctpm_fw_upgrade error!\n");
		return -1;
	 }
	 return 1;
}

/*******************************************************************************
* Function    : unsigned char  raysens_ctpm_fw_160para(struct i2c_client *client,u8* pbt_buf, u16 dw_lenth)
* Description : write 160 para
* Parameters  : 
* Return      :    0: successful ; error: -1
*******************************************************************************/
unsigned char  raysens_ctpm_fw_160para(struct i2c_client *client, u8* pbt_buf, u16 dw_lenth)
{
	int  i;
	unsigned char IICBufOnlineW[10];

	//write 160 parameter 
	TS_DBG("raysens_ctpm_fw_160para start \n");
	//0x810000a568xxxx    //指向Flash 最后配置字 地址
	i2c_write_bytes(client, Register[7], 7); 
	// 写配置字00
	raysens_ctpm_crc_get(Register[17]);
	i2c_write_bytes(client,  Register[17], 7); 

	i2c_write_bytes(client, Register[5], 7); 
	udelay(30);

       for (i = 0; i < 127; i++ ) 
	{ 
		i2c_write_bytes(client, Register[8], 7); 
	}
	
       for (i =0; i < dw_lenth; i++ ) 
	{ 
		IICBufOnlineW[0] = 0x80; 
		IICBufOnlineW[1] = pbt_buf[4 * i];//写数据
		IICBufOnlineW[2] = pbt_buf[4 * i + 1];
		IICBufOnlineW[3] = pbt_buf[4 * i + 2];
		IICBufOnlineW[4] = pbt_buf[4 * i + 3];
		raysens_ctpm_crc_get(IICBufOnlineW);		
		i2c_write_bytes(client,  IICBufOnlineW, 7);      
		i2c_write_bytes(client, Register[5], 7); 
		udelay(30);
		i2c_write_bytes(client, Register[6], 7);  
	}
	   TS_DBG("raysens_ctpm_fw_160para ok \n");
	   return 1;
}

/*******************************************************************************
* 函数名称static int raysens_ctpm_main_erase(struct i2c_client *client)
* 功能描述：擦除整片FLASH
* 输入参数：
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/
static int raysens_ctpm_main_erase(struct i2c_client *client)
{
	unsigned char CTPM_FLASH[10];
	TS_DBG("raysens_ctpm_main_erase  start \n");
	i2c_write_bytes(client, Register[4], 7); 

	i2c_write_bytes(client, Register[9], 7); 
	msleep(40);  
	CTS12_read_flash(client,Register[4], Register[6], 1, CTPM_FLASH, bin);  //19
	 if ((CTPM_FLASH[0] == 0xff) && (CTPM_FLASH[1] == 0xff) && (CTPM_FLASH[2] == 0xff) && (CTPM_FLASH[3] == 0xff)) 
	 { //如果读到的数不是 0xab9d573b
		TS_DBG("raysens_ctpm_main_erase  ok \n");
		return 1;
	  } 
	 else
	 {
		TS_DBG("raysens_ctpm_main_erase error \n");
		strcpy(fw_status,"err: ctpm_main_erase error!\n");
		return -1;
	 }

}

/*******************************************************************************
* 函数名称：static void raysens_ctpm_fw_online_exit(struct i2c_client *client)
* 功能描述：在线编程退出判断
* 输入参数：client
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/
static int raysens_ctpm_fw_online_exit(struct i2c_client *client)
{
	unsigned char IICBufOnlineR[10];	
	//在线编程结束指令
	i2c_write_bytes(client,Register[15], 10); 
	TS_DBG("ONLINE_EXIT \n");
	//判断在线编程结束指令
	msleep(20);  
	i2c_write_bytes(client,  Register[16], 3); 
	i2c_read_data(client, IICBufOnlineR, 4);
	if ((IICBufOnlineR[0] == Register[3][0]) && (IICBufOnlineR[1] == Register[3][1]) && (IICBufOnlineR[2] == Register[3][2]) && (IICBufOnlineR[3] == Register[3][3])) 
	{ //如果读到的数是 0xab9d573b ,则未退出，需重新退出
		TS_DBG("IN ONLINE MODE, out error \n");
		strcpy(fw_status,"err: IN ONLINE MODE, out error\n");
		return -1;
	} 
	else if ((IICBufOnlineR[0] == Register[2][0]) && (IICBufOnlineR[1] == Register[2][1]) && (IICBufOnlineR[2] == Register[2][2]))
	{//读到的是0x0a7558zz则已经退出
		TS_DBG("ONLINE_EXIT_CHECK ok\n");
	}
	else
	{
		TS_DBG("ONLINE_EXIT_CHECK  error\n");	
		strcpy(fw_status,"err: ONLINE_EXIT_CHECK\n");
		return -1;
	}
	return 0;
}


/*******************************************************************************
* 函数名称：static int raysens_ctpm_page_erase(struct i2c_client *client,uint8_t *address)
* 功能描述：页擦除函数
* 输入参数：address: 擦除的起始页地址
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/

static int raysens_ctpm_page_erase(struct i2c_client *client, uint8_t *address)
{	
	int  i;
	 unsigned char CTPM_FLASH[10];
	//0x810000a505xxxx   
	TS_DBG("raysens_ctpm_rdn1_erase  start \n");
	raysens_ctpm_crc_get(address);
	i2c_write_bytes(client,  address, 7); 
#if (defined(CTS13_PARA) || defined(CTS13_BIN))	
  for (i = 0; i < 254; i++ ) 
	{ 
		i2c_write_bytes(client, Register[8], 7);  //address -1
	}	
 #endif
	//0x8100005ac1xxxx
	i2c_write_bytes(client,  Register[20], 7); 	
	TS_DBG("PROG_ERASE_CODE ok\n");
	msleep(40); 
	CTS12_read_flash(client,address, Register[6], 1, CTPM_FLASH, para);  //19
	 if ((CTPM_FLASH[0] == 0xff) && (CTPM_FLASH[1] == 0xff) && (CTPM_FLASH[2] == 0xff) && (CTPM_FLASH[3] == 0xff)) 
	{ //如果读到的数不是 0xab9d573b
		TS_DBG("raysens_ctpm_rdn1_erase  ok\n");
		return 1;
	} 
	 else
	 {
		TS_DBG("raysens_ctpm_rdn1_erase error\n");
		return -1;
	 }


}


/*******************************************************************************
* 函数名称：unsigned int  raysens_ctpm_write_para(struct i2c_client *client,uint8_t *address,u8* pbt_buf, u16 dw_lenth)
* 功能描述：参数写入函数
* 输入参数：address: 写数据输入地址
				   pbt_buf:写入的数据buffer
				   dw_lenth: 数据的长度
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/

unsigned int  raysens_ctpm_write_para(struct i2c_client *client, uint8_t *address, u8* pbt_buf, u16 dw_lenth)
{
	int  i;
	unsigned char IICBufOnlineW[10];
	static unsigned char CTPM_FLASH[10];
	TS_DBG("raysens_ctpm_write_para  start \n");
	//0x810000a568xxxx    //指向Flash 最后配置字 地址
	i2c_write_bytes(client, address, 7);
#if (defined(CTS13_PARA)|| defined(CTS13_BIN))
	for (i = 0; i < 254; i++ ) 
	{ 
		i2c_write_bytes(client, Register[8], 7);  //address -1
	}
#endif		
		
	for( i =0; i < dw_lenth; i++ ) 
	{ 
		IICBufOnlineW[0] = 0x80; 
		IICBufOnlineW[1] = pbt_buf[4 * i];//写数据
		IICBufOnlineW[2] = pbt_buf[4 * i + 1];
		IICBufOnlineW[3] = pbt_buf[4 * i + 2];
		IICBufOnlineW[4] = pbt_buf[4 * i + 3];
		raysens_ctpm_crc_get(IICBufOnlineW);	
            	i2c_write_bytes(client,  IICBufOnlineW, 7);        
		i2c_write_bytes(client, Register[5], 7); 
		udelay(100);
		i2c_write_bytes(client, Register[6], 7);  
 
	}
	i2c_write_bytes(client,  Register[7], 7); 
	// 写配置字00
	raysens_ctpm_crc_get(Register[17]);
	i2c_write_bytes(client,  Register[17], 7); 

	i2c_write_bytes(client, Register[5], 7); 
	udelay(30);
	

	CTS12_read_flash(client,address, Register[6], 1, CTPM_FLASH, para);  //19
	 if ((CTPM_FLASH[0] == pbt_buf[0]) && (CTPM_FLASH[1] == pbt_buf[1]) && (CTPM_FLASH[2] == pbt_buf[2]) && (CTPM_FLASH[3] == pbt_buf[3])) 
	{ //如果读到的数不是 0xab9d573b
		TS_DBG("raysens_ctpm_write_para  ok \n");
		return 1;
	} 
	 else
	 {
		TS_DBG("raysens_ctpm_fw_upgrade error \n");
		return -1;
	 }
	return 1;

}

/*******************************************************************************
* 函数名称：static int ctpm_fw_online(struct i2c_client *client,int ret)
* 功能描述：在线编程判断
* 输入参数：client， ret
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/

static int ctpm_fw_online(struct i2c_client *client, int ret)
{
	//进入在线编程
	if (ret < 0) 
	{
		return ret;
	}
	ret = raysens_ctpm_fw_online(client);
	return ret;
}

/*******************************************************************************
* 函数名称tatic int ctpm_main_erase_write(struct i2c_client *client,u8* pbt_buf,u16 dw_lenth,int ret)
* 功能描述：flash 全部擦除和烧写
* 输入参数：
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/
static int ctpm_main_erase_write(struct i2c_client *client, u8* pbt_buf, u16 dw_lenth, int ret)
{
	if (ret < 0) 
	{
		return ret;
	}
			//擦除整片FLASH
	ret = raysens_ctpm_main_erase(client);
	if (ret < 0) 
	{
		return ret;
	}
	ret = raysens_ctpm_fw_upgrade(client, pbt_buf, dw_lenth);
	return ret;
	
}

/*******************************************************************************
* 函数名称static int ctpm_erase_write_para(struct i2c_client *client,uint8_t *address,u8* pbt_buf,u16 dw_lenth,int ret)
* 功能描述：参数擦写
* 输入参数：
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/
static int ctpm_erase_write_para(struct i2c_client *client, uint8_t *address, u8* pbt_buf, u16 dw_lenth, int ret)
{	
	if (ret < 0) 
	{
		return ret;
	}
	ret = raysens_ctpm_page_erase(client, address);
	if (ret < 0) 
	{
		return ret;
	}	
	ret = raysens_ctpm_write_para(client, address, pbt_buf, dw_lenth);
	return ret;
	}

/*******************************************************************************
* 函数名称static int raysens_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
* 功能描述:II2C烧录入口函数
* 输入参数：
* 返回值   ：ret
* 其它说明：无
*******************************************************************************/
static int raysens_ctpm_fw_upgrade_with_i_file(struct i2c_client *client,char * FILENAME, bool force)
{

	int  ret=0;
	int  num = 0;
	unsigned char *starstring = NULL;
	unsigned char *CTPM_FW; // reverse count 7: 1e
	int ui_sz = 25*1024; //32K
	struct sTYPEID_Info *gp_CTP_sInfo;
	int newVersion,oldVersion;

	if((gp_Default_CTP_sInfo->sTYPEID_Ver_Str[0]-'0')!=4 && (gp_Default_CTP_sInfo->sTYPEID_Ver_Str[0]-'0')!=5)
	{
		strcpy(fw_status,"err: The current version does not support the upgrade\n");
		return -1;
	}
	
	 ui_sz=GetFirmwareSize(FILENAME);
	 printk("fwpath: %s\n",FILENAME);
	 printk("fwsize: 0x%x\n",ui_sz);
		 
	 if (ui_sz <= 0) {
		strcpy(fw_status,"***err: get fw size error ***\n");
		return -1;
	 }

	 if (ui_sz < 8 || ui_sz > 32 * 1024) {
		strcpy(fw_status,"***err: fw length error***\n");
		return -1;
	 }
	
	 CTPM_FW = kmalloc(ui_sz + 32, GFP_KERNEL);
	 if(CTPM_FW == NULL)
	 {
		strcpy(fw_status,"***err: kmalloc fail***\n");
		return -1;
	 }
	
#if (defined(CTS12_BIN) || defined(CTS13_BIN))	
	ret = file_read(FILENAME, &CTPM_FW[0], ui_sz, 0);    //FIRM_FILENAME为待升级的TP固件，放在sdcard相应位置
	pr_info("FW_DATA = %x, %x, %x, %x, %x \n", CTPM_FW[0], CTPM_FW[1], CTPM_FW[2], CTPM_FW[3], CTPM_FW[4]);
    starstring = &CTPM_FW[256];//定位到TP固件信息参数处，用ultral edit打开固件中对应地址00000100h，从此处后64个字节为TP信息参数
	for(num = 0; num < 64; num ++)
	{
		g_FW_CTP_Type[num] = *starstring++;
	}
	gp_CTP_sInfo=(struct sTYPEID_Info *)g_FW_CTP_Type;
	printk("Raysens CTP date info:%s\n",gp_CTP_sInfo->sTYPEID_DateNo);//固件的日期信息
	printk("Raysens CTP Chip Num:%s\n",gp_CTP_sInfo->sTYPEID_ChipNo);//芯片序号
	printk("Raysens CTP Type:%s\n",gp_CTP_sInfo->sTYPEID_Type_Str);//模组型号
	printk("Raysens CTP FPC Type:%s\n",gp_CTP_sInfo->sTYPEID_FPC_Str);//FPC型号
	printk("Raysens CTP Chip Type:%s\n",gp_CTP_sInfo->sTYPEID_Chip_Str);//芯片型号
	printk("Raysens CTP Version:%s\n",gp_CTP_sInfo->sTYPEID_Ver_Str);//固件版本号
	starstring=NULL;
	if (ret < 0){
		TS_DBG(" 12C CALI_FILENAME failed to open file ");	
		strcpy(fw_status,"err: read fw error\n");
		ret= -1;
		goto EXIT;
	}	
	if(!force){
		if(strcmp(gp_CTP_sInfo->sTYPEID_FPC_Str,gp_Default_CTP_sInfo->sTYPEID_FPC_Str)){
			strcpy(fw_status,"err: FW FPC is not match.\n");
			ret= -1;
			goto EXIT;	
		}
		sscanf(gp_CTP_sInfo->sTYPEID_DateNo,"%d",&newVersion);
		sscanf(gp_Default_CTP_sInfo->sTYPEID_DateNo,"%d",&oldVersion);
		if(newVersion <= oldVersion){
			strcpy(fw_status,"err: fw version is not the latest version.\n");
			ret= -1;
			goto EXIT;	
		}
	}
#endif
	client->addr = I2C_CTPM_ADDRESS;//在线升级固件的烧录地址
	//进入在线编程
	if(ctpm_fw_online(client,ret)<0)
	{
		ret= -1;
		goto EXIT;
	}
    
	//烧录全部flash
#if (defined(CTS12_BIN) ||defined (CTS13_BIN))
	//这里将待升级固件写入touch IC的flash中
	if(ctpm_main_erase_write(client, CTPM_FW, ui_sz, 1)<0)
	{
		ret= -1;
		goto EXIT;
	}
#endif
	//在线编程结束
	if(raysens_ctpm_fw_online_exit(client) >= 0)
	{
		strcpy(fw_status,"OK: BYD ctp firmware upgrade successful!\n");
	}
	
EXIT:	
	client->addr = 0x52;//驱动I2C通信地址，升级完固件后地址还原
	kfree(CTPM_FW);
	return ret;
}


/*******************************************************************************
* Function    : file_write 
* Description : write data to file 
* Parameters  : filepath, buffer, len, offset
* Return      : file len
*******************************************************************************
static int flie_write(char *filename, char *buf, ssize_t len, int offset)
{	
    struct file *fd;
    int retLen = -1;

    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);

    fd = filp_open(filename, O_WRONLY|O_CREAT, 0666);
    // fd = filp_open(filename, O_WRONLY, 0666);   
    if (IS_ERR(fd)) {
        TS_DBG("[BF6852_ts][flie_write] : failed to open!!\n");
        return -1;
    }
    do {
        if ((fd->f_op == NULL) || (fd->f_op->write == NULL)) {
            printk("[BF6852_ts][flie_write] : file can not be write!!\n");
            break;
        } // End of if 

        if (fd->f_pos != offset) {
            if (fd->f_op->llseek) {
                if(fd->f_op->llseek(fd, offset, 0) != offset) {
                    printk("[BF6852_ts][flie_write] : failed to seek!!\n");
                    break;
                }
            } else {
                fd->f_pos = offset;
            }
        }       		

        retLen = fd->f_op->write(fd, buf, len, &fd->f_pos);

    } while (false);

    filp_close(fd, NULL);

    set_fs(old_fs);

    return retLen;
}
*/

/*******************************************************************************
* Module      : Touchscreen working parameter updating 
* Description : tuning touchscreen by writing parameters to its registers 
* Interface   : int byd_write_para(i2c_client)
*               in:  i2c_client
*               out: status of parameter updating
*******************************************************************************/


#if defined(CONFIG_SUPPORT_PARAMETER_UPG) || defined(CONFIG_SUPPORT_PARAMETER_FILE) 

#ifdef CONFIG_CRC_REQUIRED // This is used only for parameter tune
static unsigned char calc_crc(unsigned char* buf, int len)
{
	unsigned int i;
	unsigned char crc = 0;
	
	for(i = 1; i < len - 1; i++) {
		crc += buf[i];
	}
	//buf[len - 1] = 0x00 - crc;
	return buf[len - 1];
}
#endif

//write parameter to TP IC
static void byd_write_para(struct i2c_client *client)
{
#ifdef CONFIG_SUPPORT_PARAMETER_UPG // This is used for parameter tune
	unsigned char buf[] = {
#ifdef CONFIG_BF685X
#include "byd_bf685x_ts.inc"
#elif defined(CONFIG_CTS12) || defined(CONFIG_CTS13)
#include "byd_cts12_13_ts.inc"
#elif defined(CONFIG_CTS11C)
#include "byd_cts11c_ts.inc"
#endif
	};
#elif defined CONFIG_SUPPORT_PARAMETER_FILE
	unsigned char buf[1000];
#endif

	int ret, len;
	
	len = (int)sizeof(buf);
#ifdef CONFIG_SUPPORT_PARAMETER_FILE // This can be used for parameter tune
	len = file_read(PARA_DOWNLOAD_FILE, buf, len, 0);
	if (len <= 0)
		return; // no file exist, return to use chip's defaults
	else
		printk("*** BYD TS: Overriding default parameter using file: %s\n", PARA_DOWNLOAD_FILE);
#endif
	TS_DBG(" buf[0]=%x,  buf[1]=%x,  buf[2]=%x, len=%d ", buf[0], buf[1], buf[2], len);
	
#ifdef CONFIG_CRC_REQUIRED // This is used only for parameter tune
	buf[len - 1] = calc_crc(buf, len);
	//ret = file_write(CALI_FILENAME, buf, len, 0);
	TS_DBG("calc_crc() : CRC = %x,\n", buf[len - 1]);
#endif

	ret = i2c_write_bytes(client, buf, len); 
	if (ret < 0) {
		pr_err("BF685XA/CTS11C write register failed! ret: %d\n", ret);
		return;
	}
	
#if defined(CONFIG_SUPPORT_PARAMETER_FILE) && defined(TS_DEBUG_MSG) // this is for test only
	{
		uint8_t buf1[2];
		msleep(100);
		i2c_read_data(this_client, buf1, 1);
		TS_DBG("BF685XA/CTS11C write test : crc read = %x\n", buf1[0]);
	}
#endif
}
#endif // defined(CONFIG_SUPPORT_PARAMETER_FILE) || defined(CONFIG_SUPPORT_FIRMWARE_UPG)

