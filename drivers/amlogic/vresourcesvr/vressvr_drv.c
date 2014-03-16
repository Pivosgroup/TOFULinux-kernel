/*
 * vressvr_drv.c
 *
 *  Created on: Apr 18, 2013
 *      Author: amlogic
 */

#include <linux/platform_device.h>
#include <linux/amports/canvas.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/amports/vframe.h>
#include <linux/amports/vframe_receiver.h>

#include "vressvr.h"
#include "v_res_svr_ctl.h"

static struct vressvr_drv vressvr_drv_ctx;
#define DRIVER_DEVICE_NAME "resmgr"

/***********************************************************************
 *
 * file op section.
 *
 ************************************************************************/
static int vressvr_open(struct inode *inode, struct file *file)
{
	struct vressvr_dev* dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		VRSVR_ERROR("no memory to alloc device node.\n");
		return -ENOMEM;
	}
	dev->vf_ops = &vressvr_vfps;
	dev->drv = &vressvr_drv_ctx;
	file->private_data = (void*) dev;
	return 0;
}

static long vressvr_ioctl(struct file *file, unsigned int cmd, ulong args)
{
	struct vressvr_dev* dev = file->private_data;
	vframe_t* vf;
	canvas_t cs;
	struct v_res_svr_vf_s vframe_usr;
	void * argp = (void __user*) args;
	int ret = 0;
	switch (cmd)
	{
	case V_RES_SVR_IOC_VF_GET:
		vf = dev->vf_ops->vf_get();
		if (!vf)
			ret = -EAGAIN;
		else {
			canvas_read(vf->canvas0Addr & 0xff, &cs);
			vframe_usr.handle = (u32) vf;
			vframe_usr.bufwidth = cs.width;
			vframe_usr.bufheight = cs.height;
			vframe_usr.l_off = cs.addr - dev->drv->buffer_start;

			canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs);
			vframe_usr.u_off = cs.addr - dev->drv->buffer_start;

			if ((vf->type & VIDTYPE_VIU_NV21) == 0) {
				canvas_read((vf->canvas0Addr >> 16) & 0xff,
				                &cs);
				vframe_usr.v_off = cs.addr
				                - dev->drv->buffer_start;
			} else
				vframe_usr.v_off = NULL;

			vframe_usr.width = vf->width;
			vframe_usr.height = vf->height;
			vframe_usr.duration = vf->duration;
			vframe_usr.pts = vf->pts;
			copy_to_user((void*) argp, (void*) &vframe_usr,
			                sizeof(vframe_usr));
		}
		break;
	case V_RES_SVR_IOC_VF_PEEK:
		vf = dev->vf_ops->vf_peek();
		if (!vf)
			ret = -EAGAIN;
		else {
			canvas_read(vf->canvas0Addr & 0xff, &cs);
			vframe_usr.handle = (u32) vf;
			vframe_usr.bufwidth = cs.width;
			vframe_usr.bufheight = cs.height;
			vframe_usr.l_off = cs.addr - dev->drv->buffer_start;

			canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs);
			vframe_usr.u_off = cs.addr - dev->drv->buffer_start;

			if ((vf->type & VIDTYPE_VIU_NV21) == 0) {
				canvas_read((vf->canvas0Addr >> 16) & 0xff,
				                &cs);
				vframe_usr.v_off = cs.addr
				                - dev->drv->buffer_start;
			} else
				vframe_usr.v_off = NULL;

			vframe_usr.width = vf->width;
			vframe_usr.height = vf->height;
			vframe_usr.duration = vf->duration;
			vframe_usr.pts = vf->pts;
			copy_to_user((void*) argp, (void*) &vframe_usr,
			                sizeof(vframe_usr));
		}
		break;
	case V_RES_SVR_IOC_VF_PUT:
		copy_from_user((void*) &vframe_usr, (void*) argp,
		                sizeof(vframe_usr));
		dev->vf_ops->vf_put(&vframe_usr);
		break;
	case V_RES_SVR_IOC_BUF_LEN:
		*((unsigned int *) argp) = dev->drv->buffer_size;
		break;
	default:
		return -ENOIOCTLCMD;

	}
	return ret;
}

static int vressvr_release(struct inode *inode, struct file *file)
{
	struct vressvr_dev* dev = file->private_data;
	kfree(dev);
	file->private_data = NULL;
	return 0;
}

static int vressvr_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vressvr_dev* dev = file->private_data;
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned vm_size = vma->vm_end - vma->vm_start;
	unsigned size = min(vm_size, dev->drv->buffer_size);
	int ret = 0;

	if (vm_size == 0) {
		return -EINVAL;
	}

	VRSVR_PRINT(
	                LOG_LEVEL_MEDIUM,
	                "mmap buffer size:%dkb, vframe buffer size:%dkb.\n", vm_size/1024, dev->drv->buffer_size/1024);

	off += dev->drv->buffer_start;

	vma->vm_flags |= VM_RESERVED | VM_IO;

	ret = remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT, size
	                , vma->vm_page_prot);
	if (ret < 0) {
		VRSVR_ERROR("set_cached: failed remap_pfn_range\n");
		return -EINVAL;
	}
	return ret;
}

/***********************************************************************
 *
 * file op initintg section.
 *
 ************************************************************************/

/* driver class. */
static struct class driver_class = {
	.name = DRIVER_DEVICE_NAME,
	.class_attrs = NULL,
};

static struct class* init_driver_class()
{
	int ret = 0;
	ret = class_register(&driver_class);
	if (ret < 0) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "error create resmgr class\r\n");
		return NULL;
	}
	return &driver_class;
}

/* device node create. */

static const struct file_operations vressvr_fops = {
	.owner = THIS_MODULE,
	.open = vressvr_open,
	.unlocked_ioctl = vressvr_ioctl,
	.mmap = vressvr_mmap,
	.release = vressvr_release,
};

int init_vressvr_device(phys_addr_t buf_addr, unsigned int bsize)
{
	int ret = 0;

	strcpy(vressvr_drv_ctx.name, DRIVER_DEVICE_NAME);
	ret = register_chrdev(0, vressvr_drv_ctx.name, &vressvr_fops);
	if (ret <= 0) {
		VRSVR_ERROR("register resmgr device error\r\n");
		return ret;
	}
	vressvr_drv_ctx.major = ret;
	vressvr_drv_ctx.class = init_driver_class();
	if (vressvr_drv_ctx.class == NULL
	)
		goto unregister_dev;
	vressvr_drv_ctx.dev = device_create(vressvr_drv_ctx.class, NULL,
	                MKDEV(vressvr_drv_ctx.major,0), NULL,
	                vressvr_drv_ctx.name);
	if (IS_ERR(vressvr_drv_ctx.dev)) {
		VRSVR_ERROR(
		                "create ppmgr device error:%d\n", ERR_PTR(vressvr_drv_ctx.dev));
		goto unregister_dev;
	}
	vressvr_drv_ctx.buffer_start = buf_addr;
	vressvr_drv_ctx.buffer_size = bsize;
	return 0;

	unregister_dev: if (vressvr_drv_ctx.major > 0)
		unregister_chrdev(vressvr_drv_ctx.major, vressvr_drv_ctx.name);
	if (vressvr_drv_ctx.class)
		class_unregister(vressvr_drv_ctx.class);
	vressvr_drv_ctx.major = 0;
	vressvr_drv_ctx.class = NULL;
	return -1;
}

int uninit_vressvr_device(void)
{
	if (vressvr_drv_ctx.dev)
		device_destroy(NULL, MKDEV(vressvr_drv_ctx.major, 0));
	unregister_chrdev(vressvr_drv_ctx.major, vressvr_drv_ctx.name);
	return 0;
}

/*******************************************************************
 *
 * interface for Linux driver
 *
 * ******************************************************************/
static struct platform_device *vressvr_dev = NULL;

/* for driver. */
static int vressvr_drv_probe(struct platform_device *pdev)
{
	phys_addr_t buf_start;
	unsigned int buf_size;
	struct resource *mem;

	if (!(mem = platform_get_resource(pdev, IORESOURCE_MEM, 0))) {
		VRSVR_LOG(LOG_LEVEL_HIGH, "codec memory resource undefined.\n");
		return -EFAULT;
	}

	buf_start = mem->start;
	buf_size = mem->end - mem->start + 1;
	init_vressvr_device(buf_start, buf_size);
	init_dec_resource_manager(buf_start, buf_size);
	vressvr_vfps.reg_recv();
	return 0;
}

static int vressvr_drv_remove(struct platform_device *plat_dev)
{
	uninit_vressvr_device();
	return 0;
}

/* general interface for a linux driver .*/
struct platform_driver vressvr_drv = {
	.probe = vressvr_drv_probe,
	.remove = vressvr_drv_remove,
	.driver = {
		.name = DRIVER_DEVICE_NAME,
		.owner = THIS_MODULE,
	}
};

static int __init vressvr_init_module(void)
{
	int err;

	VRSVR_LOG(LOG_LEVEL_HIGH, "video resource server initialized\n");
	if ((err = platform_driver_register(&vressvr_drv))) {
		VRSVR_LOG(
		                LOG_LEVEL_ERROR,
		                "error to initialize video resource server:%d\n", err);
		return err;
	}

	return err;

}

static void __exit vressvr_remove_module(void)
{
	platform_device_put(vressvr_dev);
	platform_driver_unregister(&vressvr_drv);
	VRSVR_LOG(LOG_LEVEL_HIGH, "video resource server module removed.\n");
}

module_init(vressvr_init_module);
module_exit(vressvr_remove_module);

MODULE_DESCRIPTION("Video resource manager For future use");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("aml-sh");
