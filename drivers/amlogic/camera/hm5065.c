/*
 *HM5065 - This code emulates a real video device with v4l2 api
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
#include <linux/mutex.h>

#include <linux/i2c.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-i2c-drv.h>
#include <media/amlogic/aml_camera.h>

#include <mach/am_regs.h>
//#include <mach/am_eth_pinmux.h>
#include <mach/pinmux.h>
#include <linux/tvin/tvin_v4l2.h>
#include "common/plat_ctrl.h"
#include "common/vmapi.h"
#include "hm5065_firmware.h"

#ifdef CONFIG_ARCH_MESON6
#include <mach/mod_gate.h>
#endif
#define HM5065_CAMERA_MODULE_NAME "hm5065"

#include <media/amlogic/flashlight.h>

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define HM5065_CAMERA_MAJOR_VERSION 0
#define HM5065_CAMERA_MINOR_VERSION 7
#define HM5065_CAMERA_RELEASE 0

#define AF_STATUS     	0x07AE
#define FACE_LC			0x0714
#define FACE_START_XH 	0x0715
#define FACE_START_XL 	0x0716
#define FACE_SIZE_XH  	0x0717
#define FACE_SIZE_XL 	0x0718
#define FACE_START_YH 	0x0719
#define FACE_START_YL 	0x071A
#define FACE_SIZE_YH 	0x071B
#define FACE_SIZE_YL 	0x071C
#define HM5065_CAMERA_VERSION \
	KERNEL_VERSION(HM5065_CAMERA_MAJOR_VERSION, HM5065_CAMERA_MINOR_VERSION, HM5065_CAMERA_RELEASE)

MODULE_DESCRIPTION("hm5065 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");
static DEFINE_MUTEX(firmware_mutex);
static int customer_i2c_write(struct i2c_client *i2client,unsigned char *buff, int len)
{
    int res = 0;
    struct i2c_msg msg[] = {
        {
        .addr = i2client->addr,
        .flags = 0,
        .len = len,
        .buf = buff,
        }
    };
    //printk( "i2c addr %#x reg:%#x  value:%#x \n",i2client->addr,buff[0],buff[1]);
    res = i2c_transfer(i2client->adapter, msg, 1);
    if (res < 0) {
        printk("%s: i2c transfer failed\n", __FUNCTION__);
    }
    
    return res;
}

static int customer_i2c_read(struct i2c_client *i2client,unsigned char *buff, int addr_len, int data_len)
{
    int res = 0;
    struct i2c_msg msgs[] = {
        {
            .addr = i2client->addr,
            .flags = 0,
            .len = addr_len,
            .buf = buff,
        },
        {
            .addr = i2client->addr,
            .flags = I2C_M_RD,
            .len = data_len,
            .buf = buff,
        }
    };
    res = i2c_transfer(i2client->adapter, msgs, 2);
	//printk( "i2c addr %#x reg:%#x  value:%#x \n",i2client->addr,buff[0],res);
    if (res < 0) {
        printk("%s: i2c transfer failed\n", __FUNCTION__);
    }

    return res;
}

static int i2c_get_byte_new(struct i2c_client *client,unsigned short addr)
{
	unsigned char buff[4];
	
	buff[0] = (unsigned char)(((addr)>>8) & 0xff);
	buff[1] = (unsigned char)((addr) & 0xff);
	if (customer_i2c_read(client, buff, 2, 1)<0)
		return -1;
	else
		return buff[0];
}

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static struct v4l2_fract hm5065_frmintervals_active = {
    .numerator = 1,
    .denominator = 15,
};	//huangbo


//extern int disable_hm5065;
static int hm5065_have_opened = 0;
static struct i2c_client *this_client;

static unsigned char focus_position_hi, focus_position_lo;

//static void do_download(struct work_struct *work);
//static DECLARE_DELAYED_WORK(dl_work, do_download);

/*typedef enum camera_focus_mode_e {
    CAM_FOCUS_MODE_RELEASE = 0,
    CAM_FOCUS_MODE_FIXED,
    CAM_FOCUS_MODE_INFINITY,
    CAM_FOCUS_MODE_AUTO,
    CAM_FOCUS_MODE_MACRO,
    CAM_FOCUS_MODE_EDOF,
    CAM_FOCUS_MODE_CONTI_VID,
    CAM_FOCUS_MODE_CONTI_PIC,
}camera_focus_mode_t;*/

static bool bDoingAutoFocusMode=false;
/* supported controls */
static struct v4l2_queryctrl hm5065_qctrl[] = {
    {
        .id            = V4L2_CID_BRIGHTNESS,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Brightness",
        .minimum       = 0,
        .maximum       = 255,
        .step          = 1,
        .default_value = 127,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    }, {
        .id            = V4L2_CID_CONTRAST,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Contrast",
        .minimum       = 0x10,
        .maximum       = 0x60,
        .step          = 0xa,
        .default_value = 0x30,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    },/* {
        .id            = V4L2_CID_SATURATION,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Saturation",
        .minimum       = 0x28,
        .maximum       = 0x60,
        .step          = 0x8,
        .default_value = 0x48,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    }, */{
        .id            = V4L2_CID_HFLIP,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "flip on horizontal",
        .minimum       = 0,
        .maximum       = 1,
        .step          = 0x1,
        .default_value = 0,
        .flags         = V4L2_CTRL_FLAG_DISABLED,
    } ,{
        .id            = V4L2_CID_VFLIP,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "flip on vertical",
        .minimum       = 0,
        .maximum       = 1,
        .step          = 0x1,
        .default_value = 0,
        .flags         = V4L2_CTRL_FLAG_DISABLED,
    },{
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
        .id            = V4L2_CID_FOCUS_AUTO,
        .type          = V4L2_CTRL_TYPE_MENU,
        .name          = "auto focus",
        .minimum       = CAM_FOCUS_MODE_RELEASE,
        .maximum       = CAM_FOCUS_MODE_CONTI_PIC,
        .step          = 0x1,
        .default_value = CAM_FOCUS_MODE_CONTI_PIC,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    },{
        .id            = V4L2_CID_BACKLIGHT_COMPENSATION,
        .type          = V4L2_CTRL_TYPE_MENU,
        .name          = "flash",
        .minimum       = FLASHLIGHT_ON,
        .maximum       = FLASHLIGHT_TORCH,
        .step          = 0x1,
        .default_value = FLASHLIGHT_OFF,
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
	}, {
	    .id		= V4L2_CID_FOCUS_ABSOLUTE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "focus center",
		.minimum	= 0,
		.maximum	= ((2000) << 16) | 2000,
        .step		= 1,
        .default_value	= (1000 << 16) | 1000,
        .flags         = V4L2_CTRL_FLAG_SLIDER, 	
     }
};


static struct v4l2_frmivalenum hm5065_frmivalenum[]={
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
		},{
			.index 		= 1,
			.pixel_format	= V4L2_PIX_FMT_NV21,
			.width		= 2048,
			.height		= 1536,
			.type		= V4L2_FRMIVAL_TYPE_DISCRETE,
			{
			    .discrete	={
			.numerator	= 1,
			.denominator	= 5,
			    }
			}
		},{
			.index 		= 1,
			.pixel_format	= V4L2_PIX_FMT_NV21,
			.width		= 2592,
			.height		= 1936,
			.type		= V4L2_FRMIVAL_TYPE_DISCRETE,
			{
			    .discrete	={
			.numerator	= 1,
			.denominator	= 5,
			    }
			}
		},
};


struct v4l2_querymenu hm5065_qmenu_autofocus[] = {
    {
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_INFINITY,
        .name       = "infinity",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_AUTO,
        .name       = "auto",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_CONTI_VID,
        .name       = "continuous-video",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_FOCUS_AUTO,
        .index      = CAM_FOCUS_MODE_CONTI_PIC,
        .name       = "continuous-picture",
        .reserved   = 0,
    }
};

struct v4l2_querymenu hm5065_qmenu_wbmode[] = {
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
    }
};

struct v4l2_querymenu hm5065_qmenu_anti_banding_mode[] = {
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
#if 1
typedef struct {
    __u32   id;
    int     num;
    struct v4l2_querymenu* hm5065_qmenu;
}hm5065_qmenu_set_t;

hm5065_qmenu_set_t hm5065_qmenu_set[] = {
    {
        .id         	= V4L2_CID_DO_WHITE_BALANCE,
        .num            = ARRAY_SIZE(hm5065_qmenu_wbmode),
        .hm5065_qmenu   = hm5065_qmenu_wbmode,
    },{
        .id         	= V4L2_CID_POWER_LINE_FREQUENCY,
        .num            = ARRAY_SIZE(hm5065_qmenu_anti_banding_mode),
        .hm5065_qmenu   = hm5065_qmenu_anti_banding_mode,
    },{
	 	.id             = V4L2_CID_FOCUS_AUTO,
	 	.num            = ARRAY_SIZE(hm5065_qmenu_autofocus),
	 	.hm5065_qmenu   = hm5065_qmenu_autofocus,
	}
};
#else
typedef struct {
    __u32   id;
    int     num;
    struct v4l2_querymenu* hm5065_qmenu;
}hm5065_qmenu_set_t;

hm5065_qmenu_set_t hm5065_qmenu_set[] = {
    {
        .id             = V4L2_CID_FOCUS_AUTO,
        .num            = ARRAY_SIZE(hm5065_qmenu_autofocus),
        .hm5065_qmenu   = hm5065_qmenu_autofocus,
    }, {
        .id             = V4L2_CID_BACKLIGHT_COMPENSATION,
        .num            = ARRAY_SIZE(hm5065_qmenu_flashmode),
        .hm5065_qmenu   = hm5065_qmenu_flashmode,
    }
};
#endif

#if 0
static int vidioc_querymenu(struct file *file, void *priv,
                struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(hm5065_qmenu_set); i++)
	if (a->id && a->id == hm5065_qmenu_set[i].id) {
	    for(j = 0; j < hm5065_qmenu_set[i].num; j++)
		if (a->index == hm5065_qmenu_set[i].hm5065_qmenu[j].index) {
			memcpy(a, &( hm5065_qmenu_set[i].hm5065_qmenu[j]),
				sizeof(*a));
			return (0);
		}
	}

	return -EINVAL;
}
#endif



struct v4l2_querymenu hm5065_qmenu_flashmode[] = {
    {
        .id         = V4L2_CID_BACKLIGHT_COMPENSATION,
        .index      = FLASHLIGHT_ON,
        .name       = "on",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_BACKLIGHT_COMPENSATION,
        .index      = FLASHLIGHT_OFF,
        .name       = "off",
        .reserved   = 0,
    },{
        .id         = V4L2_CID_BACKLIGHT_COMPENSATION,
        .index      = FLASHLIGHT_TORCH,
        .name       = "torch",
        .reserved   = 0,
    }
};

#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, debug, &dev->v4l2_dev, fmt, ## arg)

/* ------------------------------------------------------------------
	Basic structures
   ------------------------------------------------------------------*/

struct hm5065_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct hm5065_fmt formats[] = {
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

static struct hm5065_fmt *get_format(struct v4l2_format *f)
{
	struct hm5065_fmt *fmt;
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
struct hm5065_buffer {
    /* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct hm5065_fmt        *fmt;
};

struct hm5065_dmaqueue {
	struct list_head       active;

    /* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
    /* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

typedef enum resulution_size_type{
	SIZE_NULL = 0,
	SIZE_QVGA_320x240,
	SIZE_VGA_640X480,
	SIZE_UXGA_1600X1200,
	SIZE_QXGA_2048X1536,
	SIZE_QSXGA_2560X2048,
} resulution_size_type_t;

typedef struct resolution_param {
	struct v4l2_frmsize_discrete frmsize;
	struct v4l2_frmsize_discrete active_frmsize;
	int active_fps;
	resulution_size_type_t size_type;
	struct aml_camera_i2c_fig_s* reg_script;
} resolution_param_t;

static LIST_HEAD(hm5065_devicelist);

struct hm5065_device {
	struct list_head	    	hm5065_devicelist;
	struct v4l2_subdev	    	sd;
	struct v4l2_device	    	v4l2_dev;

	spinlock_t                 slock;
	struct mutex	        	mutex;

	int                        users;

    /* various device info */
	struct video_device        *vdev;

	struct hm5065_dmaqueue       vidq;

    /* Several counters */
	unsigned long              jiffies;

    /* Input Number */
	int	           input;

    /* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;
    
    /* Control 'registers' */
	int                qctl_regs[ARRAY_SIZE(hm5065_qctrl)];
	
	/* current resolution param for preview and capture */
	resolution_param_t* cur_resolution_param;
	
	/* wake lock */
	struct wake_lock	wake_lock;
	
	/* for down load firmware */
	//struct work_struct dl_work;
	
	int firmware_ready;
};

static inline struct hm5065_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct hm5065_device, sd);
}

struct hm5065_fh {
	struct hm5065_device            *dev;

    /* video capture */
	struct hm5065_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int	           input;     /* Input Number on bars */
	int  stream_on;
		unsigned int		f_flags;	//huangbo
};

static inline struct hm5065_fh *to_fh(struct hm5065_device *dev)
{
	return container_of(dev, struct hm5065_fh, dev);
}

struct aml_camera_i2c_fig_s HM5065_preview_script[] = {
    /*preview*/  
		{0x0083,0x00},   //	; HFlip disable
		{0x0084,0x00},   //	; VFlip	disable
		{0x0040,0x01},//	binning mode and subsampling mode for frame rate   
		{0x0041,0x04},//	04 : VGA mode : 0A : self define , 00 : 5M ,03:SVGA
		//{0x0085,0x03},  
		{0xffff, 0xff},
};

struct aml_camera_i2c_fig_s HM5065_preview_QVGA_script[] = {
    //{0x0085,0x03},   
    {0xffff, 0xff}
};

struct aml_camera_i2c_fig_s HM5065_capture_5M_script[] = {
		{0x0030,0x11},
		{0x0040,0x00},//Full size                    
		{0x0041,0x0A},//00:full size                 
		{0x0042,0x0A},//X:2048                       
		{0x0043,0x20},                               
		{0x0044,0x07},//Y:1536                       
		{0x0045,0x98},   	
		{0xffff, 0xff}
};

struct aml_camera_i2c_fig_s HM5065_capture_3M_script[] = {
 		{0x0030,0x11},
		{0x0040,0x00},
		{0x0041,0x0a},
		{0x0042,0x08},
		{0x0043,0x00},
		{0x0044,0x06},
		{0x0045,0x00},
		{0x0010,0x01},
	  	{0xffff, 0xff}
};

struct aml_camera_i2c_fig_s HM5065_capture_2M_script[] = {
    	{0x0030,0x11},
		{0x0040,0x00},
		{0x0041,0x01},
	  	{0xffff, 0xff}
};

static int exposure_500m_setting(struct hm5065_device *dev);

static resolution_param_t  prev_resolution_array[] = {
	{
		.frmsize			= {640, 480},
		.active_frmsize		= {640, 478},
		.active_fps			= 15,
		.size_type			= SIZE_VGA_640X480,
		.reg_script			= HM5065_preview_script,
	}, {
		.frmsize			= {320, 240},
		.active_frmsize		= {320, 240},
		.active_fps			= 15,
		.size_type			= SIZE_QVGA_320x240,
		.reg_script			= HM5065_preview_QVGA_script,
	},
};

static resolution_param_t  capture_resolution_array[] = {
	{
		.frmsize			= {2592, 1944},
		.active_frmsize		= {2592, 1942},
		.active_fps			= 5,
		.size_type			= SIZE_QSXGA_2560X2048,
		.reg_script			= HM5065_capture_5M_script,
	}, 
	{
		.frmsize			= {1600, 1200},
		.active_frmsize		= {1600, 1198},
		.active_fps			= 5,
		.size_type			= SIZE_UXGA_1600X1200,
		.reg_script			= HM5065_capture_2M_script,
		//.reg_script		= HM5065_preview_script,
	},{
		.frmsize			= {2048, 1536},
		.active_frmsize		= {2032, 1534},
		.active_fps			= 5,
		.size_type			= SIZE_QXGA_2048X1536,
		.reg_script			= HM5065_capture_3M_script,
	},
};

void HM5065_init_regs(struct hm5065_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    int i=0;
#if 0
    while(1)
    {
        if (HM5065_script_step1[i].val==0xff&&HM5065_script_step1[i].addr==0xffff)
        {
        	printk("success in initial HM5065 step1.\n");
        	break;
        }
        if((i2c_put_byte(client,HM5065_script_step1[i].addr, HM5065_script_step1[i].val)) < 0)
        {
        	printk("fail in initial HM5065. \n");
		return;
		}
		i++;
    }
	msleep(200);
	while(1)
    {
        if (HM5065_script_step2[i].val==0xff&&HM5065_script_step2[i].addr==0xffff)
        {
        	printk("success in initial HM5065 step2.\n");
        	break;
        }
        if((i2c_put_byte(client,HM5065_script_step2[i].addr, HM5065_script_step2[i].val)) < 0)
        {
        	printk("fail in initial HM5065. \n");
		return;
		}
		i++;
    }	
	msleep(200); 
	while(1)
    {
        if (HM5065_script_step3[i].val==0xff&&HM5065_script_step3[i].addr==0xffff)
        {
        	printk("success in initial HM5065 step3.\n");
        	break;
        }
        if((i2c_put_byte(client,HM5065_script_step3[i].addr, HM5065_script_step3[i].val)) < 0)
        {
        	printk("fail in initial HM5065. \n");
		return;
		}
		i++;
    }
	msleep(200);  
#else
	while(1)
    {
        if (HM5065_script_step[i].val==0xff&&HM5065_script_step[i].addr==0xffff)
        {
        	printk("success in initial HM5065 step.\n");
        	break;
        }
		if (HM5065_script_step[i].addr==0x0010)
        {
        	msleep(200);
        }
	    if (HM5065_script_step[i].addr==0xffff&&HM5065_script_step[i].val==0x00)
        {
        	msleep(200);
        }
        if((i2c_put_byte(client,HM5065_script_step[i].addr, HM5065_script_step[i].val)) < 0)
        {
        	printk("fail in initial HM5065. \n");
		return;
		}
		
		i++;
    }	
#endif	
    aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
    if (plat_dat&&plat_dat->custom_init_script) {
		i=0;
		aml_camera_i2c_fig_t*  custom_script = (aml_camera_i2c_fig_t*)plat_dat->custom_init_script;
		while(1)
		{
			if (custom_script[i].val==0xff&&custom_script[i].addr==0xffff)
			{
				printk("HM5065_write_custom_regs success in initial HM5065.\n");
				break;
			}
			if((i2c_put_byte(client,custom_script[i].addr, custom_script[i].val)) < 0)
			{
				printk("fail in initial HM5065 custom_regs. \n");
				return;
			}
			i++;
		}
    }
    return;
}

/*************************************************************************
* FUNCTION
*    HM5065_set_param_brightness
*
* DESCRIPTION
*    brightness setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HM5065_set_param_brightness(struct hm5065_device *dev,enum camera_brightness_e para)//亮度调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para) {
    	case BRIGHTNESS_N4_STEP:  //负4档  
            i2c_put_byte(client, 0x0082, 0x24); 
        	break;
            
    	case BRIGHTNESS_N3_STEP:
            i2c_put_byte(client, 0x0082, 0x34); 
        	break;
            
    	case BRIGHTNESS_N2_STEP:
            i2c_put_byte(client, 0x0082, 0x44); 
        	break;
            
    	case BRIGHTNESS_N1_STEP:
            i2c_put_byte(client, 0x0082, 0x54); 
        	break;
            
    	case BRIGHTNESS_0_STEP://默认零档
            i2c_put_byte(client, 0x0082, 0x64); 
        	break;
            
    	case BRIGHTNESS_P1_STEP://正一档
            i2c_put_byte(client, 0x0082, 0x74); 
        	break;
            
    	case BRIGHTNESS_P2_STEP:
            i2c_put_byte(client, 0x0082, 0x84); 
        	break;
            
    	case BRIGHTNESS_P3_STEP:
            i2c_put_byte(client, 0x0082, 0x94); 
        	break;
            
    	case BRIGHTNESS_P4_STEP:    
            i2c_put_byte(client, 0x0082, 0xa4); 
        	break;
            
    	default:
            i2c_put_byte(client, 0x0082, 0x64); 
        	break;
    }
} /* HM5065_set_param_brightness */

/*************************************************************************
* FUNCTION
*    HM5065_set_param_contrast
*
* DESCRIPTION
*    contrast setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HM5065_set_param_contrast(struct hm5065_device *dev,enum camera_contrast_e para)//对比度调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para) {
    	case CONTRAST_N4_STEP:  //负4档  
            i2c_put_byte(client, 0x0080, 0x24);       	
        	break;
            
    	case CONTRAST_N3_STEP:
            i2c_put_byte(client, 0x0080, 0x34); 
        	break;
            
    	case CONTRAST_N2_STEP:
            i2c_put_byte(client, 0x0080, 0x44); 
        	break;
            
    	case CONTRAST_N1_STEP:
            i2c_put_byte(client, 0x0080, 0x54); 
        	break;
            
    	case CONTRAST_0_STEP://默认零档
            i2c_put_byte(client, 0x0080, 0x64); 
        	break;
            
    	case CONTRAST_P1_STEP://正一档
            i2c_put_byte(client, 0x0080, 0x74); 
        	break;
            
    	case CONTRAST_P2_STEP:
            i2c_put_byte(client, 0x0080, 0x84); 
        	break;
            
    	case CONTRAST_P3_STEP:
            i2c_put_byte(client, 0x0080, 0x94); 
        	break;
            
    	case CONTRAST_P4_STEP:    
            i2c_put_byte(client, 0x0080, 0xa4); 
        	break;
            
    	default:
            i2c_put_byte(client, 0x0080, 0x64); 
        	break;
    }
} /* HM5065_set_param_contrast */

/*************************************************************************
* FUNCTION
*    HM5065_set_param_saturation
*
* DESCRIPTION
*    saturation setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HM5065_set_param_saturation(struct hm5065_device *dev,enum camera_saturation_e para)//饱和度调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para) {
    	case SATURATION_N4_STEP:  //负4档  
            i2c_put_byte(client, 0x0081, 0x24); 
        	break;
            
    	case SATURATION_N3_STEP:
            i2c_put_byte(client, 0x0081, 0x34);
        	break;
            
    	case SATURATION_N2_STEP:
            i2c_put_byte(client, 0x0081, 0x44);
        	break;
            
    	case SATURATION_N1_STEP:
            i2c_put_byte(client, 0x0081, 0x54);
        	break;
            
    	case SATURATION_0_STEP://默认零档
            i2c_put_byte(client, 0x0081, 0x64);
        	break;
            
    	case SATURATION_P1_STEP://正一档
            i2c_put_byte(client, 0x0081, 0x74);
        	break;
            
    	case SATURATION_P2_STEP:
            i2c_put_byte(client, 0x0081, 0x84);
        	break;
            
    	case SATURATION_P3_STEP:
            i2c_put_byte(client, 0x0081, 0x94);
        	break;
            
    	case SATURATION_P4_STEP:    
            i2c_put_byte(client, 0x0081, 0xa4);
        	break;
            
    	default:
            i2c_put_byte(client, 0x0081, 0x64);
        	break;
    }
} /* HM5065_set_param_saturation */

/*************************************************************************
* FUNCTION
*    HM5065_set_param_wb
*
* DESCRIPTION
*    wb setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HM5065_set_param_wb(struct hm5065_device *dev,enum  camera_wb_flip_e para)//白平衡
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para) {      
    	case CAM_WB_AUTO://自动
    	   // i2c_put_byte(client, 0x0085, 0x02);   
        	i2c_put_byte(client, 0x01a0, 0x01);            
        	break;

    	case CAM_WB_CLOUD: //阴天
        	i2c_put_byte(client, 0x01a0, 0x03);
        	i2c_put_byte(client, 0x01a1, 0x62);
        	i2c_put_byte(client, 0x01a2, 0x08);
        	i2c_put_byte(client, 0x01a3, 0x00);        	
        	break;

    	case CAM_WB_DAYLIGHT: //
    	    //i2c_put_byte(client, 0x0085, 0x03); 
            i2c_put_byte(client, 0x01a0, 0x03);
        	i2c_put_byte(client, 0x01a1, 0x7F);
        	i2c_put_byte(client, 0x01a2, 0x3F);
        	i2c_put_byte(client, 0x01a3, 0x01);
        	break;

    	case CAM_WB_INCANDESCENCE: 
			i2c_put_byte(client, 0x01a0, 0x03);
        	i2c_put_byte(client, 0x01a1, 0x39);
        	i2c_put_byte(client, 0x01a2, 0x00);
        	i2c_put_byte(client, 0x01a3, 0x59);
        	break;
            
    	case CAM_WB_TUNGSTEN: 
            i2c_put_byte(client, 0x01a0, 0x03);
        	i2c_put_byte(client, 0x01a1, 0x05);
        	i2c_put_byte(client, 0x01a2, 0x00);
        	i2c_put_byte(client, 0x01a3, 0x7f);
        	break;

      	case CAM_WB_FLUORESCENT:
			i2c_put_byte(client, 0x01a0, 0x03);
        	i2c_put_byte(client, 0x01a1, 0x1F);
        	i2c_put_byte(client, 0x01a2, 0x00);
        	i2c_put_byte(client, 0x01a3, 0x4D);
        	break;

    	case CAM_WB_MANUAL:
                // TODO
        	break;
    }
    

} /* HM5065_set_param_wb */
/*************************************************************************
* FUNCTION
*    HM5065_set_param_exposure
*
* DESCRIPTION
*    exposure setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HM5065_set_param_exposure(struct hm5065_device *dev,enum camera_exposure_e para)//曝光调节
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

    switch (para) {
    	case EXPOSURE_N4_STEP:  //负4档  
            i2c_put_byte(client, 0x0130, 0xfc);       	
        	break;
            
    	case EXPOSURE_N3_STEP:
            i2c_put_byte(client, 0x0130, 0xfd); 
        	break;
            
    	case EXPOSURE_N2_STEP:
            i2c_put_byte(client, 0x0130, 0xfe); 
        	break;
            
    	case EXPOSURE_N1_STEP:
            i2c_put_byte(client, 0x0130, 0xff); 
        	break;
            
    	case EXPOSURE_0_STEP://默认零档
            i2c_put_byte(client, 0x0130, 0x00); 
        	break;
            
    	case EXPOSURE_P1_STEP://正一档
            i2c_put_byte(client, 0x0130, 0x01); 
        	break;
            
    	case EXPOSURE_P2_STEP:
            i2c_put_byte(client, 0x0130, 0x02); 
        	break;
            
    	case EXPOSURE_P3_STEP:
            i2c_put_byte(client, 0x0130, 0x03); 
        	break;
            
    	case EXPOSURE_P4_STEP:    
            i2c_put_byte(client, 0x0130, 0x04); 
        	break;
            
    	default:
            i2c_put_byte(client, 0x0130, 0x00); 
        	break;
    }
} /* HM5065_set_param_exposure */
/*************************************************************************
* FUNCTION
*    HM5065_set_param_effect
*
* DESCRIPTION
*    effect setting.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HM5065_set_param_effect(struct hm5065_device *dev,enum camera_effect_flip_e para)//特效设置
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
  
    switch (para) {
    	case CAM_EFFECT_ENC_NORMAL://正常
      i2c_put_byte(client, 0x0380, 0x00);
			i2c_put_byte(client, 0x0381, 0x00);
			i2c_put_byte(client, 0x0382, 0x00);
			i2c_put_byte(client, 0x0384, 0x00);
			i2c_put_byte(client, 0x01a0, 0x01);
			i2c_put_byte(client, 0x01a1, 0x80);
			i2c_put_byte(client, 0x01a2, 0x80);
			i2c_put_byte(client, 0x01a3, 0x80);
			i2c_put_byte(client, 0x01a5, 0x3e);
			i2c_put_byte(client, 0x01a6, 0x00);
			i2c_put_byte(client, 0x01a7, 0x3e);
			i2c_put_byte(client, 0x01a8, 0x00);
        	break;        

    	case CAM_EFFECT_ENC_GRAYSCALE://灰阶
      i2c_put_byte(client, 0x0380, 0x00);
			i2c_put_byte(client, 0x0381, 0x00);
			i2c_put_byte(client, 0x0382, 0x00);
			i2c_put_byte(client, 0x0384, 0x05);
			i2c_put_byte(client, 0x01a0, 0x01);
			i2c_put_byte(client, 0x01a1, 0x80);
			i2c_put_byte(client, 0x01a2, 0x80);
			i2c_put_byte(client, 0x01a3, 0x80);
			i2c_put_byte(client, 0x01a5, 0x3e);
			i2c_put_byte(client, 0x01a6, 0x00);
			i2c_put_byte(client, 0x01a7, 0x3e);
			i2c_put_byte(client, 0x01a8, 0x00);
        	break;

    	case CAM_EFFECT_ENC_SEPIA://复古
      i2c_put_byte(client, 0x0380, 0x00);
			i2c_put_byte(client, 0x0381, 0x00);
			i2c_put_byte(client, 0x0382, 0x00);
			i2c_put_byte(client, 0x0384, 0x06);
			i2c_put_byte(client, 0x01a0, 0x01);
			i2c_put_byte(client, 0x01a1, 0x80);
			i2c_put_byte(client, 0x01a2, 0x80);
			i2c_put_byte(client, 0x01a3, 0x80);
			i2c_put_byte(client, 0x01a5, 0x3e);
			i2c_put_byte(client, 0x01a6, 0x00);
			i2c_put_byte(client, 0x01a7, 0x3e);
			i2c_put_byte(client, 0x01a8, 0x00);
        	break;        
                
    	case CAM_EFFECT_ENC_SEPIAGREEN://复古绿
      i2c_put_byte(client, 0x0380, 0x00);
			i2c_put_byte(client, 0x0381, 0x00);
			i2c_put_byte(client, 0x0382, 0x00);
			i2c_put_byte(client, 0x0384, 0x07);
			i2c_put_byte(client, 0x01a0, 0x01);
			i2c_put_byte(client, 0x01a1, 0x80);
			i2c_put_byte(client, 0x01a2, 0x80);
			i2c_put_byte(client, 0x01a3, 0x80);
			i2c_put_byte(client, 0x01a5, 0x3e);
			i2c_put_byte(client, 0x01a6, 0x00);
			i2c_put_byte(client, 0x01a7, 0x3e);
			i2c_put_byte(client, 0x01a8, 0x00);
        	break;                    

    	case CAM_EFFECT_ENC_SEPIABLUE://复古蓝
      i2c_put_byte(client, 0x0380, 0x00);
			i2c_put_byte(client, 0x0381, 0x00);
			i2c_put_byte(client, 0x0382, 0x00);
			i2c_put_byte(client, 0x0384, 0x08);
			i2c_put_byte(client, 0x01a0, 0x01);
			i2c_put_byte(client, 0x01a1, 0x80);
			i2c_put_byte(client, 0x01a2, 0x80);
			i2c_put_byte(client, 0x01a3, 0x80);
			i2c_put_byte(client, 0x01a5, 0x3e);
			i2c_put_byte(client, 0x01a6, 0x00);
			i2c_put_byte(client, 0x01a7, 0x3e);
			i2c_put_byte(client, 0x01a8, 0x00);
        	break;                                

    	case CAM_EFFECT_ENC_COLORINV://底片
      i2c_put_byte(client, 0x0380, 0x01);
			i2c_put_byte(client, 0x0381, 0x00);
			i2c_put_byte(client, 0x0382, 0x00);
			i2c_put_byte(client, 0x0384, 0x00);
			i2c_put_byte(client, 0x01a0, 0x01);
			i2c_put_byte(client, 0x01a1, 0x80);
			i2c_put_byte(client, 0x01a2, 0x80);
			i2c_put_byte(client, 0x01a3, 0x80);
			i2c_put_byte(client, 0x01a5, 0x3e);
			i2c_put_byte(client, 0x01a6, 0x00);
			i2c_put_byte(client, 0x01a7, 0x3e);
			i2c_put_byte(client, 0x01a8, 0x00);
        	break;        

    	default:
        	break;
    }
} /* HM5065_set_param_effect */

/*************************************************************************
* FUNCTION
*	HM5065_night_mode
*
* DESCRIPTION
*    This function night mode of HM5065.
*
* PARAMETERS
*    none
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static camera_focus_mode_t start_focus_mode = CAM_FOCUS_MODE_RELEASE;
static int HM5065_AutoFocus(struct hm5065_device *dev, int focus_mode);

static int set_focus_zone(struct hm5065_device *dev, int value)
{	  
		int xc, yc;
		struct i2c_client *client =v4l2_get_subdevdata(&dev->sd);
		int retry_count = 9;
		int reg_value = 0;
		int ret = -1;
		printk("xc = %d, yc = %d\n", xc, yc); 	
		xc = ((value >> 16) & 0xffff) * 80 / 2000;
		yc = (value & 0xffff) * 60 / 2000;
		printk("xc1 = %d, yc1 = %d\n", xc, yc); 	
	    if(-1==ret)
	   {
		i2c_put_byte(client,0x0808,0x01);
		i2c_put_byte(client,0x0809,0x00); 
		i2c_put_byte(client,0x080a,0x00);
		i2c_put_byte(client,0x080b,0x00);  
		i2c_put_byte(client,0x080c,0x00);
		i2c_put_byte(client,0x080d,0x00);
		i2c_put_byte(client,0x080e,0x00); 
		#if 1
	  i2c_put_byte(client,FACE_LC,0x01);//enable	
		i2c_put_byte(client,FACE_START_XH, xc>>8);
		i2c_put_byte(client,FACE_START_XL, xc&0xFF);		
		i2c_put_byte(client,FACE_START_YH, yc>>8);
		i2c_put_byte(client,FACE_START_YL, yc&0xFF);
		i2c_put_byte(client,FACE_SIZE_XH, 0x00);
		i2c_put_byte(client,FACE_SIZE_XL, 80);
		i2c_put_byte(client,FACE_SIZE_YH, 0x00);
		i2c_put_byte(client,FACE_SIZE_YL, 60);
		printk("SENSOR: _hm5065_Foucs_stareX: %d, %d\n",(xc>>8),(xc&0xFF)); 
        printk("SENSOR: _hm5065_Foucs_stareY: %d, %d\n",(yc>>8),(yc&0xFF)); 
		#endif
		i2c_put_byte(client, 0x070a , 0x03);			
		//msleep(100);
		i2c_put_byte(client, 0x070b , 0x01);			
		msleep(200);
		i2c_put_byte(client, 0x070b , 0x02);
		do
		{
			if(0x00==retry_count)
			{
				printk("SENSOR: _hm5065_AutoFocusZone error!\n"); 
				ret=-1;
				i2c_put_byte(client,0x0700, 0x01);
	   		   i2c_put_byte(client,0x0701, 0xFD);
				break ;
			}
			msleep(1);        
			reg_value=i2c_get_byte(client,AF_STATUS);
			retry_count--;
		}while(0x01!=reg_value);
		ret=0;
		focus_position_hi = i2c_get_byte(client,0x06F0);
		focus_position_lo = i2c_get_byte(client,0x06F1);
		i2c_put_byte(client,0x0700, focus_position_hi&0xFF);// target position H
		i2c_put_byte(client,0x0701, focus_position_lo&0xFF);// target position L
		printk("SENSOR: _hm5065_AF status %d\n",i2c_get_byte(client,AF_STATUS)); 
	}
	return ret;			
	}

static void HM5065_set_param_banding(struct hm5065_device *dev,enum  camera_night_mode_flip_e banding)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char buf[4];
    switch(banding){
        case CAM_BANDING_60HZ:
						printk("set banding 60Hz\n");
						i2c_put_byte(client, 0x0190, 0x00);
						i2c_put_byte(client, 0x019c, 0x4b);
						i2c_put_byte(client, 0x019d, 0xc0);
            break;
        case CAM_BANDING_50HZ:
						printk("set banding 50Hz\n");
						i2c_put_byte(client, 0x0190, 0x00);
						i2c_put_byte(client, 0x019c, 0x4b);
						i2c_put_byte(client, 0x019d, 0x20);
            break;
    }
    }

static int HM5065_AutoFocus(struct hm5065_device *dev, int focus_mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int ret = 0;
    int i = 0;
    
	switch (focus_mode) {
					case CAM_FOCUS_MODE_AUTO:
					i2c_put_byte(client, 0x070a , 0x03);			
					//msleep(100);
					i2c_put_byte(client, 0x070b , 0x01);			
					msleep(200);
					i2c_put_byte(client, 0x070b , 0x02);
					bDoingAutoFocusMode = true;
					printk("single auto focus mode start\n");
					break;
					case CAM_FOCUS_MODE_CONTI_VID:
					case CAM_FOCUS_MODE_CONTI_PIC:
					i2c_put_byte(client, 0x070a , 0x01); //start to continous focus            
					printk("start continous focus\n");
					break;
					case CAM_FOCUS_MODE_RELEASE:
					case CAM_FOCUS_MODE_FIXED:
					default:
					//i2c_put_byte(client, 0x070a , 0x00);
					//i2c_put_byte(client, 0x070c , 0x00);
					//i2c_put_byte(client, 0x070c , 0x03);			
					printk("release focus to infinit\n");
					break;
    }
    return ret;

}    /* HM5065_AutoFocus */

static void get_focus_position(struct hm5065_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	
	focus_position_hi = i2c_get_byte(client, 0x06f0);
	focus_position_lo = i2c_get_byte(client, 0x06f1);
	printk("focus_position_1=%x, focus_position_2=%x\n", focus_position_hi, focus_position_lo);	
}

static void set_focus_position(struct hm5065_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);	
	i2c_put_byte(client, 0x070a , 0x00);	//manual focus
	i2c_put_byte(client, 0x0700 , focus_position_hi);
	i2c_put_byte(client, 0x0701 , focus_position_lo);
	i2c_put_byte(client, 0x070c , 0x00);
	msleep(100);
	i2c_put_byte(client, 0x070c , 0x07);	//focus goto recovery	
}

static int HM5065_FlashCtrl(struct hm5065_device *dev, int flash_mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int ret = 0;
    int i = 0;
    
	switch (flash_mode) {
        case FLASHLIGHT_ON:
    	case FLASHLIGHT_AUTO:
    	case FLASHLIGHT_TORCH:
            if (dev->platform_dev_data.flash_ctrl)
            	if(dev->platform_dev_data.flash_ctrl->flashlight_on)
            		dev->platform_dev_data.flash_ctrl->flashlight_on();
            printk("flash on\n");
            break;
        case FLASHLIGHT_OFF:
     		if (dev->platform_dev_data.flash_ctrl)
            	if(dev->platform_dev_data.flash_ctrl->flashlight_off)
            		dev->platform_dev_data.flash_ctrl->flashlight_off();
            printk("flash off\n");
			break;
        default:
            printk("this flash mode not support yet\n");
            break;
    }
    return ret;

}    /* HM5065_FlashCtrl */

static resulution_size_type_t get_size_type(int width, int height)
{
	resulution_size_type_t rv = SIZE_NULL; 
	if (width * height >= 2500 * 1900)
		rv = SIZE_QSXGA_2560X2048;
	else if (width * height >= 2000 * 1500)
		rv = SIZE_QXGA_2048X1536;
	else if (width * height >= 1600 * 1200)
		rv = SIZE_UXGA_1600X1200;
	else if (width * height >= 600 * 400)
		rv = SIZE_VGA_640X480;
	else if (width * height >= 300 * 200)
		rv = SIZE_QVGA_320x240;
	return rv;
}

static resolution_param_t* get_resolution_param(struct hm5065_device *dev, int is_capture, int width, int height)
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
		        if(is_capture)
			{
				printk("is_capture============%d\n",res_type);
			}
			else
			{
				printk("!!!!!!!!!!!!!is_capture============%d\n",res_type);
			}
			
				
			if (tmp_resolution_param[i].size_type == res_type)
			{
				 if(is_capture)
				{
					printk("iiiiiiiiiiiiiiiiiiiiiiiii===%d\n",i);
				}
				return &tmp_resolution_param[i];
			}
	}
	return NULL;
}

static int set_resolution_param(struct hm5065_device *dev, resolution_param_t* res_param)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int rc = -1;
	if (!res_param->reg_script) {
		printk("error, resolution reg script is NULL\n");
		return -1;
	}
    int i=0;
    while(1) {
        if (res_param->reg_script[i].val==0xff&&res_param->reg_script[i].addr==0xffff) {
            printk("setting resolutin param complete\n");
            break;
        }
        if((i2c_put_byte(client,res_param->reg_script[i].addr, res_param->reg_script[i].val)) < 0) {
            printk("fail in setting resolution param. i=%d\n",i);
        	break;
        }
		if (res_param->reg_script[i].addr==0x0010) {
            msleep(200);            
        }
        i++;
    }
	dev->cur_resolution_param = res_param;
}

static int hm5065_setting(struct hm5065_device *dev,int PROP_ID,int value ) 
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
	case V4L2_CID_BRIGHTNESS:
    	if(hm5065_qctrl[0].default_value!=value){
        	hm5065_qctrl[0].default_value=value;
        	HM5065_set_param_brightness(dev,value);
        	printk(KERN_INFO " set camera  brightness=%d. \n ",value);
        }
    	break;
	case V4L2_CID_CONTRAST:
    	if(hm5065_qctrl[1].default_value!=value){
        	hm5065_qctrl[1].default_value=value;
        	HM5065_set_param_contrast(dev,value);
        	printk(KERN_INFO " set camera  brightness=%d. \n ",value);
        }
    	break; 
#if 0		
	case V4L2_CID_SATURATION:
    	ret=i2c_put_byte(client,0x0080, v4l_2_hm5065(value));
    	break;
	
	case V4L2_CID_EXPOSURE:
    	ret=i2c_put_byte(client,0x0201, value);
    	break;    

	case V4L2_CID_HFLIP:    /* set flip on H. */
    	ret=i2c_put_byte(client,0x0083, value);    	
        if(ret<0) 
			dprintk(dev, 1, "V4L2_CID_HFLIP setting error\n");        
        
    	break;
	case V4L2_CID_VFLIP:    /* set flip on V. */
    	ret=i2c_put_byte(client,0x0084, value);
		if(ret<0) 
			dprintk(dev, 1, "V4L2_CID_VFLIP setting error\n");        
    	
    	break;    
#endif
	case V4L2_CID_DO_WHITE_BALANCE:
        if(hm5065_qctrl[4].default_value!=value){
        	hm5065_qctrl[4].default_value=value;
        	HM5065_set_param_wb(dev,value);
        	printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
        }
    	break;
	case V4L2_CID_EXPOSURE:
        if(hm5065_qctrl[5].default_value!=value){
        	hm5065_qctrl[5].default_value=value;
        	HM5065_set_param_exposure(dev,value);
        	printk(KERN_INFO " set camera  exposure=%d. \n ",value);
        }
    	break;
	case V4L2_CID_COLORFX:
        if(hm5065_qctrl[6].default_value!=value){
        	hm5065_qctrl[6].default_value=value;
        	HM5065_set_param_effect(dev,value);
        	printk(KERN_INFO " set camera  effect=%d. \n ",value);
        }
    	break;
    case V4L2_CID_WHITENESS:
		if(hm5065_qctrl[7].default_value!=value){
			hm5065_qctrl[7].default_value=value;
			HM5065_set_param_banding(dev,value);
			printk(KERN_INFO " set camera  banding=%d. \n ",value);
		}
		break;
    case V4L2_CID_BACKLIGHT_COMPENSATION:
    	if (dev->platform_dev_data.flash_support) 
        	ret = HM5065_FlashCtrl(dev,value);
        else
        	ret = -1;
        break;
   	case V4L2_CID_ZOOM_ABSOLUTE:
		if(hm5065_qctrl[10].default_value!=value){
			hm5065_qctrl[10].default_value=value;
			//printk(KERN_INFO " set camera  zoom mode=%d. \n ",value);
		}
		break;
   	case V4L2_CID_FOCUS_ABSOLUTE:
		printk("V4L2_CID_FOCUS_ABSOLUTE\n");
		if(hm5065_qctrl[11].default_value!=value){
			hm5065_qctrl[11].default_value=value;
			printk(" set camera  focus zone =%d. \n ",value);
			set_focus_zone(dev, value);	
		  }		
		break;
   case V4L2_CID_FOCUS_AUTO:
		mutex_lock(&firmware_mutex);
		if (dev->firmware_ready) 
			ret = HM5065_AutoFocus(dev,value);
		else if (value == CAM_FOCUS_MODE_CONTI_VID ||
        			value == CAM_FOCUS_MODE_CONTI_PIC)
			start_focus_mode = value;
		else
			ret = -1;
		mutex_unlock(&firmware_mutex);
		break;
	default:
    	ret=-1;
    	break;
    }
	return ret;    
}

static void power_down_hm5065(struct hm5065_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	i2c_put_byte(client,0x070a, 0x00); //release focus
	i2c_put_byte(client,0x0010, 0x02);//in soft power down mode
}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void hm5065_fillbuff(struct hm5065_fh *fh, struct hm5065_buffer *buf)
{   
    
	struct hm5065_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	vm_output_para_t para = {0};
	dprintk(dev,1,"%s\n", __func__);    
	if (!vbuf)
    	return;
 /*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
	para.mirror = -1;// not set
	para.v4l2_format = fh->fmt->fourcc;
	para.v4l2_memory = 0x18221223;
	para.zoom = hm5065_qctrl[10].default_value;
	//para.angle = ov5640_qctrl[11].default_value;
	para.vaddr = (unsigned)vbuf;
	vm_fill_buffer(&buf->vb,&para);
	buf->vb.state = VIDEOBUF_DONE;
 
	
}

static void hm5065_thread_tick(struct hm5065_fh *fh)
{
	struct hm5065_buffer *buf;
	struct hm5065_device *dev = fh->dev;
	struct hm5065_dmaqueue *dma_q = &dev->vidq;

	unsigned long flags = 0;

	dprintk(dev, 1, "Thread tick\n");
	
	if(!fh->stream_on){			//huangbo
		dprintk(dev, 1, "sensor doesn't stream on\n");
		return ;
	}

	spin_lock_irqsave(&dev->slock, flags);
	if (list_empty(&dma_q->active)) {
    	dprintk(dev, 1, "No active queue to serve\n");
    	goto unlock;
    }

	buf = list_entry(dma_q->active.next,
             struct hm5065_buffer, vb.queue);
    dprintk(dev, 1, "%s\n", __func__);
    dprintk(dev, 1, "list entry get buf is %x\n",buf);

 if(!(fh->f_flags & O_NONBLOCK)){	//huangbo
    /* Nobody is waiting on this buffer, return */
	if (!waitqueue_active(&buf->vb.done))
    	goto unlock;
  }
    buf->vb.state = VIDEOBUF_ACTIVE;
    
	list_del(&buf->vb.queue);

	do_gettimeofday(&buf->vb.ts);

    /* Fill buffer */
	spin_unlock_irqrestore(&dev->slock, flags);
	hm5065_fillbuff(fh, buf);
	dprintk(dev, 1, "filled buffer %p\n", buf);

	wake_up(&buf->vb.done);
	dprintk(dev, 2, "[%p/%d] wakeup\n", buf, buf->vb. i);
	return;
unlock:
	spin_unlock_irqrestore(&dev->slock, flags);
	return;
}

#define frames_to_ms(frames)                    \
    ((frames * WAKE_NUMERATOR * 1000) / WAKE_DENOMINATOR)

static void hm5065_sleep(struct hm5065_fh *fh)
{
	struct hm5065_device *dev = fh->dev;
	struct hm5065_dmaqueue *dma_q = &dev->vidq;

	int timeout;
	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
        (unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
    	goto stop_task;

    /* Calculate time to wake up */
	timeout = msecs_to_jiffies(frames_to_ms(1));

	hm5065_thread_tick(fh);

	schedule_timeout_interruptible(timeout);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int hm5065_thread(void *data)
{
	struct hm5065_fh  *fh = data;
	struct hm5065_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
    	hm5065_sleep(fh);

    	if (kthread_should_stop())
        	break;
    }
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int hm5065_start_thread(struct hm5065_fh *fh)
{
	struct hm5065_device *dev = fh->dev;
	struct hm5065_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(hm5065_thread, fh, "hm5065");

	if (IS_ERR(dma_q->kthread)) {
    	v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
    	return PTR_ERR(dma_q->kthread);
    }
    /* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void hm5065_stop_thread(struct hm5065_dmaqueue  *dma_q)
{
	struct hm5065_device *dev = container_of(dma_q, struct hm5065_device, vidq);

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
	struct hm5065_fh  *fh = vq->priv_data;
	struct hm5065_device *dev  = fh->dev;
    //int bytes = fh->fmt->depth >> 3 ;
    *size = (fh->width*fh->height*fh->fmt->depth)>>3;    
	if (0 == *count)
        *count = 32;

	while (*size * *count > vid_limit * 1024 * 1024)
        (*count)--;

	dprintk(dev, 1, "%s, count=%d, size=%d\n", __func__,
        *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq, struct hm5065_buffer *buf)
{
	struct hm5065_fh  *fh = vq->priv_data;
	struct hm5065_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);
	videobuf_waiton(vq, &buf->vb, 0, 0);	//huangbo
	if (in_interrupt())
    	BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 3000
#define norm_maxh() 3000
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
                    	enum v4l2_field field)
{
	struct hm5065_fh     *fh  = vq->priv_data;
	struct hm5065_device    *dev = fh->dev;
	struct hm5065_buffer *buf = container_of(vb, struct hm5065_buffer, vb);
	int rc;
    //int bytes = fh->fmt->depth >> 3 ;
	dprintk(dev, 1, "%s, field=%d\n", __func__, field);

	BUG_ON(NULL == fh->fmt);

	if (fh->width  < 48 || fh->width  > norm_maxw() ||
        fh->height < 32 || fh->height > norm_maxh())
    	return -EINVAL;

	buf->vb.size = (fh->width*fh->height*fh->fmt->depth)>>3;
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
	struct hm5065_buffer    *buf  = container_of(vb, struct hm5065_buffer, vb);
	struct hm5065_fh        *fh   = vq->priv_data;
	struct hm5065_device       *dev  = fh->dev;
	struct hm5065_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
               struct videobuf_buffer *vb)
{
	struct hm5065_buffer   *buf  = container_of(vb, struct hm5065_buffer, vb);
	struct hm5065_fh       *fh   = vq->priv_data;
	struct hm5065_device      *dev  = (struct hm5065_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops hm5065_video_qops = {
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
	struct hm5065_fh  *fh  = priv;
	struct hm5065_device *dev = fh->dev;

	strcpy(cap->driver, "hm5065");
	strcpy(cap->card, "hm5065");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = HM5065_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
            	V4L2_CAP_STREAMING     |
            	V4L2_CAP_READWRITE;
	return 0;
}


static int vidioc_enum_frameintervals(struct file *file, void *priv,
        struct v4l2_frmivalenum *fival)
{
    struct hm5065_fmt *fmt;
    unsigned int k;

    if(fival->index > ARRAY_SIZE(hm5065_frmivalenum))
        return -EINVAL;

    for(k =0; k< ARRAY_SIZE(hm5065_frmivalenum); k++)
    {
        if( (fival->index==hm5065_frmivalenum[k].index)&&
                (fival->pixel_format ==hm5065_frmivalenum[k].pixel_format )&&
                (fival->width==hm5065_frmivalenum[k].width)&&
                (fival->height==hm5065_frmivalenum[k].height)){
            memcpy( fival, &hm5065_frmivalenum[k], sizeof(struct v4l2_frmivalenum));
            return 0;
        }
    }

    return -EINVAL;

}


static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
                	struct v4l2_fmtdesc *f)
{
	struct hm5065_fmt *fmt;

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
	struct hm5065_fh *fh = priv;

	printk("vidioc_g_fmt_vid_cap...fh->width =%d,fh->height=%d\n",fh->width,fh->height);
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
	struct hm5065_fh  *fh  = priv;
	struct hm5065_device *dev = fh->dev;
	struct hm5065_fmt *fmt;
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
	struct hm5065_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct hm5065_device *dev = fh->dev;
	resolution_param_t* res_param = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

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
    printk("system aquire ...fh->height=%d, fh->width= %d\n",fh->height,fh->width);//potti
#if 1
    if(f->fmt.pix.pixelformat==V4L2_PIX_FMT_RGB24){
    	res_param = get_resolution_param(dev, 1, fh->width,fh->height);
    	if (!res_param) {
    		printk("error, resolution param not get\n");
    		goto out;
   		}
    	    	
    	get_focus_position(dev);
		set_resolution_param(dev, res_param);
    	set_focus_position(dev);		
    }
    else {
        res_param = get_resolution_param(dev, 0, fh->width,fh->height);
        if (!res_param) {
    		printk("error, resolution param not get\n");
    		goto out;
   		}
   		set_resolution_param(dev, res_param);
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
    struct hm5065_fh *fh = priv;
    struct hm5065_device *dev = fh->dev;
    struct v4l2_captureparm *cp = &parms->parm.capture;
    int ret;
    int i;

    dprintk(dev,3,"vidioc_g_parm\n");
    if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;

    memset(cp, 0, sizeof(struct v4l2_captureparm));
    cp->capability = V4L2_CAP_TIMEPERFRAME;

    cp->timeperframe = hm5065_frmintervals_active;
    printk("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
            cp->timeperframe.numerator );
    return 0;
}


static int vidioc_reqbufs(struct file *file, void *priv,
              struct v4l2_requestbuffers *p)
{
	struct hm5065_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct hm5065_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct hm5065_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct hm5065_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
            	file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct hm5065_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct hm5065_fh  *fh = priv;
    vdin_parm_t para;
    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    	return -EINVAL;
	if (i != fh->type)
    	return -EINVAL;
    memset( &para, 0, sizeof( para ));
    para.port  = TVIN_PORT_CAMERA;
    para.fmt = TVIN_SIG_FMT_MAX;//TVIN_SIG_FMT_MAX+1;;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
    printk("vidioc_streamon =%d,=%s \n ",TVIN_SIG_FMT_MAX,TVIN_SIG_FMT_MAX);
    if (fh->dev->cur_resolution_param) {
		para.frame_rate = fh->dev->cur_resolution_param->active_fps;//175;
		para.h_active = fh->dev->cur_resolution_param->active_frmsize.width;
		para.v_active = fh->dev->cur_resolution_param->active_frmsize.height;
	} else {
		para.frame_rate = hm5065_frmintervals_active.denominator;
		para.h_active = 640;
		para.v_active = 478;
	}
    para.hsync_phase = 1;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 2;
	para.cfmt = TVIN_YUV422;
	para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;	
	para.reserved = 2; //skip_num
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
        fh->stream_on = 1;
    }
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct hm5065_fh  *fh = priv;

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
	struct hm5065_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;
	for (i = 0; i < ARRAY_SIZE(formats); i++) {
    	if (formats[i].fourcc == fsize->pixel_format){
        	fmt = &formats[i];
        	break;
        }
    }
	if (fmt == NULL)
    	return -EINVAL;
	if ((fmt->fourcc ==  V4L2_PIX_FMT_NV21)||(fmt->fourcc == V4L2_PIX_FMT_NV12)
		||(fmt->fourcc == V4L2_PIX_FMT_YUV420)
		||(fmt->fourcc == V4L2_PIX_FMT_YVU420)){
		printk("hm5065_prev_resolution[fsize->index]   before fsize->index== %d\n",fsize->index);//potti
    	if (fsize->index >= ARRAY_SIZE(prev_resolution_array))
        	return -EINVAL;
    	frmsize = &prev_resolution_array[fsize->index].frmsize;
		printk("hm5065_prev_resolution[fsize->index]   after fsize->index== %d\n",fsize->index);
    	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    	fsize->discrete.width = frmsize->width;
    	fsize->discrete.height = frmsize->height;
    }
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		printk("hm5065_pic_resolution[fsize->index]   before fsize->index== %d\n",fsize->index);
    	if (fsize->index >= ARRAY_SIZE(capture_resolution_array))
        	return -EINVAL;
    	frmsize = &capture_resolution_array[fsize->index].frmsize;
		printk("hm5065_pic_resolution[fsize->index]   after fsize->index== %d\n",fsize->index);    
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
	struct hm5065_fh *fh = priv;
	struct hm5065_device *dev = fh->dev;

    *i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct hm5065_fh *fh = priv;
	struct hm5065_device *dev = fh->dev;

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
	struct hm5065_fh *fh = priv;
	struct hm5065_device *dev = fh->dev;
	
	if (!dev->platform_dev_data.flash_support 
			&& qc->id == V4L2_CID_BACKLIGHT_COMPENSATION)
			return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(hm5065_qctrl); i++)
    	if (qc->id && qc->id == hm5065_qctrl[i].id) {
            memcpy(qc, &(hm5065_qctrl[i]),
            	sizeof(*qc));
            if (hm5065_qctrl[i].type == V4L2_CTRL_TYPE_MENU)
                return hm5065_qctrl[i].maximum+1;
            else
        	return (0);
        }

	return -EINVAL;
}

static int vidioc_querymenu(struct file *file, void *priv,
                struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(hm5065_qmenu_set); i++)
    	if (a->id && a->id == hm5065_qmenu_set[i].id) {
    	    for(j = 0; j < hm5065_qmenu_set[i].num; j++)
    	        if (a->index == hm5065_qmenu_set[i].hm5065_qmenu[j].index) {
        	        memcpy(a, &( hm5065_qmenu_set[i].hm5065_qmenu[j]),
            	        sizeof(*a));
        	        return (0);
        	    }
        }

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
             struct v4l2_control *ctrl)
{
	struct hm5065_fh *fh = priv;
	struct hm5065_device *dev = fh->dev;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int i;

	for (i = 0; i < ARRAY_SIZE(hm5065_qctrl); i++)
    	if (ctrl->id == hm5065_qctrl[i].id) {
		if( (V4L2_CID_FOCUS_AUTO == ctrl->id)
		    && bDoingAutoFocusMode){			
			if(i2c_get_byte(client, 0x0725) == 0){
				return -EBUSY;
		    }else{
				bDoingAutoFocusMode = false;
				if(i2c_get_byte(client, 0x07ae) == 0){
					printk("auto mode failed!\n");
					return -EAGAIN;
				}else {					
					printk("pause auto focus\n");
				}
			}
		}
        	ctrl->value = dev->qctl_regs[i];
        	return 0;
        }

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
            	struct v4l2_control *ctrl)
{
	struct hm5065_fh *fh = priv;
	struct hm5065_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(hm5065_qctrl); i++)
    	if (ctrl->id == hm5065_qctrl[i].id) {
        	if (ctrl->value < hm5065_qctrl[i].minimum ||
                ctrl->value > hm5065_qctrl[i].maximum ||
                hm5065_setting(dev,ctrl->id,ctrl->value)<0) {
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

static int hm5065_open(struct file *file)
{
	struct hm5065_device *dev = video_drvdata(file);
	struct hm5065_fh *fh = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int retval = 0;
	int reg_val;
	int i = 0;
	hm5065_have_opened=1;
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif	
	if(dev->platform_dev_data.device_init) {
    	dev->platform_dev_data.device_init();
    	printk("+++%s found a init function, and run it..\n", __func__);
    }
	HM5065_init_regs(dev);
	
	msleep(10);
	
	/*if(HM5065_download_firmware(dev) >= 0) {
		while(i2c_get_byte(client, 0x3029) != 0x70 && i < 10) { //wait for the mcu ready 
        	msleep(5);
        	i++;
    	}
    	dev->firmware_ready = 1;
	}*/
	
	//schedule_work(&(dev->dl_work));

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
	fh->f_flags  = file->f_flags;	//huangbo
    /* Resets frame counters */
	dev->jiffies = jiffies;
            
	videobuf_queue_vmalloc_init(&fh->vb_vidq, &hm5065_video_qops,
        	NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
        	sizeof(struct hm5065_buffer), fh, NULL);
	bDoingAutoFocusMode=false;
	hm5065_start_thread(fh);

	return 0;
}

static ssize_t
hm5065_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct hm5065_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    	return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
                	file->f_flags & O_NONBLOCK);
    }
	return 0;
}

static unsigned int
hm5065_poll(struct file *file, struct poll_table_struct *wait)
{
	struct hm5065_fh        *fh = file->private_data;
	struct hm5065_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
    	return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int hm5065_close(struct file *file)
{
	struct hm5065_fh         *fh = file->private_data;
	struct hm5065_device *dev       = fh->dev;
	struct hm5065_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	hm5065_have_opened=0;
	//dev->firmware_ready = 0;
	hm5065_stop_thread(vidq);
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
	hm5065_qctrl[4].default_value=0;
	hm5065_qctrl[5].default_value=4;
	hm5065_qctrl[6].default_value=0;
	hm5065_frmintervals_active.numerator = 1;
	hm5065_frmintervals_active.denominator = 15;
	//power_down_hm5065(dev);
#endif
	msleep(2);

	if(dev->platform_dev_data.device_uninit) {
    	dev->platform_dev_data.device_uninit();
    	printk("+++%s found a uninit function, and run it..\n", __func__);
    }
    if (dev->platform_dev_data.flash_ctrl)
		if(dev->platform_dev_data.flash_ctrl->flashlight_off)
			dev->platform_dev_data.flash_ctrl->flashlight_off();
	msleep(2); 
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 0);
#endif	
	wake_unlock(&(dev->wake_lock));
	return 0;
}

static int hm5065_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct hm5065_fh  *fh = file->private_data;
	struct hm5065_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
        (unsigned long)vma->vm_start,
        (unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
    	ret);

	return ret;
}

static const struct v4l2_file_operations hm5065_fops = {
    .owner	    = THIS_MODULE,
    .open       = hm5065_open,
    .release    = hm5065_close,
    .read       = hm5065_read,
    .poll	    = hm5065_poll,
    .ioctl      = video_ioctl2, /* V4L2 ioctl handler */
    .mmap       = hm5065_mmap,
};

static const struct v4l2_ioctl_ops hm5065_ioctl_ops = {
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

static struct video_device hm5065_template = {
    .name	        = "hm5065_v4l",
    .fops           = &hm5065_fops,
    .ioctl_ops      = &hm5065_ioctl_ops,
    .release	    = video_device_release,

    .tvnorms        = V4L2_STD_525_60,
    .current_norm   = V4L2_STD_NTSC_M,
};

static int hm5065_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV5642, 0);
}

static const struct v4l2_subdev_core_ops hm5065_core_ops = {
    .g_chip_ident = hm5065_g_chip_ident,
};

static const struct v4l2_subdev_ops hm5065_ops = {
    .core = &hm5065_core_ops,
};

//****************************
static ssize_t camera_ctrl(struct class *class, 
			struct class_attribute *attr,	const char *buf, size_t count)
{
    unsigned int reg, val, ret;	
	unsigned char buff[4];
	int n=1,i;
	if(buf[0] == 'w'){
		ret = sscanf(buf, "w %x %x", &reg, &val);		
		//printk("write camera reg 0x%x value 0x%x\n", reg, val);
		//i2c_put_byte(this_client, reg, val);		
		if (i2c_put_byte(this_client, reg, val) < 0)
			printk("write camera reg error: 0x%x = 0x%x.\n", reg, val);
		else
			printk("write camera reg successful: 0x%x = 0x%x.\n", reg, val);
	}
	else{
		ret =  sscanf(buf, "r %x %d", &reg,&n);
		printk("read %d camera register from reg: 0x%x \n",n,reg);
		for(i=0;i<n;i++)
		{			
			//val = i2c_get_byte_new(this_client, reg+i);
			val = i2c_get_byte(this_client, reg+i);
			buff[0] = (unsigned char)(((reg+i)>>8) & 0xff);
			buff[1] = (unsigned char)((reg+i) & 0xff);
			if (customer_i2c_read(this_client, buff, 2, 1)<0)
				printk("read camera reg error: 0x%x.\n", reg+i);
			else
				printk("reg 0x%x = 0x%x, val=0x%x.\n", reg+i, buff[0], val);
		}
	}
	
	if (ret != 1 || ret !=2)
		return -EINVAL;
	
	return count;
	//return 0;
}

static struct class_attribute camera_ctrl_class_attrs[] = {
    __ATTR(reg,  S_IRUGO | S_IWUSR, NULL,    camera_ctrl), 
    __ATTR_NULL
};

static struct class camera_ctrl_class = {
    .name = "camera",
    .class_attrs = camera_ctrl_class_attrs,
};
//****************************

static int hm5065_probe(struct i2c_client *client,
        	const struct i2c_device_id *id)
{
	int pgbuf;
	int err;
	struct hm5065_device *t;
	struct v4l2_subdev *sd;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
        	client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
    	return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &hm5065_ops);
	mutex_init(&t->mutex);
	this_client=client;
	
    /* Now create a video4linux device */
	t->vdev = video_device_alloc();
	if (t->vdev == NULL) {
    	kfree(t);
    	kfree(client);
    	return -ENOMEM;
    }
	memcpy(t->vdev, &hm5065_template, sizeof(*t->vdev));
	
	video_set_drvdata(t->vdev, t);
	
	//INIT_WORK(&(t->dl_work), do_download);
	
	wake_lock_init(&(t->wake_lock),WAKE_LOCK_SUSPEND, "hm5065");
    /* Register it */
	aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
	if (plat_dat) {
    	t->platform_dev_data.device_init=plat_dat->device_init;
    	t->platform_dev_data.device_uninit=plat_dat->device_uninit;
    	t->platform_dev_data.flash_support=plat_dat->flash_support;
    	t->platform_dev_data.flash_ctrl	= plat_dat->flash_ctrl;
    	if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
    	if(t->platform_dev_data.device_init) {
        	t->platform_dev_data.device_init();
        	printk("+++%s found a device_probe function, and run it..\n", __func__);
        }
    	power_down_hm5065(t);
    	if(t->platform_dev_data.device_uninit) {
    	t->platform_dev_data.device_uninit();
    	printk("+++%s found a uninit function, and run it..\n", __func__);
        }
    }
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
    	video_device_release(t->vdev);
    	kfree(t);
    	return err;
    }
	
	int ret;
	ret = class_register(&camera_ctrl_class);
	if(ret){
		printk(" class register camera_ctrl_class fail!\n");
	}
	
	return 0;
}

static int hm5065_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct hm5065_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	wake_lock_destroy(&(t->wake_lock));
	kfree(t);
	return 0;
}

static const struct i2c_device_id hm5065_id[] = {
    { "hm5065_i2c", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, hm5065_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
    .name = "hm5065",
    .probe = hm5065_probe,
    .remove = hm5065_remove,
    .id_table = hm5065_id,
};

