/*
 * res_server.c
 *
 *  Created on: May 15, 2013
 *      Author: amlogic
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/amports/canvas.h>
#include <linux/vout/vinfo.h>
#include <linux/vout/vout_notify.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vfp.h>
#include <linux/amports/vframe_provider.h>
#include <linux/amports/vframe_receiver.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/list.h>

#include "vressvr.h"
#include "dec_vf_groups.h"

#include "res_server_api_test.h"

#define SLOT_NUM 2
#define PROVIDER_NAME  "resmgr"

static const struct vframe_operations_s vf_provider =
{
    .peek = NULL,
};

static struct vframe_provider_s vf_prov;

struct codec_memory_entry_s {
	phys_addr_t paddr;
	size_t size;
	u32 alloced;
};

struct codec_memory_group_s {
	phys_addr_t paddr;
	size_t size;
	u32 alloced_number;
	struct codec_memory_entry_s list[SLOT_NUM];
};

static DEFINE_SPINLOCK(ressvr_vfp_lock);
static DEFINE_SPINLOCK(ressrv_mem_lock);

static struct dec_vfp_list_s dec_vfp_list;
static struct codec_memory_group_s codec_memory_group;

phys_addr_t resmgr_codec_mem_alloc(size_t size)
{
	int i;
	ulong flags;
	phys_addr_t ret = 0;

	if (codec_memory_group.alloced_number < SLOT_NUM) {
		spin_lock_irqsave(&ressrv_mem_lock, flags);
		for (i = 0; i< SLOT_NUM; i++) {
			if (codec_memory_group.list[i].alloced == 0) {
				codec_memory_group.list[i].alloced = 1;
				ret = codec_memory_group.list[i].paddr;
				codec_memory_group.alloced_number++;
				break;
			}
		}
		spin_unlock_irqrestore(&ressrv_mem_lock, flags);
	}
	return ret;
}

void resmgr_codec_mem_free(u32 addr)
{
	int i;
	ulong flags;
	spin_lock_irqsave(&ressrv_mem_lock, flags);
	for (i = 0; i< SLOT_NUM; i++) {
		if (codec_memory_group.list[i].paddr == addr) {
			codec_memory_group.list[i].alloced = 0;
			codec_memory_group.alloced_number--;
		}
	}
	spin_unlock_irqrestore(&ressrv_mem_lock, flags);
}

void resmgr_dump_codec_mem(void) {
	int i;
	VRSVR_LOG(LOG_LEVEL_HIGH, "\n***********resmgr_dump_codec_mem************\n");
	VRSVR_LOG(LOG_LEVEL_HIGH, "dump_codec_mem   \n");
	VRSVR_LOG(LOG_LEVEL_HIGH,  "\talloced_number:%d.\n", codec_memory_group.alloced_number);
	VRSVR_LOG(LOG_LEVEL_HIGH,  "\tmemory address:%x.\n", codec_memory_group.paddr);
	VRSVR_LOG(LOG_LEVEL_HIGH,  "\tmemory size:%x.\n", codec_memory_group.size);
	for (i = 0; i< SLOT_NUM; i++) {
		VRSVR_LOG(LOG_LEVEL_HIGH,  "\titems %4d:\n",i);
		VRSVR_LOG(LOG_LEVEL_HIGH,  "\t\talloced:%4d.\n", codec_memory_group.list[i].alloced);
		VRSVR_LOG(LOG_LEVEL_HIGH,  "\t\tpaddr:%32x.\n", codec_memory_group.list[i].paddr);
		VRSVR_LOG(LOG_LEVEL_HIGH,  "\t\tsize:%32x.\n", codec_memory_group.list[i].size);
	}

	VRSVR_LOG(LOG_LEVEL_HIGH, "************** end ******************\n\n");
}

int resmgr_dec_vfp_register(struct vframe_operations_s *ops, u32* id)
{
	ulong flags;
	int i;
	int ret = -1;

	if (ops == NULL) ret = -1;
	if (dec_vfp_list.number >= DEC_VFP_LIST_LEN) {
		VRSVR_LOG(
		                LOG_LEVEL_ERROR,
		                "Too many decoder registered.\n");
		ret = -1;
	}
	spin_lock_irqsave(&ressvr_vfp_lock, flags);
	for (i = 0; i < DEC_VFP_LIST_LEN; i++) {
		if (dec_vfp_list.group[i].id < 0) {
			dec_vfp_list.group[i].id = i;
			dec_vfp_list.group[i].vfp = ops;
			dec_vfp_list.number++;
			*id = i;
			ret = 0;
			break;
		}
	}
	if (dec_vfp_list.number == 1) {
		spin_unlock_irqrestore(&ressvr_vfp_lock, flags);
		vf_reg_provider(&vf_prov);
		vf_notify_receiver(PROVIDER_NAME,VFRAME_EVENT_PROVIDER_START,NULL);
	} else {
		spin_unlock_irqrestore(&ressvr_vfp_lock, flags);
	}

	return ret;
}

int resmgr_dec_vfp_unregister(u32 id)
{
	ulong flags;
	int ret = -1;
	spin_lock_irqsave(&ressvr_vfp_lock, flags);

	if (id == dec_vfp_list.group[id].id) {
		dec_vfp_list.group[id].id = -1;
		dec_vfp_list.group[id].vfp = 0;
		dec_vfp_list.number--;
		ret = 0;
	}
	if (dec_vfp_list.number == 0) {
		spin_unlock_irqrestore(&ressvr_vfp_lock, flags);
		vf_unreg_provider(&vf_prov);
	} else {
		spin_unlock_irqrestore(&ressvr_vfp_lock, flags);
	}

	return ret;
}

int resmgr_get_vfp_by_id(u32 id, struct dec_vfp_entry_s** pentry)
{
	int ret =-1;
	if (id >= DEC_VFP_LIST_LEN)
		return ret;
	if (dec_vfp_list.group[id].id == id) {
		*pentry = &dec_vfp_list.group[id];
	} else
		*pentry = NULL;

	return ret;
}

int resmgr_get_decoder_num()
{
	return dec_vfp_list.number;
}

void resmgr_dump_vfp_list(void)
{
	int i;
	VRSVR_LOG(LOG_LEVEL_HIGH, "\n**** resmgr_dump_vfp_list ****\n");
	VRSVR_LOG(LOG_LEVEL_HIGH, "\tdump_vfp_list   \n");
	VRSVR_LOG(LOG_LEVEL_HIGH,  "\tnumber:%d.\n", dec_vfp_list.number);

	for (i = 0; i< DEC_VFP_LIST_LEN; i++) {
		VRSVR_LOG(LOG_LEVEL_HIGH,  "\titems %4d:\n",i);
		VRSVR_LOG(LOG_LEVEL_HIGH,  "\t\tid:%4d.\n", dec_vfp_list.group[i].id);
		VRSVR_LOG(LOG_LEVEL_HIGH,  "\t\tvfpAddr:%32x.\n", (u32)dec_vfp_list.group[i].vfp);
	}

	VRSVR_LOG(LOG_LEVEL_HIGH, "**** dump end ****\n\n");
}

void resmgr_probe_registered_items(void)
{
	int id;
	struct dec_vfp_entry_s* entry;
	VRSVR_LOG(LOG_LEVEL_HIGH, "\n---- dump registered VFPs. ----\n");
	for (id = 0; id < DEC_VFP_LIST_LEN; id++) {
		resmgr_get_vfp_by_id(id, &entry);
		if (entry)
			VRSVR_LOG(LOG_LEVEL_HIGH,
			                "  registered item:%x.\n", entry->vfp);
	}
	VRSVR_LOG(LOG_LEVEL_HIGH,"---- end dump  ---\n\n");
}

void init_dec_resource_manager(phys_addr_t paddr, u32 size)
{
	int i;
	size_t slot_size;

	dec_vfp_list.number = 0;
	for(i =0; i< DEC_VFP_LIST_LEN; i++) {
		dec_vfp_list.group[i].id = -1;
	}

	codec_memory_group.alloced_number = 0;
	codec_memory_group.paddr = paddr;
	codec_memory_group.size = size;
	slot_size = size / SLOT_NUM;
	slot_size = (slot_size >>4) << 4;
	for (i = 0; i < SLOT_NUM; i++) {
		codec_memory_group.list[i].paddr = slot_size * i + paddr;
		codec_memory_group.list[i].size = slot_size;
		codec_memory_group.list[i].alloced = 0;
	}
	vf_provider_init(&vf_prov, PROVIDER_NAME ,&vf_provider, NULL);
#ifdef RES_SERVER_API_TEST_H_
	resource_server_api_test ();
#endif /*  RES_SERVER_API_TEST_H_ */
}
