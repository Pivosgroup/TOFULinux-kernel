/*
 * drivers/amlogic/pivos/aml_snapshot.c
 *
 * video scaler for thumbnails/snapshots
 * CONFIG_PIVOS_AM_SNAPSHOT
 *
 * Copyright (C) 2013 PivosGroup
 *
 * Written by Scott Davilla <scott.davilla@pivosgroup.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the BSD Licence or GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <linux/amports/canvas.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_provider.h>
#include <linux/ge2d/ge2d.h>

#include <asm/uaccess.h>
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
#include <mach/mod_gate.h>
#endif

#include <linux/pivos/aml_snapshot.h>

extern u32 get_amvideo_frame_count(void);

#if 1
  #define DPRINT(...) printk(KERN_INFO __VA_ARGS__)
  #define DTRACE()    DPRINT(KERN_INFO "%s()\n", __FUNCTION__)
#else
  #define DPRINT(...)
  #define DTRACE()
#endif
#define   DERROR(...) printk(KERN_ERR __VA_ARGS__)

#define SNAPSHOT_DRIVER_NAME "aml_snapshot"
#define SNAPSHOT_DEFAULT_WIDTH  (1280 / 2)
#define SNAPSHOT_DEFAULT_HEIGHT (720  / 2)

#define CANVAS_ALIGNED(x)	(((x) + 7) & ~7)

#define AMVIDEOCAP_CANVAS_INDEX 0x6e

struct aml_internal_snapshot_t {
  u32                   frame_count;
  // canvas details.
  unsigned int          psize;
  unsigned long         paddr;
  void                 *vaddr;

  // user snapshot info mirror.
  // dst_format is AMSNAPSHOT_FMT_XXX
  struct aml_snapshot_t user_snapshot;

  // device config
  int                   config_major;
  struct device        *config_device;
};

static struct aml_internal_snapshot_t *g_snapshot;

/*********************************************************
 *********************************************************/
static size_t GE2D_FORMAT_to_size(unsigned int format)
{
  switch (format & GE2D_BPP_MASK) {
    case GE2D_BPP_8BIT:
      return 1;
    case GE2D_BPP_16BIT:
      return 2;
    case GE2D_BPP_24BIT:
      return 3;
    case GE2D_BPP_32BIT:
    default:
      return 4;
  }
}

static int VIDTYPE_to_GE2D_FORMAT(vframe_t* vf)
{
  int format = GE2D_FORMAT_M24_YUV420;

  if (vf->type & VIDTYPE_VIU_422) {
    //DPRINT("GE2D_FORMAT_S16_YUV422.\n");
    format = GE2D_FORMAT_S16_YUV422;
    if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
      format |= GE2D_FORMAT_S16_YUV422B & (3<<3);
    else if(vf->type & VIDTYPE_INTERLACE_TOP)
      format |= GE2D_FORMAT_S16_YUV422T & (3<<3);

  } else if (vf->type & VIDTYPE_VIU_444) {
    //DPRINT("GE2D_FORMAT_S24_YUV444.\n");
    format = GE2D_FORMAT_S24_YUV444;
    if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
      format |= GE2D_FORMAT_S24_YUV444B & (3<<3);
    else if (vf->type & VIDTYPE_INTERLACE_TOP)
      format |= GE2D_FORMAT_S24_YUV444T & (3<<3);

  } else if (vf->type & VIDTYPE_VIU_NV21) {
    //DPRINT("GE2D_FORMAT_M24_NV21.\n");
    format = GE2D_FORMAT_M24_NV21;
    if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
      format |= GE2D_FORMAT_M24_NV21B   & (3<<3);
    else if (vf->type & VIDTYPE_INTERLACE_TOP)
      format |= GE2D_FORMAT_M24_NV21T   & (3<<3);

  } else {
    //DPRINT("GE2D_FORMAT_M24_YUV420.\n");
    format = GE2D_FORMAT_M24_YUV420;
    if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
      format |= GE2D_FORMAT_M24_YUV420B & (3<<3);
    else if (vf->type & VIDTYPE_INTERLACE_TOP)
      format |= GE2D_FORMAT_M24_YUV420T & (3<<3);
  }

  return format;
}

static unsigned int AMSNAPSHOT_FMT_to_GE2D_FORMAT(int format)
{
  unsigned int ge2d_format;
  switch(format) {
  case AMSNAPSHOT_FMT_S24_BGR:
    ge2d_format = GE2D_FORMAT_S24_RGB;
    break;
  case AMSNAPSHOT_FMT_S24_RGB:
    ge2d_format = GE2D_FORMAT_S24_BGR;
    break;
  case AMSNAPSHOT_FMT_S32_RGBA:
    ge2d_format = GE2D_FORMAT_S32_RGBA;
    break;
  case AMSNAPSHOT_FMT_S32_BGRA:
    ge2d_format = GE2D_FORMAT_S32_BGRA;
    break;
  case AMSNAPSHOT_FMT_S32_ABGR:
    ge2d_format = GE2D_FORMAT_S32_ABGR;
    break;
  default:
  case AMSNAPSHOT_FMT_S32_ARGB:
    ge2d_format = GE2D_FORMAT_S32_ARGB;
    break;
  }

  return ge2d_format;
}

/*********************************************************
 *********************************************************/
static ssize_t frame_type_show(struct class *cla, struct class_attribute *attr, char *buf)
{
  vframe_t *cur_dispbuf = get_cur_dispbuf();

  if (cur_dispbuf) {
    if ((cur_dispbuf->type & VIDTYPE_VIU_422) == VIDTYPE_VIU_422)
      return sprintf(buf, "VIDTYPE_VIU_422\n");
    else if ((cur_dispbuf->type & VIDTYPE_VIU_444) == VIDTYPE_VIU_444)
      return sprintf(buf, "VIDTYPE_VIU_444\n");
#ifdef MESON_CPU_TYPE
    else if((cur_dispbuf->type & VIDTYPE_VIU_NV21) == VIDTYPE_VIU_NV21)
      return sprintf(buf, "VIDTYPE_VIU_NV21\n");
#endif
    else
      return sprintf(buf, "VIDTYPE_VIU_??? = (%d)\n", cur_dispbuf->type);
  }

  return sprintf(buf, "NA\n");
}

static ssize_t frame_addr_show(struct class *cla, struct class_attribute *attr, char *buf)
{
  vframe_t *cur_dispbuf = get_cur_dispbuf();

  if (cur_dispbuf) {
    u32 addr[3];
    canvas_t canvas;

    canvas_read(cur_dispbuf->canvas0Addr & 0xff, &canvas);
    addr[0] = canvas.addr;
    canvas_read((cur_dispbuf->canvas0Addr >> 8) & 0xff, &canvas);
    addr[1] = canvas.addr;
    canvas_read((cur_dispbuf->canvas0Addr >> 16) & 0xff, &canvas);
    addr[2] = canvas.addr;

    return sprintf(buf, "0x%x-0x%x-0x%x\n", addr[0], addr[1], addr[2]);
  }
  return sprintf(buf, "NA\n");
}

static ssize_t frame_count_show(struct class *cla, struct class_attribute *attr, char *buf)
{
  return sprintf(buf, "%d\n", get_amvideo_frame_count());
}

static ssize_t frame_width_show(struct class *cla, struct class_attribute *attr, char *buf)
{
  vframe_t *cur_dispbuf = get_cur_dispbuf();

  if (cur_dispbuf)
    return sprintf(buf, "%d\n", cur_dispbuf->width);

  return sprintf(buf, "NA\n");
}

static ssize_t frame_height_show(struct class *cla, struct class_attribute *attr, char *buf)
{
  vframe_t *cur_dispbuf = get_cur_dispbuf();

  if (cur_dispbuf)
    return sprintf(buf, "%d\n", cur_dispbuf->height);

  return sprintf(buf, "NA\n");
}

/*********************************************************
 *********************************************************/
static int aml_snapshot_snap(void)
{
  int rtn = 0;
  int output_format;
  int input_x, input_y, input_width, input_height, input_format;
  vframe_t *vf = get_cur_dispbuf();

  if (vf) {
    canvas_t canvas;
    vframe_t vf_local = *vf;
    int canvas_idx = AMVIDEOCAP_CANVAS_INDEX;
    ge2d_context_t *ge2d_context = create_ge2d_work_queue();
    config_para_ex_t ge2d_config;
    struct aml_snapshot_t user_snapshot = g_snapshot->user_snapshot;
    // check for special 'zero'flags
    if (user_snapshot.src_width  == 0)
      user_snapshot.src_width  = vf_local.width;
    if (user_snapshot.src_height == 0)
      user_snapshot.src_height = vf_local.height;

    output_format = AMSNAPSHOT_FMT_to_GE2D_FORMAT(user_snapshot.dst_format);

    switch_mod_gate_by_name("ge2d", 1);
    canvas_config(canvas_idx,
      g_snapshot->paddr,
      user_snapshot.dst_width * GE2D_FORMAT_to_size(output_format),
      user_snapshot.dst_height,
      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);

    input_x = 0;
    input_width = vf->width;

    input_y = 0;
    input_height = vf->height;

    input_format = VIDTYPE_to_GE2D_FORMAT(vf);
    if (input_format == GE2D_FORMAT_S16_YUV422) {
      input_height = input_height / 2;
    }

    // setup ge2d_config
    memset(&ge2d_config, 0x00, sizeof(config_para_ex_t));
    ge2d_config.alu_const_color = 0; //0x000000ff;
    ge2d_config.src1_gb_alpha   = 0; //0xff;
    // src, handle all planes as the src format can change
    canvas_read( vf_local.canvas0Addr & 0xff, &canvas);
    ge2d_config.src_planes[0].addr    = canvas.addr;
    ge2d_config.src_planes[0].w       = canvas.width;
    ge2d_config.src_planes[0].h       = canvas.height;
    canvas_read((vf_local.canvas0Addr >>8) & 0xff, &canvas);
    ge2d_config.src_planes[1].addr    = canvas.addr;
    ge2d_config.src_planes[1].w       = canvas.width;
    ge2d_config.src_planes[1].h       = canvas.height;
    canvas_read((vf_local.canvas0Addr >> 16) & 0xff, &canvas);
    ge2d_config.src_planes[2].addr    = canvas.addr;
    ge2d_config.src_planes[2].w       = canvas.width;
    ge2d_config.src_planes[2].h       = canvas.height;
    //
    ge2d_config.src_para.canvas_index = vf_local.canvas0Addr;
    ge2d_config.src_para.mem_type     = CANVAS_TYPE_INVALID;
    ge2d_config.src_para.format       = input_format;
    ge2d_config.src_para.top          = input_y;
    ge2d_config.src_para.left         = input_x;
    ge2d_config.src_para.width        = input_width;
    ge2d_config.src_para.height       = input_height;
    ge2d_config.src_para.color        = 0xffffffff;

    // dst (always rgb+a) so only one plane
    canvas_read(canvas_idx & 0xff, &canvas);
    ge2d_config.dst_planes[0].addr    = canvas.addr;
    ge2d_config.dst_planes[0].w       = canvas.width;
    ge2d_config.dst_planes[0].h       = canvas.height;
    //
    ge2d_config.dst_para.canvas_index = canvas_idx;
    ge2d_config.dst_para.mem_type     = CANVAS_TYPE_INVALID;
    ge2d_config.dst_para.format       = output_format;
    ge2d_config.dst_para.top          = 0;
    ge2d_config.dst_para.left         = 0;
    ge2d_config.dst_para.width        = user_snapshot.dst_width;
    ge2d_config.dst_para.height       = user_snapshot.dst_height;
    ge2d_config.dst_para.color        = 0;

    if (ge2d_context && ge2d_context_config_ex(ge2d_context, &ge2d_config) == 0) {
      // stretch to fit
      stretchblt_noalpha(ge2d_context,
        0, 0, ge2d_config.src_para.width, ge2d_config.src_para.height,
        0, 0, ge2d_config.dst_para.width, ge2d_config.dst_para.height);
    } else {
      DERROR("ge2d_context_config error.\n");
      rtn = -EFAULT;
    }
    switch_mod_gate_by_name("ge2d", 0);

    if (ge2d_context)
      destroy_ge2d_work_queue(ge2d_context);

  } else {
    rtn = -EAGAIN;
  }

  return rtn;
}

/*********************************************************
 * /dev/aml_snapshot APIs
 *********************************************************/
static int aml_snapshot_open(struct inode *inode, struct file *file)
{
  return 0;
}

static int aml_snapshot_release(struct inode *inode, struct file *file)
{
  return 0;
}

static long aml_snapshot_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  int rtn = 0;
  switch (cmd) {
    case AMSNAPSHOT_IOC_GET_FRAME:
      {
        u32 frame_count;
        struct aml_snapshot_t *user_snapshot = (struct aml_snapshot_t*)arg;

        if (user_snapshot->dst_format == 0)
          user_snapshot->dst_format = AMSNAPSHOT_FMT_S24_RGB;

        // update interal settings from user settings.
        g_snapshot->user_snapshot = *user_snapshot;
        // paranoia, dst_vaddr is not valid internally,
        g_snapshot->user_snapshot.dst_vaddr = 0;

        frame_count = get_amvideo_frame_count();
        if (frame_count != g_snapshot->frame_count) {
          g_snapshot->frame_count = frame_count;
          rtn = aml_snapshot_snap();
          if (rtn == 0)
          {
            char *dst, *src = (char*)g_snapshot->vaddr;
            // user height/stride matches captured height/stride
            int size = user_snapshot->dst_stride * user_snapshot->dst_height;
            dst = (char*)user_snapshot->dst_vaddr;
            if (copy_to_user(dst, src, size) != 0)
                rtn = -EFAULT;
          }
        }
        else
        {
          rtn = -EAGAIN;
        }
      }
      break;

    default:
      rtn = -EINVAL;
  }

  return rtn;
}

static int aml_snapshot_mmap(struct file *file, struct vm_area_struct *vma)
{
  unsigned long off;
  unsigned vm_size = vma->vm_end - vma->vm_start;

  if (vm_size == 0)
    return -EAGAIN;

  // convert from pfn (page frame number) to physical address
  off = vma->vm_pgoff << PAGE_SHIFT;
  // add the canvas physical address
  off += g_snapshot->paddr;
  // convert physical address back to pfn
  off = off >> PAGE_SHIFT;

  vma->vm_flags |= VM_RESERVED | VM_IO;

  if (remap_pfn_range(vma, vma->vm_start, off,
    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {

    DERROR("aml_snapshot failed remap_pfn_range\n");
    return -EAGAIN;
  }

  return 0;
}

static void aml_snapshot_init(struct resource *iomem)
{
  g_snapshot->frame_count = 0;
  // canvas setup
  g_snapshot->paddr  = iomem->start;
  g_snapshot->psize  = iomem->end - iomem->start + 1;
  g_snapshot->vaddr  = ioremap_nocache(g_snapshot->paddr, g_snapshot->psize);

  // default src/dst bounds for snapshot
  g_snapshot->user_snapshot.src_x      = 0;
  g_snapshot->user_snapshot.src_y      = 0;
  g_snapshot->user_snapshot.src_width  = 0; // implies vf_local.width
  g_snapshot->user_snapshot.src_height = 0; // implies vf_local.height
  g_snapshot->user_snapshot.dst_width  = 0; // implies vf_local.width
  g_snapshot->user_snapshot.dst_height = 0; // implies vf_local.height
  // helper vars for copy back to userland
  g_snapshot->user_snapshot.dst_stride = 0;
  g_snapshot->user_snapshot.dst_format = AMSNAPSHOT_FMT_S24_RGB;
  g_snapshot->user_snapshot.dst_vaddr  = 0;
}

/*********************************************************
 *********************************************************/
static struct class_attribute aml_snapshot_class_attrs[] = {
  __ATTR_RO(frame_type),
  __ATTR_RO(frame_addr),
  __ATTR_RO(frame_count),
  __ATTR_RO(frame_width),
  __ATTR_RO(frame_height),
  __ATTR_NULL
};

static struct class aml_snapshot_class = {
  .name           = SNAPSHOT_DRIVER_NAME,
  .class_attrs    = aml_snapshot_class_attrs,
};

const static struct file_operations aml_snapshot_fops = {
  .owner          = THIS_MODULE,
  .open           = aml_snapshot_open,
  .release        = aml_snapshot_release,
  .unlocked_ioctl = aml_snapshot_ioctl,
  .mmap           = aml_snapshot_mmap,
};

static int __devinit aml_snapshot_probe(struct platform_device *pdev)
{
  int rtn = 0;
  struct resource *iomem;

  g_snapshot = kzalloc(sizeof(struct aml_internal_snapshot_t), GFP_KERNEL);
  if (!g_snapshot) {
    DERROR("Cannot kzalloc for aml_snapshot device\n");
    rtn = -ENOMEM;
    goto err1;
  }
  memset(g_snapshot, 0x00, sizeof(struct aml_internal_snapshot_t));

  iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!iomem)
  {
    DERROR("aml_snapshot memory resource undefined.\n");
    rtn = -ENOMEM;
    goto err1;
  }
  aml_snapshot_init(iomem);

  rtn = register_chrdev(0, SNAPSHOT_DRIVER_NAME, &aml_snapshot_fops);
  if (rtn <= 0) {
    DERROR("Cannot register major for aml_snapshot device\n");
    goto err2;
  }

  g_snapshot->config_major = rtn;
  DPRINT("aml_snapshot config major:%d\n", rtn);
  rtn = class_register(&aml_snapshot_class);
  if (rtn) {
    DPRINT(" failed to class register aml_snapshot_class\n");
    goto err3;
  }

  g_snapshot->config_device = device_create(&aml_snapshot_class, NULL,
    MKDEV(g_snapshot->config_major, 0), NULL, SNAPSHOT_DRIVER_NAME);

  if (IS_ERR(g_snapshot->config_device)) {
    DERROR("Cannot create aml_snapshot device\n");
    goto err4;
  }

  return 0;

err4:
  class_unregister(&aml_snapshot_class);
err3:
  unregister_chrdev(g_snapshot->config_major, SNAPSHOT_DRIVER_NAME);
err2:
  kfree(g_snapshot), g_snapshot = NULL;
err1:
  return rtn;
}

static int __devexit aml_snapshot_remove(struct platform_device *pdev)
{
  if (g_snapshot->config_device) {
  //device_unregister(g_snapshot->config_device);
    device_destroy(&aml_snapshot_class, MKDEV(g_snapshot->config_major, 0));
    g_snapshot->config_device = NULL;
  }
  class_unregister(&aml_snapshot_class);
  class_destroy(&aml_snapshot_class);
	unregister_chrdev(g_snapshot->config_major, SNAPSHOT_DRIVER_NAME);

  if (g_snapshot->vaddr)
    iounmap(g_snapshot->vaddr);
  kfree(g_snapshot), g_snapshot = NULL;

  return 0;
}

/*********************************************************
 *********************************************************/
static struct platform_driver aml_snapshot_driver = {
  .probe          = aml_snapshot_probe,
  .remove         = aml_snapshot_remove,
  .suspend        = NULL,
  .resume         = NULL,
  .driver         = {
    .owner        = THIS_MODULE,
    .name         = SNAPSHOT_DRIVER_NAME,
  },
};

static int __init aml_snapshot_module_init(void)
{
  if (platform_driver_register(&aml_snapshot_driver)) {
    DPRINT("failed to register aml_snapshot module\n");
  return -ENODEV;
  }

  return 0;
}

static void __exit aml_snapshot_module_exit(void)
{
  platform_driver_unregister(&aml_snapshot_driver);
}

module_init(aml_snapshot_module_init);
module_exit(aml_snapshot_module_exit);

MODULE_AUTHOR("scott.davilla@pivosgroup.com>");
MODULE_DESCRIPTION("Pivos AMLogic Video SnapShot");
MODULE_LICENSE("GPL");

/*
cd /cache/recovery/test/
cp /storage/external_storage/sdcard1/test /cache/recovery/test/
cp /storage/external_storage/sdcard1/test2 /cache/recovery/test/
rm -f /storage/external_storage/sdcard1/test*.raw

./boblightd -c boblight.conf

cp /storage/external_storage/sdcard1/boblight-aml .
./boblight-aml -o gamma=1.75

insmod /storage/external_storage/sdcard1/aml_snapshot.ko
rmmod  /storage/external_storage/sdcard1/aml_snapshot.ko

cp *.raw /storage/external_storage/sdcard1/
sync

cat /sys/class/video/axis
cat /sys/class/aml_snapshot/frame_width
cat /sys/class/aml_snapshot/frame_height
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq

libboblight.so -> /system/lib
boblight.conf  -> /system/etc
boblightd      -> /system/bin
boblight-aml   -> /system/bin

service boblightd /system/bin/boblightd -c /system/etc/boblight.conf
  user system

service boblight-aml /system/bin/boblight-aml
  user system


*/

