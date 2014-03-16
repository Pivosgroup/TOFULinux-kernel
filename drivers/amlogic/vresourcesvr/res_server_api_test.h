/*
 * res_server_api_test.h
 *
 *  Created on: May 16, 2013
 *      Author: amlogic
 */
//#define RES_SERVER_API_TEST_H_
#ifdef RES_SERVER_API_TEST_H_

void resource_server_api_test () {
	/* test memory alloc & free. */
	size_t mem_size = 16 * 1024 * 1024;
	phys_addr_t addr1, addr2, addr3;
	int id;
	struct vframe_operations_s ops1,ops2,ops3;
	u32 id1,id2,id3;

	VRSVR_LOG(LOG_LEVEL_HIGH,"start to test memory allocing & free.\n");

	resmgr_dump_codec_mem();

	addr1 = resmgr_codec_mem_alloc(mem_size);
	VRSVR_LOG(LOG_LEVEL_HIGH, "allocing mem size:%x..\n",mem_size);
	if (addr1 > 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, " success..addr,%x.\n",addr1);
	} else {
		VRSVR_LOG(LOG_LEVEL_HIGH, " fail...\n");
	}

	resmgr_dump_codec_mem();
	addr2 = resmgr_codec_mem_alloc(mem_size);
	VRSVR_LOG(LOG_LEVEL_HIGH, "allocing mem size:%x..\n",mem_size);
	if (addr2 > 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, " success..addr,%x.\n",addr2);
	} else {
		VRSVR_LOG(LOG_LEVEL_HIGH, " fail...\n");
	}

	resmgr_dump_codec_mem();
	addr3 = resmgr_codec_mem_alloc(mem_size);
	VRSVR_LOG(LOG_LEVEL_HIGH, "allocing mem size:%x..\n",mem_size);
	if (addr3 > 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, " success..addr,%x.\n",addr3);
	} else {
		VRSVR_LOG(LOG_LEVEL_HIGH, " fail...\n");
	}

	resmgr_dump_codec_mem();
	VRSVR_LOG(LOG_LEVEL_HIGH,"trying free %x.\n",addr1);
	if(addr1 > 0)
		resmgr_codec_mem_free(addr1);

	resmgr_dump_codec_mem();
	VRSVR_LOG(LOG_LEVEL_HIGH,"trying free %x.\n",addr2);
	if(addr2 > 0)
		resmgr_codec_mem_free(addr2);
	resmgr_dump_codec_mem();
	VRSVR_LOG(LOG_LEVEL_HIGH,"end of test memory.\n");

	/* test vfp alloc & free. */
	VRSVR_LOG(
	                LOG_LEVEL_HIGH,
	                "start to test vfp registering & unregistering.\n");
	VRSVR_LOG(LOG_LEVEL_HIGH,
	                "trying to register ops1:%x, ops2:%x.ops3:%x\n", &ops1,
	                &ops2, &ops3);

	resmgr_dump_vfp_list();

	VRSVR_LOG(LOG_LEVEL_HIGH, "trying to register ops1:%x........\n", &ops1);
	if (resmgr_dec_vfp_register(&ops1, &id1) < 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "fail.\n");
	} else {
		VRSVR_LOG(LOG_LEVEL_HIGH, "success, id is:%x.\n", id1);
	}

	resmgr_dump_vfp_list();

	VRSVR_LOG(LOG_LEVEL_HIGH, "trying to register ops2:%x........\n", &ops2);
	if (resmgr_dec_vfp_register(&ops2, &id2) < 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "fail.\n");
	} else {
		VRSVR_LOG(LOG_LEVEL_HIGH, "success, id is:%x.\n",id2);
	}

	resmgr_dump_vfp_list();

	if (id1 >= 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "\ntrying to unregister ops1%x........\n", &ops1);
		resmgr_dec_vfp_unregister(id1);
		id1 = -1;
	}

	resmgr_dump_vfp_list();

	VRSVR_LOG(LOG_LEVEL_HIGH, "trying to register ops3:%x........\n", &ops3);
	if (resmgr_dec_vfp_register(&ops3, &id3) < 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "fail.\n");
	} else {
		VRSVR_LOG(LOG_LEVEL_HIGH, "success, id is:%x.\n",id3);
	}

	resmgr_dump_vfp_list();

	VRSVR_LOG(LOG_LEVEL_HIGH, "probe registered VFPs..\n");

	resmgr_probe_registered_items();

	if (id1 >= 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "trying to unregister ops1%x........\n", &ops1);
		resmgr_dec_vfp_unregister(id1);
	}

	resmgr_dump_vfp_list();

	if (id2 >= 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "trying to unregister ops1%x........\n", &ops2);
		resmgr_dec_vfp_unregister(id2);
	}

	resmgr_dump_vfp_list();
	if (id3 >= 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "trying to unregister ops1%x........\n", &ops3);
		resmgr_dec_vfp_unregister(id3);
	}

	resmgr_dump_vfp_list();

	VRSVR_LOG(LOG_LEVEL_HIGH,"end of test vfp registering & unregistering.\n\n");

}

#endif /* RES_SERVER_API_TEST_H_ */
