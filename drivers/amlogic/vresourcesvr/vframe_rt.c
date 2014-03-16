/*
 * vframe_rt.c
 *
 *  Created on: Apr 19, 2013
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

#include "vressvr.h"

#define RECEIVER_NAME "vressvr"

/************************************************
 *
 *   register vresource server as a frame receiver
 *
 *************************************************/

static int receiver_event_fun(int type, void *data, void *private_data)
{
	switch (type)
	{
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		return RECEIVER_ACTIVE;
		break;
	default:
		break;
	}
	return 0;
}

static const struct vframe_receiver_op_s ppmgr_vf_receiver = {
	.event_cb = receiver_event_fun
};


static struct vframe_receiver_s vf_recv;

static void reg_receiver(void)
{
	vf_receiver_init(&vf_recv, RECEIVER_NAME, &ppmgr_vf_receiver,
	                NULL);
	vf_reg_receiver(&vf_recv);
}

static vframe_t *peek_dec(void)
{
	struct vframe_provider_s *vfp;
	vframe_t *vf;
	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->peek))
		return NULL;

	vf = vfp->ops->peek(vfp->op_arg);
	return vf;
}

static vframe_t *get_dec(void)
{
	struct vframe_provider_s *vfp;
	vframe_t *vf;
	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->get))
		return NULL;
	vf = vfp->ops->get(vfp->op_arg);
	return vf;
}

static void put_dec(vframe_t *vf)
{
	struct vframe_provider_s *vfp;
	vfp = vf_get_provider(RECEIVER_NAME);
	if (!(vfp && vfp->ops && vfp->ops->put))
		return;
	vfp->ops->put(vf, vfp->op_arg);
}

/* global pointer of vresserver frame operations. */
struct vressvr_vf_ops vressvr_vfps = {
	.reg_recv = reg_receiver,
	.vf_peek = peek_dec,
	.vf_get = get_dec,
	.vf_put = put_dec
};
