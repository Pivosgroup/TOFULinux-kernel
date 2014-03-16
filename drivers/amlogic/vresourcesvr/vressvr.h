/*
 * vressvr.h
 *
 *  Created on: Apr 18, 2013
 *      Author: amlogic
 */

#ifndef VRESSVR_H_
#define VRESSVR_H_


/*   log   */
#ifndef LOG_LEVEL
#define LOG_LEVEL  1
#endif

#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_HIGH 2
#define LOG_LEVEL_MEDIUM 1
#define LOG_LEVEL_LOW 0
#define LOCAL_TAG "vressvr"

#ifdef LOG_LEVEL
#define VRSVR_PRINT(level,x...)  	\
	do { \
		if (level >= LOG_LEVEL) { \
			printk ("%s:", LOCAL_TAG); \
			if (level == LOG_LEVEL_ERROR) \
				printk ("*Error*==>"); \
			else if (level == LOG_LEVEL_HIGH) \
				printk ("########>"); \
			else if (level == LOG_LEVEL_MEDIUM) \
				printk ("++++++++>"); \
			else if (level == LOG_LEVEL_LOW) \
				printk ("-------->"); \
			printk (x); \
		} \
	} while (0);

#define VRSVR_LOG(level,x...)  VRSVR_PRINT(level,x)
#define VRSVR_ERROR(x...)  VRSVR_PRINT(LOG_LEVEL_ERROR,x)
#else
#define VRSVR_LOG(level,x...)
#define VRSVR_ERROR(level,x...)
#endif

/* video frame operation. */
struct vressvr_vf_ops {
	void (*reg_recv)(void); /* register function of a receiver. */

	vframe_t* (*vf_peek)(void);	 /* peek a frame from current decoder. */
	vframe_t* (*vf_get)(void); /* get a frame from current decoder and delete it from queue. */
	void (*vf_put)(vframe_t *vf); /* return a frame to decoder. */
};

/* driver base and io. */

struct vressvr_drv {
	char name[20];
	unsigned int open_count;
	int major;
	unsigned int dbg_enable;
	phys_addr_t buffer_start;
	unsigned int buffer_size;
	struct device *dev;
	struct class* class;
};

struct vressvr_dev {
	struct vressvr_vf_ops* vf_ops;
	struct vressvr_drv* drv;
};

/* get information of dedicated decoder memory. */
extern void init_dec_resource_manager(phys_addr_t paddr, u32 size);

/* video layer operation macros. */
#define EnableVideoLayer()  \
    do { SET_MPEG_REG_MASK(VPP_MISC, \
         VPP_VD1_PREBLEND | VPP_PREBLEND_EN | VPP_VD1_POSTBLEND); \
    } while (0)

#define DisableVideoLayer() \
    do { CLEAR_MPEG_REG_MASK(VPP_MISC, \
         VPP_VD1_PREBLEND|VPP_VD1_POSTBLEND ); \
    } while (0)

#define DisableVideoLayer_PREBELEND() \

extern struct vressvr_vf_ops vressvr_vfps;

#endif /* VRESSVR_H_ */
