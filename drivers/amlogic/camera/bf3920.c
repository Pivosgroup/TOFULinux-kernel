/*
 *bf3920 - This code emulates a real video device with v4l2 api
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

#define BF3920_CAMERA_MODULE_NAME "bf3920"

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001
#define BUFFER_TIMEOUT     msecs_to_jiffies(500)  /* 0.5 seconds */

#define BF3920_CAMERA_MAJOR_VERSION 0
#define BF3920_CAMERA_MINOR_VERSION 7
#define BF3920_CAMERA_RELEASE 0
#define BF3920_CAMERA_VERSION \
	KERNEL_VERSION(BF3920_CAMERA_MAJOR_VERSION, BF3920_CAMERA_MINOR_VERSION, BF3920_CAMERA_RELEASE)

MODULE_DESCRIPTION("bf3920 On Board");
MODULE_AUTHOR("amlogic-sh");
MODULE_LICENSE("GPL v2");

static unsigned video_nr = -1;  /* videoX start number, -1 is autodetect. */

static unsigned debug;
//module_param(debug, uint, 0644);
//MODULE_PARM_DESC(debug, "activates debug info");

static unsigned int vid_limit = 16;
//module_param(vid_limit, uint, 0644);
//MODULE_PARM_DESC(vid_limit, "capture memory limit in megabytes");

static int vidio_set_fmt_ticks=0;

extern int disable_bf3920;

static int bf3920_h_active=800;
static int bf3920_v_active=598;
static struct v4l2_fract bf3920_frmintervals_active = {
    .numerator = 1,
    .denominator = 15,
};

static int bf3920_have_open=0;

/* supported controls */
static struct v4l2_queryctrl bf3920_qctrl[] = {
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

static struct v4l2_frmivalenum bf3920_frmivalenum[]={
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

struct v4l2_querymenu bf3920_qmenu_wbmode[] = {
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
    },
};

struct v4l2_querymenu bf3920_qmenu_anti_banding_mode[] = {
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
    struct v4l2_querymenu* bf3920_qmenu;
}bf3920_qmenu_set_t;

bf3920_qmenu_set_t bf3920_qmenu_set[] = {
    {
        .id         	= V4L2_CID_DO_WHITE_BALANCE,
        .num            = ARRAY_SIZE(bf3920_qmenu_wbmode),
        .bf3920_qmenu   = bf3920_qmenu_wbmode,
    },{
        .id         	= V4L2_CID_POWER_LINE_FREQUENCY,
        .num            = ARRAY_SIZE(bf3920_qmenu_anti_banding_mode),
        .bf3920_qmenu   = bf3920_qmenu_anti_banding_mode,
    },
};

static int vidioc_querymenu(struct file *file, void *priv,
                struct v4l2_querymenu *a)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(bf3920_qmenu_set); i++)
	if (a->id && a->id == bf3920_qmenu_set[i].id) {
	    for(j = 0; j < bf3920_qmenu_set[i].num; j++)
		if (a->index == bf3920_qmenu_set[i].bf3920_qmenu[j].index) {
			memcpy(a, &( bf3920_qmenu_set[i].bf3920_qmenu[j]),
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

struct bf3920_fmt {
	char  *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
};

static struct bf3920_fmt formats[] = {
	{
		.name     = "RGB565 (BE)",
		.fourcc   = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
		.depth    = 16,
	},{
		.name     = "RGB888 (24)",
		.fourcc   = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
		.depth    = 24,
	},{
		.name     = "BGR888 (24)",
		.fourcc   = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
		.depth    = 24,
	},{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV12,
		.depth    = 12,
	},{
		.name     = "12  Y/CbCr 4:2:0",
		.fourcc   = V4L2_PIX_FMT_NV21,
		.depth    = 12,
	},{
		.name     = "YUV420P",
		.fourcc   = V4L2_PIX_FMT_YUV420,
		.depth    = 12,
	},{
		.name     = "YVU420P",
		.fourcc   = V4L2_PIX_FMT_YVU420,
		.depth    = 12,
	}
};

static struct bf3920_fmt *get_format(struct v4l2_format *f)
{
	struct bf3920_fmt *fmt;
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
struct bf3920_buffer {
	/* common v4l buffer stuff -- must be first */
	struct videobuf_buffer vb;

	struct bf3920_fmt        *fmt;
};

struct bf3920_dmaqueue {
	struct list_head       active;

	/* thread for generating video stream*/
	struct task_struct         *kthread;
	wait_queue_head_t          wq;
	/* Counters to control fps rate */
	int                        frame;
	int                        ini_jiffies;
};

static LIST_HEAD(bf3920_devicelist);

struct bf3920_device {
	struct list_head			bf3920_devicelist;
	struct v4l2_subdev			sd;
	struct v4l2_device			v4l2_dev;

	spinlock_t                 slock;
	struct mutex				mutex;

	int                        users;

	/* various device info */
	struct video_device        *vdev;

	struct bf3920_dmaqueue       vidq;

	/* Several counters */
	unsigned long              jiffies;

	/* Input Number */
	int			   input;

	/* platform device data from board initting. */
	aml_plat_cam_data_t platform_dev_data;
	
	/* wake lock */
	struct wake_lock	wake_lock;

	/* Control 'registers' */
	int 			   qctl_regs[ARRAY_SIZE(bf3920_qctrl)];
};

static inline struct bf3920_device *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct bf3920_device, sd);
}

struct bf3920_fh {
	struct bf3920_device            *dev;

	/* video capture */
	struct bf3920_fmt            *fmt;
	unsigned int               width, height;
	struct videobuf_queue      vb_vidq;

	enum v4l2_buf_type         type;
	int			   input; 	/* Input Number on bars */
	int  stream_on;
	unsigned int		f_flags;
};

static inline struct bf3920_fh *to_fh(struct bf3920_device *dev)
{
	return container_of(dev, struct bf3920_fh, dev);
}

static struct v4l2_frmsize_discrete bf3920_prev_resolution[]= //should include 352x288 and 640x480, those two size are used for recording
{
	{320,240},
	{352,288},
	{640,480},
};

static struct v4l2_frmsize_discrete bf3920_pic_resolution[]=
{
	{1600,1200},
	{800,600},
	{640,480}
};

/* ------------------------------------------------------------------
	reg spec of bf3920
   ------------------------------------------------------------------*/

struct aml_camera_i2c_fig1_s BF3920_script[] = {
    
    {0x1b,0x2c}, //PLL
	{0x11,0x30},
	{0x8a,0x79},
	{0x2a,0x0c},
	{0x2b,0x7c},
	{0x92,0x02},
	{0x93,0x00},
	        	     
	{0x21,0x15},//driver capability
	{0x15,0x82},// 采数时序 
	{0x3a,0x01},// YUV 输出顺序；03:VYUY; 02:UYVY; 01:YVYU; 00:YUYV 

	//initial AE and AWB
	{0x13,0x00},
	{0x01,0x12},
	{0x02,0x22},
	{0x24,0xc8},
	{0x87,0x2d},
	{0x13,0x07},

	//black level  
	{0x27,0x98},
	{0x28,0xff},
	{0x29,0x60},

	//black target  
	{0x1F,0x20},
	{0x22,0x20},
	{0x20,0x20},
	{0x26,0x20},

	//analog
	{0xe2,0xc4},
	{0xe7,0x4e},
	{0x08,0x16},
	{0x16,0x28},
	{0x1c,0x00},
	{0x1d,0xf1},
	{0xbb,0x30},
	{0xf9,0x00},
	{0xed,0x8f},
	{0x3e,0x80},

	//Shading
	{0x1e,0x66},
	{0x35,0x30},
	{0x65,0x30},
	{0x66,0x2a},
	{0xbc,0xd3},
	{0xbd,0x5c},
	{0xbe,0x24},
	{0xbf,0x13},
	{0x9b,0x5c},
	{0x9c,0x24},
	{0x36,0x13},
	{0x37,0x5c},
	{0x38,0x24},

	//denoise
	{0x70,0x01},
	{0x72,0x62},
	{0x78,0x37},
	{0x7a,0x29},
	{0x7d,0x85},

        //AE gain curve
	{0x13,0x0f},
 	{0x8a,0x10},
 	{0x8b,0x1d},
 	{0x8e,0x1e},
 	{0x8f,0x3a},
 	{0x94,0x3d},
 	{0x95,0x71},
 	{0x96,0x76},
 	{0x97,0xc5},
 	{0x98,0xc6},
 	{0x99,0x7b}, 
	// AE
	{0x13,0x07},
	{0x24,0x48},
	{0x25,0x88},
	{0x97,0x30},
	{0x98,0x0a},
	{0x80,0x9a}, //Bit[3:2]: the bigger, Y_AVER_MODIFY is smaller     
	{0x81,0xe0},
	{0x82,0x30},
	{0x83,0x60},
	{0x84,0x55},              
	{0x85,0x80},                
	{0x86,0xc0},
	{0x89,0x55},
	{0x94,0x82}, //Bit[7:4]: Threshold for over exposure pixels, the smaller, the more over exposure pixels; Bit[3:0]: Control the start of AE.

	//Gamma default
	{0x3f,0x20},
	{0x39,0xa0},

	{0x40,0x20},
	{0x41,0x25},
	{0x42,0x23},
	{0x43,0x1f},
	{0x44,0x18},
	{0x45,0x15},
	{0x46,0x12},
	{0x47,0x10},
	{0x48,0x10},
	{0x49,0x0f},
	{0x4b,0x0e},
	{0x4c,0x0c},
	{0x4e,0x0b},
	{0x4f,0x0a},
	{0x50,0x07},

	/*
	//gamma 过曝过度好，高亮                                                       
	{0x40,0x28},            
	{0x41,0x28},            
	{0x42,0x30},            
	{0x43,0x29},            
	{0x44,0x23},            
	{0x45,0x1b},            
	{0x46,0x17},
	{0x47,0x0f},
	{0x48,0x0d},
	{0x49,0x0b},
	{0x4b,0x09},
	{0x4c,0x08},
	{0x4e,0x07},
	{0x4f,0x05},
	{0x50,0x04},
	{0x39,0xa0},
	{0x3f,0x20},
	    
	//gamma 清晰亮丽                                                           
	{0x40,0x28},
	{0x41,0x26},
	{0x42,0x24},
	{0x43,0x22},
	{0x44,0x1c},
	{0x45,0x19},
	{0x46,0x15},
	{0x47,0x11},
	{0x48,0x0f},
	{0x49,0x0e},
	{0x4b,0x0c},
	{0x4c,0x0a},
	{0x4e,0x09},
	{0x4f,0x08},
	{0x50,0x04},
	{0x39,0xa0},
	{0x3f,0x20},
	*/


	{0x5c,0x80},
	{0x51,0x22},
	{0x52,0x00},
	{0x53,0x96},
	{0x54,0x8C},
	{0x57,0x7F},
	{0x58,0x0B},
	{0x5a,0x14},

	//Color default
	{0x5c,0x00},
	{0x51,0x32},
	{0x52,0x17},
	{0x53,0x8C},
	{0x54,0x79},
	{0x57,0x6E},
	{0x58,0x01},
	{0x5a,0x36},
	{0x5e,0x38},

	/*                                                                                            
	//color 绿色好 
	{0x5c,0x00},                
	{0x51,0x24},                
	{0x52,0x0b},                
	{0x53,0x77},                
	{0x54,0x65},                
	{0x57,0x5e},                
	{0x58,0x19},                               
	{0x5a,0x16},                
	{0x5e,0x38},     

		
	//color 艳丽
	{0x5c,0x00},               
	{0x51,0x2b},               
	{0x52,0x0e},               
	{0x53,0x9f},               
	{0x54,0x7d},               
	{0x57,0x91},               
	{0x58,0x21},                              
	{0x5a,0x16},               
	{0x5e,0x38},
		                         

	//color 色彩淡                         
	{0x5c,0x00},             
	{0x51,0x24},             
	{0x52,0x0b},             
	{0x53,0x77},             
	{0x54,0x65},             
	{0x57,0x5e},             
	{0x58,0x19},                            
	{0x5a,0x16},             
	{0x5e,0x38},
	*/

	//AWB
	{0x6a,0x81},	
	{0x23,0x66},
	{0xa1,0x31},
	{0xa2,0x0b},
	{0xa3,0x25},
	{0xa4,0x09},
	{0xa5,0x26},
	{0xa7,0x9a},
	{0xa8,0x15},
	{0xa9,0x13},
	{0xaa,0x12},
	{0xab,0x16},
	{0xc8,0x10},
	{0xc9,0x15},
	{0xd2,0x78},
	{0xd4,0x20},

	//saturation
	{0x56,0x28},
	{0xb0,0x8d},

	{0x56,0xe8},
	{0x55,0xf0},
	{0xb0,0xb8},

	{0x56,0xa8},
	{0x55,0xc8},
	{0xb0,0x98},
	
	//Resolution Setting : 800*600
	{0x1b,0x2c},
	{0x11,0x30},//34_9f//30_7f
	{0x8a,0x78},//c1//0x7f
	{0x2a,0x0c},
	{0x2b,0x90},//0x1c
	{0x92,0x02},
	{0x93,0x00},

	//window	
	{0x4a,0x80},
	{0xcc,0x00},
	{0xca,0x00},
	{0xcb,0x00},
	{0xcf,0x00},
	{0xcd,0x00},
	{0xce,0x00},
	{0xc0,0x00},
	{0xc1,0x00},
	{0xc2,0x00},
	{0xc3,0x00},
	{0xc4,0x00},
	{0xb5,0x00},
	{0x03,0x60},
	{0x17,0x00},
	{0x18,0x40},
	{0x10,0x40},
	{0x19,0x00},
	{0x1a,0xb0},
	{0x0B,0x10},        
	
	{0x09,0x42},
	//skin
	{0xee,0x2a},
	{0xef,0x1b}, 
	{0x8a,0x79},
	{0xff,0xff},
};

//load bf3920 parameters
void BF3920_init_regs(struct bf3920_device *dev)
{
		int i=0;//,j;
		unsigned char buf[2];
		struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	
		while(1)
		{
			buf[0] = BF3920_script[i].addr;//(unsigned char)((BF3920_script[i].addr >> 8) & 0xff);
			//buf[1] = (unsigned char)(BF3920_script[i].addr & 0xff);
			buf[1] = BF3920_script[i].val;
			//printk("buf[0]---0x%s-------0x%s--",buf[0],buf[1]);
			if(BF3920_script[i].val==0xff&&BF3920_script[i].addr==0xff){
				printk("BF3920_write_regs success in initial bf3920.\n");
				break;
			}
			if((i2c_put_byte_add8(client,buf, 2)) < 0){
				printk("fail in initial bf3920. \n");
				return;
			}
			i++;
		}
		aml_plat_cam_data_t* plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;
		if (plat_dat&&plat_dat->custom_init_script) {
			i=0;
			aml_camera_i2c_fig1_t*	custom_script = (aml_camera_i2c_fig1_t*)plat_dat->custom_init_script;
			while(1)
			{
				buf[0] = custom_script[i].addr;
				buf[1] = custom_script[i].val;
				if (custom_script[i].val==0xff&&custom_script[i].addr==0xff){
					printk("BF3920_write_custom_regs success in initial bf3920.\n");
					break;
				}
				if((i2c_put_byte_add8(client,buf, 2)) < 0){
					printk("fail in initial bf3920 custom_regs. \n");
					return;
				}
				i++;
			}
		}
		return;
	
	}

/*************************************************************************
* FUNCTION
*    bf3920_set_param_wb
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
void BF3920_set_param_wb(struct bf3920_device *dev,enum  camera_wb_flip_e para)//white balance
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char buf[2]; 
    switch (para)
	{
         
		case CAM_WB_AUTO:
			buf[0]=0x13;
			buf[1]=0x07;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x01;
			buf[1]=0x12;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x22;	
			i2c_put_byte_add8(client,buf,2);

			break;

		case CAM_WB_CLOUD: //cloud
			buf[0]=0x13;
			buf[1]=0x05;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x01;
			buf[1]=0x0e;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x24;	
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_DAYLIGHT: //
			buf[0]=0x13;
			buf[1]=0x05;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x01;
			buf[1]=0x0c;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x10;	
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_INCANDESCENCE:
			buf[0]=0x13;
			buf[1]=0x05;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x01;
			buf[1]=0x26;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x1a;	
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_TUNGSTEN:
			buf[0]=0x13;
			buf[1]=0x05;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x01;
			buf[1]=0x20;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x10;	
			i2c_put_byte_add8(client,buf,2);
			break;

      	case CAM_WB_FLUORESCENT:
			buf[0]=0x13;
			buf[1]=0x05;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x01;
			buf[1]=0x21;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x02;
			buf[1]=0x1e;	
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_WB_MANUAL:
		    	// TODO
			break;
	default:
			break;
	}


} /* bf3920_set_param_wb */
/*************************************************************************
* FUNCTION
*    bf3920_set_param_exposure
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
void BF3920_set_param_exposure(struct bf3920_device *dev,enum camera_exposure_e para)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[2];
    switch (para)
	{
		case EXPOSURE_N4_STEP:
			buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0xc8;	
			i2c_put_byte_add8(client,buf,2);
			break;
		case EXPOSURE_N3_STEP:
		    buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0xb0;	
			i2c_put_byte_add8(client,buf,2);
			break;
		case EXPOSURE_N2_STEP:
			buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0xa0;	
			i2c_put_byte_add8(client,buf,2);
			break;
		case EXPOSURE_N1_STEP:
			buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0x90;	
			i2c_put_byte_add8(client,buf,2);
			break;		
		case EXPOSURE_0_STEP:
		    buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0x00;	
			i2c_put_byte_add8(client,buf,2);
			break;		
		case EXPOSURE_P1_STEP:
			buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0x10;	
			i2c_put_byte_add8(client,buf,2);
			break;	
		case EXPOSURE_P2_STEP:
		    buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0x20;	
			i2c_put_byte_add8(client,buf,2);
			break;
		case EXPOSURE_P3_STEP:
			buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0x30;	
			i2c_put_byte_add8(client,buf,2);
			break;						
		case EXPOSURE_P4_STEP:
			buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0x40;	
			i2c_put_byte_add8(client,buf,2);
			break;
		default:
			buf[0]=0x56;
			buf[1]=0x28;	
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x55;
			buf[1]=0x00;	
			i2c_put_byte_add8(client,buf,2);
			break;
	}
} /* bf3920_set_param_exposure */
/*************************************************************************
* FUNCTION
*    bf3920_set_param_effect
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
void BF3920_set_param_effect(struct bf3920_device *dev,enum camera_effect_flip_e para)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char buf[2];
    switch (para)
	{
		case CAM_EFFECT_ENC_NORMAL:
            buf[0]=0x98;
			buf[1]=0x8a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x70;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x69;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x67;
			buf[1]=0x80;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x68;
			buf[1]=0x80;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0x28;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb0;
			buf[1]=0x8d;
			i2c_put_byte_add8(client,buf,2);
            buf[0]=0xb1;
			buf[1]=0x15; 
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0xe8;
			i2c_put_byte_add8(client,buf,2);
		    buf[0]=0xb4;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);	
																													
			break;

		case CAM_EFFECT_ENC_GRAYSCALE:
			buf[0]=0x98;
			buf[1]=0x8a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x70;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x69;
			buf[1]=0x20;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x67;
			buf[1]=0x80;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x68;
			buf[1]=0x80;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0x28;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb0;
			buf[1]=0x8d;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb1;
			buf[1]=0x95;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0xe8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_EFFECT_ENC_SEPIA:
			buf[0]=0x98;
			buf[1]=0x8a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x70;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x69;
			buf[1]=0x20;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x67;
			buf[1]=0x58;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x68;
			buf[1]=0xa0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0x28;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb0;
			buf[1]=0x8d;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb1;
			buf[1]=0x95;
			i2c_put_byte_add8(client,buf,2);
		    buf[0]=0x56;
			buf[1]=0xe8;
            i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
			buf[1]=0x40;
			i2c_put_byte_add8(client,0xb4,0x40);
            i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_EFFECT_ENC_SEPIAGREEN:
			buf[0]=0x98;
			buf[1]=0x8a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x70;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
            buf[0]=0x69;
			buf[1]=0x20;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x67;
			buf[1]=0x68;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x68;
			buf[1]=0x60;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0x28;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb0;
			buf[1]=0x8d;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb1;
			buf[1]=0x95;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0xe8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_EFFECT_ENC_SEPIABLUE:
			buf[0]=0x98;
			buf[1]=0x8a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x70;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x69;
			buf[1]=0x20;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x67;
			buf[1]=0xc0;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x68;
			buf[1]=0x58;
			i2c_put_byte_add8(client,buf,2);
            buf[0]=0x56;
			buf[1]=0x28;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb0;
			buf[1]=0x8d;
		    i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb1;
			buf[1]=0x95;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0xe8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			break;

		case CAM_EFFECT_ENC_COLORINV:
			buf[0]=0x98;
			buf[1]=0x8a;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x70;
			buf[1]=0x00;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x69;
			buf[1]=0x01;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x67;
			buf[1]=0x80;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x68;
			buf[1]=0x80;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0x28;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb0;
			buf[1]=0x8d;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb1;
			buf[1]=0x95;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0x56;
			buf[1]=0xe8;
			i2c_put_byte_add8(client,buf,2);
			buf[0]=0xb4;
			buf[1]=0x40;
			i2c_put_byte_add8(client,buf,2);
			break;

		default:
			break;
	}



} /* bf3920_set_param_effect */

/*************************************************************************
* FUNCTION
*    bf3920_NightMode
*
* DESCRIPTION
*    This function night mode of bf3920.
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
void BF3920_set_night_mode(struct bf3920_device *dev,enum  camera_night_mode_flip_e enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
    unsigned char buf[2];
	if (enable) {
		buf[0]=0x86;
		buf[1]=0xff;
		i2c_put_byte_add8(client,buf ,2); //Camera Enable night mode 
	    buf[0]=0x89;
		buf[1]=0xa5;
	    i2c_put_byte_add8(client,buf, 2); //Camera Enable night mode  
	}
	else {
		buf[0]=0x86;
		buf[1]=0xc0;
		i2c_put_byte_add8(client,buf , 2); // //97
		buf[0]=0x89;
		buf[1]=0x5d;
	    i2c_put_byte_add8(client,0x89 , 0x5d);  
	}

}    /* bf3920_NightMode */
void BF3920_set_param_banding(struct bf3920_device *dev,enum  camera_night_mode_flip_e banding)
{
    struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[2];

	switch(banding){
		case CAM_BANDING_50HZ:
			buf[0]=0x80;
			buf[1]=0x9a;
		    i2c_put_byte_add8(client,buf ,2); // //97
		    buf[0]=0x8a;
			buf[1]=0x79;//7f
	        i2c_put_byte_add8(client,buf ,2);  
			break;
		case CAM_BANDING_60HZ:
		    buf[0]=0x80;
			buf[1]=0x98;
		    i2c_put_byte_add8(client,buf,2);
		    buf[0]=0x8b;
			buf[1]=0x65;//6a
		    i2c_put_byte_add8(client,buf,2);  
			break;
	}

}

void BF3920_set_resolution(struct bf3920_device *dev,int height,int width)
{

	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[2];
	if (width*height<640*480){
		
		//320*240
		printk("####################set 320X240 #################################\n");
		buf[0]=0x4a;
		buf[1]=0x80;
		i2c_put_byte_add8(client,buf,2); 
		buf[0]=0xcc;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0xca;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcb;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcf;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcd;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xce;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0xc0;
		buf[1]=0xd3;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc1;
		buf[1]=0x04;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc2;
		buf[1]=0xc4;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc3;
		buf[1]=0x11;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc4;
		buf[1]=0x3f;
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0xb5;
		buf[1]=0x3f; 
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0x03;
		buf[1]=0x50; 
	    i2c_put_byte_add8(client,buf,2);
	    buf[0]=0x17;
		buf[1]=0x00; 
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0x18;
		buf[1]=0x00; 
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0x10;
		buf[1]=0x30;
		i2c_put_byte_add8(client,buf,2);
        buf[0]=0x19;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0x1a;
		buf[1]=0xc0;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x0b;
		buf[1]=0x30;
		i2c_put_byte_add8(client,buf,2);

		mdelay(100);
		bf3920_frmintervals_active.denominator 	= 15;
		bf3920_frmintervals_active.numerator	= 1;
		bf3920_h_active=320;
		bf3920_v_active=238;
		 printk(" vidioc_streamoff320----1200 \n ");
	} else if (width*height<1600*1200){
	      printk(" vidioc_streamoff800----600 \n ");
		//800*600
        buf[0]=0x4a;
		buf[1]=0x80;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcc;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xca;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcb;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcf;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcd;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xce;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc0;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc1;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc2;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc3;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0xc4;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0xb5;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x03;
		buf[1]=0x60;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x17;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x18;
		buf[1]=0x40;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x10;
		buf[1]=0x40;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x19;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x1a;
		buf[1]=0xb0;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x0b;
		buf[1]=0x10;
        i2c_put_byte_add8(client,buf,2);
		mdelay(100);
		bf3920_frmintervals_active.denominator 	= 15;
		bf3920_frmintervals_active.numerator	= 1;
		bf3920_h_active=800;
		bf3920_v_active=598;
	} else if(width*height>=1600*1200){
	   printk(" vidioc_streamoff1600----1200 \n ");
		//1600x1200				
		//window
		buf[0]=0x4a;
		buf[1]=0x80;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcc;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xca;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcb;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcf;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xcd;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xce;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc0;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc1;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc2;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc3;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xc4;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0xb5;
		buf[1]=0x00; 
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x03;
		buf[1]=0x60;
		i2c_put_byte_add8(client,buf,2);
	    buf[0]=0x17;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x18;
		buf[1]=0x40;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x10;
		buf[1]=0x40;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x19;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x1a;
		buf[1]=0xb0;
		i2c_put_byte_add8(client,buf,2);
		buf[0]=0x0b;
		buf[1]=0x00;
		i2c_put_byte_add8(client,buf,2);
	
		msleep(100);

		bf3920_h_active=1600;
		bf3920_v_active=1198;
		bf3920_frmintervals_active.denominator 	= 5;
		bf3920_frmintervals_active.numerator	= 1;
	}
	printk(KERN_INFO " set camera  bf3920_set_resolution=w=%d,h=%d. \n ",width,height);
}    /* bf3920_set_resolution */

unsigned char v4l_2_bf3920(int val)
{
	int ret=val/0x20;
	if(ret<4) return ret*0x20+0x80;
	else if(ret<8) return ret*0x20+0x20;
	else return 0;
}

static int bf3920_setting(struct bf3920_device *dev,int PROP_ID,int value )
{
	int ret=0;
	unsigned char cur_val;
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	switch(PROP_ID)  {
#if 0
	case V4L2_CID_EXPOSURE:
		ret=i2c_put_byte_add8(client,0x0201, value);
		break;
	case V4L2_CID_HFLIP:    /* set flip on H. */
		ret=i2c_get_byte(client,0x0101);
		if(ret>0) {
			cur_val=(char)ret;
			if(value!=0)
				cur_val=cur_val|0x1;
			else
				cur_val=cur_val&0xFE;
			ret=i2c_put_byte_add8(client,0x0101,cur_val);
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
				cur_val=cur_val|0x02;
			else
				cur_val=cur_val&0xFD;
			ret=i2c_put_byte_add8(client,0x0101,cur_val);
		} else {
			dprintk(dev, 1, "vertical read error\n");
		}
		break;
#endif
	case V4L2_CID_DO_WHITE_BALANCE:
		if(bf3920_qctrl[0].default_value!=value){
			bf3920_qctrl[0].default_value=value;
			BF3920_set_param_wb(dev,value);
			printk(KERN_INFO " set camera  white_balance=%d. \n ",value);
		}
		break;
	case V4L2_CID_EXPOSURE:
		if(bf3920_qctrl[1].default_value!=value){
			bf3920_qctrl[1].default_value=value;
			BF3920_set_param_exposure(dev,value);
			printk(KERN_INFO " set camera  exposure=%d. \n ",value);
		}
		break;
	case V4L2_CID_COLORFX:
		if(bf3920_qctrl[2].default_value!=value){
			bf3920_qctrl[2].default_value=value;
			BF3920_set_param_effect(dev,value);
			printk(KERN_INFO " set camera  effect=%d. \n ",value);
		}
		break;
	case V4L2_CID_WHITENESS:
		if(bf3920_qctrl[3].default_value!=value){
			bf3920_qctrl[3].default_value=value;
			BF3920_set_param_banding(dev,value);
			printk(KERN_INFO " set camera  banding=%d. \n ",value);
		}
		break;
	case V4L2_CID_BLUE_BALANCE:
		if(bf3920_qctrl[4].default_value!=value){
			bf3920_qctrl[4].default_value=value;
			BF3920_set_night_mode(dev,value);
			printk(KERN_INFO " set camera  scene mode=%d. \n ",value);
		}
		break;
	case V4L2_CID_HFLIP:    /* set flip on H. */          
		value = value & 0x3;
		if(bf3920_qctrl[5].default_value!=value){
			bf3920_qctrl[5].default_value=value;
			printk(" set camera  h filp =%d. \n ",value);
		}
		break;
	case V4L2_CID_VFLIP:    /* set flip on V. */
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		if(bf3920_qctrl[7].default_value!=value){
			bf3920_qctrl[7].default_value=value;
			//printk(KERN_INFO " set camera  zoom mode=%d. \n ",value);
		}
		break;
	case V4L2_CID_ROTATE:
		if(bf3920_qctrl[8].default_value!=value){
			bf3920_qctrl[8].default_value=value;
			printk(" set camera  rotate =%d. \n ",value);
		}
		break;
	default:
		ret=-1;
		break;
	}
	return ret;

}

static void power_down_bf3920(struct bf3920_device *dev)
{
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	unsigned char buf[2];
	buf[0]=0x09;
	buf[1]=0x80;
	i2c_put_byte_add8(client,buf, 2);

}

/* ------------------------------------------------------------------
	DMA and thread functions
   ------------------------------------------------------------------*/

#define TSTAMP_MIN_Y	24
#define TSTAMP_MAX_Y	(TSTAMP_MIN_Y + 15)
#define TSTAMP_INPUT_X	10
#define TSTAMP_MIN_X	(54 + TSTAMP_INPUT_X)

static void bf3920_fillbuff(struct bf3920_fh *fh, struct bf3920_buffer *buf)
{
	struct bf3920_device *dev = fh->dev;
	void *vbuf = videobuf_to_vmalloc(&buf->vb);
	vm_output_para_t para = {0};
	dprintk(dev,1,"%s\n", __func__);
	if (!vbuf)
		return;
	/*  0x18221223 indicate the memory type is MAGIC_VMAL_MEM*/
	para.mirror = bf3920_qctrl[5].default_value&3;// not set
	para.v4l2_format = fh->fmt->fourcc;
	para.v4l2_memory = 0x18221223;
	para.zoom = bf3920_qctrl[7].default_value;
	para.angle = bf3920_qctrl[8].default_value;
	para.vaddr = (unsigned)vbuf;
	vm_fill_buffer(&buf->vb,&para);
	buf->vb.state = VIDEOBUF_DONE;
}

static void bf3920_thread_tick(struct bf3920_fh *fh)
{
	struct bf3920_buffer *buf;
	struct bf3920_device *dev = fh->dev;
	struct bf3920_dmaqueue *dma_q = &dev->vidq;

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
			 struct bf3920_buffer, vb.queue);
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
	bf3920_fillbuff(fh, buf);
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

static void bf3920_sleep(struct bf3920_fh *fh)
{
	struct bf3920_device *dev = fh->dev;
	struct bf3920_dmaqueue *dma_q = &dev->vidq;

	DECLARE_WAITQUEUE(wait, current);

	dprintk(dev, 1, "%s dma_q=0x%08lx\n", __func__,
		(unsigned long)dma_q);

	add_wait_queue(&dma_q->wq, &wait);
	if (kthread_should_stop())
		goto stop_task;

	/* Calculate time to wake up */
	//timeout = msecs_to_jiffies(frames_to_ms(1));

	bf3920_thread_tick(fh);

	schedule_timeout_interruptible(2);

stop_task:
	remove_wait_queue(&dma_q->wq, &wait);
	try_to_freeze();
}

static int bf3920_thread(void *data)
{
	struct bf3920_fh  *fh = data;
	struct bf3920_device *dev = fh->dev;

	dprintk(dev, 1, "thread started\n");

	set_freezable();

	for (;;) {
		bf3920_sleep(fh);

		if (kthread_should_stop())
			break;
	}
	dprintk(dev, 1, "thread: exit\n");
	return 0;
}

static int bf3920_start_thread(struct bf3920_fh *fh)
{
	struct bf3920_device *dev = fh->dev;
	struct bf3920_dmaqueue *dma_q = &dev->vidq;

	dma_q->frame = 0;
	dma_q->ini_jiffies = jiffies;

	dprintk(dev, 1, "%s\n", __func__);

	dma_q->kthread = kthread_run(bf3920_thread, fh, "bf3920");

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&dev->v4l2_dev, "kernel_thread() failed\n");
		return PTR_ERR(dma_q->kthread);
	}
	/* Wakes thread */
	wake_up_interruptible(&dma_q->wq);

	dprintk(dev, 1, "returning from %s\n", __func__);
	return 0;
}

static void bf3920_stop_thread(struct bf3920_dmaqueue  *dma_q)
{
	struct bf3920_device *dev = container_of(dma_q, struct bf3920_device, vidq);

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
	struct bf3920_fh  *fh = vq->priv_data;
	struct bf3920_device *dev  = fh->dev;
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

static void free_buffer(struct videobuf_queue *vq, struct bf3920_buffer *buf)
{
	struct bf3920_fh  *fh = vq->priv_data;
	struct bf3920_device *dev  = fh->dev;

	dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);

	videobuf_waiton(vq, &buf->vb, 0, 0);
	if (in_interrupt())
		BUG();

	videobuf_vmalloc_free(&buf->vb);
	dprintk(dev, 1, "free_buffer: freed\n");
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define norm_maxw() 1920
#define norm_maxh() 1600
static int
buffer_prepare(struct videobuf_queue *vq, struct videobuf_buffer *vb,
						enum v4l2_field field)
{
	struct bf3920_fh     *fh  = vq->priv_data;
	struct bf3920_device    *dev = fh->dev;
	struct bf3920_buffer *buf = container_of(vb, struct bf3920_buffer, vb);
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
	struct bf3920_buffer    *buf  = container_of(vb, struct bf3920_buffer, vb);
	struct bf3920_fh        *fh   = vq->priv_data;
	struct bf3920_device       *dev  = fh->dev;
	struct bf3920_dmaqueue *vidq = &dev->vidq;

	dprintk(dev, 1, "%s\n", __func__);
	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue, &vidq->active);
}

static void buffer_release(struct videobuf_queue *vq,
			   struct videobuf_buffer *vb)
{
	struct bf3920_buffer   *buf  = container_of(vb, struct bf3920_buffer, vb);
	struct bf3920_fh       *fh   = vq->priv_data;
	struct bf3920_device      *dev  = (struct bf3920_device *)fh->dev;

	dprintk(dev, 1, "%s\n", __func__);

	free_buffer(vq, buf);
}

static struct videobuf_queue_ops bf3920_video_qops = {
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
	struct bf3920_fh  *fh  = priv;
	struct bf3920_device *dev = fh->dev;

	strcpy(cap->driver, "bf3920");
	strcpy(cap->card, "bf3920");
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = BF3920_CAMERA_VERSION;
	cap->capabilities =	V4L2_CAP_VIDEO_CAPTURE |
				V4L2_CAP_STREAMING     |
				V4L2_CAP_READWRITE;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	struct bf3920_fmt *fmt;

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
    struct bf3920_fmt *fmt;
    unsigned int k;

    if(fival->index > ARRAY_SIZE(bf3920_frmivalenum))
        return -EINVAL;

    for(k =0; k< ARRAY_SIZE(bf3920_frmivalenum); k++)
    {
        if( (fival->index==bf3920_frmivalenum[k].index)&&
                (fival->pixel_format ==bf3920_frmivalenum[k].pixel_format )&&
                (fival->width==bf3920_frmivalenum[k].width)&&
                (fival->height==bf3920_frmivalenum[k].height)){
            memcpy( fival, &bf3920_frmivalenum[k], sizeof(struct v4l2_frmivalenum));
            return 0;
        }
    }

    return -EINVAL;

}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct bf3920_fh *fh = priv;

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
	struct bf3920_fh  *fh  = priv;
	struct bf3920_device *dev = fh->dev;
	struct bf3920_fmt *fmt;
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
	struct bf3920_fh *fh = priv;
	struct videobuf_queue *q = &fh->vb_vidq;
	struct bf3920_device *dev = fh->dev;

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
	
	if (f->fmt.pix.pixelformat==V4L2_PIX_FMT_RGB24) {
		vidio_set_fmt_ticks=1;
		BF3920_set_resolution(dev,fh->height,fh->width);
	} else if (vidio_set_fmt_ticks==1) {
		BF3920_set_resolution(dev,fh->height,fh->width);
	}

	ret = 0;
out:
	mutex_unlock(&q->vb_lock);

	return ret;
}

static int vidioc_g_parm(struct file *file, void *priv,
        struct v4l2_streamparm *parms)
{
    struct bf3920_fh *fh = priv;
    struct bf3920_device *dev = fh->dev;
    struct v4l2_captureparm *cp = &parms->parm.capture;
    int ret;
    int i;

    dprintk(dev,3,"vidioc_g_parm\n");
    if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return -EINVAL;

    memset(cp, 0, sizeof(struct v4l2_captureparm));
    cp->capability = V4L2_CAP_TIMEPERFRAME;

    cp->timeperframe = bf3920_frmintervals_active;
    printk("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
            cp->timeperframe.numerator );
    return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	struct bf3920_fh  *fh = priv;

	return (videobuf_reqbufs(&fh->vb_vidq, p));
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct bf3920_fh  *fh = priv;

	return (videobuf_querybuf(&fh->vb_vidq, p));
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct bf3920_fh *fh = priv;

	return (videobuf_qbuf(&fh->vb_vidq, p));
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct bf3920_fh  *fh = priv;

	return (videobuf_dqbuf(&fh->vb_vidq, p,
				file->f_flags & O_NONBLOCK));
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	struct bf3920_fh  *fh = priv;

	return videobuf_cgmbuf(&fh->vb_vidq, mbuf, 8);
}
#endif

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct bf3920_fh  *fh = priv;
    vdin_parm_t para;
    int ret = 0 ;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	if (i != fh->type)
		return -EINVAL;
    memset( &para, 0, sizeof( para ));
    para.port  = TVIN_PORT_CAMERA;
    para.fmt = TVIN_SIG_FMT_MAX;//TVIN_SIG_FMT_MAX+1;;TVIN_SIG_FMT_CAMERA_1280X720P_30Hz
	para.frame_rate = bf3920_frmintervals_active.denominator
					/bf3920_frmintervals_active.numerator;//175
	para.h_active = bf3920_h_active;
	para.v_active = bf3920_v_active;
	para.hsync_phase = 1;
	para.vsync_phase  = 1;
	para.hs_bp = 0;
	para.vs_bp = 2;
	para.cfmt = TVIN_YUV422;
	para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;	
	para.reserved = 2; //skip_num
	ret =  videobuf_streamon(&fh->vb_vidq);
	if(ret == 0){
    start_tvin_service(0,&para);
	    fh->stream_on        = 1;
	}
	return ret;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct bf3920_fh  *fh = priv;

    int ret = 0 ;
	printk(KERN_INFO " vidioc_streamoff+++ \n ");
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
	struct bf3920_fmt *fmt = NULL;
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
		||(fmt->fourcc == V4L2_PIX_FMT_YVU420)
		||(fmt->fourcc == V4L2_PIX_FMT_YUV420))
	{
		if (fsize->index >= ARRAY_SIZE(bf3920_prev_resolution))
			return -EINVAL;
		frmsize = &bf3920_prev_resolution[fsize->index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	else if(fmt->fourcc == V4L2_PIX_FMT_RGB24){
		if (fsize->index >= ARRAY_SIZE(bf3920_pic_resolution))
			return -EINVAL;
		frmsize = &bf3920_pic_resolution[fsize->index];
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
	struct bf3920_fh *fh = priv;
	struct bf3920_device *dev = fh->dev;

	*i = dev->input;

	return (0);
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct bf3920_fh *fh = priv;
	struct bf3920_device *dev = fh->dev;

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

	for (i = 0; i < ARRAY_SIZE(bf3920_qctrl); i++)
		if (qc->id && qc->id == bf3920_qctrl[i].id) {
			memcpy(qc, &(bf3920_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct bf3920_fh *fh = priv;
	struct bf3920_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(bf3920_qctrl); i++)
		if (ctrl->id == bf3920_qctrl[i].id) {
			ctrl->value = dev->qctl_regs[i];
			return 0;
		}

	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct bf3920_fh *fh = priv;
	struct bf3920_device *dev = fh->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(bf3920_qctrl); i++)
		if (ctrl->id == bf3920_qctrl[i].id) {
			if (ctrl->value < bf3920_qctrl[i].minimum ||
			    ctrl->value > bf3920_qctrl[i].maximum ||
			    bf3920_setting(dev,ctrl->id,ctrl->value)<0) {
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

static int bf3920_open(struct file *file)
{
	struct bf3920_device *dev = video_drvdata(file);
	struct bf3920_fh *fh = NULL;
	int retval = 0;
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 1);
#endif		
	if(dev->platform_dev_data.device_init) {
		dev->platform_dev_data.device_init();
		printk("+++found a init function, and run it..\n");
	}
	BF3920_init_regs(dev);
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

	videobuf_queue_vmalloc_init(&fh->vb_vidq, &bf3920_video_qops,
			NULL, &dev->slock, fh->type, V4L2_FIELD_INTERLACED,
			sizeof(struct bf3920_buffer), fh,NULL);

	bf3920_start_thread(fh);
	bf3920_have_open = 1;
	return 0;
}

static ssize_t
bf3920_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct bf3920_fh *fh = file->private_data;

	if (fh->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		return videobuf_read_stream(&fh->vb_vidq, data, count, ppos, 0,
					file->f_flags & O_NONBLOCK);
	}
	return 0;
}

static unsigned int
bf3920_poll(struct file *file, struct poll_table_struct *wait)
{
	struct bf3920_fh        *fh = file->private_data;
	struct bf3920_device       *dev = fh->dev;
	struct videobuf_queue *q = &fh->vb_vidq;

	dprintk(dev, 1, "%s\n", __func__);

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != fh->type)
		return POLLERR;

	return videobuf_poll_stream(file, q, wait);
}

static int bf3920_close(struct file *file)
{
	struct bf3920_fh         *fh = file->private_data;
	struct bf3920_device *dev       = fh->dev;
	struct bf3920_dmaqueue *vidq = &dev->vidq;
	struct video_device  *vdev = video_devdata(file);
	bf3920_have_open = 0;
	bf3920_stop_thread(vidq);
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
	bf3920_h_active=800;
	bf3920_v_active=598;
	bf3920_qctrl[0].default_value= CAM_WB_AUTO;
	bf3920_qctrl[1].default_value=4;
	bf3920_qctrl[2].default_value=0;
	bf3920_qctrl[3].default_value=CAM_BANDING_50HZ;
	bf3920_qctrl[4].default_value=0;

	bf3920_qctrl[5].default_value=0;
	bf3920_qctrl[7].default_value=100;
	bf3920_qctrl[8].default_value=0;
	bf3920_frmintervals_active.numerator = 1;
	bf3920_frmintervals_active.denominator = 15;
	power_down_bf3920(dev);
#endif
	msleep(10);
	if(dev->platform_dev_data.device_uninit) {
		dev->platform_dev_data.device_uninit();
		printk("+++found a uninit function, and run it..\n");
	}

	msleep(10);
#ifdef CONFIG_ARCH_MESON6
	switch_mod_gate_by_name("ge2d", 0);
#endif		
	wake_unlock(&(dev->wake_lock));
	return 0;
}

static int bf3920_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct bf3920_fh  *fh = file->private_data;
	struct bf3920_device *dev = fh->dev;
	int ret;

	dprintk(dev, 1, "mmap called, vma=0x%08lx\n", (unsigned long)vma);

	ret = videobuf_mmap_mapper(&fh->vb_vidq, vma);

	dprintk(dev, 1, "vma start=0x%08lx, size=%ld, ret=%d\n",
		(unsigned long)vma->vm_start,
		(unsigned long)vma->vm_end-(unsigned long)vma->vm_start,
		ret);

	return ret;
}

static const struct v4l2_file_operations bf3920_fops = {
	.owner		= THIS_MODULE,
	.open           = bf3920_open,
	.release        = bf3920_close,
	.read           = bf3920_read,
	.poll		= bf3920_poll,
	.ioctl          = video_ioctl2, /* V4L2 ioctl handler */
	.mmap           = bf3920_mmap,
};

static const struct v4l2_ioctl_ops bf3920_ioctl_ops = {
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
	.vidioc_querymenu     = vidioc_querymenu,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,
	.vidioc_g_parm = vidioc_g_parm,
	.vidioc_enum_frameintervals = vidioc_enum_frameintervals,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf          = vidiocgmbuf,
#endif
};

static struct video_device bf3920_template = {
	.name		= "bf3920_v4l",
	.fops           = &bf3920_fops,
	.ioctl_ops 	= &bf3920_ioctl_ops,
	.release	= video_device_release,

	.tvnorms              = V4L2_STD_525_60,
	.current_norm         = V4L2_STD_NTSC_M,
};

static int bf3920_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_BF3920, 0);
}

static const struct v4l2_subdev_core_ops bf3920_core_ops = {
	.g_chip_ident = bf3920_g_chip_ident,
};

static const struct v4l2_subdev_ops bf3920_ops = {
	.core = &bf3920_core_ops,
};

static int bf3920_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct bf3920_device *t;
	struct v4l2_subdev *sd;
	aml_plat_cam_data_t* plat_dat;
	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);
	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return -ENOMEM;
	sd = &t->sd;
	v4l2_i2c_subdev_init(sd, client, &bf3920_ops);
	plat_dat= (aml_plat_cam_data_t*)client->dev.platform_data;

	/* test if devices exist. */
#ifdef CONFIG_VIDEO_AMLOGIC_CAPTURE_PROBE
	unsigned char buf[4];
	buf[0]=0;
	plat_dat->device_init();
	err=i2c_get_byte(client,0); 
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
	memcpy(t->vdev, &bf3920_template, sizeof(*t->vdev));

	video_set_drvdata(t->vdev, t);
	
	wake_lock_init(&(t->wake_lock),WAKE_LOCK_SUSPEND, "bf3920");
	/* Register it */
	if (plat_dat) {
		t->platform_dev_data.device_init=plat_dat->device_init;
		t->platform_dev_data.device_uninit=plat_dat->device_uninit;
		t->platform_dev_data.device_disable=plat_dat->device_disable;
		t->platform_dev_data.flash_support=plat_dat->flash_support;
		if(plat_dat->video_nr>=0)  video_nr=plat_dat->video_nr;
	}
	err = video_register_device(t->vdev, VFL_TYPE_GRABBER, video_nr);
	if (err < 0) {
		video_device_release(t->vdev);
		kfree(t);
		return err;
	}

	return 0;
}

static int bf3920_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct bf3920_device *t = to_dev(sd);

	video_unregister_device(t->vdev);
	v4l2_device_unregister_subdev(sd);
	wake_lock_destroy(&(t->wake_lock));
	kfree(t);
	return 0;
}

static const struct i2c_device_id bf3920_id[] = {
	{ "bf3920_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bf3920_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "bf3920",
	.probe = bf3920_probe,
	.remove = bf3920_remove,
	.id_table = bf3920_id,
};

