/*
 *ov7736 - This code emulates a real video device with v4l2 api
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence, GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <media/videobuf-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/wakelock.h>

#include <linux/i2c.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-i2c-drv.h>
#include <media/amlogic/aml_camera.h>

#include <mach/am_regs.h>
#include <mach/pinmux.h>
#include <linux/tvin/tvin_v4l2.h>
#include "common/plat_ctrl.h"
#include "common/vmapi.h"

#ifdef CONFIG_ARCH_MESON6
#include <mach/mod_gate.h>
#endif

#define OV7736_CAMERA_MODULE_NAME "ov7736"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define OV7736_CAMERA_MAJOR_VERSION 0
#define OV7736_CAMERA_MINOR_VERSION 7
#define OV7736_CAMERA_RELEASE 0
#define OV7736_CAMERA_VERSION \
	KERNEL_VERSION(OV7736_CAMERA_MAJOR_VERSION, OV7736_CAMERA_MINOR_VERSION, OV7736_CAMERA_RELEASE)

MODULE_DESCRIPTION("ov7736 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
static unsigned int ov7736_have_opened = 0;
static struct i2c_client *this_client;
static struct v4l2_fract ov7736_frmintervals_active = {
	.numerator = 1,
	.denominator = 15,
};



typedef enum resulution_size_type{
	SIZE_NULL = 0,
	SIZE_QVGA_320x240,
	SIZE_VGA_640X480,
} resulution_size_type_t;

typedef struct resolution_param {
	struct v4l2_frmsize_discrete frmsize;
	struct v4l2_frmsize_discrete active_frmsize;
	int active_fps;
	resulution_size_type_t size_type;
	struct aml_camera_i2c_fig_s* reg_script;
} resolution_param_t;



//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");


/* supported controls */
static struct v4l2_queryctrl ov7736_qctrl[] = {
	{
		.id            = V4L2_CID_DO_WHITE_BALANCE,
		.type          = V4L2_CTRL_TYPE_MENU,
   		.name		   = "white balance",
   		.minimum	   = CAM_WB_AUTO,
   		.maximum	   = CAM_WB_FLUORESCENT,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_EXPOSURE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "exposure",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 4,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_COLORFX,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "effect",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_WHITENESS,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "banding",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_HFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on horizontal",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_VFLIP,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "flip on vertical",
		.minimum       = 0,
		.maximum       = 1,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_ZOOM_ABSOLUTE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Zoom, Absolute",
		.minimum       = 100,
		.maximum       = 300,
		.step          = 20,
		.default_value = 100,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id		= V4L2_CID_ROTATE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Rotate",
		.minimum	= 0,
		.maximum	= 270,
		.step		= 90,
		.default_value	= 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	}
};
struct v4l2_querymenu ov7736_qmenu_wbmode[] = {
    {
        .id         = V4L2_CID_DO_WHITE_BALANCE,
        .index      = CAM_WB_AUTO,
        .name       = "auto",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_DO_WHITE_BALANCE,
        .index      = CAM_WB_CLOUD,
        .name       = "cloudy-daylight",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_DO_WHITE_BALANCE,
        .index      = CAM_WB_INCANDESCENCE,
        .name       = "incandescent",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_DO_WHITE_BALANCE,
        .index      = CAM_WB_DAYLIGHT,
        .name       = "daylight",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_DO_WHITE_BALANCE,
        .index      = CAM_WB_FLUORESCENT,
        .name       = "fluorescent", 
        .reserved   = 0,
    },{
        .id         = V4L2_CID_DO_WHITE_BALANCE,
        .index      = CAM_WB_FLUORESCENT,
        .name       = "warm-fluorescent", 
        .reserved   = 0,
    },
};

struct v4l2_querymenu ov7736_qmenu_anti_banding_mode[] = {
    {
        .id         = V4L2_CID_POWER_LINE_FREQUENCY,
        .index      = CAM_BANDING_50HZ, 
        .name       = "50hz",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_POWER_LINE_FREQUENCY,
        .index      = CAM_BANDING_60HZ, 
        .name       = "60hz",
        .reserved   = 0,
    },
};
typedef struct {
    __u32   id;
    int     num;
    struct v4l2_querymenu* ov7736_qmenu;
}ov7736_qmenu_set_t;

ov7736_qmenu_set_t ov7736_qmenu_set[] = {
    {
        .id         	= V4L2_CID_DO_WHITE_BALANCE,
        .num            = ARRAY_SIZE(ov7736_qmenu_wbmode),
        .ov7736_qmenu   = ov7736_qmenu_wbmode,
    },{
        .id         	= V4L2_CID_POWER_LINE_FREQUENCY,
        .num            = ARRAY_SIZE(ov7736_qmenu_anti_banding_mode),
        .ov7736_qmenu   = ov7736_qmenu_anti_banding_mode,
    },
};
static int vidioc_querymenu(struct file *file, void *priv,
                struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(ov7736_qmenu_set); i++)
	if (a->id && a->id == ov7736_qmenu_set[i].id) {
	    for(j = 0; j < ov7736_qmenu_set[i].num; j++)
		if (a->index == ov7736_qmenu_set[i].ov7736_qmenu[j].index) {
			memcpy(a, &( ov7736_qmenu_set[i].ov7736_qmenu[j]),
				sizeof(*a));
			return (0);
		}
	}

	return -EINVAL;
}

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct ov7736_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct ov7736_fmt formats[] = {
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
	},

	{
		.name     = "RGB888 (24)",
		.fourcc   = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth    = 24,
	},
	{
		.name     = "BGR888 (24)",
		.fourcc   = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth    = 24,
	},
	{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	},
	{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
	},
	{
		.name     = "YUV420P",
		.fourcc   = V4L2_PIX_FMT_YUV420,
		.depth    = 12,
	},
	{
		.name     = "YVU420P",
		.fourcc   = V4L2_PIX_FMT_YVU420,
		.depth    = 12,
	}

};

static struct ov7736_fmt *get_format(struct v4l2_format *f)
{
	struct ov7736_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

struct sg_to_addr {
	int pos;
	struct scatterlist *sg;
};

/* buffer for one video frame */
struct ov7736_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct ov7736_fmt        *fmt;
};

struct ov7736_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(ov7736_devicelist);

struct ov7736_device {
	struct list_head			ov7736_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct ov7736_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;
	
	/* current resolution param for preview and capture */
	resolution_param_t* cur_resolution_param;
	
	/* wake lock */
	struct wake_lock	wake_lock;

	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(ov7736_qctrl)];
};

static inline struct ov7736_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov7736_device, sd);
}

struct ov7736_fh {
	struct ov7736_device            *dev;

	/* video capture */
	struct ov7736_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
	unsigned int  f_flags;
};

static inline struct ov7736_fh *to_fh(struct ov7736_device *dev)
{
	return container_of(dev, struct ov7736_fh, dev);
}

/* ------------------------------------------------------------------
	reg spec of OV7736
   ------------------------------------------------------------------*/

#if 1

struct aml_camera_i2c_fig_s OV7736_script[] = {
      {0x3008, 0x82},
      {0x3008, 0x42},
      {0x3630, 0x11},
      {0x3104, 0x03},
      {0x3017, 0x7f},
      {0x3018, 0xfc},
      {0x3600, 0x1c},
      {0x3602, 0x04},
      {0x3611, 0x44},
      {0x3612, 0x63},
      {0x3631, 0x22},
      {0x3622, 0x00},
      {0x3633, 0x25},
      {0x370d, 0x07},
      {0x3620, 0x42},
      {0x3714, 0x19},
      {0x3715, 0xfa},
      {0x370b, 0x43},
      {0x3713, 0x1a},
      {0x401c, 0x00},
      {0x401e, 0x11},
      {0x4702, 0x01},
      {0x3a00, 0x7a},
      {0x3a18, 0x00},
      {0x3a19, 0x3f},
      {0x300f, 0x88},
      {0x3011, 0x08},
      {0x4303, 0xff},
      {0x4307, 0xff},
      {0x430b, 0xff},
      {0x4305, 0x00},
      {0x4309, 0x00},
      {0x430d, 0x00},
      {0x5181, 0x04},
      {0x5481, 0x26},
      {0x5482, 0x35},
      {0x5483, 0x48},
      {0x5484, 0x63},
      {0x5485, 0x6e},
      {0x5486, 0x77},
      {0x5487, 0x80},
      {0x5488, 0x88},
      {0x5489, 0x8f},
      {0x548a, 0x96},
      {0x548b, 0xa3},
      {0x548c, 0xaf},
      {0x548d, 0xc5},
      {0x548e, 0xd7},
      {0x548f, 0xe8},
      {0x5490, 0x0f},
      {0x4001, 0x02},
      {0x4004, 0x06},
      {0x3800, 0x00},
      {0x3801, 0x8e},
      {0x3810, 0x08},
      {0x3811, 0x02},
      {0x380c, 0x03},
      {0x380d, 0x20},
      {0x380e, 0x01},
      {0x380f, 0xf4},
      {0x3a09, 0x96},
      {0x3a0b, 0x7d},
      {0x4300, 0x31},
      {0x501f, 0x01},
      {0x5000, 0x4f},
      {0x5001, 0x47},
      {0x370d, 0x0b},
      {0x3715, 0x1a},
      {0x370e, 0x00},
      {0x3713, 0x08},
      {0x3703, 0x2c},
      {0x3620, 0xc2},
      {0x3714, 0x36},
      {0x3716, 0x01},
      {0x3623, 0x03},
      {0x3c00, 0x00},
      {0x3c01, 0x32},
      {0x3c04, 0x12},
      {0x3c05, 0x60},
      {0x3c06, 0x00},
      {0x3c07, 0x20},
      {0x3c08, 0x00},
      {0x3c09, 0xc2},
      {0x300d, 0x22},
      {0x3c0a, 0x9c},
      {0x3c0b, 0x40},
      {0x3008, 0x02},
      {0x5180, 0x02},
      {0x5181, 0x02},
      {0x3a0f, 0x35},
      {0x3a10, 0x2c},
      {0x3a1b, 0x36},
      {0x3a1e, 0x2d},
      {0x3a11, 0x90},
      {0x3a1f, 0x10},
      {0x5000, 0xcf},
      {0x5481, 0x0a},
      {0x5482, 0x13},
      {0x5483, 0x23},
      {0x5484, 0x40},
      {0x5485, 0x4d},
      {0x5486, 0x58},
      {0x5487, 0x64},
      {0x5488, 0x6e},
      {0x5489, 0x78},
      {0x548a, 0x81},
      {0x548b, 0x92},
      {0x548c, 0xa1},
      {0x548d, 0xbb},
      {0x548e, 0xcf},
      {0x548f, 0xe3},
      {0x5490, 0x26},
      {0x5380, 0x42},
      {0x5381, 0x33},
      {0x5382, 0x0f},
      {0x5383, 0x0b},
      {0x5384, 0x42},
      {0x5385, 0x4d},
      {0x5392, 0x1e},
      {0x5801, 0x00},
      {0x5802, 0x00},
      {0x5803, 0x00},
      {0x5804, 0x12},
      {0x5805, 0x15},
      {0x5806, 0x08},
      {0x5001, 0xc7},
      {0x5580, 0x06},
      {0x5583, 0x40},
      {0x5584, 0x26},
      {0x5589, 0x10},
      {0x558a, 0x00},
      {0x558b, 0x3e},
      {0x5300, 0x0f},
      {0x5301, 0x30},
      {0x5302, 0x0d},
      {0x5303, 0x02},
      {0x5300, 0x0f},
      {0x5301, 0x30},
      {0x5302, 0x0d},
      {0x5303, 0x02},
      {0x5304, 0x0e},
      {0x5305, 0x30},
      {0x5306, 0x06},
      {0x5307, 0x40},
      {0x5680, 0x00},
      {0x5681, 0x50},
      {0x5682, 0x00},
      {0x5683, 0x3c},
      {0x5684, 0x11},
      {0x5685, 0xe0},
      {0x5686, 0x0d},
      {0x5687, 0x68},
      {0x5688, 0x03},
      {0x3008, 0x02},
      {0xffff, 0xff}

};


struct aml_camera_i2c_fig_s OV7736_preview_qvga_script[] = {
      {0x3008, 0x82},
      {0x3008, 0x42},
      {0x3630, 0x11},
      {0x3104, 0x03},
      {0x3017, 0x7f},
      {0x3018, 0xfc},
      {0x3600, 0x1c},
      {0x3602, 0x04},
      {0x3611, 0x44},
      {0x3612, 0x63},
      {0x3631, 0x22},
      {0x3622, 0x00},
      {0x3633, 0x25},
      {0x370d, 0x07},
      {0x3620, 0x42},
      {0x3714, 0x19},
      {0x3715, 0xfa},
      {0x370b, 0x43},
      {0x3713, 0x1a},
      {0x401c, 0x00},
      {0x401e, 0x11},
      {0x4702, 0x01},
      {0x3a00, 0x7a},
      {0x3a18, 0x00},
      {0x3a19, 0x3f},
      {0x300f, 0x88},
      {0x3011, 0x08},
      {0x4303, 0xff},
      {0x4307, 0xff},
      {0x430b, 0xff},
      {0x4305, 0x00},
      {0x4309, 0x00},
      {0x430d, 0x00},
      {0x5181, 0x04},
      {0x5481, 0x26},
      {0x5482, 0x35},
      {0x5483, 0x48},
      {0x5484, 0x63},
      {0x5485, 0x6e},
      {0x5486, 0x77},
      {0x5487, 0x80},
      {0x5488, 0x88},
      {0x5489, 0x8f},
      {0x548a, 0x96},
      {0x548b, 0xa3},
      {0x548c, 0xaf},
      {0x548d, 0xc5},
      {0x548e, 0xd7},
      {0x548f, 0xe8},
      {0x5490, 0x0f},
      {0x4001, 0x02},
      {0x4004, 0x02},
      {0x3800, 0x00},
      {0x3801, 0x88},
      {0x3804, 0x01},
      {0x3805, 0x40},
      {0x3802, 0x00},
      {0x3803, 0x0a},
      {0x3806, 0x00},
      {0x3807, 0xf0},
      {0x3808, 0x01},
      {0x3809, 0x40},
      {0x380a, 0x00},
      {0x380b, 0xf0},
      {0x380c, 0x03},
      {0x380d, 0x10},
      {0x380e, 0x01},
      {0x380f, 0x00},
      {0x3810, 0x08},
      {0x3811, 0x02},
      {0x3622, 0x88},
      {0x3818, 0x81},
      {0x3a08, 0x00},
      {0x3a09, 0x99},
      {0x3a0a, 0x00},
      {0x3a0b, 0x80},
      {0x3a0d, 0x02},
      {0x3a0e, 0x01},
      {0x3705, 0xdc},
      {0x3a1a, 0x05},
      {0x370a, 0x01},
      {0x4300, 0x31},
      {0x501f, 0x01},
      {0x5000, 0x4f},
      {0x5001, 0x47},
      {0x370d, 0x0b},
      {0x3715, 0x1a},
      {0x370e, 0x00},
      {0x3713, 0x08},
      {0x3703, 0x2c},
      {0x3620, 0xc2},
      {0x3714, 0x36},
      {0x3716, 0x01},
      {0x3623, 0x03},
      {0x3c00, 0x00},
      {0x3c01, 0x32},
      {0x3c04, 0x12},
      {0x3c05, 0x60},
      {0x3c06, 0x00},
      {0x3c07, 0x20},
      {0x3c08, 0x00},
      {0x3c09, 0xc2},
      {0x300d, 0x22},
      {0x3c0a, 0x9c},
      {0x3c0b, 0x40},
      {0x3008, 0x02},
      {0x5180, 0x02},
      {0x5181, 0x02},
      {0x3a0f, 0x35},
      {0x3a10, 0x2c},
      {0x3a1b, 0x36},
      {0x3a1e, 0x2d},
      {0x3a11, 0x90},
      {0x3a1f, 0x10},
      {0x5000, 0xcf},
      {0x5481, 0x0a},
      {0x5482, 0x13},
      {0x5483, 0x23},
      {0x5484, 0x40},
      {0x5485, 0x4d},
      {0x5486, 0x58},
      {0x5487, 0x64},
      {0x5488, 0x6e},
      {0x5489, 0x78},
      {0x548a, 0x81},
      {0x548b, 0x92},
      {0x548c, 0xa1},
      {0x548d, 0xbb},
      {0x548e, 0xcf},
      {0x548f, 0xe3},
      {0x5490, 0x26},
      {0x5380, 0x42},
      {0x5381, 0x33},
      {0x5382, 0x0f},
      {0x5383, 0x0b},
      {0x5384, 0x42},
      {0x5385, 0x4d},
      {0x5392, 0x1e},
      {0x5801, 0x00},
      {0x5802, 0x00},
      {0x5803, 0x00},
      {0x5804, 0x12},
      {0x5805, 0x15},
      {0x5806, 0x08},
      {0x5001, 0xc7},
      {0x5580, 0x06},
      {0x5583, 0x40},
      {0x5584, 0x26},
      {0x5589, 0x10},
      {0x558a, 0x00},
      {0x558b, 0x3e},
      {0x5300, 0x0f},
      {0x5301, 0x30},
      {0x5302, 0x0d},
      {0x5303, 0x02},
      {0x5300, 0x0f},
      {0x5301, 0x30},
      {0x5302, 0x0d},
      {0x5303, 0x02},
      {0x5304, 0x0e},
      {0x5305, 0x30},
      {0x5306, 0x06},
      {0x5307, 0x40},
      {0x5680, 0x00},
      {0x5681, 0x50},
      {0x5682, 0x00},
      {0x5683, 0x3c},
      {0x5684, 0x11},
      {0x5685, 0xe0},
      {0x5686, 0x0d},
      {0x5687, 0x68},
      {0x5688, 0x03},
      {0x3008, 0x02},
      {0xffff, 0xff}
};

struct aml_camera_i2c_fig_s OV7736_capture_script[] = {
      {0xffff,0xff}
};

static resolution_param_t  prev_resolution_array[] = {
	{
		.frmsize		= {640, 480},
		.active_frmsize	= {640, 480},
		.active_fps		= 15,
		.size_type		= SIZE_VGA_640X480,
		.reg_script		= OV7736_script,
	}, {
		.frmsize		= {320, 240},
		.active_frmsize	= {320, 240},
		.active_fps		= 20,
		.size_type		= SIZE_QVGA_320x240,
		.reg_script		= OV7736_preview_qvga_script,
	},
};

static resolution_param_t  capture_resolution_array[] = {
	{
		.frmsize		= {640, 480},
		.active_frmsize		= {640, 480},
		.active_fps		= 15,
		.size_type		= SIZE_VGA_640X480,
		.reg_script		= OV7736_capture_script,
	}
};
/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 * V4L2_CAP_TIMEPERFRAME need to be supported furthermore.
 */
static int vidioc_g_parm(struct file *file, void *priv,
				struct v4l2_streamparm *parms)
{
	struct ov7736_fh *fh = priv;
	struct ov7736_device *dev = fh->dev;
	struct v4l2_captureparm *cp = &parms->parm.capture;

	dprintk(dev,3,"vidioc_g_parm\n");
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;

	cp->timeperframe = ov7736_frmintervals_active;
	dprintk(dev, 3,"g_parm,deno=%d, numerator=%d\n",
		cp->timeperframe.denominator, cp->timeperframe.numerator );
	return 0;
}	

static resulution_size_type_t get_size_type(int width, int height)
{
	resulution_size_type_t rv = SIZE_NULL;
	if (width * height >= 600 * 400)
		rv = SIZE_VGA_640X480;
	else if (width * height >= 300 * 200)
		rv = SIZE_QVGA_320x240;
	return rv;
}

static resolution_param_t* get_resolution_param(struct ov7736_device *dev, int is_capture, int width, int height)
{
	int i = 0;
	int arry_size = 0;
	resolution_param_t* tmp_resolution_param = NULL;
	resulution_size_type_t res_type = SIZE_NULL;
	res_type = get_size_type(width, height);
	if (res_type == SIZE_NULL)
		return NULL;
	if (is_capture) {
		tmp_resolution_param = capture_resolution_array;
		arry_size = sizeof(capture_resolution_array);
	} else {
		tmp_resolution_param = prev_resolution_array;
		arry_size = sizeof(prev_resolution_array);
	}
	
	for (i = 0; i < arry_size; i++) {
		if (tmp_resolution_param[i].size_type == res_type)
			return &tmp_resolution_param[i];
	}
	return NULL;
}

static int set_resolution_param(struct ov7736_device *dev, resolution_param_t* res_param)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	//int rc = -1;
	int i=0;
	if (!res_param->reg_script) {
		printk("error, resolution reg script is NULL\n");
		return -1;
	}
	while(1) {
		if (res_param->reg_script[i].val==0xff&&res_param->reg_script[i].addr==0xffff) {
			printk("setting resolutin param complete\n");
			break;
		}
		if((i2c_put_byte(client, res_param->reg_script[i].addr, res_param->reg_script[i].val)) < 0) {
			printk("fail in setting resolution param. i=%d\n",i);
			break;
		}
		i++;
	}
	dev->cur_resolution_param = res_param;
	return 1;
}


void OV7736_init_regs(struct ov7736_device *dev)
{
	int i=0;//,j;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	
	while(1)
	{
		if (OV7736_script[i].val == 0xff 
				&& OV7736_script[i].addr == 0xffff) {
		    	printk("OV7736_write_regs success" \
		    			" in initial OV7736.\n");
		 	break;
		}
		//printk("reg:0x%X val:0x%x\n", OV7736_script[i].addr, OV7736_script[i].val);
		if((i2c_put_byte(client,OV7736_script[i].addr,OV7736_script[i].val)) < 0) {
		    	printk("fail in initial OV7736.i=%d \n",i);
			return;
		}
		if (i == 1)
			msleep(40);
		i++;
	}
    printk("3008-----:0x%x\n", i2c_get_byte(client,0x3008));
	return;
}

#endif

/*************************************************************************
* FUNCTION
*	set_OV7736_param_wb
*
* DESCRIPTION
*	OV7736 wb setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED  白平衡参数
*
*************************************************************************/
void set_OV7736_param_wb(struct ov7736_device *dev,enum  camera_wb_flip_e para)
{
//	kal_uint16 rgain=0x80, ggain=0x80, bgain=0x80;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch (para) {
	case CAM_WB_AUTO:
	    i2c_put_byte(client,0x3212, 0x00); // enable group 0
        i2c_put_byte(client,0x5186, 0x02); //AWB auto
        i2c_put_byte(client,0x3212, 0x10); // end group 0
        i2c_put_byte(client,0x3212, 0xa0); // launch group 0
		break;

	case CAM_WB_CLOUD:
        i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5186, 0x03); //AWB manual
        i2c_put_byte(client,0x5052, 0x04); // b gain
        i2c_put_byte(client,0x5053, 0xb4);
        i2c_put_byte(client,0x5050, 0x04); // g gain
        i2c_put_byte(client,0x5051, 0x00);
        i2c_put_byte(client,0x504e, 0x07); // r gain
        i2c_put_byte(client,0x504f, 0x6d);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;

	case CAM_WB_DAYLIGHT:   // tai yang guang
        i2c_put_byte(client,0x3212, 0x00); // enable group 0
        i2c_put_byte(client,0x5186, 0x03); //AWB manual
        i2c_put_byte(client,0x5052, 0x05); // b gain high
        i2c_put_byte(client,0x5053, 0xd7); // b gain low
        i2c_put_byte(client,0x5050, 0x04); // g gain high
        i2c_put_byte(client,0x5051, 0x00); // g gain low
        i2c_put_byte(client,0x504e, 0x06); // r gain high
        i2c_put_byte(client,0x504f, 0x35); // r gain low
        i2c_put_byte(client,0x3212, 0x10); // end group 0
        i2c_put_byte(client,0x3212, 0xa0);
		break;

	case CAM_WB_INCANDESCENCE:   // bai re guang
        i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5186, 0x03); //AWB manual
        i2c_put_byte(client,0x5052, 0x08); // b gain
        i2c_put_byte(client,0x5053, 0xab);
        i2c_put_byte(client,0x5050, 0x04); // g gain
        i2c_put_byte(client,0x5051, 0x00);
        i2c_put_byte(client,0x504e, 0x05); // r gain
        i2c_put_byte(client,0x504f, 0x41);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;

	case CAM_WB_FLUORESCENT:   //ri guang deng
        i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5186, 0x03); //AWB manual
        i2c_put_byte(client,0x5052, 0x09); // b gain
        i2c_put_byte(client,0x5053, 0x18);
        i2c_put_byte(client,0x5050, 0x04); // g gain
        i2c_put_byte(client,0x5051, 0x06);
        i2c_put_byte(client,0x504e, 0x04); // r gain
        i2c_put_byte(client,0x504f, 0x00);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;

	case CAM_WB_TUNGSTEN:   // wu si deng

		break;

	case CAM_WB_MANUAL:
		// TODO
		break;
	default:
		break;
	}
//	kal_sleep_task(20);
}

/*************************************************************************
* FUNCTION
*	OV7736_night_mode
*
* DESCRIPTION
*	This function night mode of OV7736.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV7736_night_mode(struct ov7736_device *dev,enum  camera_night_mode_flip_e enable)
{
	//struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	//unsigned char buf[4];
	//unsigned char  temp_reg;
}
/*************************************************************************
* FUNCTION
*	OV7736_night_mode
*
* DESCRIPTION
*	This function night mode of OV7736.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/

void OV7736_set_param_banding(struct ov7736_device *dev,enum  camera_night_mode_flip_e banding)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(banding)
		{
		case CAM_BANDING_60HZ:
			i2c_put_byte(client,0x3c01, 0xb2); // auto off
            i2c_put_byte(client,0x3c00, 0x00); // manual 60hz
			break;
		case CAM_BANDING_50HZ:
			i2c_put_byte(client,0x3c01, 0xb2);
			i2c_put_byte(client,0x3c00, 0x04);
			break;
        case CAM_NM_AUTO:
			break;
		}

}


/*************************************************************************
* FUNCTION
*	set_OV7736_param_exposure
*
* DESCRIPTION
*	OV7736 exposure setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED  亮度等级 调节参数
*
*************************************************************************/
void set_OV7736_param_exposure(struct ov7736_device *dev,enum camera_exposure_e para)//曝光调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch (para) {
	case EXPOSURE_N3_STEP:
	    i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5587, 0x30);
        i2c_put_byte(client,0x5588, 0x09);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0); 
		break;
		
	case EXPOSURE_N2_STEP:
		i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5587, 0x20);
        i2c_put_byte(client,0x5588, 0x09);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
	case EXPOSURE_N1_STEP:
		i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5587, 0x10);
        i2c_put_byte(client,0x5588, 0x09);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;

    case EXPOSURE_0_STEP:
		i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5587, 0x00);
        i2c_put_byte(client,0x5588, 0x01);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
	case EXPOSURE_P1_STEP:
        i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5587, 0x10);
        i2c_put_byte(client,0x5588, 0x01);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
	case EXPOSURE_P2_STEP:
		i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5587, 0x20);
        i2c_put_byte(client,0x5588, 0x01);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
    case EXPOSURE_P3_STEP:
	    i2c_put_byte(client,0x3212, 0x00); // enable group 0
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5587, 0x30);
        i2c_put_byte(client,0x5588, 0x01);
        i2c_put_byte(client,0x3212, 0x10); // end group 0
        i2c_put_byte(client,0x3212, 0xa0);
		break;
	}

}

/*************************************************************************
* FUNCTION
*	set_OV7736_param_effect
*
* DESCRIPTION
*	OV7736 effect setting.
*
* PARAMETERS
*	none
*
* RETURNS
*	None
*
* GLOBALS AFFECTED  特效参数
*
*************************************************************************/
void set_OV7736_param_effect(struct ov7736_device *dev,enum camera_effect_flip_e para)//特效设置
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch (para){
	case CAM_EFFECT_ENC_NORMAL:
        i2c_put_byte(client,0x3212, 0x00); // enable group 0
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5580, 0x06);
        i2c_put_byte(client,0x5583, 0x40);
        i2c_put_byte(client,0x5584, 0x26);
        i2c_put_byte(client,0x3212, 0x10); // end group 0
        i2c_put_byte(client,0x3212, 0xa0); // launch group 0
		break;
		
	case CAM_EFFECT_ENC_GRAYSCALE:
        i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5580, 0x24);
        i2c_put_byte(client,0x5583, 0x80);
        i2c_put_byte(client,0x5584, 0x80);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0); 
		break;
		
	case CAM_EFFECT_ENC_SEPIA:
		i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5580, 0x1c);
        i2c_put_byte(client,0x5583, 0x40);
        i2c_put_byte(client,0x5584, 0xa0);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
	case CAM_EFFECT_ENC_COLORINV:
        i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5580, 0x1c);
        i2c_put_byte(client,0x5583, 0x80);
        i2c_put_byte(client,0x5584, 0xc0);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
	case CAM_EFFECT_ENC_SEPIAGREEN:
		i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5580, 0x1c);
        i2c_put_byte(client,0x5583, 0x60);
        i2c_put_byte(client,0x5584, 0x60);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
	case CAM_EFFECT_ENC_SEPIABLUE:
		i2c_put_byte(client,0x3212, 0x00);
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5580, 0x1c);
        i2c_put_byte(client,0x5583, 0xa0);
        i2c_put_byte(client,0x5584, 0x40);
        i2c_put_byte(client,0x3212, 0x10);
        i2c_put_byte(client,0x3212, 0xa0);
		break;
		
	default:
		i2c_put_byte(client,0x3212, 0x00); // enable group 0
        i2c_put_byte(client,0x5001, 0xc7);
        i2c_put_byte(client,0x5580, 0x06);
        i2c_put_byte(client,0x5583, 0x40);
        i2c_put_byte(client,0x5584, 0x26);
        i2c_put_byte(client,0x3212, 0x10); // end group 0
        i2c_put_byte(client,0x3212, 0xa0); // launch group 0
		break;
	}

}

static int ov7736_setting(struct ov7736_device *dev,int PROP_ID,int value )
{
	int ret=0;
	//unsigned char cur_val;
	//struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
	case V4L2_CID_DO_WHITE_BALANCE:
		if(ov7736_qctrl[0].default_value!=value){
			ov7736_qctrl[0].default_value=value;
			set_OV7736_param_wb(dev,value);
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
        	}
		break;
	case V4L2_CID_EXPOSURE:
		if(ov7736_qctrl[1].default_value!=value){
			ov7736_qctrl[1].default_value=value;
			set_OV7736_param_exposure(dev,value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
        	}
		break;
	case V4L2_CID_COLORFX:
		if(ov7736_qctrl[2].default_value!=value){
			ov7736_qctrl[2].default_value=value;
			set_OV7736_param_effect(dev,value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
        	}
		break;
	case V4L2_CID_WHITENESS:
		 if(ov7736_qctrl[3].default_value!=value){
			ov7736_qctrl[3].default_value=value;
			OV7736_set_param_banding(dev,value);
			printk(KERN_INFO " set camera  banding=%d. \n ",value);
        	}
		break;
	case V4L2_CID_HFLIP:
		value = value & 0x3;
		if(ov7736_qctrl[4].default_value!=value){
			ov7736_qctrl[4].default_value=value;
			printk(" set camera  h filp =%d. \n ",value);
        	}
		break;
	case V4L2_CID_VFLIP:    /* set flip on V. */         
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		if(ov7736_qctrl[6].default_value!=value){
			ov7736_qctrl[6].default_value=value;
			//printk(KERN_INFO " set camera  zoom mode=%d. \n ",value);
        	}
		break;
	case V4L2_CID_ROTATE:
		 if(ov7736_qctrl[7].default_value!=value){
			ov7736_qctrl[7].default_value=value;
			printk(" set camera  rotate =%d. \n ",value);
        	}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;

}

static void power_down_ov7736(struct ov7736_device *dev)
{
	//struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	//unsigned char buf[4];
	//buf[0]=0x12;
	//buf[1]=0x80;
	//i2c_put_byte(client,buf,2);
	msleep(5);
	//buf[0]=0xb8;
	//buf[1]=0x12;
	//i2c_put_byte(client,buf,2);
	msleep(1);
	return;
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void ov7736_fillbuff(struct ov7736_fh *fh, struct ov7736_buffer *buf)
{
	struct ov7736_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	vm_output_para_t para = {0};
	dprintk(dev,1,"%s\n", __func__);
	if (!vbuf)
		return;
	/*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
	para.mirror = ov7736_qctrl[4].default_value&3;
	para.v4l2_format = fh->fmt->fourcc;
	para.v4l2_memory = 0x18221223;
	para.zoom = ov7736_qctrl[6].default_value;
	para.angle = ov7736_qctrl[7].default_value;
	para.vaddr = (unsigned)vbuf;
	vm_fill_buffer(&buf->vb,&para);
	buf->vb.state = VIDEOBUF_DONE;
}

static void ov7736_thread_tick(struct ov7736_fh *fh)
{
  
	struct ov7736_buffer *buf;
	struct ov7736_device *dev = fh->dev;
	struct ov7736_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");
	if(!fh->stream_on){
		dprintk(dev, 1, "sensor doesn't stream on\n");
		return ;
	}
	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
		dprintk(dev, 1, "No active queue to serve\n");
		goto unlock;
	}
	buf = list_entry(dma_q->active.next,
			 struct ov7736_buffer, vb.queue);
	dprintk(dev, 1, "%s\n", __func__);
	dprintk(dev, 1, "list entry get buf is %x\n",(unsigned)buf);

	/* Nobody is waiting on this buffer, return */
    if( ! (fh->f_flags & O_NONBLOCK) ){
 	/* Nobody is waiting on this buffer, return */
		if (!waitqueue_active(&buf->vb.done))
			goto unlock;
	}
	buf->vb.state = VIDEOBUF_ACTIVE;
	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	ov7736_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);
	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

#define frames_to_ms(frames)					\
	((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void ov7736_sleep(struct ov7736_fh *fh)
{
	struct ov7736_device *dev = fh->dev;
	struct ov7736_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	ov7736_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int ov7736_thread(void *data)
{
	struct ov7736_fh  *fh = data;
	struct ov7736_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		ov7736_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int ov7736_start_thread(struct ov7736_fh *fh)
{
	struct ov7736_device *dev = fh->dev;
	struct ov7736_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(ov7736_thread, fh, "ov7736");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void ov7736_stop_thread(struct ov7736_dmaqueue  *dma_q)
{
	struct ov7736_device *dev = container_of(dma_q, struct ov7736_device, vidq);

	dprintk(dev, 1, "%s\n", __func__);
	/* shutdown control thread */
	if (dma_q->kthread) {
		kthread_stop(dma_q->kthread);
		dma_q->kthread = NULL;
	}
}

/* ------------------------------------------------------------------
	Videobuf operations
   ------------------------------------------------------------------*/
static int
buffer_setup(struct videobuf_queue *vq, unsigned int *count, unsigned int *size)
{
	struct ov7736_fh  *fh = vq->priv_data;
	struct ov7736_device *dev  = fh->dev;
    //int bytes = fh->fmt->depth >> 3 ;
	*size = fh->width*fh->height*fh->fmt->depth >> 3;
	if (0 == *count)
		*count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
		(*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
		*count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct ov7736_buffer *buf)
{
	struct ov7736_fh  *fh = vq->priv_data;
	struct ov7736_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);
    videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 1600
#define norm_maxh() 1600
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						enum v4l2_field field)
{
	struct ov7736_fh     *fh  = vq->priv_data;
	struct ov7736_device    *dev = fh->dev;
	struct ov7736_buffer *buf = container_of(vb, struct ov7736_buffer, vb);
	int rc;
	//int bytes = fh->fmt->depth >> 3 ;
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	BUG_ON(NULL == fh->fmt);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
	    fh->height < 32 || fh->height > norm_maxh())
		return -EINVAL;

	buf->vb.size = fh->width*fh->height*fh->fmt->depth >> 3;
	if (0 != buf->vb.baddr  &&  buf->vb.bsize < buf->vb.size)
		return -EINVAL;

	/* These properties only change when queue is idle, see s_fmt */
	buf->fmt       = fh->fmt;
	buf->vb.width  = fh->width;
	buf->vb.height = fh->height;
	buf->vb.field  = field;

	//precalculate_bars(fh);

	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		rc = videobuf_iolock(vq, &buf->vb, NULL);
		if (rc < 0)
			goto fail;
	}

	buf->vb.state = VIDEOBUF_PREPARED;

	return 0;

fail:
	free_buffer(vq, buf);
	return rc;
}

static void
buffer_queue(struct videobuf_queue *vq, struct videobuf_buffer *vb)
{
	struct ov7736_buffer    *buf  = container_of(vb, struct ov7736_buffer, vb);
	struct ov7736_fh        *fh   = vq->priv_data;
	struct ov7736_device       *dev  = fh->dev;
	struct ov7736_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct ov7736_buffer   *buf  = container_of(vb, struct ov7736_buffer, vb);
	struct ov7736_fh       *fh   = vq->priv_data;
	struct ov7736_device      *dev  = (struct ov7736_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops ov7736_video_qops = {
	.buf_setup      = buffer_setup,
	.buf_prepare    = buffer_prepare,
	.buf_queue      = buffer_queue,
	.buf_release    = buffer_release,
};

/* ------------------------------------------------------------------
	IOCTL vidioc handling
   ------------------------------------------------------------------*/
static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct ov7736_fh  *fh  = priv;
	struct ov7736_device *dev = fh->dev;

	strcpy(cap->driver, "ov7736");
	strcpy(cap->card, "ov7736");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = OV7736_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct ov7736_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct ov7736_fh *fh = priv;

	f->fmt.pix.width        = fh->width;
	f->fmt.pix.height       = fh->height;
	f->fmt.pix.field        = fh->vb_vidq.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fh->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return (0);
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	struct ov7736_fh  *fh  = priv;
	struct ov7736_device *dev = fh->dev;
	struct ov7736_fmt *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;

	fmt = get_format(f);
	if (!fmt) {
		dprintk(dev, 1, "Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (V4L2_FIELD_INTERLACED != field) {
		dprintk(dev, 1, "Field type invalid.\n");
		return -EINVAL;
	}

	maxw  = norm_maxw();
	maxh  = norm_maxh();

	f->fmt.pix.field = field;
	v4l_bound_align_image(&f->fmt.pix.width, 48, maxw, 2,
			      &f->fmt.pix.height, 32, maxh, 0, 0);
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;

	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct ov7736_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct ov7736_device *dev = fh->dev;
	resolution_param_t* res_param = NULL;

	int ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;

	mutex_lock(&q->vb_lock);

	if (videobuf_queue_is_busy(&fh->vb_vidq)) {
		dprintk(fh->dev, 1, "%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	fh->fmt           = get_format(f);
	fh->width         = f->fmt.pix.width;
	fh->height        = f->fmt.pix.height;
	fh->vb_vidq.field = f->fmt.pix.field;
	fh->type          = f->type;
	
	if(f->fmt.pix.pixelformat==V4L2_PIX_FMT_RGB24){
		res_param = get_resolution_param(dev, 1, fh->width,fh->height);
		if (!res_param) {
			printk("error, resolution param not get\n");
			goto out;
		}
		//Get_preview_exposure_gain(dev);
		set_resolution_param(dev, res_param);

		//cal_exposure(dev);
	} else {
		res_param = get_resolution_param(dev, 0, fh->width,fh->height);
		if (!res_param) {
			printk("error, resolution param not get\n");
			goto out;
		}
		set_resolution_param(dev, res_param);
	}
	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct ov7736_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov7736_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov7736_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct ov7736_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct ov7736_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov7736_fh  *fh = priv;
	struct ov7736_device *dev = fh->dev;
	vdin_parm_t para;
	int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
    memset( &para, 0, sizeof( para ));
	para.port  = TVIN_PORT_CAMERA;
	para.fmt = TVIN_SIG_FMT_MAX;//TVIN_SIG_FMT_MAX+1;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
	if (fh->dev->cur_resolution_param) {
		para.frame_rate = ov7736_frmintervals_active.denominator;//175;
		para.h_active = 640;
		para.v_active = 478;
	} else {
		para.frame_rate = ov7736_frmintervals_active.denominator;
		para.h_active = 320;
		para.v_active = 240;
	}
	para.hsync_phase =0;
	para.vsync_phase  = 1;	
	para.hs_bp = 0;
	para.vs_bp = 2;
	para.cfmt = TVIN_YUV422;
	para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;	
	para.reserved = 10; //skip_num
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
		start_tvin_service(0,&para);
		fh->stream_on        = 1;
	}
	msleep(500);
	set_OV7736_param_wb(dev,ov7736_qctrl[0].default_value);
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct ov7736_fh  *fh = priv;

    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if(ret == 0 ){
		stop_tvin_service(0);
		fh->stream_on = 0;
	}
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,struct v4l2_frmsizeenum *fsize)
{
	int ret = 0,i=0;
	struct ov7736_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;
	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		if (formats[i].fourcc == fsize->pixel_format){
			fmt = &formats[i];
			break;
		}
	}
	if (fmt == NULL)
    	return -EINVAL;
	if ((fmt->fourcc == V4L2_PIX_FMT_NV21)
		||(fmt->fourcc == V4L2_PIX_FMT_NV12)
		||(fmt->fourcc == V4L2_PIX_FMT_YUV420)
		||(fmt->fourcc == V4L2_PIX_FMT_YVU420)
		){
		printk("ov7736_prev_resolution[fsize->index]   before fsize->index== %d\n",fsize->index);//potti
		if (fsize->index >= ARRAY_SIZE(prev_resolution_array))
			return -EINVAL;
		frmsize = &prev_resolution_array[fsize->index].frmsize;
		printk("ov7736_prev_resolution[fsize->index]   after fsize->index== %d\n",fsize->index);
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	} else if (fmt->fourcc == V4L2_PIX_FMT_RGB24){
		printk("ov7736_pic_resolution[fsize->index]   before fsize->index== %d\n",fsize->index);
		if (fsize->index >= ARRAY_SIZE(capture_resolution_array))
			return -EINVAL;
		frmsize = &capture_resolution_array[fsize->index].frmsize;
		printk("ov7736_pic_resolution[fsize->index]   after fsize->index== %d\n",fsize->index);    
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	return ret;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id *i)
{
	return 0;
}

/* only one input in this sample driver */
static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	//if (inp->index >= NUM_INPUTS)
		//return -EINVAL;

	inp->type = V4L2_INPUT_TYPE_CAMERA;
	inp->std = V4L2_STD_525_60;
	sprintf(inp->name, "Camera %u", inp->index);

	return (0);
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct ov7736_fh *fh = priv;
	struct ov7736_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct ov7736_fh *fh = priv;
	struct ov7736_device *dev = fh->dev;

	//if (i >= NUM_INPUTS)
		//return -EINVAL;

	dev->input = i;
	//precalculate_bars(fh);

	return (0);
}

	/* --- controls ---------------------------------------------- */
static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov7736_qctrl); i++)
		if (qc->id && qc->id == ov7736_qctrl[i].id) {
			memcpy(qc, &(ov7736_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct ov7736_fh *fh = priv;
	struct ov7736_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(ov7736_qctrl); i++)
		if (ctrl->id == ov7736_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct ov7736_fh *fh = priv;
	struct ov7736_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(ov7736_qctrl); i++)
		if (ctrl->id == ov7736_qctrl[i].id) {
			if (ctrl->value < ov7736_qctrl[i].minimum ||
			    ctrl->value > ov7736_qctrl[i].maximum ||
			    ov7736_setting(dev,ctrl->id,ctrl->value)<0) {
				return -ERANGE;
			}
			dev->qctl_regs[i] = ctrl->value;
			return 0;
		}
	return -EINVAL;
}

/* ------------------------------------------------------------------
	File operations for the device
   ------------------------------------------------------------------*/

static int ov7736_open(struct file *file)
{
	struct ov7736_device *dev = video_drvdata(file);
	struct ov7736_fh *fh = NULL;
	int retval = 0;
	ov7736_have_opened=1;
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	OV7736_init_regs(dev);
	msleep(40);
	mutex_lock(&dev->mutex);
	dev->users++;
	if (dev->users > 1) {
		dev->users--;
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	dprintk(dev, 1, "open %s type=%s users=%d\n",
		video_device_node_name(dev->vdev),
		v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->users);

    	/* init video dma queues */
	INIT_LIST_HEAD(&dev->vidq.active);
	init_waitqueue_head(&dev->vidq.wq);
	spin_lock_init(&dev->slock);
	/* allocate + initialize per filehandle data */
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh) {
		dev->users--;
		retval = -ENOMEM;
	}
	mutex_unlock(&dev->mutex);

	if (retval)
		return retval;
	
	wake_lock(&(dev->wake_lock));
	file->private_data = fh;
	fh->dev      = dev;

	fh->type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt      = &formats[0];
	fh->width    = 640;
	fh->height   = 480;
	fh->stream_on = 0 ;
	fh->f_flags  = file->f_flags;
	/* Resets frame counters */
	dev->jiffies = jiffies;

//    TVIN_SIG_FMT_CAMERA_640X480P_30Hz,
//    TVIN_SIG_FMT_CAMERA_800X600P_30Hz,
//    TVIN_SIG_FMT_CAMERA_1024X768P_30Hz, // 190
//    TVIN_SIG_FMT_CAMERA_1920X1080P_30Hz,
//    TVIN_SIG_FMT_CAMERA_1280X720P_30Hz,

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &ov7736_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct ov7736_buffer), fh,NULL);

	ov7736_start_thread(fh);

	return 0;
}

static ssize_t
ov7736_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct ov7736_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
ov7736_poll(struct file *file, struct poll_table_struct *wait)
{
	struct ov7736_fh        *fh = file->private_data;
	struct ov7736_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int ov7736_close(struct file *file)
{
	struct ov7736_fh         *fh = file->private_data;
	struct ov7736_device *dev       = fh->dev;
	struct ov7736_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	ov7736_have_opened=0;

	ov7736_stop_thread(vidq);
	videobuf_stop(&fh->vb_vidq);
	if(fh->stream_on){
	    stop_tvin_service(0);
	}
	videobuf_mmap_free(&fh->vb_vidq);

	kfree(fh);

	mutex_lock(&dev->mutex);
	dev->users--;
	mutex_unlock(&dev->mutex);

	dprintk(dev, 1, "close called (dev=%s, users=%d)\n",
		video_device_node_name(vdev), dev->users);
#if 1
	ov7736_qctrl[0].default_value=0;
	ov7736_qctrl[1].default_value=4;
	ov7736_qctrl[2].default_value=0;
	ov7736_qctrl[3].default_value=0;
	
	ov7736_qctrl[4].default_value=0;
	ov7736_qctrl[6].default_value=100;
	ov7736_qctrl[7].default_value=0;
   	ov7736_frmintervals_active.numerator = 1;
	ov7736_frmintervals_active.denominator = 15;
	power_down_ov7736(dev);
#endif
	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 0);
#endif	
	wake_unlock(&(dev->wake_lock));
	return 0;
}

static int ov7736_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ov7736_fh  *fh = file->private_data;
	struct ov7736_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations ov7736_fops = {
	.owner		= THIS_MODULE,
	.open           = ov7736_open,
	.release        = ov7736_close,
	.read           = ov7736_read,
	.poll		= ov7736_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = ov7736_mmap,
};

static const struct v4l2_ioctl_ops ov7736_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap  = vidioc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_s_std         = vidioc_s_std,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_input       = vidioc_g_input,
	.vidioc_s_input       = vidioc_s_input,
	.vidioc_queryctrl     = vidioc_queryctrl,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
	.vidioc_g_parm 		  = vidioc_g_parm,
	.vidioc_querymenu     = vidioc_querymenu,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device ov7736_template = {
	.name		= "ov7736_v4l",
	.fops           = &ov7736_fops,
	.ioctl_ops 	= &ov7736_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int ov7736_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV7736, 0);
}

static const struct v4l2_subdev_core_ops ov7736_core_ops = {
	.g_chip_ident = ov7736_g_chip_ident,
};

static const struct v4l2_subdev_ops ov7736_ops = {
	.core = &ov7736_core_ops,
};

static int ov7736_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	aml_plat_cam_data_t* plat_dat;
	int err;
	struct ov7736_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &ov7736_ops);
	mutex_init(&t->mutex);
	this_client=client;

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &ov7736_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	wake_lock_init(&(t->wake_lock),WAKE_LOCK_SUSPEND, "ov7736");
	/* Register it */
	plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		t->platform_dev_data.device_probe=plat_dat->device_probe;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
		power_down_ov7736(t);
		if(t->platform_dev_data.device_uninit) {
		t->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
		}
	}
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

	return 0;
}

static int ov7736_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov7736_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	wake_lock_destroy(&(t->wake_lock));
	kfree(t);
	return 0;
}

static const struct i2c_device_id ov7736_id[] = {
	{ "ov7736_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov7736_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "ov7736",
	.probe = ov7736_probe,
	.remove = ov7736_remove,
	.id_table = ov7736_id,
};

