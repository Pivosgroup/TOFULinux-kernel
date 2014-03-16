/*
 * author:      wei.liu@amlogic.com
 * date:	2012-08-01
 * usage: 	security support for IPTV
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>

#include "security.h"

#define SECURITY_DEBUG

#define SECURITY_RA_LENGTH 517
#define SECURITY_IN_LENGTH 24
#define SECURITY_CI_LENGTH 4

typedef enum security_status_e { // task status
  SECURITY_STATUS_NULL = 0,      // no task, not initialized
  SECURITY_STATUS_IDLE,          // no task, initialized
  SECURITY_STATUS_S_ME,          // to decrypt ME
  SECURITY_STATUS_G_CD,          // ready to get CD
  SECURITY_STATUS_S_IV,          // to encrypt upon IV
  SECURITY_STATUS_G_XX,          // ready to get XX
} security_status_t;

typedef struct security_in_s { // IN, 24 bytes
  u8 factory_id[4];            // 厂商代码
  u8 chip_batch[8];            // 芯片批次
  u8 chip_sn[12];              // 芯片序号
} security_in_t;



typedef struct security_ra_s { // RA (RSA1024 Algorithm factors), 517 bytes
  u8 e[  5];
  u8 n[256];
  u8 d[256];
} security_ra_t;

typedef struct security_gp_s   // GP (GlobalParameters)
{   
    security_status_t status;  // status
    security_rn_t         rn;  // RN (RandomNumber)
    security_sn_t         sn;  // SN
    security_me_t         me;  // ME
    security_cd_t         cd;  // CD
    security_iv_t         iv;  // IV
    security_xx_t         xx;  // XX
    security_in_t         in;  // IN
    security_ci_t         ci;  // CI (ChipID)
    security_ra_t         ra;  // RA (RSA1024 Algorithm factors)
} security_gp_t;

// security_gp_t gp;
struct security_device
{
    struct cdev dev;   
    security_gp_t security_gp;
};

int security_major = 0;
int security_minor = 0;
dev_t security_dev_no = 0;
struct class* security_class = NULL;
struct security_device* security_dev = NULL;

function secure_call;
spinlock_t secure_lock;
int procedure = 0;
int hashtable_length = 0;
int double_rsa_file_length=0;

/* functions to do */
static void security_to_gain_rn(void)  // gp.rn : RN generated dynamically
{
    int index = 0;
    
    security_rn_t* rn = NULL;

    unsigned char* ptr = NULL;
    
    printk(KERN_INFO "security_to_gain_rn\n");

    rn = &(security_dev->security_gp.rn);

    // rn->data;

    ptr = (unsigned char*)rn->data;
/*    for(index = 0; index < 16; index++)
    {
        *ptr++ = '8';  // 'Z' - index;  // + index;
    }*/
    char random_number[16];
       
	do
	{
	  memset((void*)random_number, 0, 16);
	  get_random_bytes((void*)random_number, 16);
	  
	  int tmp_index = 0;
	  for(tmp_index = 0; tmp_index < 16; tmp_index++)
	  {
         if((random_number[tmp_index] >= '0') && (random_number[tmp_index] <= '9') && (index < 16))
         {
            *(ptr + index) = random_number[tmp_index];
            index++;
            
            printk(KERN_INFO "get the random %c\n", random_number[tmp_index]);
            
            if(index == 16)
            {
                   break;
            }
         }
	  }
	}while(index < 16);

    // get_random_bytes((void*)ptr, sizeof(security_rn_t));

    for(index = 0; index < sizeof(security_rn_t); index++)
    {
        printk(KERN_INFO "rn[%d] is: %x", index, ptr[index]);
    }
    printk(KERN_INFO "\n");
}

// read sn to security_gp.sn
static bool security_to_gain_sn(void)      // gp.sn : SN stored in normal world, visible to user space
{
    int result = 0;
    int length = 0;

    struct security_sn_s* sn = NULL;  

    unsigned long flags = 0;

    length = sizeof(security_sn_t);

    sn = &(security_dev->security_gp.sn);

    spin_lock_irqsave(&secure_lock, flags);
    result = secure_call(NULL, 0, (char*)sn, &length, -1, SECURE_CMD_GAIN_SN);
    spin_unlock_irqrestore(&secure_lock, flags);

    printk(KERN_INFO "result is: %d\n", result);

    // if((length != sizeof(security_sn_t)) || (result != 0))
    if(result != 0)
    {
        memset((void*)(&security_dev->security_gp.sn), 0, sizeof(security_sn_t));

        return 0;
    }

    return 1;
}

#define ENTRY_PHYS_ADDRESS 0x9FE01000
static bool security_init(void)
{
        int result = 0;

        unsigned int phys_address = ENTRY_PHYS_ADDRESS;
        unsigned int virt_address = 0;

        unsigned long flags = 0;
		
		printk(KERN_INFO "security_init!\n");

        virt_address = (unsigned int)__phys_to_virt(phys_address);
        secure_call = (function)virt_address;
		printk(KERN_INFO "security_init! virt_address %x\n",virt_address);
        spin_lock_init(&secure_lock);

        spin_lock_irqsave(&secure_lock, flags);
        result = secure_call(NULL, 0, NULL, NULL, -1, SECURE_CMD_INIT);
        spin_unlock_irqrestore(&secure_lock, flags);

        security_to_gain_sn();
		
		if(result == 0)
		{
			printk(KERN_INFO "security init successfully!\n");
		}
		else
		{
			printk(KERN_INFO "security init unsuccessfully!\n");
		}

        return result == 0 ? 1 : 0;
}

// decrypt md, output cd
static bool security_decrypt_me(void)  // gp.me/ra -> RSA1024 -> gp.cd
{
    int result = 0;
    int length = 0;

    unsigned long flags = 0;

    // int me_length = 0;
    // int cd_length = 0;

    char buffer[128];

    length = 128;

    printk(KERN_INFO "into security_decrypt_me!\n");

    spin_lock_irqsave(&secure_lock, flags);
    printk(KERN_INFO "&security_dev->security_gp is: %p\n", (void*)&security_dev->security_gp);
    
    // me_length = sizeof(security_me_t);
    // cd_length = sizeof(security_cd_t);

    // printk(KERN_INFO "me_length is: %d\n", me_length);
    printk(KERN_INFO "&security_dev->security_gp.me is: %p\n", &security_dev->security_gp.me);
    printk(KERN_INFO "&security_dev->security_gp.cd is: %p\n", &security_dev->security_gp.cd);

    result = secure_call((char*)(&security_dev->security_gp.me), sizeof(security_me_t),
        // (char*)(&security_dev->security_gp.cd), &length, procedure,
        buffer, &length, procedure,
        SECURE_CMD_RSA_PROC_DEC_ME);
    
    spin_unlock_irqrestore(&secure_lock, flags);

    printk(KERN_INFO "result is: %d\n", result);

    memcpy((void*)&security_dev->security_gp.cd, buffer, 64);

    printk(KERN_INFO "length is: %d\n", length);
    printk(KERN_INFO "outof security_decrypt_me!\n");

    // if(((length + 1) != sizeof(security_cd_t)) || (result != 0))
    if(result != 0)
    {
        memset((void*)(&security_dev->security_gp.cd), 0, sizeof(security_cd_t));

        return 0;
    }

    return 1;
}

// decrypt iv, output xx
static bool security_encrypt_iv(void)  // gp.sn/ci/in/iv(t2/r2) -> HASH -> gp.xx
{
    int result = 0;
    int length = 0;

    unsigned long flags = 0;

    length = sizeof(security_xx_t);
    printk(KERN_INFO "length is: %d\n", length);

    spin_lock_irqsave(&secure_lock, flags);
    result = secure_call((char*)(&security_dev->security_gp.iv), sizeof(security_iv_t),
        (char*)(&security_dev->security_gp.xx), &length, procedure,
        SECURE_CMD_SHA_PROC_ENC_IV);
    spin_unlock_irqrestore(&secure_lock, flags);

    // if((length != sizeof(security_xx_t)) || (result != 0))
    if(result != 0)
    {
        memset((void*)(&security_dev->security_gp.xx), 0, sizeof(security_xx_t));

        return 0;
    }

    return 1;
}

// validation check
static bool security_validation(security_oi_t iv, security_oi_t cd)
{
    int i = 0;

    for (i = 0; i < 4; i++)
    {
        if (iv.data[i] != cd.data[i])
            break;
    }

    if (i == 4)
        return true;
    else
        return false;
}

void security_task(void)
{
	printk(KERN_INFO "into security_task, status is: %d\n", security_dev->security_gp.status);
	
    switch(security_dev->security_gp.status)
    {
        case SECURITY_STATUS_NULL:
            // if (security_init())
			security_init();
                security_dev->security_gp.status = SECURITY_STATUS_IDLE;
            break;
            
        case SECURITY_STATUS_S_ME:
            security_decrypt_me();
            security_dev->security_gp.status = SECURITY_STATUS_G_CD;
            break;
            
        case SECURITY_STATUS_S_IV:
            security_encrypt_iv();
            security_dev->security_gp.status = SECURITY_STATUS_G_XX;
            break;
            
        default:
            break;
    }
	
	printk(KERN_INFO "outof security_task, status is: %d\n", security_dev->security_gp.status);
}

/*result 
-1;board is not encrypt
0:board is encrypt with 1 rsa
1:board is encypt with 2 rsa
*/
static int is_board_2rsa_encrypt(){
	
	int result = 0;

	unsigned long flags = 0;
	spin_lock_irqsave(&secure_lock, flags);
	result = secure_call(NULL, 0,NULL,NULL, 0,
	SECURE_CMD_IS_BOARD_ENC_VIA_2SECU_KEY);
	spin_unlock_irqrestore(&secure_lock, flags);
	return result;
}

void* in = NULL;
void* out = NULL;
static int security_2rsa_decrypt(unsigned char *pszContent, int *nLength,int procedure,int uboot_flag)
{
	int ret=-1;
	unsigned long flags = 0;
	struct_aml_chk_blk blk;
	char *pBufferCHK;
	int outLength=RSA1024_DEC_LEN;
	memset(&blk,0,sizeof(blk));
	if((NULL==pszContent)||(*nLength<sizeof(blk)))
		return ret;
	printk("security_2rsa_decrypt uboot_flag is %d file length is %d \n",uboot_flag,*nLength);
	if(uboot_flag)
	{
		pBufferCHK = pszContent;	
		memcpy(&blk,pszContent+*nLength-sizeof(blk),sizeof(blk));
	}
	else
		{
		pBufferCHK = pszContent + sizeof(blk);
		memcpy(&blk,pszContent,sizeof(blk));
		}

	if(((AMLOGIC_CHKBLK_ID_2 == blk.unAMLID) ||(AMLOGIC_CHKBLK_ID == blk.unAMLID)) &&
			(AMLOGIC_CHKBLK_VER >= blk.nVer) &&
			sizeof(blk) == blk.nSize2)
		{
		unsigned char szRSADecBuff[RSA1024_DEC_LEN];
		memset(szRSADecBuff,0,sizeof(szRSADecBuff));
		spin_lock_irqsave(&secure_lock, flags);
		ret = secure_call((void*)blk.szCHK, RSA1024_DEC_LEN, szRSADecBuff, &outLength, procedure, SECURE_CMD_RSA_DEC_VIA_2SECU_KEY);
		spin_unlock_irqrestore(&secure_lock, flags);
		if(ret)
			{
			printk(KERN_INFO "security_2rsa_decrypt dec fail \n");
			return -EFAULT;
			}
		memcpy(blk.szCHK,szRSADecBuff,RSA1024_DEC_LEN);
		if(1 == blk.secure.nInfoType)
		{	
						
			extern int aml_aes_decrypt (uint8_t *ct, uint8_t *pt, int size, uint8_t *pAESkey);
			if(blk.secure.nAESLength>*nLength)
				return -EFAULT;
			ret = aml_aes_decrypt(pBufferCHK,pBufferCHK,blk.secure.nAESLength,blk.secure.szAESKey);
			if(ret)
				{
					printk("AES decrypt fail\n");
					return -EFAULT;
				}
			
		}
		unsigned char *pBuffer = pBufferCHK;

		if(pBuffer)
		{				
			unsigned char szHashCal[32];
			memset(szHashCal,0,sizeof(szHashCal));
			//hash
			extern int sha2_sum( unsigned char outbuf[32],const unsigned char *pbuff, int nLen );
			if(blk.secure.nHashDataLen>*nLength)
				return -EFAULT;
			sha2_sum(szHashCal,pBuffer,blk.secure.nHashDataLen);
			//compare the hash value			
			if(!memcmp(blk.secure.szHashKey,szHashCal,AMLOGIC_CHKBLK_HASH_KEY_LEN))
			{				
				memcpy(pszContent,pBufferCHK,blk.secure.nTotalFileLen);
				*nLength=blk.secure.nTotalFileLen;
				ret = 0;
			}
			else
			{
				printk("sha2 sum check failed\n");
				return -EFAULT;
			}
		}
		}
		else
		{
			printk("security_2rsa_decrypt : Invalid AML-CHK-BLK ID or Ver!\n");
			return -EFAULT;
		}	
	return ret;
}

static int security_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    void __user *argp = (void __user *)arg;

    int result = 0;
	
	printk(KERN_INFO "security_ioctl!\n");
	printk(KERN_INFO "current status is: %d\n", security_dev->security_gp.status);

    if (_IOC_TYPE(cmd) != SECURITY_IOC_MAGIC) 
    {
        return -ENOSYS;
    }

    switch (cmd)
    {
        case SECURITY_IOC_G_SN:
        {
			printk(KERN_INFO "SECURITY_IOC_G_SN!\n");
			
            if(security_dev->security_gp.status != SECURITY_STATUS_IDLE)
            {
                return -EFAULT;
            }
            else
            {
                if(copy_to_user(argp, (void*)(&security_dev->security_gp.sn), sizeof(security_sn_t)) != 0)
                {
                    return -EFAULT;
                }
            }

            break;
        }

        case SECURITY_IOC_G_RN:
        {
            printk(KERN_INFO "SECURITY_IOC_G_RN!\n");
		
            if(security_dev->security_gp.status != SECURITY_STATUS_IDLE)
            {
				printk(KERN_INFO "status is not correct!\n");
				
                return -EFAULT;
            }
            else
            {
                security_to_gain_rn();
				
		int ret = copy_to_user(argp, (void*)(&security_dev->security_gp.rn), sizeof(security_rn_t));

                if( ret != 0)
                {
                    printk(KERN_INFO "copy to user unsuccessfully!\n");
                    printk(KERN_INFO "ret is: %d\n", ret);
                    return -EFAULT;
                }
            }

            break;
        }

        case SECURITY_IOC_S_ME_G_CD:
        {
			printk(KERN_INFO "SECURITY_IOC_S_ME_G_CD!\n");
		
            if (security_dev->security_gp.status != SECURITY_STATUS_IDLE)
            {
                ret = -EFAULT;
            }
            else
            {
                if(copy_from_user((void*)(&security_dev->security_gp.me), argp, sizeof(security_me_t)) != 0)
                {
                    memset((void*)(&security_dev->security_gp.me), 0, sizeof(security_me_t));

                    ret = -EFAULT;
                }
            }

            security_dev->security_gp.status = SECURITY_STATUS_S_ME;
            security_task();

            if (security_dev->security_gp.status != SECURITY_STATUS_G_CD)
            {
                ret = -EFAULT;
            }
            else
            {
                if(copy_to_user(argp, (void*)(&security_dev->security_gp.cd), sizeof(security_cd_t)) != 0)
                {
                    ret = -EFAULT;
                }
            }

            security_dev->security_gp.status = SECURITY_STATUS_IDLE;

            break;
        }

        case SECURITY_IOC_S_IV_G_XX:
        {
            if (security_dev->security_gp.status != SECURITY_STATUS_IDLE)
            {
                ret = -EFAULT;
            }
            else
            {
                if(copy_from_user((void*)(&security_dev->security_gp.iv), argp, sizeof(security_iv_t)) != 0)
                {
                    memset((void*)(&security_dev->security_gp.iv), 0, sizeof(security_iv_t));

                    return -EFAULT;
                }
            }

            if(!security_validation(security_dev->security_gp.iv.oi, security_dev->security_gp.cd.oi))
            {
                return -EFAULT;
            }

            security_dev->security_gp.status = SECURITY_STATUS_S_IV;
            security_task();

            if (security_dev->security_gp.status != SECURITY_STATUS_G_XX)
            {
                ret = -EFAULT;
            }
            else
            {
                if(copy_to_user(argp, (void*)(&security_dev->security_gp.xx), sizeof(security_xx_t)) != 0)
                {
                    return -EFAULT;
                }
            }

            security_dev->security_gp.status = SECURITY_STATUS_IDLE;

            break;
        }
        
        case SECURITY_IOC_G_SH_256:
        {
            if (security_dev->security_gp.status != SECURITY_STATUS_IDLE)
            {
                ret = -EFAULT;
            }
            else
            {
                if(copy_from_user((void*)(&security_dev->security_gp.iv), argp, sizeof(security_iv_t)) != 0)
                {
                    memset((void*)(&security_dev->security_gp.iv), 0, sizeof(security_iv_t));

                    return -EFAULT;
                }
            }

//            if(!security_validation(security_dev->security_gp.iv.oi, security_dev->security_gp.cd.oi))
//            {
//                return -EFAULT;
//            }

            security_dev->security_gp.status = SECURITY_STATUS_S_IV;
            security_task();

            if (security_dev->security_gp.status != SECURITY_STATUS_G_XX)
            {
                ret = -EFAULT;
            }
            else
            {
                if(copy_to_user(argp, (void*)(&security_dev->security_gp.xx), sizeof(security_xx_t)) != 0)
                {
                    return -EFAULT;
                }
            }

            security_dev->security_gp.status = SECURITY_STATUS_IDLE;

            break;
        }

        case SECURITY_IOC_B_SN:
        {
            unsigned long flags = 0;
	    char sn_enc[128];
	    memset((void*)sn_enc, 0, 128);

            // if(copy_from_user((void*)(&security_dev->security_gp.sn), argp, sizeof(security_sn_t)) != 0)
            if(copy_from_user((void*)sn_enc, argp, 128) != 0)
	    {
                // memset((void*)(&security_dev->security_gp.sn), 0, sizeof(security_sn_t));
		memset((void*)(void*)sn_enc, 0, 128);
                return -EFAULT;
            }

            spin_lock_irqsave(&secure_lock, flags);
            result = secure_call((void*)sn_enc, 128, NULL, NULL, -1,
                SECURE_CMD_BURN_SN);
            spin_unlock_irqrestore(&secure_lock, flags);

	    if(result != 0)
	    {
	    	printk(KERN_INFO "kernel, burn sn unsuccessfully!\n");
	    }
	    else
	    {
		printk(KERN_INFO "kernel, burn sn successfully!\n");
	    }

            if(result != 0)
            {
                return -EFAULT;
            }

            break;
        }

        case SECURITY_IOC_G_IN_STU:
        {
            unsigned long flags = 0;

            spin_lock_irqsave(&secure_lock, flags);
            result = secure_call(NULL, 0, NULL, NULL, -1, SECURE_CMD_CHECK_IN_FREE);
            spin_unlock_irqrestore(&secure_lock, flags);

            if(result != 0)
            {
                return -EFAULT;
            }

            break;
        }

        case SECURITY_IOC_B_IN:
        {
            unsigned long flags = 0;
	    char in_enc[128];
	    memset((void*)in_enc, 0, 128);

            // if(copy_from_user((void*)(&security_dev->security_gp.in), argp, sizeof(security_in_t)) != 0)
	    if(copy_from_user((void*)in_enc, argp, 128) != 0)
            {
               //  memset((void*)(&security_dev->security_gp.in), 0, sizeof(security_in_t));
		memset((void*)in_enc, 0, 128);
                return -EFAULT;
            }

            spin_lock_irqsave(&secure_lock, flags);
            result = secure_call((void*)in_enc, 128, NULL, NULL, -1,
                SECURE_CMD_BURN_IN);
            spin_unlock_irqrestore(&secure_lock, flags);

		if(result != 0)
		{
			printk(KERN_INFO "kernel, burn in unsuccessfully!\n");
		}
		else
		{
			printk(KERN_INFO "kernel, burn in successfully!\n");
		}

            if(result != 0)
            {
                return -EFAULT;
            }

            break;
        }

        // set procedure
        case SECURITY_IOC_S_PR:
        {
            // procedure = *((int*)argp);

            copy_from_user((void*)&procedure, argp, sizeof(int));

            printk(KERN_INFO "set procedure!\n");

            break;
        }

        // set hashtable length
        case SECURITY_IOC_S_HS_LEN:
        {
            // hashtable_length = *((int*)argp);

            copy_from_user((void*)&hashtable_length, argp, sizeof(int));

            printk(KERN_INFO "set hashtable length is: %d\n", hashtable_length);

            break;
        }

        case SECURITY_IOC_G_HS_LEN:
        {
            copy_to_user(argp, (void*)&hashtable_length, sizeof(int));

            printk(KERN_INFO "get hashtable length is: %d\n", hashtable_length);

            break;
        }

        // decrypt hashtable
        case SECURITY_IOC_D_HS:
        {
        	printk(KERN_INFO "set hashtable content, then decrypt it!\n");

            int index = 0;
            int length = 0;
            int result = 0;

            unsigned long flags = 0;

            int encrypted_block_size = 128;
            // int decrypted_block_size = 127;

            int total_length = 0;

            in = kmalloc(hashtable_length, GFP_KERNEL);
            out = kmalloc(hashtable_length, GFP_KERNEL);

            if((in == NULL) || (out == NULL))
            {
                return -ENOMEM;
            }

            if(copy_from_user(in, argp, hashtable_length) != 0)
            {
                kfree(in);
                kfree(out);

                return -EFAULT;
            }

            char in_block[128];
            char out_block[127];

            memset((void*)in_block, 0, 128);
            memset((void*)out_block, 0, 127);

            index = 1;
            while(index * encrypted_block_size <= hashtable_length)
            {
                memcpy((void*)in_block, in + (index - 1) * encrypted_block_size, encrypted_block_size);

                length = 127;

                spin_lock_irqsave(&secure_lock, flags);
                result = secure_call(in_block, encrypted_block_size, out_block, &length, procedure,
                    SECURE_CMD_RSA_DEC_VIA_SECU_KEY);
                spin_unlock_irqrestore(&secure_lock, flags);

                memcpy(out + total_length, out_block, length);

                total_length += length;

                memset((void*)in_block, 0, 128);
                memset((void*)out_block, 0, 127);

                index ++;
            }

            length = hashtable_length - (index - 1) * encrypted_block_size;
            if(length > 0)
            {
                memcpy((void*)in_block, in + (index - 1) * encrypted_block_size, length);

                spin_lock_irqsave(&secure_lock, flags);
                result = secure_call(in_block, length, out_block, &length, procedure,
                    SECURE_CMD_RSA_DEC_VIA_SECU_KEY);
                spin_unlock_irqrestore(&secure_lock, flags);

                memcpy(out + total_length, out_block, length);

                total_length += length;

                memset((void*)in_block, 0, 128);
                memset((void*)out_block, 0, 127);

                index++;
            }

            hashtable_length = total_length;

            break;
        }

        // return decrpyted hashtable
        case SECURITY_IOC_G_HS:
        {
        	printk(KERN_INFO "get hashtable content!\n");

        	int ret = copy_to_user(argp, out, hashtable_length);
        	printk(KERN_INFO "ret is: %d\n", ret);

          // if(copy_to_user(argp, out, hashtable_length) != 0)
        	if(ret != 0)
        	{
        		kfree(in);
        		kfree(out);

        		return -EFAULT;
        	}

          kfree(in);
          kfree(out);

          break;
        }
        
        case SECURITY_IOC_B_SN_RAW:
        {
        	unsigned long flags = 0;
        	if(copy_from_user((void*)(&security_dev->security_gp.sn), argp, sizeof(security_sn_t)) != 0)
        	{
        		memset((void*)(&security_dev->security_gp.sn), 0, sizeof(security_sn_t));
        		
        		return -EFAULT;
        	}
        	
        	spin_lock_irqsave(&secure_lock, flags);
        	result = secure_call((void*)(&security_dev->security_gp.sn), sizeof(security_sn_t), NULL, NULL, -1,
        			SECURE_CMD_BURN_SN_RAW);
        	spin_unlock_irqrestore(&secure_lock, flags);
        	
        	if(result != 0)
        	{
        		return -EFAULT;
        	}
        	
        	break;
        }
        
        case SECURITY_IOC_B_IN_RAW:
        {
        	unsigned long flags = 0;

			if(copy_from_user((void*)(&security_dev->security_gp.in), argp, sizeof(security_in_t)) != 0)
			{
				memset((void*)(&security_dev->security_gp.in), 0, sizeof(security_in_t));

				return -EFAULT;
			}

			spin_lock_irqsave(&secure_lock, flags);
			result = secure_call((void*)(&security_dev->security_gp.in), sizeof(security_in_t), NULL, NULL, -1,
					SECURE_CMD_BURN_IN_RAW);
			spin_unlock_irqrestore(&secure_lock, flags);

			if(result != 0)
			{
				return -EFAULT;
			}

        	break;
        }
        
        case SECURITY_IOC_B_MAC:
        {
        	unsigned long flags = 0;
        	
        	char mac[6];
        	
        	memset((void*)mac, 0, 6);
        	
        	if(copy_from_user((void*)mac, argp, 6) != 0)
        	{
        		memset((void*)mac, 0, 6);
        		
        		return -EFAULT;
        	}
        	
        	spin_lock_irqsave(&secure_lock, flags);
        	result = secure_call((void*)mac, 6, NULL, NULL, -1, SECURE_CMD_BURN_MAC);
        	spin_unlock_irqrestore(&secure_lock, flags);
        	
        	if(result != 0)
        	{
        		return -EFAULT;
        	}
        	
        	break;
        }
        
        case SECURITY_IOC_G_MAC:
        {
        	unsigned long flags = 0;
        	
        	int length = 6;
        	
        	char mac[6];
        	
        	memset((void*)mac, 0, 6);
        	
        	spin_lock_irqsave(&secure_lock, flags);
        	result = secure_call(NULL, 0, (void*)mac, &length, -1, SECURE_CMD_GAIN_MAC);
        	spin_unlock_irqrestore(&secure_lock, flags);
        	
        	if(result != 0)
        	{
        		return -EFAULT;
        	}
        	
        	if(copy_to_user(argp, (void*)mac, 6) != 0)
        	{
        		return -EFAULT;
        	}
        	
        	break;
        }
		case SECURITY_IOC_G_CI:
		{
			unsigned long flags = 0;
        	
        	int length = 4;
        	
        	char ci[4];
        	
        	memset((void*)ci, 0, 4);
        	
        	spin_lock_irqsave(&secure_lock, flags);
        	result = secure_call(NULL, 0, (void*)ci, &length, -1, SECURE_CMD_GAIN_CI);
        	spin_unlock_irqrestore(&secure_lock, flags);
        	
        	if(result != 0)
        	{
        		return -EFAULT;
        	}
        	
        	if(copy_to_user(argp, (void*)ci, 4) != 0)
        	{
        		return -EFAULT;
        	}
        	
        	break;
		}
		case SECURITY_IOC_B_MAC_BT:
        {
        	unsigned long flags = 0;
        	
        	char mac_bt[6];
        	
        	memset((void*)mac_bt, 0, 6);
        	
        	if(copy_from_user((void*)mac_bt, argp, sizeof(mac_bt)) != 0)
        	{
        		memset((void*)mac_bt, 0, sizeof(mac_bt));

				printk("SECURITY_IOC_B_MAC copy mac form user  failed! \n");
        		
        		return -EFAULT;
        	}
        	
        	spin_lock_irqsave(&secure_lock, flags);
        	result = secure_call((void*)mac_bt, 6, NULL, NULL, -1, SECURE_CMD_BRUN_MAC_BT);
        	spin_unlock_irqrestore(&secure_lock, flags);
        	
        	if(result != 0)
        	{
				printk("SECURITY_IOC_B_MAC burn mac bt failed! \n");
        		return -EFAULT;
        	}
        	
        	break;
        }
        
        case SECURITY_IOC_G_MAC_BT:
        {
        	unsigned long flags = 0;
        	
        	int length = 6;
        	
        	char mac_bt[6];
        	
        	memset((void*)mac_bt, 0, 6);
        	
        	spin_lock_irqsave(&secure_lock, flags);
        	result = secure_call(NULL, 0, (void*)mac_bt, &length, -1, SECURE_CMD_GAIN_MAC_BT);
        	spin_unlock_irqrestore(&secure_lock, flags);
        	
        	if(result != 0)
        	{
        		return -EFAULT;
        	}
        	
        	if(copy_to_user(argp, (void*)mac_bt, 6) != 0)
        	{
        		return -EFAULT;
        	}
        	
        	break;
        }
		case SECURITY_IOC_G_2RSA_FILE_BUF:
		case SECURITY_IOC_C_2RSA_FILE_BUF://only check dec,not do copy 
		case SECURITY_IOC_C_2RSA_FILE_UBOOT://check the uboot file
        {
			char *pszContent=NULL;
			int ret=-1;	
			if(double_rsa_file_length<=0)
				{
					printk(KERN_EMERG "double_rsa_file_length <=0 \n");
					return -EFAULT;
				}
			pszContent=vmalloc(double_rsa_file_length);
			if(pszContent == NULL)
			{
				printk(KERN_ALERT "Fail to alloc 2rsa file buf ! %d \n",double_rsa_file_length);
				return  -ENOMEM;				
			}
			memset(pszContent,0,double_rsa_file_length);
			if(copy_from_user((void*)pszContent, argp, double_rsa_file_length) != 0)
        	{
				printk(KERN_INFO "secure secure_buf copy_from_user failed!! \n");
				goto fail;
        	}
			ret=security_2rsa_decrypt(pszContent,&double_rsa_file_length,0,cmd==SECURITY_IOC_C_2RSA_FILE_UBOOT?1:0);
			if(ret)
				goto fail;
			if((cmd!=SECURITY_IOC_C_2RSA_FILE_UBOOT)&&(is_board_2rsa_encrypt()>0))
				{
				printk("===do second rsa decrypt==== \n");
				ret=security_2rsa_decrypt(pszContent,&double_rsa_file_length,1,cmd==SECURITY_IOC_C_2RSA_FILE_UBOOT?1:0);
				if(ret)
					goto fail;
				}	
			if(cmd==SECURITY_IOC_C_2RSA_FILE_BUF||cmd==SECURITY_IOC_C_2RSA_FILE_UBOOT)
				{
				if(pszContent)
					vfree(pszContent);
				 pszContent=NULL;
				 double_rsa_file_length=0;
				 return ret;
				}
			if(copy_to_user(argp, (void*)(pszContent), double_rsa_file_length) != 0)
                {
                	printk("copy dec result to user failed \n");
					goto fail;
                }
			if(pszContent)
				vfree(pszContent);
			pszContent=NULL;
			break;			
fail:
			if(pszContent)
				vfree(pszContent);
			pszContent=NULL;
			double_rsa_file_length=0;
			return -EFAULT;		
        }
		case SECURITY_IOC_G_2RSA_FILE_LEN:
        {
            copy_to_user(argp,(void*)&double_rsa_file_length, sizeof(int));
			double_rsa_file_length=0;
			break;
        }
		case SECURITY_IOC_S_2RSA_FILE_LEN:
        {
            copy_from_user((void*)&double_rsa_file_length, argp, sizeof(int));
			break;
        }
			
        default:
        {
            ret = -ENOIOCTLCMD;

            break;
        }
    }

    return ret;
}

static ssize_t device_security_sn_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

    


   if(security_dev->security_gp.status != SECURITY_STATUS_IDLE)
    {

        return -EFAULT;
    }
    else
    {
	memcpy(buf, (void*)(&security_dev->security_gp.sn), sizeof(security_sn_t));
    }

     return SECURITY_SN_LENGTH;

}
static DEVICE_ATTR(security_sn, S_IRUGO | S_IWUGO,
		device_security_sn_show, NULL);


static ssize_t device_security_mac_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long flags = 0;

	int result;
		
	int length = 6;
		
	char mac[6];


	memset((void*)mac, 0, 6);
        	
	spin_lock_irqsave(&secure_lock, flags);
	result = secure_call(NULL, 0, (void*)mac, &length, -1, SECURE_CMD_GAIN_MAC);
	spin_unlock_irqrestore(&secure_lock, flags);

	
	if(result != 0)
	{
		return -EFAULT;
	}
	
	sprintf(buf,"%02x:%02x:%02x:%02x:%02x:%02x",*((unsigned char *)mac),*((unsigned char *)mac+1),*((unsigned char *)mac+2), \
	*((unsigned char *)mac+3),*((unsigned char *)mac+4),*((unsigned char *)mac+5));

//  printk(KERN_INFO "buf is %s \n",buf);
   return 17;

}

static ssize_t device_security_mac_bt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long flags = 0;

	int result;
		
	int length = 6;
		
	char mac_bt[6];


	memset((void*)mac_bt, 0, 6);
        	
	spin_lock_irqsave(&secure_lock, flags);
	result = secure_call(NULL, 0, (void*)mac_bt, &length, -1, SECURE_CMD_GAIN_MAC_BT);
	spin_unlock_irqrestore(&secure_lock, flags);

	
	if(result != 0)//use eth mac
	{
		printk("bt is not set ,use eth mac \n");
		spin_lock_irqsave(&secure_lock, flags);
		result = secure_call(NULL, 0, (void*)mac_bt, &length, -1, SECURE_CMD_GAIN_MAC);
		spin_unlock_irqrestore(&secure_lock, flags);
		if(result!=0)
			return -EFAULT;
	}
	
	sprintf(buf,"%02x:%02x:%02x:%02x:%02x:%02x",*((unsigned char *)mac_bt),*((unsigned char *)mac_bt+1),*((unsigned char *)mac_bt+2), \
	*((unsigned char *)mac_bt+3),*((unsigned char *)mac_bt+4),*((unsigned char *)mac_bt+5));

  //  printk(KERN_INFO "mac_bt is %s \n",buf);

    return 17;
}


static DEVICE_ATTR(security_mac, S_IRUGO | S_IWUGO,
		device_security_mac_show, NULL);

static DEVICE_ATTR(security_mac_bt, S_IRUGO | S_IWUGO,
		device_security_mac_bt_show, NULL);



static ssize_t device_security_ci_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long flags = 0;

	int result;
	
	int length = 4;
	
	char ci[4];
	
	memset((void*)ci, 0, 4);
	
	spin_lock_irqsave(&secure_lock, flags);
	result = secure_call(NULL, 0, (void*)ci, &length, -1, SECURE_CMD_GAIN_CI);
	spin_unlock_irqrestore(&secure_lock, flags);
	
	if(result != 0)
	{
		return -EFAULT;
	}
	
	memcpy(buf, (void*)ci, 4);

	printk("chipid  is %s \n",buf);
	
	return 4;
}

static DEVICE_ATTR(security_ci, S_IRUGO | S_IWUGO,
		device_security_ci_show, NULL);

static struct file_operations security_fops =
{
		.owner = THIS_MODULE,
		.unlocked_ioctl = security_ioctl,
};

static int __init security_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct device* device = NULL;

	printk(KERN_INFO "security_probe\n");

	ret = alloc_chrdev_region(&security_dev_no, 0, 1, SECURITY_DEVICE_NODE_NAME);
	if(ret < 0)
	{
		printk(KERN_ALERT "Fail to alloc char device region!\n");

		goto fail_alloc_chrdev_region;
	}
	else
	{
		security_major = MAJOR(security_dev_no);
		security_minor = MINOR(security_dev_no);

		printk(KERN_INFO "major is: %d\n", security_major);
		printk(KERN_INFO "minor is: %d\n", security_minor);
	}

	security_dev = kmalloc(sizeof(struct security_device), GFP_KERNEL);
	if(security_dev == NULL)
	{
		ret = -ENOMEM;

		printk(KERN_ALERT "Fail to alloc security_dev!\n");

		goto fail_kmalloc;
	}
	else
	{
	        printk(KERN_INFO "Succeed to alloc security_dev!\n");
	}

	memset(security_dev, 0, sizeof(struct security_device));

	cdev_init(&(security_dev->dev), &security_fops);
	security_dev->dev.owner = THIS_MODULE;

	ret = cdev_add(&(security_dev->dev), security_dev_no, 1);
	if(ret < 0)
	{
		printk(KERN_ALERT "Fail to add char device!\n");

		goto fail_cdev_add;
	}
	else
	{
	        printk(KERN_INFO "Succeed to add char device!\n");
	}

	security_class = class_create(THIS_MODULE, SECURITY_DEVICE_CLASS_NAME);
	if(IS_ERR(security_class))
	{
		ret = PTR_ERR(security_class);

		printk(KERN_ALERT "Fail to create class security!\n");

		goto fail_class_create;
	}
	else
	{
	        printk(KERN_INFO "Succeed to create class security!\n");
	}

	device = device_create(security_class, NULL, security_dev_no, NULL,
	    SECURITY_DEVICE_FILE_NAME);
	if(IS_ERR(device))
	{
		ret = PTR_ERR(device);

		printk(KERN_ALERT "Fail to create device security!\n");

		goto fail_device_create;
	}
	else
	{
	        printk(KERN_INFO "Succeed to create device security!\n");
	}

	dev_set_drvdata(device, security_dev);

        security_dev->security_gp.status = SECURITY_STATUS_NULL;
    
        security_task();

	ret = device_create_file(device, &dev_attr_security_sn);
	if(ret < 0)
	{
		printk(KERN_ALERT "Fail to create file security_sn!\n");

		goto fail_device_create;
	}

	ret = device_create_file(device, &dev_attr_security_mac);
	if(ret < 0)
	{
		printk(KERN_ALERT "Fail to create file security_mac!\n");

		goto fail_device_create_file_mac;
	}

	ret = device_create_file(device, &dev_attr_security_ci);
	if(ret < 0)
	{
		printk(KERN_ALERT "Fail to create file security_ci!\n");

		goto fail_device_create_file_ci;
	}

	ret = device_create_file(device, &dev_attr_security_mac_bt);
	if(ret < 0)
	{
		printk(KERN_ALERT "Fail to create file security_mac_bt!\n");

		goto fail_device_create_file_mac_bt;
	}
	
	printk(KERN_INFO "Succeed to initialize security device!\n");

	return 0;
fail_device_create_file_mac_bt:
	device_remove_file(device, &dev_attr_security_mac_bt);

fail_device_create_file_ci:	
	device_remove_file(device, &dev_attr_security_ci);

fail_device_create_file_mac:
	device_remove_file(device, &dev_attr_security_sn);

fail_device_create:
	class_destroy(security_class);

fail_class_create:
	cdev_del(&(security_dev->dev));

fail_cdev_add:
	kfree(security_dev);

fail_kmalloc:
	unregister_chrdev_region(security_dev_no, 1);

fail_alloc_chrdev_region:
	return ret;
}

static int security_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "security_probe\n");

	if(security_class != NULL)
	{
		device_destroy(security_class, security_dev_no);

		class_destroy(security_class);
	}

	if(security_dev != NULL)
	{
		cdev_del(&(security_dev->dev));

		kfree(security_dev);
	}

	unregister_chrdev_region(security_dev_no, 1);

	return 0;
}

static struct platform_driver security_driver =
{
    .probe = security_probe,
    .remove = security_remove,
    .shutdown = NULL,
    .suspend = NULL,
    .resume = NULL,
    .driver =
    {
        .name = "security",
        .owner = THIS_MODULE,
    },
};

#ifdef SECURITY_DEBUG
static struct platform_device mock_device =
{
    .name = "security",
};
#endif 

static int __init security_module_init(void)
{
    printk(KERN_INFO "security_module_init!\n");

#ifdef SECURITY_DEBUG
    platform_device_register(&mock_device);
#endif

    return platform_driver_register(&security_driver);
}

static void __exit security_module_exit(void)
{
    printk(KERN_INFO "security_module_exit!\n");

#ifdef SECURITY_DEBUG
    platform_device_unregister(&mock_device);
#endif

    platform_driver_unregister(&security_driver);
}

module_init(security_module_init);
module_exit(security_module_exit);

MODULE_DESCRIPTION("Amlogic IPTV Security Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wei.liu@amlogic.com");
