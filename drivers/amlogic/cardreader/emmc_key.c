
#include <linux/types.h>
#include "emmc_key.h"
//#include <linux/device.h>
//#include <linux/blkdev.h>
#if defined(EMMC_KEY_KERNEL)
#include <linux/cardreader/card_block.h>
#include <linux/err.h>
#include "card_data.h"
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include "sd/sd_protocol.h"
#include <linux/efuse.h>
#endif
/*
 * kernel head file
 * 
 ********************************** */

#ifdef EMMC_KEY_UBOOT
#include <ubi_uboot.h>
#include <mmc.h>
#include <amlogic/securitykey.h>
#endif
/*
 * uboot head file
 * ****************************/

static u32 emmckey_calculate_checksum(u8 *buf,u32 lenth)
{
	u32 checksum = 0;
	u32 cnt;
	for(cnt=0;cnt<lenth;cnt++){
		checksum += buf[cnt];
	}
	return checksum;
}
static u32 uint_to_shift(u32 value)
{
	u32 shift=0;
	while(value){
		if(value > 1){
			shift++;
		}
		value >>= 1;
	}
	return shift;
}
static int emmc_key_transfer(u8 *buf,u32 *value,u32 len,u32 direct)
{
	u8 checksum=0;
	u32 i;
	if(direct){
		for(i=0;i<len;i++){
			checksum += buf[i];
		}
		for(i=0;i<len;i++){
			buf[i] ^= checksum;
		}
		*value = checksum;
		return 0;
	}
	else{
		checksum = *value;
		for(i=0;i<len;i++){
			buf[i] ^= checksum;
		}
		checksum = 0;
		for(i=0;i<len;i++){
			checksum += buf[i];
		}
		if(checksum == *value){
			return 0;
		}
		return -1;
	}
}

#if defined(EMMC_KEY_KERNEL)
//static struct card_queue *memory_card_to_card_queue(struct memory_card *emmccard)
//{
//	return container_of(emmccard, struct card_queue, card);
//}

static int card_claim_card_get(struct memory_card *card)
{
	if(card->card_status == CARD_REMOVED){
		printk("%s:%d,card->card_status: removed\n",__func__,__LINE__);
		return -ENODEV;
	}
	printk("%s:%d,card->card_status:%d\n",__func__,__LINE__,card->card_status);
	return __card_claim_host(card->host, card);
}
static void card_claim_card_put(struct card_host *host)
{
	card_release_host(host);
}

static int emmc_key_kernel_rw(struct memory_card *emmccard,struct emmckey_valid_node_t *emmckey_valid_node,u8 *buf,u32 direct)
{
	struct card_blk_data *card_data = card_get_drvdata(emmccard);
	//SD_MMC_Card_Info_t *sd_mmc_info = (SD_MMC_Card_Info_t *)emmccard->card_info;
	struct card_blk_request *brq;
	//struct card_blk_request *brq2;
	struct card_queue *cq;
	struct request_queue *q ;
	struct request *req;
	u32 blk_shift,blk_cnt,blk_count;
	u64 pos;
	int ret;
	//cq = memory_card_to_card_queue(emmccard);
	cq = &card_data->queue;
	q = cq->queue;
	if(cq){
		//printk("%s:%d,cq exist cq:0x%x,cq->bounce_buf:0x%x\n",__func__,__LINE__,(unsigned int)cq,cq->bounce_buf);
		//printk("%s:%d,cq->req:0x%x,cq->queue:0x%x,cq->sg:0x%x,cq->bounce_sg:0x%x,cq->bounce_sg_len:0x%x\n",
		//__func__,__LINE__,cq->req,cq->queue,cq->sg,cq->bounce_sg,cq->bounce_sg_len);
	}
	
	brq = kzalloc(sizeof(*brq), GFP_KERNEL);
	brq->crq.buf = cq->bounce_buf;
	//printk("%s:%d sd_mmc_phy_buf:0x%x, sd_mmc_buf:0x%x\n",__func__,__LINE__,(unsigned int)sd_mmc_info->sd_mmc_phy_buf,(unsigned int)sd_mmc_info->sd_mmc_buf);
	
	//emmccard->card_io_init(emmccard);
	//emmccard->card_detector(emmccard);
	//emmccard->card_insert_process(emmccard);
	//printk("%s:%d,sd_mmc_info->blk_len:%d\n",__func__,__LINE__,1 << card_data->block_bits);

	brq->card_data.sg = cq->sg;
	brq->card_data.sg_len = 1;//card_queue_map_sg(cq);
	//printk("%s:%d,cq->sg:0x%x \n",__func__,__LINE__,(unsigned int)cq->sg);

	ret = card_claim_card_get(emmccard);
	if(ret){
		printk("%s:%d,card_claim_card_get fail,err:%d\n",__func__,__LINE__,ret);
	}
	
	//printk("%s:%d,host->max_blk_size:%d,host->max_blk_count:%d,\n",
	//	__func__,__LINE__,emmccard->host->max_blk_size,emmccard->host->max_blk_count);
	/*note: emmccard->host->max_blk_size is equal (1 << card_data->block_bits)*/
	brq->card_data.blk_size = 1 << card_data->block_bits;
	blk_shift = card_data->block_bits;
	blk_count = emmckey_valid_node->phy_size>>blk_shift;
	pos = 0;
	blk_cnt = 0;
	emmccard->key_protect = 0;
	while(blk_count){
		if(blk_count > emmccard->host->max_blk_count){
			brq->card_data.blk_nums = emmccard->host->max_blk_count;
		}
		else{
			brq->card_data.blk_nums = blk_count;
		}

		brq->card_data.lba = (emmckey_valid_node->phy_addr>>blk_shift) + blk_cnt;
		if(direct){
			brq->crq.cmd = WRITE;
			memcpy(brq->crq.buf,&buf[pos],brq->card_data.blk_size * brq->card_data.blk_nums);
		}
		else{
			brq->crq.cmd = READ;
		}

		emmccard->host->card_type = emmccard->card_type;
		card_wait_for_req(emmccard->host, brq);
		if(brq->card_data.error != 0){
			card_claim_card_put(emmccard->host);
			printk("%s:%d save key data to emmc fail,error:%d\n",__func__,__LINE__,brq->card_data.error);
			goto error;
		}
		if(!direct){//read
			memcpy(&buf[pos],brq->crq.buf,brq->card_data.blk_size * brq->card_data.blk_nums);
		}
		pos += (brq->card_data.blk_size * brq->card_data.blk_nums);
		blk_cnt += brq->card_data.blk_nums ;
		blk_count -= brq->card_data.blk_nums;
	}
	card_claim_card_put(emmccard->host);
error:
	if(brq){
		kfree(brq);
	}
	emmccard->key_protect = 1;
	return brq->card_data.error;
}
#endif

#if defined(EMMC_KEY_UBOOT)

static int emmc_key_uboot_rw(struct mmc *emmccard,struct emmckey_valid_node_t *emmckey_valid_node,u8 *buf,u32 direct)
{
	u64 blk,cnt,n;
	int dev;
	
	blk = emmckey_valid_node->phy_addr;
	cnt = emmckey_valid_node->phy_size;
	dev = emmccard->block_dev.dev;
	mmc_init(emmccard);
	emmccard->key_protect = 0;
	if(direct){
		blk >>= uint_to_shift(emmccard->write_bl_len);
		cnt >>= uint_to_shift(emmccard->write_bl_len);
		n = emmccard->block_dev.block_write(dev, blk, cnt, buf);
	}
	else{
		blk >>= uint_to_shift(emmccard->read_bl_len);
		cnt >>= uint_to_shift(emmccard->read_bl_len);
		n = emmccard->block_dev.block_read(dev, blk, cnt, buf);
	}
	emmccard->key_protect = 1;
	return 0;
}
#endif

static int emmc_key_rw(aml_keybox_provider_t *provider,struct emmckey_valid_node_t *emmckey_valid_node,u8 *buf,u32 direct)
{
#if defined(EMMC_KEY_KERNEL)
	struct memory_card *emmccard = (struct memory_card *)provider->priv;
#elif defined(EMMC_KEY_UBOOT)
	struct mmc *emmccard = (struct mmc*)provider->priv;
#endif
#if defined(EMMC_KEY_KERNEL)
	return emmc_key_kernel_rw(emmccard,emmckey_valid_node, buf,direct);
#elif defined(EMMC_KEY_UBOOT)
	return emmc_key_uboot_rw(emmccard,emmckey_valid_node,buf,direct);
#endif
}

static int aml_emmc_key_check(aml_keybox_provider_t *provider)
{
	u8 keypart_cnt;
	u64 part_size;
	u32 checksum;
#if defined(EMMC_KEY_KERNEL)
	struct memory_card *emmccard = (struct memory_card *)provider->priv;
#elif defined(EMMC_KEY_UBOOT)
	struct mmc *emmccard = (struct mmc*)provider->priv;
#endif
	struct aml_emmckey_info_t *emmckey_info = emmccard->aml_emmckey_info;
	struct emmckey_valid_node_t *emmckey_valid_node,*temp_valid_node;
	struct emmckey_data_t *emmckey_data;
	emmckey_info->key_part_count = emmckey_info->keyarea_phy_size / EMMC_KEYAREA_SIZE;
	if(emmckey_info->key_part_count > EMMC_KEYAREA_COUNT){
		emmckey_info->key_part_count = EMMC_KEYAREA_COUNT;
	}
	keypart_cnt = 0;
	part_size = EMMC_KEYAREA_SIZE;
	do{
		emmckey_valid_node = kzalloc(sizeof(*emmckey_valid_node), GFP_KERNEL);
		if(emmckey_valid_node == NULL){
			printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
			return -ENOMEM;
		}
		emmckey_valid_node->phy_addr = emmckey_info->keyarea_phy_addr + part_size * keypart_cnt;
		emmckey_valid_node->phy_size = EMMC_KEYAREA_SIZE;
		emmckey_info->key_valid = 0;
		if(emmckey_info->key_valid_node == NULL){
			emmckey_info->key_valid_node = emmckey_valid_node;
		}
		else{
			temp_valid_node = emmckey_info->key_valid_node;
			while(temp_valid_node->next != NULL){
				temp_valid_node = temp_valid_node->next;
			}
			temp_valid_node->next = emmckey_valid_node;
		}
	}while(++keypart_cnt < emmckey_info->key_part_count);
	
#if 0
	/*read key data from emmc key area*/
	temp_valid_node = emmckey_info->key_valid_node;
	if(temp_valid_node == NULL){
		printk("%s:%d,don't find emmc key valid node\n",__func__,__LINE__);
		return -1;
	}
	emmckey_data = kzalloc(sizeof(*emmckey_data), GFP_KERNEL);
	if(emmckey_data == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}
	/*read key data */
	memset(emmckey_data,0,sizeof(*emmckey_data));
	emmc_key_rw(provider,temp_valid_node,(u8*)emmckey_data,0);
	if (!memcmp(emmckey_data->keyarea_mark, EMMC_KEY_AREA_SIGNAL, 8)) {
		checksum = emmckey_calculate_checksum(emmckey_data->data,EMMCKEY_DATA_VALID_LEN);
		if(checksum == emmckey_data->checksum){
			emmckey_info->key_valid = 1;
		}
	}
	/*write key data to emmc key area*/
	if(emmckey_info->key_valid == 0){
		temp_valid_node = emmckey_info->key_valid_node;
		while(temp_valid_node){
			memset(emmckey_data,0,sizeof(*emmckey_data));
			memcpy(emmckey_data->keyarea_mark, EMMC_KEY_AREA_SIGNAL, 8);
			emmckey_data->checksum = emmckey_calculate_checksum(emmckey_data->data,EMMCKEY_DATA_VALID_LEN);
			emmc_key_rw(provider,temp_valid_node,(u8*)emmckey_data,1);
			temp_valid_node = temp_valid_node->next;
		}
		emmckey_info->key_valid = 1;
	}
	kfree(emmckey_data);
#else
	emmckey_info->key_valid = 1;
#endif
	return 0;
}

static int32_t emmc_keybox_read(aml_keybox_provider_t * provider, uint8_t *buf,int32_t size,int flags)
{
	int err = -1;
	u32 checksum;
#if defined(EMMC_KEY_KERNEL)
	struct memory_card *emmccard = (struct memory_card *)provider->priv;
#elif defined(EMMC_KEY_UBOOT)
	struct mmc *emmccard = (struct mmc*)provider->priv;
#endif
	struct aml_emmckey_info_t *emmckey_info = emmccard->aml_emmckey_info;
	struct emmckey_valid_node_t *emmckey_valid_node;
	struct emmckey_data_t *emmckey_data;

	if(!emmckey_info->key_valid){
		printk("%s:%d,can't read emmc key\n",__func__,__LINE__);
		return -1;
	}
	if(size > EMMCKEY_DATA_VALID_LEN){
		printk("%s:%d,size is too big,fact:0x%x,need:0x%x\n",__func__,__LINE__,size,EMMCKEY_DATA_VALID_LEN);
		return -1;
	}
	emmckey_data = kzalloc(sizeof(*emmckey_data), GFP_KERNEL);
	if(emmckey_data == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}

	emmckey_valid_node = emmckey_info->key_valid_node;
	while(emmckey_valid_node){
		memset(emmckey_data,0,sizeof(*emmckey_data));
		err = emmc_key_rw(provider,emmckey_valid_node,(u8*)emmckey_data,0);
		if(err == 0){
			emmc_key_transfer(emmckey_data->keyarea_mark,&emmckey_data->keyarea_mark_checksum,8,0);
			if (!memcmp(emmckey_data->keyarea_mark, EMMC_KEY_AREA_SIGNAL, 8)){
				checksum = emmckey_calculate_checksum(emmckey_data->data,EMMCKEY_DATA_VALID_LEN);
				if(checksum == emmckey_data->checksum){
					memcpy(buf,emmckey_data->data,size);
					err = 0;
					break;
				}
			}
		}
		emmckey_valid_node = emmckey_valid_node->next;
	}
	if(emmckey_data){
		kfree(emmckey_data);
	}
	return err;
}


static int32_t emmc_keybox_write(aml_keybox_provider_t * provider, uint8_t *buf,int32_t size)
{
	int err = 0;
#if defined(EMMC_KEY_KERNEL)
	struct memory_card *emmccard = (struct memory_card *)provider->priv;
#elif defined(EMMC_KEY_UBOOT)
	struct mmc *emmccard = (struct mmc*)provider->priv;
#endif
	struct aml_emmckey_info_t *emmckey_info = emmccard->aml_emmckey_info;
	struct emmckey_valid_node_t *emmckey_valid_node;
	struct emmckey_data_t *emmckey_data;

	if(!emmckey_info->key_valid){
		printk("%s:%d,can't write emmc key\n",__func__,__LINE__);
		return -1;
	}
	if(size > EMMCKEY_DATA_VALID_LEN){
		printk("%s:%d,size is too big,fact:0x%x,need:0x%x\n",__func__,__LINE__,size,EMMCKEY_DATA_VALID_LEN);
		return -1;
	}
	emmckey_data = kzalloc(sizeof(*emmckey_data), GFP_KERNEL);
	if(emmckey_data == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}
	
	memset(emmckey_data,0,sizeof(*emmckey_data));
	memcpy(emmckey_data->keyarea_mark, EMMC_KEY_AREA_SIGNAL, 8);
	emmc_key_transfer(emmckey_data->keyarea_mark,&emmckey_data->keyarea_mark_checksum,8,1);
	memcpy(emmckey_data->data,buf,size);
	emmckey_data->checksum = emmckey_calculate_checksum(emmckey_data->data,EMMCKEY_DATA_VALID_LEN);
	emmckey_valid_node = emmckey_info->key_valid_node;
	while(emmckey_valid_node){
		err = emmc_key_rw(provider,emmckey_valid_node,(u8*)emmckey_data,1);
		if(err != 0){
			printk("%s:%d,write key data fail,err:%d\n",__func__,__LINE__,err);
		}
		emmckey_valid_node = emmckey_valid_node->next;
	}
	if(emmckey_data){
		kfree(emmckey_data);
	}
	return err;
}


/*
 * 1  when key data is wrote to emmc, add checksum to verify if the data is correct;
 *    when key data is read from emmc, use checksum to verify if the data is correct.
 * 2  read/write size is constant 
 * 3  setup link table to link different area same as nand.
 * 4  key area is split 2 or more area to save key data
 * */

#if 0
//static char write_buf[2][50]={"write 2 : 22222222222","write 3 : 33333333333333"};
static char write_buf[50]={"write 2 : 22222222222"};

static char read_buf[100]={0};

#include <asm-generic/uaccess.h>
static int32_t emmc_keybox_read_file(aml_keybox_provider_t * provider, uint8_t *buf,int32_t size,int flags)
{
	char filename[3][30]={"/dev/cardblkinand2","/dev/cardblkinand3","/dev/cardblkinand4"};
	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos;
	int i;
	//char *file = provider->priv;
	char *file;
	memset(read_buf,0,sizeof(read_buf));
	for(i=0;i<3;i++){
		file = &filename[i][0];
		fp = filp_open(file, O_RDWR, 0644); 
		if (IS_ERR(fp)) { 
			printk("open file error:%s \n",file); 
			return -1; 
		}
		printk("open file ok:%s \n",file); 
		fs = get_fs(); 
		set_fs(KERNEL_DS); 
		pos = 0; 
		//buf = &read_buf[i][0];
		//size = 55;
		vfs_read(fp, buf, size, &pos); 
		filp_close(fp, NULL); 
		set_fs(fs); 
		memcpy(&read_buf[0],buf,55);
		printk("%s:%d,read data:%s \n",__func__,__LINE__,&read_buf[0]); 
	}
	return 0;
}
static int32_t emmc_keybox_write_file(aml_keybox_provider_t * provider, uint8_t *buf,int32_t size)
{
	char filename[2][30]={"/dev/cardblkinand2","/dev/cardblkinand3"};
	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos;
	int i;
	char *file;// = provider->priv;
	for(i=0;i<2;i++){
		file = &filename[i][0];
		fp = filp_open(file, O_RDWR, 0644); 
		if (IS_ERR(fp)) { 
			printk("open file error\n"); 
			return -1; 
		} 
		printk("%s:%d,open file ok:%s \n",__func__,__LINE__,file); 
		fs = get_fs(); 
		set_fs(KERNEL_DS); 
		pos = 0; 
		memcpy(buf,&write_buf[0],55);
		//size = 55;
		vfs_write(fp, buf, size, &pos); 
		filp_close(fp, NULL); 
		set_fs(fs); 
	}
	return 0;
}
#endif

static aml_keybox_provider_t emmc_provider={
		.name="emmc_key",
		.read=emmc_keybox_read,
		.write=emmc_keybox_write,
	//	.read=emmc_keybox_read_file,
	//	.write=emmc_keybox_write_file,

};


int emmc_key_init( void *keypara)
{
	int i;
	u64  addr=0;
	u32  size=0,blk_len=0;
	u64  lba_start=0,lba_end=0;
	int err = 0;
#if defined(EMMC_KEY_KERNEL) //kernel
	struct memory_card *card = (struct memory_card*)keypara;
	struct card_blk_data *card_data = card_get_drvdata(card);
#elif defined(EMMC_KEY_UBOOT) //uboot
	struct mmc *card = (struct mmc*)keypara;
#endif
	struct aml_emmckey_info_t *emmckey_info;
	//printk("card key: card_blk_probe. \n");
	emmckey_info = kzalloc(sizeof(*emmckey_info), GFP_KERNEL);
	if(emmckey_info == NULL){
		printk("%s:%d,kzalloc memory fail\n",__func__,__LINE__);
		return -ENOMEM;
	}
	emmckey_info->key_init = 0;
	card->aml_emmckey_info = emmckey_info;

#if defined(EMMC_KEY_KERNEL) 
	#if 0
		if(card->card_plat_info->nr_partitions>0)
		{
			struct mtd_partition    *partitions_start=NULL,*partitions_end=NULL;
			int part_start=-1,part_end=-1;
			
			for(i=0;i<card->card_plat_info->nr_partitions;i++){
				if(strcmp(card->card_plat_info->partitions[i].name,KEY_PREVIOUS_PARTITION)==0){
					partitions_start = &card->card_plat_info->partitions[i];
					part_start = i;
				}
				if(strcmp(card->card_plat_info->partitions[i].name,KEY_LATER_PARTITION)==0){
					partitions_end = &card->card_plat_info->partitions[i];
					part_end = i;
					break;
				}
			}
			if(i>=card->card_plat_info->nr_partitions){
				printk("don't find correct partition name\n");
				return -1;
			}
			if((part_end == -1) || (part_start == -1) ){
				printk("don't find part:%s or part:%s\n",KEY_PREVIOUS_PARTITION,KEY_LATER_PARTITION);
				return -1;
			}
			if(((part_end-part_start) > 1) || ((part_end-part_start) < 0)){
				printk("between %s and %s,interval parts is %d more than 1,or order error\n",KEY_PREVIOUS_PARTITION,KEY_LATER_PARTITION,part_end-part_start);
				return -1;
			}
			addr = partitions_start->offset + partitions_start->size;
			size = partitions_end->offset - addr;
			if(size < EMMC_KEYAREA_SIZE){
				printk("emmc save key area is too smaller,fact size:0x%x,need size:0x%xn\n",size,EMMC_KEYAREA_SIZE);
				return -1;
			}
			if(size > (EMMC_KEYAREA_SIZE * EMMC_KEYAREA_COUNT)){
				size = EMMC_KEYAREA_SIZE * EMMC_KEYAREA_COUNT;
			}
			lba_start = addr >> card_data->block_bits;
			lba_end = lba_start + (size >> card_data->block_bits) + 1;
			emmckey_info->key_init = 1;
			card->key_protect = 1;
			printk("%s:%d,card->capacity:%d,\n",__func__,__LINE__,card->capacity);
			printk("%s:%d emmc key lba_start:0x%x,lba_end:0x%x \n",__func__,__LINE__,lba_start,lba_end);
			//printk("%s:%d,host->max_blk_size:%d,host->max_blk_count:%d,block_bits:%d\n",
			//	__func__,__LINE__,card->host->max_blk_size,card->host->max_blk_count,card_data->block_bits);

		}
	#else
		size = EMMCKEY_AREA_PHY_SIZE;
		addr = card->capacity - (size >> card_data->block_bits); //card->capacity is block number
		addr <<= card_data->block_bits;
		lba_start = card->capacity - (size >> card_data->block_bits); 
		lba_end = card->capacity;
		emmckey_info->key_init = 1;
		card->key_protect = 1;
		//printk("%s:%d,card->capacity:%d,\n",__func__,__LINE__,card->capacity);
		printk("%s:%d emmc key lba_start:0x%llx,lba_end:0x%llx \n",__func__,__LINE__,lba_start,lba_end);
	#endif
#elif defined(EMMC_KEY_UBOOT)
	size = EMMCKEY_AREA_PHY_SIZE;
	addr = (card->capacity - size);
	blk_len = card->write_bl_len;
	lba_start = addr >> uint_to_shift(card->write_bl_len);
	lba_end = card->capacity >> uint_to_shift(card->write_bl_len);
	emmckey_info->key_init = 1;
	card->key_protect = 1;
	printk("%s:%d emmc key lba_start:0x%llx,lba_end:0x%llx \n",__func__,__LINE__,lba_start,lba_end);
#endif
	if(!emmckey_info->key_init){
		printk("%s:%d,emmc key init fail\n",__func__,__LINE__);
		kfree(emmckey_info);
		card->aml_emmckey_info = NULL;
		return -1;
	}
	emmckey_info->keyarea_phy_addr = addr;
	emmckey_info->keyarea_phy_size = size;
	emmckey_info->lba_start = lba_start;
	emmckey_info->lba_end   = lba_end;
	emmckey_info->blk_size = blk_len;
	emmckey_info->blk_shift = uint_to_shift(blk_len);
	emmc_provider.priv=card;
	err = aml_emmc_key_check(&emmc_provider);
	if(err){
		printk("%s:%d,emmc key check fail\n",__func__,__LINE__);
		kfree(emmckey_info);
		card->aml_emmckey_info = NULL;
		return err;
	}
	err = aml_keybox_provider_register(&emmc_provider);
	if(err){
		BUG();
	}
	printk("emmc key: %s:%d ok. \n",__func__,__LINE__);

	return err;
}

