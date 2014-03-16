/*
 *sp0718 - This code emulates a real video device with v4l2 api
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

#define SP0718_CAMERA_MODULE_NAME "sp0718"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */
#define  SP0718_P1_0xeb  0x78
#define  SP0718_P1_0xec  0x6c//HEQ
#define  SP0718_P1_0x10  0x00//outdoor
#define  SP0718_P1_0x14  0x20
#define  SP0718_P1_0x11  0x00//nr
#define  SP0718_P1_0x15  0x18
#define  SP0718_P1_0x12  0x00//dummy
#define  SP0718_P1_0x16  0x10
#define  SP0718_P1_0x13  0x00//low
#define  SP0718_P1_0x17  0x00

#define SP0718_CAMERA_MAJOR_VERSION 0
#define SP0718_CAMERA_MINOR_VERSION 7
#define SP0718_CAMERA_RELEASE 0
#define SP0718_CAMERA_VERSION \
	KERNEL_VERSION(SP0718_CAMERA_MAJOR_VERSION, SP0718_CAMERA_MINOR_VERSION, SP0718_CAMERA_RELEASE)



MODULE_DESCRIPTION("sp0718 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static int sp0718_have_open=0;

static int sp0718_h_active=320;
static int sp0718_v_active=240;
static struct v4l2_fract sp0718_frmintervals_active = {
    .numerator = 1,
    .denominator = 15,
};


/* supported controls */
static struct v4l2_queryctrl sp0718_qctrl[] = {
	{
		.id            = V4L2_CID_DO_WHITE_BALANCE,
		.type          = V4L2_CTRL_TYPE_MENU,
		.name          = "white balance",
		.minimum       = 0,
		.maximum       = 6,
		.step          = 0x1,
		.default_value = 0,
		.flags         = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id            = V4L2_CID_EXPOSURE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "exposure",
		.minimum       = 0,
		.maximum       = 8,
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
		.id            = V4L2_CID_BLUE_BALANCE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "scene mode",
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

static struct v4l2_frmivalenum sp0718_frmivalenum[]={
    {
        .index 		= 0,
        .pixel_format	= V4L2_PIX_FMT_NV21,
        .width		= 640,
        .height		= 480,
        .type		= V4L2_FRMIVAL_TYPE_DISCRETE,
        {
            .discrete	={
                .numerator	= 1,
                .denominator	= 15,
            }
        }
    },{
        .index 		= 1,
        .pixel_format	= V4L2_PIX_FMT_NV21,
        .width		= 1600,
        .height		= 1200,
        .type		= V4L2_FRMIVAL_TYPE_DISCRETE,
        {
            .discrete	={
                .numerator	= 1,
                .denominator	= 5,
            }
        }
    },
};

struct v4l2_querymenu sp0718_qmenu_wbmode[] = {
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

struct v4l2_querymenu sp0718_qmenu_anti_banding_mode[] = {
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
    struct v4l2_querymenu* sp0718_qmenu;
}sp0718_qmenu_set_t;

sp0718_qmenu_set_t sp0718_qmenu_set[] = {
    {
        .id         	= V4L2_CID_DO_WHITE_BALANCE,
        .num            = ARRAY_SIZE(sp0718_qmenu_wbmode),
        .sp0718_qmenu   = sp0718_qmenu_wbmode,
    },{
        .id         	= V4L2_CID_POWER_LINE_FREQUENCY,
        .num            = ARRAY_SIZE(sp0718_qmenu_anti_banding_mode),
        .sp0718_qmenu   = sp0718_qmenu_anti_banding_mode,
    },
};

static int vidioc_querymenu(struct file *file, void *priv,
                struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(sp0718_qmenu_set); i++)
	if (a->id && a->id == sp0718_qmenu_set[i].id) {
	    for(j = 0; j < sp0718_qmenu_set[i].num; j++)
		if (a->index == sp0718_qmenu_set[i].sp0718_qmenu[j].index) {
			memcpy(a, &( sp0718_qmenu_set[i].sp0718_qmenu[j]),
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

struct sp0718_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct sp0718_fmt formats[] = {
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

static struct sp0718_fmt *get_format(struct v4l2_format *f)
{
	struct sp0718_fmt *fmt;
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
struct sp0718_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct sp0718_fmt        *fmt;
};

struct sp0718_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(sp0718_devicelist);

struct sp0718_device {
	struct list_head			sp0718_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct sp0718_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;
	
	/* wake lock */
	struct wake_lock	wake_lock;

	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(sp0718_qctrl)];
};

static inline struct sp0718_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sp0718_device, sd);
}

struct sp0718_fh {
	struct sp0718_device            *dev;

	/* video capture */
	struct sp0718_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
	unsigned int		f_flags;
};

static inline struct sp0718_fh *to_fh(struct sp0718_device *dev)
{
	return container_of(dev, struct sp0718_fh, dev);
}

static struct v4l2_frmsize_discrete sp0718_prev_resolution[3]= //should include 320x240 and 640x480, those two size are used for recording
{
	{320,240},
    //{352,288},
	{640,480},
};

static struct v4l2_frmsize_discrete sp0718_pic_resolution[1]=
{
	{640,480},
};

/* ------------------------------------------------------------------
	reg spec of SP0718
   ------------------------------------------------------------------*/

#if 1

struct aml_camera_i2c_fig1_s SP0718_script[] = {
		{0xfd,0x00},
		{0x1C,0x00},
		{0x31,0x10},
		{0x27,0xb3},//0xb3	//2x gain
		{0x1b,0x17},
		{0x26,0xaa},
		{0x37,0x02},
		{0x28,0x8f},
		{0x1a,0x73},
		{0x1e,0x1b},
		{0x21,0x06},  //blackout voltage
		{0x22,0x2a},  //colbias
		{0x0f,0x3f},
		{0x10,0x3e},
		{0x11,0x00},
		{0x12,0x01},
		{0x13,0x3f},
		{0x14,0x04},
		{0x15,0x30},
		{0x16,0x31},
		{0x17,0x01},
		{0x69,0x31},
		{0x6a,0x2a},
		{0x6b,0x33},
		{0x6c,0x1a},
		{0x6d,0x32},
		{0x6e,0x28},
		{0x6f,0x29},
		{0x70,0x34},
		{0x71,0x18},
		{0x36,0x00},//02 delete badframe
		{0xfd,0x01},
		{0x5d,0x51},//position
		{0xf2,0x19},

		//Blacklevel
		{0x1f,0x10},
		{0x20,0x1f},
		//pregain 
		{0xfd,0x02},
		{0x00,0x88},
		{0x01,0x88},
		//SI15_SP0718 24M 50Hz 15-8fps 
		//ae setting
		{0xfd,0x00},
		{0x03,0x01},
		{0x04,0xce},
		{0x06,0x00},
		{0x09,0x02},
		{0x0a,0xc4},
		{0xfd,0x01},
		{0xef,0x4d},
		{0xf0,0x00},
		{0x02,0x0c},
		{0x03,0x01},
		{0x06,0x47},
		{0x07,0x00},
		{0x08,0x01},
		{0x09,0x00},
		//Status   
		{0xfd,0x02},
		{0xbe,0x9c},
		{0xbf,0x03},
		{0xd0,0x9c},
		{0xd1,0x03},
		{0xfd,0x01},
		{0x5b,0x03},
		{0x5c,0x9c},

		//rpc
		{0xfd,0x01},
		{0xe0,0x40},////24//4c//48//4c//44//4c//3e//3c//3a//38//rpc_1base_max
		{0xe1,0x30},////24//3c//38//3c//36//3c//30//2e//2c//2a//rpc_2base_max
		{0xe2,0x2e},////24//34//30//34//2e//34//2a//28//26//26//rpc_3base_max
		{0xe3,0x2a},////24//2a//2e//2c//2e//2a//2e//26//24//22//rpc_4base_max
		{0xe4,0x2a},////24//2a//2e//2c//2e//2a//2e//26//24//22//rpc_5base_max
		{0xe5,0x28},////24//2c//2a//2c//28//2c//24//22//20//rpc_6base_max
		{0xe6,0x28},////24//2c//2a//2c//28//2c//24//22//20//rpc_7base_max
		{0xe7,0x26},////24//2a//28//2a//26//2a//22//20//20//1e//rpc_8base_max
		{0xe8,0x26},////24//2a//28//2a//26//2a//22//20//20//1e//rpc_9base_max
		{0xe9,0x26},////24//2a//28//2a//26//2a//22//20//20//1e//rpc_10base_max
		{0xea,0x26},////24//28//26//28//24//28//20//1f//1e//1d//rpc_11base_max
		{0xf3,0x26},////24//28//26//28//24//28//20//1f//1e//1d//rpc_12base_max
		{0xf4,0x26},////24//28//26//28//24//28//20//1f//1e//1d//rpc_13base_max
		//ae gain &status
		{0xfd,0x01},
		{0x04,0xe0},//rpc_max_indr
		{0x05,0x26},//1e//rpc_min_indr 
		{0x0a,0xa0},//rpc_max_outdr
		{0x0b,0x26},//rpc_min_outdr
		{0x5a,0x40},//dp rpc   
		{0xfd,0x02}, 
		{0xbc,0xa0},//rpc_heq_low
		{0xbd,0x80},//rpc_heq_dummy
		{0xb8,0x80},//mean_normal_dummy
		{0xb9,0x90},//mean_dummy_normal

		//ae target
		{0xfd,0x01}, 
		{0xeb,SP0718_P1_0xeb},//78 
		{0xec,SP0718_P1_0xec},//78
		{0xed,0x0a},	
		{0xee,0x10},

		//lsc       
		{0xfd,0x01},
		{0x26,0x30},
		{0x27,0x2c},
		{0x28,0x07},
		{0x29,0x08},
		{0x2a,0x40},
		{0x2b,0x03},
		{0x2c,0x00},
		{0x2d,0x00},

		{0xa1,0x24},
		{0xa2,0x27},
		{0xa3,0x27},
		{0xa4,0x2b},
		{0xa5,0x1c},
		{0xa6,0x1a},
		{0xa7,0x1a},
		{0xa8,0x1a},
		{0xa9,0x18},
		{0xaa,0x1c},
		{0xab,0x17},
		{0xac,0x17},
		{0xad,0x08},
		{0xae,0x08},
		{0xaf,0x08},
		{0xb0,0x00},
		{0xb1,0x00},
		{0xb2,0x00},
		{0xb3,0x00},
		{0xb4,0x00},
		{0xb5,0x02},
		{0xb6,0x06},
		{0xb7,0x00},
		{0xb8,0x00},


		//DP       
		{0xfd,0x01},
		{0x48,0x09},
		{0x49,0x99},

		//awb       
		{0xfd,0x01},
		{0x32,0x05},
		{0xfd,0x00},
		{0xe7,0x03},
		{0xfd,0x02},
		{0x26,0xc8},
		{0x27,0xb6},
		{0xfd,0x00},
		{0xe7,0x00},
		{0xfd,0x02},
		{0x1b,0x80},
		{0x1a,0x80},
		{0x18,0x26},
		{0x19,0x28},
		{0xfd,0x02},
		{0x2a,0x00},
		{0x2b,0x08},
		{0x28,0xef},//0xa0//f8
		{0x29,0x20},

		//d65 90  e2 93
		{0x66,0x42},//0x59//0x60////0x58//4e//0x48
		{0x67,0x62},//0x74//0x70//0x78//6b//0x69
		{0x68,0xee},//0xd6//0xe3//0xd5//cb//0xaa
		{0x69,0x18},//0xf4//0xf3//0xf8//ed
		{0x6a,0xa6},//0xa5
		//indoor 91
		{0x7c,0x3b},//0x45//30//41//0x2f//0x44
		{0x7d,0x5b},//0x70//60//55//0x4b//0x6f
		{0x7e,0x15},//0a//0xed
		{0x7f,0x39},//23//0x28
		{0x80,0xaa},//0xa6
		//cwf   92 
		{0x70,0x3e},//0x38//41//0x3b
		{0x71,0x59},//0x5b//5f//0x55
		{0x72,0x31},//0x30//22//0x28
		{0x73,0x4f},//0x54//44//0x45
		{0x74,0xaa},
		//tl84  93 
		{0x6b,0x1b},//0x18//11
		{0x6c,0x3a},//0x3c//25//0x2f
		{0x6d,0x3e},//0x3a//35
		{0x6e,0x59},//0x5c//46//0x52
		{0x6f,0xaa},
		//f    94
		{0x61,0xea},//0x03//0x00//f4//0xed
		{0x62,0x03},//0x1a//0x25//0f//0f
		{0x63,0x6a},//0x62//0x60//52//0x5d
		{0x64,0x8a},//0x7d//0x85//70//0x75//0x8f
		{0x65,0x6a},//0xaa//6a
		  
		{0x75,0x80},
		{0x76,0x20},
		{0x77,0x00},
		{0x24,0x25},

		//针对室内调偏不过灯箱测试使用//针对人脸调偏
		{0x20,0xd8},
		{0x21,0xa3},//82//a8偏暗照度还有调偏
		{0x22,0xd0},//e3//bc
		{0x23,0x86},

		//outdoor r\b range
		{0x78,0xc3},//d8
		{0x79,0xba},//82
		{0x7a,0xa6},//e3
		{0x7b,0x99},//86


		//skin 
		{0x08,0x15},//
		{0x09,0x04},//
		{0x0a,0x20},//
		{0x0b,0x12},//
		{0x0c,0x27},//
		{0x0d,0x06},//
		{0x0e,0x63},//

		//wt th
		{0x3b,0x10},
		//gw
		{0x31,0x60},
		{0x32,0x60},
		{0x33,0xc0},
		{0x35,0x6f},

		// sharp
		{0xfd,0x02},
		{0xde,0x0f},
		{0xd2,0x02},//6//控制黑白边；0-边粗，f-变细
		{0xd3,0x06},
		{0xd4,0x06},
		{0xd5,0x06},
		{0xd7,0x20},//10//2x根据增益判断轮廓阈值
		{0xd8,0x30},//24//1A//4x
		{0xd9,0x38},//28//8x
		{0xda,0x38},//16x
		{0xdb,0x08},//
		{0xe8,0x58},//48//轮廓强度
		{0xe9,0x48},
		{0xea,0x30},
		{0xeb,0x20},
		{0xec,0x48},//60//80
		{0xed,0x48},//50//60
		{0xee,0x30},
		{0xef,0x20},
		//平坦区域锐化力度
		{0xf3,0x50},
		{0xf4,0x10},
		{0xf5,0x10},
		{0xf6,0x10},
		//dns       
		{0xfd,0x01},
		{0x64,0x44},//沿方向边缘平滑力度  //0-最强，8-最弱
		{0x65,0x22},
		{0x6d,0x04},//8//强平滑（平坦）区域平滑阈值
		{0x6e,0x06},//8
		{0x6f,0x10},
		{0x70,0x10},
		{0x71,0x08},//0d//弱平滑（非平坦）区域平滑阈值	
		{0x72,0x12},//1b
		{0x73,0x1c},//20
		{0x74,0x24},
		{0x75,0x44},//[7:4]平坦区域强度，[3:0]非平坦区域强度；0-最强，8-最弱；
		{0x76,0x02},//46
		{0x77,0x02},//33
		{0x78,0x02},
		{0x81,0x10},//18//2x//根据增益判定区域阈值，低于这个做强平滑、大于这个做弱平滑；
		{0x82,0x20},//30//4x
		{0x83,0x30},//40//8x
		{0x84,0x48},//50//16x
		{0x85,0x0c},//12/8+reg0x81 第二阈值，在平坦和非平坦区域做连接
		{0xfd,0x02},
		{0xdc,0x0f},
		   
		//gamma    
		{0xfd,0x01},
		{0x8b,0x00},//00//00     
		{0x8c,0x0a},//0c//09     
		{0x8d,0x16},//19//17     
		{0x8e,0x1f},//25//24     
		{0x8f,0x2a},//30//33     
		{0x90,0x3c},//44//47     
		{0x91,0x4e},//54//58     
		{0x92,0x5f},//61//64     
		{0x93,0x6c},//6d//70     
		{0x94,0x82},//80//81     
		{0x95,0x94},//92//8f     
		{0x96,0xa6},//a1//9b     
		{0x97,0xb2},//ad//a5     
		{0x98,0xbf},//ba//b0     
		{0x99,0xc9},//c4//ba     
		{0x9a,0xd1},//cf//c4     
		{0x9b,0xd8},//d7//ce     
		{0x9c,0xe0},//e0//d7     
		{0x9d,0xe8},//e8//e1     
		{0x9e,0xef},//ef//ea     
		{0x9f,0xf8},//f7//f5     
		{0xa0,0xff},//ff//ff     
		//CCM      
		{0xfd,0x02},
		{0x15,0xd0},//b>th
		{0x16,0x95},//r<th
		//gc镜头照人脸偏黄
		//!F        
		{0xa0,0x80},//80
		{0xa1,0x00},//00
		{0xa2,0x00},//00
		{0xa3,0x00},//06
		{0xa4,0x8c},//8c
		{0xa5,0xf4},//ed
		{0xa6,0x0c},//0c
		{0xa7,0xf4},//f4
		{0xa8,0x80},//80
		{0xa9,0x00},//00
		{0xaa,0x30},//30
		{0xab,0x0c},//0c 
		//F        
		{0xac,0x8c},
		{0xad,0xf4},
		{0xae,0x00},
		{0xaf,0xed},
		{0xb0,0x8c},
		{0xb1,0x06},
		{0xb2,0xf4},
		{0xb3,0xf4},
		{0xb4,0x99},
		{0xb5,0x0c},
		{0xb6,0x03},
		{0xb7,0x0f},
		    
		//sat u     
		{0xfd,0x01},
		{0xd3,0x9c},//0x88//50
		{0xd4,0x98},//0x88//50
		{0xd5,0x8c},//50
		{0xd6,0x84},//50
		//sat v   
		{0xd7,0x9c},//0x88//50
		{0xd8,0x98},//0x88//50
		{0xd9,0x8c},//50
		{0xda,0x84},//50
		//auto_sat  
		{0xdd,0x30},
		{0xde,0x10},
		{0xd2,0x01},//autosa_en
		{0xdf,0xff},//a0//y_mean_th
		    
		//uv_th     
		{0xfd,0x01},
		{0xc2,0xaa},
		{0xc3,0xaa},
		{0xc4,0x66},
		{0xc5,0x66}, 

		//heq
		{0xfd,0x01},
		{0x0f,0xff},
		{0x10,SP0718_P1_0x10}, //out
		{0x14,SP0718_P1_0x14}, 
		{0x11,SP0718_P1_0x11}, //nr
		{0x15,SP0718_P1_0x15},  
		{0x12,SP0718_P1_0x12}, //dummy
		{0x16,SP0718_P1_0x16}, 
		{0x13,SP0718_P1_0x13}, //low 	
		{0x17,SP0718_P1_0x17},   	

		{0xfd,0x01},
		{0xcd,0x20},
		{0xce,0x1f},
		{0xcf,0x20},
		{0xd0,0x55},  
		//auto 
		{0xfd,0x01},
		{0xfb,0x33},
		//{0x32,0x15},
		{0x33,0xff},
		{0x34,0xe7},
		{0x35,0x41},
		{0xff,0xff},  
 };


void SP0718_init_regs(struct sp0718_device *dev)
{
    int i=0;//,j;
    unsigned char buf[2];
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    while(1)
    {
        buf[0] = SP0718_script[i].addr;//(unsigned char)((SP0718_script[i].addr >> 8) & 0xff);
        //buf[1] = (unsigned char)(SP0718_script[i].addr & 0xff);
        buf[1] = SP0718_script[i].val;
        if(SP0718_script[i].val==0xff&&SP0718_script[i].addr==0xff){
            printk("SP0718_write_regs success in initial SP0718.\n");
            break;
        }
        if((i2c_put_byte_add8(client,buf, 2)) < 0){
            printk("fail in initial SP0718. \n");
            return;
        }
        i++;
    }
    aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
    if (plat_dat&&plat_dat->custom_init_script) {
        i=0;
        aml_camera_i2c_fig1_t*  custom_script = (aml_camera_i2c_fig1_t*)plat_dat->custom_init_script;
        while(1)
        {
            buf[0] = custom_script[i].addr;
            buf[1] = custom_script[i].val;
            if (custom_script[i].val==0xff&&custom_script[i].addr==0xff){
                printk("SP0718_write_custom_regs success in initial SP0718.\n");
                break;
            }
            if((i2c_put_byte_add8(client,buf, 2)) < 0){
                printk("fail in initial SP0718 custom_regs. \n");
                return;
            }
            i++;
        }
    }
    return;

}

#endif


static struct aml_camera_i2c_fig1_s resolution_320x240_script[] = {
	{0xfd, 0x02},
	{0x0f, 0x01},
	{0xfd, 0x00},
	{0x30, 0x10},              
	{0xff, 0xff}
};

static struct aml_camera_i2c_fig1_s resolution_640x480_script[] = {
	{0xfd, 0x02},
	{0x0f, 0x00},
	{0xfd, 0x00},
	{0x30, 0x00},              
	{0xff, 0xff}
};

static void sp0718_set_resolution(struct sp0718_device *dev,int height,int width)
{
	int i=0;
    unsigned char buf[2];
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	struct aml_camera_i2c_fig1_s* resolution_script;
	if (width*height >= 640*480) {
		printk("set resolution 640X480\n");
		resolution_script = resolution_640x480_script;
		sp0718_h_active = 640;
		sp0718_v_active = 478;

		sp0718_frmintervals_active.denominator 	= 15;
		sp0718_frmintervals_active.numerator	= 1;
		//SP0718_init_regs(dev);
		//return;
	} else {
		printk("set resolution 320X240\n");
		sp0718_h_active = 320;
		sp0718_v_active = 238;
		sp0718_frmintervals_active.denominator 	= 15;
		sp0718_frmintervals_active.numerator	= 1;

		resolution_script = resolution_320x240_script;
	}
	
	while(1) {
        buf[0] = resolution_script[i].addr;
        buf[1] = resolution_script[i].val;
        if(resolution_script[i].val==0xff&&resolution_script[i].addr==0xff) {
            break;
        }
        if((i2c_put_byte_add8(client,buf, 2)) < 0) {
            printk("fail in setting resolution \n");
            return;
        }
        i++;
    }
	
}
/*************************************************************************
* FUNCTION
*	set_SP0718_param_wb
*
* DESCRIPTION
*	SP0718 wb setting.
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
void set_SP0718_param_wb(struct sp0718_device *dev,enum  camera_wb_flip_e para)
{
//	kal_uint16 rgain=0x80, ggain=0x80, bgain=0x80;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	unsigned char buf[4];

	unsigned char  temp_reg;
	//temp_reg=sp0718_read_byte(0x22);
	buf[0]=0x32;
	temp_reg=i2c_get_byte_add8(client,buf);

	printk(" camera set_SP0718_param_wb=%d. \n ",para);
	switch (para)
	{
		case CAM_WB_AUTO:
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x26;
			buf[1]=0xc8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x27;
			buf[1]=0xb6;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x32;
			buf[1]=0x15;  //temp_reg|0x10;    // SP0718 AWB enable bit[1]   ie. 0x02;
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_CLOUD:
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);

			buf[0]=0x32;
			buf[1]=0x05;//temp_reg&~0x10;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x26;
			buf[1]=0xe8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x27;
			buf[1]=0x70;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			//buf[0]=0x31;
			//buf[1]=0x30;//   阴天
			//i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_DAYLIGHT:   // tai yang guang
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);

			buf[0]=0x32;
			buf[1]=0x05;//temp_reg&~0x10;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x26;
			buf[1]=0xc8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x27;
			buf[1]=0x89;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_INCANDESCENCE:   // bai re guang
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);

			buf[0]=0x32;
			buf[1]=0x05;//temp_reg&~0x10;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x26;
			buf[1]=0x70;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x27;
			buf[1]=0xf0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			//buf[0]=0x31;
			//buf[1]=0x50;//日光灯
			//i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_FLUORESCENT:   //ri guang deng
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);

			buf[0]=0x32;
			buf[1]=0x05;//temp_reg&~0x10;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x26;
			buf[1]=0x91;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x27;
			buf[1]=0xc8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			//buf[0]=0x31;
			//buf[1]=0x70;//荧光灯
			//i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_TUNGSTEN:   // wu si deng
		#if 1
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);

			buf[0]=0x32;
			buf[1]=0x05;//temp_reg&~0x10;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x26;
			buf[1]=0x9c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x27;
			buf[1]=0xc0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			#else
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);

			buf[0]=0x32;
			buf[1]=0x05;//temp_reg&~0x10;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x26;
			buf[1]=0xd8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x27;
			buf[1]=0xe0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);

			#endif
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
*	SP0718_night_mode
*
* DESCRIPTION
*	This function night mode of SP0718.
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
void SP0718_night_mode(struct sp0718_device *dev,enum  camera_night_mode_flip_e enable)
	{
		struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
		unsigned char buf[4];
	
		unsigned char  temp_reg;
		//temp_reg=SP0718_read_byte(0x22);
		//buf[0]=0x20;
		temp_reg=i2c_get_byte_add8(client,buf);
		temp_reg=0xff;
	
		if(enable)
		{
	
#if 0
			capture preview night 24M 50hz 15-6FPS 
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x04;
			buf[1]=0xce;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x0a;
			buf[1]=0xc4;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xef;
			buf[1]=0x4d;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xf0;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x10;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x47;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x07;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x08;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);			 
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbe;
			buf[1]=0xd0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbf;
			buf[1]=0x04;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd0;
			buf[1]=0xd0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd1;
			buf[1]=0x04;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x04;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0xd0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x32;
			buf[1]=0x15;
			i2c_put_byte_add8(client,buf,2);
	#endif
		 }
		else
		 {
     #if 0
					  // SP0718  24M  50HZ	8-15FPS 
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x04;
			buf[1]=0xce;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x0a;
			buf[1]=0xc4;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xef;
			buf[1]=0x4d;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xf0;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x0c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x47;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x07;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x08;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);				
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbe;
			buf[1]=0x9c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbf;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd0;
			buf[1]=0x9c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd1;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x9c;
			i2c_put_byte_add8(client,buf,2);  
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x32;
			buf[1]=0x15;
			i2c_put_byte_add8(client,buf,2);
             #endif
		}
	
	}

/*************************************************************************
* FUNCTION
*	SP0718_night_mode
*
* DESCRIPTION
*	This function night mode of SP0718.
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

void SP0718_set_param_banding(struct sp0718_device *dev,enum  camera_night_mode_flip_e banding)
	{
		struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
		unsigned char buf[4];
		switch(banding){
			case CAM_BANDING_60HZ:
				
			   // SP0718  24M  60HZ  8-15FPS 
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x04;
			buf[1]=0x80;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x0a;
			buf[1]=0xc9;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xef;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xf0;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x0f;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x3a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x07;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x08;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);		  
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbe;
			buf[1]=0xc0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbf;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd0;
			buf[1]=0xc0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd1;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0xc0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x32;
			buf[1]=0x15;
			i2c_put_byte_add8(client,buf,2);
	
				break;
	   case CAM_BANDING_50HZ:
				
				// SP0718  24M	50HZ  8-15FPS 
			buf[0]=0xfd;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x04;
			buf[1]=0xce;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x0a;
			buf[1]=0xc4;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xef;
			buf[1]=0x4d;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xf0;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x0c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x03;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x06;
			buf[1]=0x47;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x07;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x08;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x09;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);				
			buf[0]=0xfd;
			buf[1]=0x02;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbe;
			buf[1]=0x9c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xbf;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd0;
			buf[1]=0x9c;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xd1;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5b;
			buf[1]=0x03;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x5c;
			buf[1]=0x9c;
			i2c_put_byte_add8(client,buf,2);  
			buf[0]=0xfd;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x32;
			buf[1]=0x15;
			i2c_put_byte_add8(client,buf,2); 
				break;
		}
	}



/*************************************************************************
* FUNCTION
*	set_SP0718_param_exposure
*
* DESCRIPTION
*	SP0718 exposure setting.
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
void set_SP0718_param_exposure(struct sp0718_device *dev,enum camera_exposure_e para)//曝光调节
	{
		struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	
		unsigned char buf1[2];
		unsigned char buf2[2];
	
		switch (para)
		{
				case EXPOSURE_N4_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0xc0;
				break;
			case EXPOSURE_N3_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0xd0;
				break;
			case EXPOSURE_N2_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0xe0;
				break;
			case EXPOSURE_N1_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0xf0;
				break;
			case EXPOSURE_0_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0x00;//6a
				break;
			case EXPOSURE_P1_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0x10;
				break;
			case EXPOSURE_P2_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0x20;
				break;
			case EXPOSURE_P3_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0x30;
				break;
			case EXPOSURE_P4_STEP:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0x40;
				break;
			default:
				buf1[0]=0xfd;
				buf1[1]=0x01;
				buf2[0]=0xdb;
				buf2[1]=0x00;
				break; 
		} 
		//msleep(300);	
		i2c_put_byte_add8(client,buf1,2);
		i2c_put_byte_add8(client,buf2,2);
	
	}


/*************************************************************************
* FUNCTION
*	set_SP0718_param_effect
*
* DESCRIPTION
*	SP0718 effect setting.
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
void set_SP0718_param_effect(struct sp0718_device *dev,enum camera_effect_flip_e para)//特效设置
{
		struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
		unsigned char buf[4];
		switch (para)
		{
			case CAM_EFFECT_ENC_NORMAL:
				buf[0]=0xfd;
				buf[1]=0x01;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x66;
				buf[1]=0x00;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x67;
				buf[1]=0x80;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x68;
				buf[1]=0x80;
				i2c_put_byte_add8(client,buf,2);
	
				break;
			case CAM_EFFECT_ENC_GRAYSCALE:
				buf[0]=0xfd;
				buf[1]=0x001;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x66;
				buf[1]=0x20;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x67;
				buf[1]=0x80;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x68;
				buf[1]=0x80;
				i2c_put_byte_add8(client,buf,2);
	
				break;
			case CAM_EFFECT_ENC_SEPIA:
				buf[0]=0xfd;
				buf[1]=0x001;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x66;
				buf[1]=0x10;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x67;
				buf[1]=0xc0;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x68;
				buf[1]=0x20;
				i2c_put_byte_add8(client,buf,2);
	
				break;
			case CAM_EFFECT_ENC_COLORINV:
				buf[0]=0xfd;
				buf[1]=0x001;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x66;
				buf[1]=0x04;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x67;
				buf[1]=0x80;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x68;
				buf[1]=0x80;
				i2c_put_byte_add8(client,buf,2);
				break;
			case CAM_EFFECT_ENC_SEPIAGREEN:
				buf[0]=0xfd;
				buf[1]=0x001;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x66;
				buf[1]=0x10;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x67;
				buf[1]=0x20;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x68;
				buf[1]=0x20;
				i2c_put_byte_add8(client,buf,2);
				break;
				
			case CAM_EFFECT_ENC_SEPIABLUE:
				buf[0]=0xfd;
				buf[1]=0x001;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x66;
				buf[1]=0x10;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x67;
				buf[1]=0x20;
				i2c_put_byte_add8(client,buf,2);
				buf[0]=0x68;
				buf[1]=0xf0;
				i2c_put_byte_add8(client,buf,2);
	
				break;
			default:
				break;	
		}
	
}

unsigned char v4l_2_sp0718(int val)
{
	int ret=val/0x20;
	if(ret<4) return ret*0x20+0x80;
	else if(ret<8) return ret*0x20+0x20;
	else return 0;
}

static int sp0718_setting(struct sp0718_device *dev,int PROP_ID,int value )
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
#if 0
	case V4L2_CID_BRIGHTNESS:
		dprintk(dev, 1, "setting brightned:%d\n",v4l_2_sp0718(value));
		ret=i2c_put_byte(client,0x0201,v4l_2_sp0718(value));
		break;
	case V4L2_CID_CONTRAST:
		ret=i2c_put_byte(client,0x0200, value);
		break;
	case V4L2_CID_SATURATION:
		ret=i2c_put_byte(client,0x0202, value);
		break;
	case V4L2_CID_HFLIP:    /* set flip on H. */
		ret=i2c_get_byte(client,0x0101);
		if(ret>0) {
			cur_val=(char)ret;
			if(value!=0)
				cur_val=cur_val|0x1;
			else
				cur_val=cur_val&0xFE;
			ret=i2c_put_byte(client,0x0101,cur_val);
			if(ret<0) dprintk(dev, 1, "V4L2_CID_HFLIP setting error\n");
		}  else {
			dprintk(dev, 1, "vertical read error\n");
		}
		break;
	case V4L2_CID_VFLIP:    /* set flip on V. */
		ret=i2c_get_byte(client,0x0101);
		if(ret>0) {
			cur_val=(char)ret;
			if(value!=0)
				cur_val=cur_val|0x10;
			else
				cur_val=cur_val&0xFD;
			ret=i2c_put_byte(client,0x0101,cur_val);
		} else {
			dprintk(dev, 1, "vertical read error\n");
		}
		break;
#endif
	case V4L2_CID_DO_WHITE_BALANCE:
		if(sp0718_qctrl[0].default_value!=value){
			sp0718_qctrl[0].default_value=value;
			set_SP0718_param_wb(dev,value);
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
		}
		break;
	case V4L2_CID_EXPOSURE:
		if(sp0718_qctrl[1].default_value!=value){
			sp0718_qctrl[1].default_value=value;
			set_SP0718_param_exposure(dev,value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
		}
		break;
	case V4L2_CID_COLORFX:
		if(sp0718_qctrl[2].default_value!=value){
			sp0718_qctrl[2].default_value=value;
			set_SP0718_param_effect(dev,value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
		}
		break;
	case V4L2_CID_WHITENESS:
		if(sp0718_qctrl[3].default_value!=value){
			sp0718_qctrl[3].default_value=value;
			SP0718_set_param_banding(dev,value);
			printk(KERN_INFO " set camera  banding=%d. \n ",value);
		}
		break;
	case V4L2_CID_BLUE_BALANCE:
		if(sp0718_qctrl[4].default_value!=value){
			sp0718_qctrl[4].default_value=value;
			SP0718_night_mode(dev,value);
			printk(KERN_INFO " set camera  scene mode=%d. \n ",value);
		}
		break;
	case V4L2_CID_HFLIP:    /* set flip on H. */          
		value = value & 0x3;
		if(sp0718_qctrl[5].default_value!=value){
			sp0718_qctrl[5].default_value=value;
			printk(" set camera  h filp =%d. \n ",value);
		}
		break;
	case V4L2_CID_VFLIP:    /* set flip on V. */         
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		if(sp0718_qctrl[7].default_value!=value){
			sp0718_qctrl[7].default_value=value;
			//printk(KERN_INFO " set camera  zoom mode=%d. \n ",value);
		}
		break;
	case V4L2_CID_ROTATE:
		if(sp0718_qctrl[8].default_value!=value){
			sp0718_qctrl[8].default_value=value;
			printk(" set camera  rotate =%d. \n ",value);
		}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;

}

static void power_down_sp0718(struct sp0718_device *dev)
{
	/*struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[4];
	buf[0]=0x1a;
	buf[1]=0x17;
	i2c_put_byte_add8(client,buf,2);
	buf[0]=0x25;
	buf[1]=0x00;
	i2c_put_byte_add8(client,buf,2);
	
	msleep(5);*/
	return;
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void sp0718_fillbuff(struct sp0718_fh *fh, struct sp0718_buffer *buf)
{
	struct sp0718_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	vm_output_para_t para = {0};
	dprintk(dev,1,"%s\n", __func__);
	if (!vbuf)
		return;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
	//para.mirror = sp0718_qctrl[5].default_value&3;// not set
	para.v4l2_format = fh->fmt->fourcc;
	para.v4l2_memory = 0x18221223;
	para.zoom = sp0718_qctrl[7].default_value;
	para.vaddr = (unsigned)vbuf;
	para.angle = sp0718_qctrl[8].default_value;
	vm_fill_buffer(&buf->vb,&para);
	buf->vb.state = VIDEOBUF_DONE;
}

static void sp0718_thread_tick(struct sp0718_fh *fh)
{
	struct sp0718_buffer *buf;
	struct sp0718_device *dev = fh->dev;
	struct sp0718_dmaqueue *dma_q = &dev->vidq;

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
			 struct sp0718_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",(unsigned)buf);

    if(!(fh->f_flags & O_NONBLOCK)){
        /* Nobody is waiting on this buffer, return */
        if (!waitqueue_active(&buf->vb.done))
            goto unlock;
    }
    buf->vb.state = VIDEOBUF_ACTIVE;

	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

	/* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	sp0718_fillbuff(fh, buf);
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

static void sp0718_sleep(struct sp0718_fh *fh)
{
	struct sp0718_device *dev = fh->dev;
	struct sp0718_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	sp0718_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int sp0718_thread(void *data)
{
	struct sp0718_fh  *fh = data;
	struct sp0718_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		sp0718_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int sp0718_start_thread(struct sp0718_fh *fh)
{
	struct sp0718_device *dev = fh->dev;
	struct sp0718_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(sp0718_thread, fh, "sp0718");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void sp0718_stop_thread(struct sp0718_dmaqueue  *dma_q)
{
	struct sp0718_device *dev = container_of(dma_q, struct sp0718_device, vidq);

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
	struct sp0718_fh  *fh = vq->priv_data;
	struct sp0718_device *dev  = fh->dev;
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

static void free_buffer(struct videobuf_queue *vq, struct sp0718_buffer *buf)
{
	struct sp0718_fh  *fh = vq->priv_data;
	struct sp0718_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 1024
#define norm_maxh() 768
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						enum v4l2_field field)
{
	struct sp0718_fh     *fh  = vq->priv_data;
	struct sp0718_device    *dev = fh->dev;
	struct sp0718_buffer *buf = container_of(vb, struct sp0718_buffer, vb);
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
	struct sp0718_buffer    *buf  = container_of(vb, struct sp0718_buffer, vb);
	struct sp0718_fh        *fh   = vq->priv_data;
	struct sp0718_device       *dev  = fh->dev;
	struct sp0718_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct sp0718_buffer   *buf  = container_of(vb, struct sp0718_buffer, vb);
	struct sp0718_fh       *fh   = vq->priv_data;
	struct sp0718_device      *dev  = (struct sp0718_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops sp0718_video_qops = {
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
	struct sp0718_fh  *fh  = priv;
	struct sp0718_device *dev = fh->dev;

	strcpy(cap->driver, "sp0718");
	strcpy(cap->card, "sp0718");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = SP0718_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct sp0718_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}
static int vidioc_enum_frameintervals(struct file *file, void *priv,
        struct v4l2_frmivalenum *fival)
{
    struct sp0718_fmt *fmt;
    unsigned int k;

    if(fival->index > ARRAY_SIZE(sp0718_frmivalenum))
        return -EINVAL;

    for(k =0; k< ARRAY_SIZE(sp0718_frmivalenum); k++)
    {
        if( (fival->index==sp0718_frmivalenum[k].index)&&
                (fival->pixel_format ==sp0718_frmivalenum[k].pixel_format )&&
                (fival->width==sp0718_frmivalenum[k].width)&&
                (fival->height==sp0718_frmivalenum[k].height)){
            memcpy( fival, &sp0718_frmivalenum[k], sizeof(struct v4l2_frmivalenum));
            return 0;
        }
    }

    return -EINVAL;

}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct sp0718_fh *fh = priv;

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
	struct sp0718_fh  *fh  = priv;
	struct sp0718_device *dev = fh->dev;
	struct sp0718_fmt *fmt;
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
	struct sp0718_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;

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
#if 1
	if(f->fmt.pix.pixelformat==V4L2_PIX_FMT_RGB24){
		sp0718_set_resolution(fh->dev,fh->height,fh->width);
	} else {
		sp0718_set_resolution(fh->dev,fh->height,fh->width);
	}
#endif
	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_g_parm(struct file *file, void *priv,
        struct v4l2_streamparm *parms)
{
    struct sp0718_fh *fh = priv;
    struct sp0718_device *dev = fh->dev;
    struct v4l2_captureparm *cp = &parms->parm.capture;
    int ret;
    int i;

    dprintk(dev,3,"vidioc_g_parm\n");
    if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;

    memset(cp, 0, sizeof(struct v4l2_captureparm));
    cp->capability = V4L2_CAP_TIMEPERFRAME;

    cp->timeperframe = sp0718_frmintervals_active;
    printk("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
            cp->timeperframe.numerator );
    return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct sp0718_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0718_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0718_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct sp0718_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct sp0718_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct sp0718_fh  *fh = priv;
    vdin_parm_t para;
    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;

    memset( &para, 0, sizeof( para ));
	para.port  = TVIN_PORT_CAMERA;
	para.fmt = TVIN_SIG_FMT_MAX;
	para.frame_rate = sp0718_frmintervals_active.denominator;
	para.h_active = sp0718_h_active;
	para.v_active = sp0718_v_active;
	para.hsync_phase = 0;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 2;
	para.cfmt = TVIN_YUV422;
	para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;	
	para.reserved = 2; //skip_num
	printk("0718,h=%d, v=%d, frame_rate=%d\n",	sp0718_h_active, sp0718_v_active, sp0718_frmintervals_active.denominator);
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
	    fh->stream_on        = 1;
	}
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct sp0718_fh  *fh = priv;

    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
	ret = videobuf_streamoff(&fh->vb_vidq);
	if(ret == 0 ){
    stop_tvin_service(0);
	    fh->stream_on        = 0;
	}
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,struct v4l2_frmsizeenum *fsize)
{
	int ret = 0,i=0;
	struct sp0718_fmt *fmt = NULL;
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
		if (fsize->index >= ARRAY_SIZE(sp0718_prev_resolution))
			return -EINVAL;
		frmsize = &sp0718_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		if (fsize->index >= ARRAY_SIZE(sp0718_pic_resolution))
			return -EINVAL;
		frmsize = &sp0718_pic_resolution[fsize->index];
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
	struct sp0718_fh *fh = priv;
	struct sp0718_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct sp0718_fh *fh = priv;
	struct sp0718_device *dev = fh->dev;

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

	for (i = 0; i < ARRAY_SIZE(sp0718_qctrl); i++)
		if (qc->id && qc->id == sp0718_qctrl[i].id) {
			memcpy(qc, &(sp0718_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct sp0718_fh *fh = priv;
	struct sp0718_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(sp0718_qctrl); i++)
		if (ctrl->id == sp0718_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct sp0718_fh *fh = priv;
	struct sp0718_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(sp0718_qctrl); i++)
		if (ctrl->id == sp0718_qctrl[i].id) {
			if (ctrl->value < sp0718_qctrl[i].minimum ||
			    ctrl->value > sp0718_qctrl[i].maximum ||
			    sp0718_setting(dev,ctrl->id,ctrl->value)<0) {
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

static int sp0718_open(struct file *file)
{
	struct sp0718_device *dev = video_drvdata(file);
	struct sp0718_fh *fh = NULL;
	int retval = 0;
	sp0718_have_open=1;
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif		
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	SP0718_init_regs(dev);
	msleep(100);//40
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

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &sp0718_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct sp0718_buffer), fh,NULL);

	sp0718_start_thread(fh);

	return 0;
}

static ssize_t
sp0718_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct sp0718_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
sp0718_poll(struct file *file, struct poll_table_struct *wait)
{
	struct sp0718_fh        *fh = file->private_data;
	struct sp0718_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int sp0718_close(struct file *file)
{
	struct sp0718_fh         *fh = file->private_data;
	struct sp0718_device *dev       = fh->dev;
	struct sp0718_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	sp0718_have_open=0;

	sp0718_stop_thread(vidq);
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
	sp0718_qctrl[0].default_value=0;
	sp0718_qctrl[1].default_value=4;
	sp0718_qctrl[2].default_value=0;
	sp0718_qctrl[3].default_value=0;
	sp0718_qctrl[4].default_value=0;

	sp0718_qctrl[5].default_value=0;
	sp0718_qctrl[7].default_value=100;
	sp0718_qctrl[8].default_value=0;

	sp0718_frmintervals_active.numerator = 1;
	sp0718_frmintervals_active.denominator = 15;
	//power_down_sp0718(dev);
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

static int sp0718_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct sp0718_fh  *fh = file->private_data;
	struct sp0718_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations sp0718_fops = {
	.owner		= THIS_MODULE,
	.open           = sp0718_open,
	.release        = sp0718_close,
	.read           = sp0718_read,
	.poll		= sp0718_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = sp0718_mmap,
};

static const struct v4l2_ioctl_ops sp0718_ioctl_ops = {
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
	.vidioc_querymenu     = vidioc_querymenu,
	.vidioc_g_ctrl        = vidioc_g_ctrl,
	.vidioc_s_ctrl        = vidioc_s_ctrl,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
	.vidioc_g_parm = vidioc_g_parm,
	.vidioc_enum_frameintervals = vidioc_enum_frameintervals,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device sp0718_template = {
	.name		= "sp0718_v4l",
	.fops           = &sp0718_fops,
	.ioctl_ops 	= &sp0718_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int sp0718_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_SP0718, 0);
}

static const struct v4l2_subdev_core_ops sp0718_core_ops = {
	.g_chip_ident = sp0718_g_chip_ident,
};

static const struct v4l2_subdev_ops sp0718_ops = {
	.core = &sp0718_core_ops,
};
static struct i2c_client *this_client;

static int sp0718_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	aml_plat_cam_data_t* plat_dat;
	int err;
	struct sp0718_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &sp0718_ops);
	plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	
	/* test if devices exist. */
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_PROBE
	unsigned char buf[4]; 
	buf[0]=0;
	plat_dat->device_init();
	err=i2c_get_byte_add8(client,buf);
	plat_dat->device_uninit();
	if(err<0) return  -ENODEV;
#endif
	/* Now create a video4linux device */
	mutex_init(&t->mutex);

	/* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
		kfree(t);
		kfree(client);
		return -ENOMEM;
	}
	memcpy(t->vdev, &sp0718_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);

	wake_lock_init(&(t->wake_lock),WAKE_LOCK_SUSPEND, "sp0718");
	
	this_client=client;
	/* Register it */
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
			if(t->platform_dev_data.device_init) {
			t->platform_dev_data.device_init();
			printk("+++found a init function, and run it..\n");
		    }
			//power_down_sp0718(t);
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

static int sp0718_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sp0718_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	wake_lock_destroy(&(t->wake_lock));
	kfree(t);
	return 0;
}

static const struct i2c_device_id sp0718_id[] = {
	{ "sp0718_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sp0718_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "sp0718",
	.probe = sp0718_probe,
	.remove = sp0718_remove,
	.id_table = sp0718_id,
};

