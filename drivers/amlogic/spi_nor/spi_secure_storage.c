
#include <linux/types.h>
#include "spi_secure_storage.h"
#ifdef SPI_SECURE_STORAGE_UBOOT
#include <spi.h>
#include <common.h>
#include <malloc.h>
#include <spi_flash.h>
#include <ubi_uboot.h>
#endif

#ifdef SPI_SECURE_STORAGE_KERNEL

#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include "spi_nor_flash.h"
#endif

//static struct spi_flash *spi_securestorage_flash=NULL;
static void *spi_securestorage_flash=NULL;

static u32 emmckey_calculate_checksum(u8 *buf,u32 lenth)
{
	u32 checksum = 0;
	u32 cnt;
	for(cnt=0;cnt<lenth;cnt++){
		checksum += buf[cnt];
	}
	return checksum;
}

static int spi_securestorage_read(void *keypara,u8 *buf,u32 len)
{
	u64 addr,size;
	u32 checksum;
	int ret;
#ifdef SPI_SECURE_STORAGE_UBOOT
	struct spi_flash *flash = (struct spi_flash*)keypara;
	struct aml_spisecurestorage_info_t *securestorage_info = flash->securestorage_info;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	int retlen;
	struct spi_nor *flash = (struct spi_nor*)keypara;
	struct aml_spisecurestorage_info_t *securestorage_info = flash->securestorage_info;
#endif
	struct aml_spi_securestorage_t *securedata;

	if(!securestorage_info->secure_init){
		printk("secure_init:%d fail,%s:%d\n",securestorage_info->secure_init,__func__,__LINE__);
		return -1;
	}
	securedata = kzalloc(sizeof(*securedata), GFP_KERNEL);
	if(securedata == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}
	memset(securedata,0,sizeof(*securedata));

	addr = securestorage_info->valid_node->offset;
	size = securestorage_info->valid_node->size;
#ifdef SPI_SECURE_STORAGE_UBOOT
	flash->secure_protect = 0;
	ret = spi_flash_read(flash, (u32)addr,(size_t)size, securedata);
	flash->secure_protect = 1;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	flash->secure_protect = 0;
	ret = flash->mtd.read(&flash->mtd,addr,size,&retlen,(u_char*)securedata);
	flash->secure_protect = 1;
#endif
	if(ret){
		printk("%s:%d,spi read secure storage addr:0x%llx,size:0x%llx fail\n",__func__,__LINE__,addr,size);
	}
	else{
		checksum = emmckey_calculate_checksum(securedata->data,SPI_SECURESTORAGE_AREA_VALID_SIZE);
		if ((!memcmp(securedata->magic, SPI_SECURESTORAGE_MAGIC, 9))
			&&(emmckey_calculate_checksum(securedata->magic,SPI_SECURESTORAGE_MAGIC_SIZE) == securedata->magic_checksum)
			&&(checksum == securedata->checksum)){
				memcpy(buf,securedata->data,len);
		}
		else{
			ret = -1;
			//printk("checksum:%x, securedata->checksum:%x \n",checksum,securedata->checksum);
			//printk("save:%s, orign:%s ",securedata->magic,SPI_SECURESTORAGE_MAGIC);
			//printk("head checksum:%x, securedata->magic_checksum:%x \n",emmckey_calculate_checksum(securedata->magic,SPI_SECURESTORAGE_MAGIC_SIZE) ,securedata->magic_checksum);
		}
	}

	kfree(securedata);
	return ret;
}
static int spi_securestorage_write(void *keypara,u8 *buf,u32 len)
{
	u64 addr,size;
	int ret=-1;
#ifdef SPI_SECURE_STORAGE_UBOOT
	struct spi_flash *flash = (struct spi_flash*)keypara;
	struct aml_spisecurestorage_info_t *securestorage_info = flash->securestorage_info;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	int retlen;
	struct spi_nor *flash = (struct spi_nor*)keypara;
	struct aml_spisecurestorage_info_t *securestorage_info = flash->securestorage_info;
	struct erase_info instr;
#endif
	struct aml_spifree_node_t *free_node,*free_tmp_node;
	//struct aml_spivalid_node_t *valid_node;
	struct aml_spi_securestorage_t *securedata;
	
	if(!securestorage_info->secure_init){
		printk("secure_init:%d fail,%s:%d\n",securestorage_info->secure_init,__func__,__LINE__);
		return -1;
	}
	securedata = kzalloc(sizeof(*securedata), GFP_KERNEL);
	if(securedata == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}
	memset(securedata,0,sizeof(*securedata));

	if(securestorage_info->secure_valid){
		free_tmp_node = kzalloc(sizeof(*free_tmp_node), GFP_KERNEL);
		if(free_tmp_node == NULL){
			printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
			return -ENOMEM;
		}
		memset(free_tmp_node,0,sizeof(*free_tmp_node));
		free_tmp_node->offset = securestorage_info->valid_node->offset;
		free_tmp_node->size = securestorage_info->valid_node->size;
		
		free_node = securestorage_info->free_node;
		
		securestorage_info->valid_node->offset = free_node->offset;
		securestorage_info->valid_node->size = free_node->size;
		securestorage_info->valid_node->timestamp += 1;

		securestorage_info->free_node = free_node->next;
		kfree(free_node);
		if(securestorage_info->free_node == NULL){
			securestorage_info->free_node = free_tmp_node;
		}
		else{
			free_node = securestorage_info->free_node;
			while(free_node->next != NULL){
				free_node = free_node->next;
			}
			free_node->next = free_tmp_node;
		}
	}
	else{
		free_node = securestorage_info->free_node;
		securestorage_info->valid_node->offset = free_node->offset;
		securestorage_info->valid_node->size = free_node->size;
		securestorage_info->valid_node->timestamp += 1;
		securestorage_info->free_node = free_node->next;
		kfree(free_node);
	}
	memcpy(securedata->magic,SPI_SECURESTORAGE_MAGIC,sizeof(SPI_SECURESTORAGE_MAGIC));
	securedata->magic_checksum = emmckey_calculate_checksum(securedata->magic,SPI_SECURESTORAGE_MAGIC_SIZE);
	securedata->timestamp = securestorage_info->valid_node->timestamp;
	securedata->version = SPI_SECURESTORAGE_VER;
	memcpy(securedata->data,buf,len);
	securedata->checksum = emmckey_calculate_checksum(securedata->data,SPI_SECURESTORAGE_AREA_VALID_SIZE);

	addr = securestorage_info->valid_node->offset;
	size = securestorage_info->valid_node->size;

#ifdef SPI_SECURE_STORAGE_UBOOT
	flash->secure_protect = 0;
	if(!spi_flash_erase(flash,(u32)addr,(size_t)size)){
		ret = spi_flash_write(flash,(u32)addr,(size_t)size,securedata);
		//static inline int spi_flash_write(struct spi_flash *flash, u32 offset,size_t len, const void *buf)
		if(ret){
			printk("%s:%d,spi flash write addr:0x%llx,size:0x%llx fail\n",__func__,__LINE__,addr,size);
		}
	}
	else{
		printk("%s:%d,spi flash erase addr:0x%llx,size:0x%llx fail\n",__func__,__LINE__,addr,size);
	}
	flash->secure_protect = 1;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	flash->secure_protect = 0;
	memset(&instr,0,sizeof(struct erase_info));
	instr.mtd = &flash->mtd;
	instr.addr = addr;
	instr.len = size;
	if(!flash->mtd.erase(&flash->mtd,&instr)){
		ret = flash->mtd.write(&flash->mtd,addr,size,&retlen,(u_char*)securedata);
		if(ret){
			printk("%s:%d,spi flash write addr:0x%llx,size:0x%llx fail\n",__func__,__LINE__,addr,size);
		}
	}
	else{
		printk("%s:%d,spi flash erase addr:0x%llx,size:0x%llx fail\n",__func__,__LINE__,addr,size);
	}
	flash->secure_protect = 1;
#endif
	securestorage_info->secure_valid = 1;
	kfree(securedata);
	return ret;
}

static int spi_securestorage_init(void *keypara)
{
	int cnt,securestorage_part,error=0;
	u64 addr,size;
	int ret=0;
	u32 checksum;
#ifdef SPI_SECURE_STORAGE_UBOOT
	struct spi_flash *flash = (struct spi_flash*)keypara;
	struct aml_spisecurestorage_info_t *securestorage_info = flash->securestorage_info;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	int retlen;
	struct spi_nor *flash = (struct spi_nor*)keypara;
	struct aml_spisecurestorage_info_t *securestorage_info = flash->securestorage_info;
#endif

	struct aml_spifree_node_t *free_node,*free_tmp_node,*free_prev_node;
	
	struct aml_spi_securestorage_t *securedata;
	securestorage_part = SPI_SECURESTORAGE_AREA_COUNT;
	
	securedata = kzalloc(sizeof(*securedata), GFP_KERNEL);
	if(securedata == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}
	securestorage_info->valid_node = kzalloc(sizeof(struct aml_spivalid_node_t), GFP_KERNEL);
	if(securestorage_info->valid_node == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		kfree(securedata);
		return -ENOMEM;
	}
	memset(securestorage_info->valid_node,0,sizeof(struct aml_spivalid_node_t));
	cnt = 0;
	do{
		size = SPI_SECURESTORAGE_AREA_SIZE;
		addr = securestorage_info->start_pos + cnt*size;
		flash->secure_protect = 0;
#ifdef SPI_SECURE_STORAGE_UBOOT
		ret = spi_flash_read(flash, (u32)addr,(size_t)size, securedata);
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
		ret = flash->mtd.read(&flash->mtd,(loff_t)addr,(size_t)size,(size_t*)&retlen,(u_char*)securedata);
#endif
		flash->secure_protect = 1;
		if(ret){
			printk("%s:%d,spi read secure storage addr:0x%llx,size:0x%llx fail\n",__func__,__LINE__,addr,size);
			error++;
			cnt++;
			if(cnt >= securestorage_part){
				break;
			}
			continue;
		}
		
		checksum = emmckey_calculate_checksum(securedata->data,SPI_SECURESTORAGE_AREA_VALID_SIZE);
		if ((!memcmp(securedata->magic, SPI_SECURESTORAGE_MAGIC, 9))
			&&(emmckey_calculate_checksum(securedata->magic,SPI_SECURESTORAGE_MAGIC_SIZE) == securedata->magic_checksum)
			&&(checksum == securedata->checksum)){
				securestorage_info->secure_valid = 1;
				if(securestorage_info->valid_node->offset > 0){
					free_node = kzalloc(sizeof(*free_node), GFP_KERNEL);
					if(free_node == NULL){
						printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
						ret = -ENOMEM;
						goto exit;
					}
					memset(free_node,0,sizeof(*free_node));
					free_node->dirty_flag = 1;
					if(securedata->timestamp > securestorage_info->valid_node->timestamp){
						free_node->offset = securestorage_info->valid_node->offset;
						free_node->size = securestorage_info->valid_node->size;
						
						securestorage_info->valid_node->offset = addr;
						securestorage_info->valid_node->size = size;
						securestorage_info->valid_node->timestamp = securedata->timestamp;
					}
					else{
						free_node->offset = addr;
						free_node->size = size;
					}
					if(securestorage_info->free_node == NULL){
						securestorage_info->free_node = free_node;
					}
					else{
						free_tmp_node = securestorage_info->free_node;
						while(free_tmp_node->next != NULL){
							free_tmp_node = free_tmp_node->next;
						}
						free_tmp_node->next = free_node;
					}
				}
				else{
					securestorage_info->valid_node->offset = addr;
					securestorage_info->valid_node->size = size;
					securestorage_info->valid_node->timestamp = securedata->timestamp;
				}
		}
		else {
			free_node = kzalloc(sizeof(*free_node), GFP_KERNEL);
			if(free_node == NULL){
				printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
				ret = -ENOMEM;
				goto exit;
			}
			memset(free_node,0,sizeof(*free_node));
			free_node->offset = addr;
			free_node->size = size;
			if(securestorage_info->free_node == NULL){
				securestorage_info->free_node = free_node;
			}
			else{
				free_tmp_node = securestorage_info->free_node;
				free_prev_node = free_tmp_node;
				while(free_tmp_node != NULL){
					if(free_tmp_node->dirty_flag == 1){
						break;
					}
					free_prev_node = free_tmp_node;
					free_tmp_node = free_tmp_node->next;
				}
				if(free_prev_node == free_tmp_node){
					free_node->next = free_tmp_node;
					securestorage_info->free_node = free_node;
				}
				else{
					free_prev_node->next = free_node;
					free_node->next = free_tmp_node;
				}
			}
		}
		securestorage_info->secure_init = 1;
		cnt++;
	}while(cnt<securestorage_part);
	ret = 0;
	if(error >= securestorage_part){
		ret = -1;
		error = securestorage_part;
	}
	printk("spi secure storage part count %d ok\n",(securestorage_part-error));
	if(securestorage_info->secure_valid == 1){
		printk("spi secure starge valid addr:%llx,size:0x%llx\n",securestorage_info->valid_node->offset,securestorage_info->valid_node->size);
	}
exit:
	kfree(securedata);
	return ret;
}

static int spi_securestorage_check(void *keypara)
{
	int err = 0;
#ifdef SPI_SECURE_STORAGE_UBOOT
	struct spi_flash *flash = (struct spi_flash*)keypara;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	struct spi_nor *flash = (struct spi_nor*)keypara;
#endif
	struct aml_spisecurestorage_info_t *securestorage_info;
	//struct aml_spi_securestorage_t *securedata;
	u8 *data_buf;
	securestorage_info = flash->securestorage_info;

	err = spi_securestorage_init(keypara);
	if(err){
		printk("%s:%d,spi secure storeage init fail\n",__func__,__LINE__);
		return err;
	}

	if(securestorage_info->secure_valid){
	}
	else{
		data_buf = kzalloc(SPI_SECURESTORAGE_AREA_VALID_SIZE, GFP_KERNEL);
		memset(data_buf,0,SPI_SECURESTORAGE_AREA_VALID_SIZE);
		err = spi_securestorage_write(keypara,data_buf,SPI_SECURESTORAGE_AREA_VALID_SIZE);
		if(err){
			printk("spi secure storage write init value fail,%s:%d\n",__func__,__LINE__);
		}
		kfree(data_buf);
		return err;
	}
	return 0;
}

void spi_securestorage_free(void)
{
#ifdef SPI_SECURE_STORAGE_UBOOT
	struct spi_flash *flash = (struct spi_flash*)spi_securestorage_flash;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	struct spi_nor *flash = (struct spi_nor*)spi_securestorage_flash;
#endif
	if(spi_securestorage_flash){
#ifdef SPI_SECURE_STORAGE_UBOOT
		if(flash->securestorage_info){
			kfree(flash->securestorage_info);
			flash->securestorage_info = NULL;
		}
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
		if(flash->securestorage_info){
			kfree(flash->securestorage_info);
			flash->securestorage_info = NULL;
		}
#endif
	}
	spi_securestorage_flash = NULL;
}

int spi_securestorage_probe(void *keypara)
{
	int err = 0;
	u64 addr,size;
#ifdef SPI_SECURE_STORAGE_UBOOT
	struct spi_flash *flash = (struct spi_flash*)keypara;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	//struct mtd_info *mtd = (struct mtd_info*)keypara;
	struct spi_nor *flash = (struct spi_nor*)keypara;
#endif
	struct aml_spisecurestorage_info_t *securestorage_info;
#ifdef SPI_SECURE_STORAGE_UBOOT
	if(flash->size < SPI_MIN_ROOM_SIZE){
		printk("spi can't setup secure storage,flash size:0x%x is smaller than 0x%x\n",flash->size,SPI_MIN_ROOM_SIZE);
		return -1;
	}
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	if(flash->mtd.size < SPI_MIN_ROOM_SIZE){
		printk("spi can't setup secure storage,flash size:0x%llx is smaller than 0x%x\n",flash->mtd.size,SPI_MIN_ROOM_SIZE);
		return -1;
	}
#endif
	securestorage_info = kzalloc(sizeof(*securestorage_info), GFP_KERNEL);
	if(securestorage_info == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}
	memset(securestorage_info,0,sizeof(*securestorage_info));
#ifdef SPI_SECURE_STORAGE_UBOOT
	flash->securestorage_info = securestorage_info;
	flash->secure_protect = 1;
	addr = SPI_SECURESTORAGE_OFFSET;
	size = SPI_SECURESTORAGE_AREA_SIZE * SPI_SECURESTORAGE_AREA_COUNT;
#endif
#ifdef SPI_SECURE_STORAGE_KERNEL
	flash->securestorage_info = securestorage_info;
	flash->secure_protect = 1;
	addr = SPI_SECURESTORAGE_OFFSET;
	size = SPI_SECURESTORAGE_AREA_SIZE * SPI_SECURESTORAGE_AREA_COUNT;
#endif
	securestorage_info->start_pos = addr;
	securestorage_info->end_pos = addr + size;//valid room: [start_pos:end_pos)

	err = spi_securestorage_check(keypara);
	if(!err){
		spi_securestorage_flash = keypara;
		printk("spi secure storage start position:0x%llx,end position:%llx ok\n",addr,(addr+size));
	}
	else{
		printk("spi secure storage fail,%s:%d\n",__func__,__LINE__);
	}
	return err;
}
int secure_storage_spi_write(u8 *buf,u32 len)
{
	int err;
	if(len > SPI_SECURESTORAGE_AREA_VALID_SIZE){
		printk("spi secure storage write fail,len 0x%x is bigger than 0x%x,%s:%d\n",len,SPI_SECURESTORAGE_AREA_VALID_SIZE,__func__,__LINE__);
		return -1;
	}
	if(spi_securestorage_flash == NULL){
		printk("spi secure storage not init,please init spi,%s:%d\n",__func__,__LINE__);
		return -1;
	}
	err = spi_securestorage_write(spi_securestorage_flash,buf, len);
	return err;
}

int secure_storage_spi_read(u8 *buf,u32 len)
{
	u32 size;
	int err;
	if(spi_securestorage_flash == NULL){
		printk("spi secure storage not init,please init spi,%s:%d\n",__func__,__LINE__);
		return -1;
	}
	if(len>SPI_SECURESTORAGE_AREA_VALID_SIZE){
		size = SPI_SECURESTORAGE_AREA_VALID_SIZE;
	}
	else{
		size = len;
	}
	err = spi_securestorage_read(spi_securestorage_flash,buf, size);
	return err;
}

#ifdef CONFIG_SPI_SECURE_STORAGE_TEST
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>

//#define KEYS_MODULE_NAME    "aml_keys_t"
#define SPISSKEY_DRIVER_NAME	"aml_keys_s"
#define SPISSKEY_DEVICE_NAME    "aml_spisskey"
#define SPISSKEY_CLASS_NAME     "aml_spisskey"

static dev_t spisskey_devno;

static struct device * spisskey_device= NULL;

typedef struct spisskey_dev_s
{
    struct cdev cdev;
    unsigned int flags;
}spisskey_dev_t;
spisskey_dev_t *spisskey_devp;

static const struct file_operations spisskey_fops =
    { .owner = THIS_MODULE,
      .open = NULL,//aml_keys_open,
      .release = NULL,//aml_keys_release,
      .unlocked_ioctl = NULL,//aml_keys_unlocked_ioctl, 
    };

/*
static struct device_attribute keys_class_attrs[] ={
	__ATTR_RO(version_available),
	__ATTR_RO(installed_keys), 
	__ATTR_NULL 
};
*/
static struct class spisskey_class ={
	.name = SPISSKEY_CLASS_NAME,
	.dev_attrs = NULL, //keys_class_attrs, 
};

static char ASCIItohex(char c)
{
	if(c>='0' && c<='9'){c -= '0';}
	else if(c>='a' && c<='f'){c =c - 'a' + 10;}
	else if(c>='A' && c<= 'F'){c =c - 'A' + 10;}
	else{return -1;	}
	return c;
}
static unsigned int ASCIItoAddr(char *buf,int len)
{
	unsigned int addr;
	int i,temlenth;
	char c;
	if(len>8){temlenth = 8;}
	else{temlenth = len;}
	addr = 0;
	for(i=0;i<temlenth;i++){
		c = ASCIItohex(buf[i]);
		if(c == -1){
			break;
		}
		addr = (addr<<4)|c;
	}
	return addr;
}

static ssize_t read_show(struct device *dev, struct device_attribute *attr,
                            char *buf)
{
	int i,n;
    int ret = -EINVAL;
    char test_buf[201];
	memset(test_buf,0,201);
	ret = secure_storage_spi_read(test_buf,200);
	if(ret == 0){
		for(i=0,n=0;i<200;i++){
			n += sprintf(&buf[n], "%02x", test_buf[i]);
		}
		buf[n++] = 0;
		printk("%s:%d: test \n",__func__,__LINE__);
		//memcpy(buf,"123456879",10);
		return n;
	}
	ret = -EINVAL;
	return ret;
}
static ssize_t read_store(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count)
{
    int ret = -EINVAL;
    char test_buf[20];

	memset(test_buf,0,20);
	memcpy(test_buf,buf,19);
	printk("%s:%d: test \n",__func__,__LINE__);
	ret = secure_storage_spi_write(test_buf,19);
	if(ret == 0){
		return count;
	}
	ret = -EINVAL;
	return ret;
}
DEVICE_ATTR(read, 0660, read_show, read_store);

static ssize_t erase_show(struct device *dev, struct device_attribute *attr,
                            char *buf)
{
	int ret = -EINVAL;
	return ret;
}
static ssize_t erase_store(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count)
{
/*
	int i,addr,size;
	struct erase_info instr;
	struct spi_nor *flash = (struct spi_nor*)spi_securestorage_flash;
	memset(&instr,0,sizeof(struct erase_info));
	addr = ASCIItoAddr((char*)buf,count);
	for(i=0;i<count;i++){
		if(buf[i]=='s'){
			i+=1;
			break;
		}
	}
	size = ASCIItoAddr((char*)&buf[i],count-i);
	printk("addr:0x%x,size:0x%x\n",addr,size);
	addr = 0x110000;
	size = 0x81;
	size = size * 0x1000;
	instr.mtd = &flash->mtd;
	instr.addr = addr;
	instr.len = size;
	if(!flash->mtd.erase(&flash->mtd,&instr)){
		printk("%s:%d:erase ok\n",__func__,__LINE__);
	}
	else{
		printk("%s:%d:erase fail\n",__func__,__LINE__);
	}
*/
	return count;
}
DEVICE_ATTR(erase, 0660, erase_show, erase_store);

static int aml_spisskey_probe(struct platform_device *pdev)
{
    int ret;
    //int32_t version;
    struct device *devp;
    printk(KERN_INFO "spisskey===========================================\n");
    ret = alloc_chrdev_region(&spisskey_devno, 0, 1, SPISSKEY_DEVICE_NAME);
    if (ret < 0)
    {
        printk(KERN_ERR "spisskey: failed to allocate major number\n");
        ret = -ENODEV;
        goto out;
    }
    printk("spisskey_devno=%x\n", spisskey_devno);

    ret = class_register(&spisskey_class);
    if (ret)
        goto error1;

    spisskey_devp = kmalloc(sizeof(spisskey_dev_t), GFP_KERNEL);
    if (!spisskey_devp)
    {
        printk(KERN_ERR "spisskey: failed to allocate memory\n");
        ret = -ENOMEM;
        goto error2;
    }

    /* connect the file operations with cdev */
    cdev_init(&spisskey_devp->cdev, &spisskey_fops);
    spisskey_devp->cdev.owner = THIS_MODULE;
    /* connect the major/minor number to the cdev */
    ret = cdev_add(&spisskey_devp->cdev, spisskey_devno, 1);
    if (ret)
    {
        printk(KERN_ERR "spisskey: failed to add device\n");
        goto error3;
    }

    devp = device_create(&spisskey_class, NULL, spisskey_devno, NULL, SPISSKEY_DEVICE_NAME);
    if (IS_ERR(devp))
    {
        printk(KERN_ERR "spisskey: failed to create device node\n");
        ret = PTR_ERR(devp);
        goto error4;
    }
    printk(KERN_INFO "spisskey: device %s created\n", SPISSKEY_DEVICE_NAME);
    if (pdev->dev.platform_data) ///@todo add some optimize here
        devp->platform_data = pdev->dev.platform_data;
    else
        devp->platform_data = NULL;
    spisskey_device = devp;
/*    if ((version = version_check()) < 0)
    {
        printk(KERN_ERR KEYS_DEVICE_NAME ": can not get current version\n");

    } else
    {
        dev_attr_version.attr.mode = 0660;
    }
    */
    /*
    ret = device_create_file(devp, &dev_attr_version);
    if (ret < 0)
    {
        ret = -ENOMEM; ///change error
        printk("============\n");
        goto error4;
    }
    */
    ret = device_create_file(devp, &dev_attr_read);
    if (ret < 0)
    {
        ret = -ENOMEM; ///change error
        printk("============\n");
        goto error4;
    }
/*    ret = device_create_file(devp, &dev_attr_erase);
    if (ret < 0)
    {
        ret = -ENOMEM; ///change error
        printk("============\n");
        goto error4;
    }
    */
    return 0;

    error4: cdev_del(&spisskey_devp->cdev);
    error3: kfree(spisskey_devp);
    error2: class_unregister(&spisskey_class);
    error1: unregister_chrdev_region(spisskey_devno, 1);
    out: return ret;
}

static int aml_spisskey_remove(struct platform_device *pdev)
{
    unregister_chrdev_region(spisskey_devno, 1);
    //device_destroy(efuse_clsp, efuse_devno);
    device_destroy(&spisskey_class, spisskey_devno);
    cdev_del(&spisskey_devp->cdev);
    kfree(spisskey_devp);
    //class_destroy(efuse_clsp);
    class_unregister(&spisskey_class);
    return 0;
}

//static char * secure_device[2]={"nand_key",NULL};
static struct platform_device aml_spisskey_device = {
    .name   = "aml_spisskey",
    .id = -1,
    .dev = {
                .platform_data = NULL,//&secure_device[0],
           },
};

static struct platform_driver aml_spisskey_driver =
{ 
	.probe = aml_spisskey_probe, 
	.remove = aml_spisskey_remove, 
	.driver =
     {
     	.name = SPISSKEY_DEVICE_NAME, 
        .owner = THIS_MODULE, 
     }, 
};

static int __init aml_spisskey_init(void)
{
    int ret = -1;

    ret = platform_driver_register(&aml_spisskey_driver);
    if (ret != 0)
    {
        printk(KERN_ERR "failed to register aml_spisskey driver, error %d\n", ret);
        return -ENODEV;
    }
    printk(KERN_INFO "platform_driver_register--aml_spisskey_driver--------------------\n");

	ret = platform_device_register(&aml_spisskey_device);
	if(ret != 0){
        printk(KERN_ERR "failed to register aml_spisskey device, error %d\n", ret);
	}
    printk(KERN_INFO "platform_device_register--aml_spisskey_driver--------------------\n");
	
    return ret;
}

static void __exit aml_spisskey_exit(void)
{
	platform_device_unregister(&aml_spisskey_device);
    platform_driver_unregister(&aml_spisskey_driver);
}

module_init(aml_spisskey_init);
module_exit(aml_spisskey_exit);

MODULE_DESCRIPTION("Amlogic keys driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zbl <benlong.zhou@amlogic.com>");


#endif


