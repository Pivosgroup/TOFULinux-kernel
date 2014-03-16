/*
 * dec_vfp_groups.h
 *
 *  Created on: May 15, 2013
 *      Author: amlogic
 */

#ifndef _DEC_VF_GROUPS_H__
#define _DEC_VF_GROUPS_H__
#define DEC_VFP_LIST_LEN 4

struct dec_vfp_entry_s {
	int id;
	struct vframe_operations_s* vfp;
};
struct dec_vfp_list_s {
	u32 number;
	struct dec_vfp_entry_s group[DEC_VFP_LIST_LEN];
};


extern phys_addr_t resmgr_codec_mem_alloc(size_t size);
extern void resmgr_codec_mem_free(u32 addr);
extern void resmgr_dump_codec_mem (void);
extern int resmgr_dec_vfp_register(struct vframe_operations_s *ops, u32* id);
extern int resmgr_dec_vfp_unregister(u32 id);
extern int resmgr_get_vfp_by_id(u32 id, struct dec_vfp_entry_s** pentry);
extern void resmgr_dump_vfp_list(void);
extern void resmgr_probe_registered_items(void);
extern int resmgr_get_decoder_num();

#endif /* _DEC_VF_GROUPS_H__ */
