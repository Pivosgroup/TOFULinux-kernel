/*
 * v_res_svr_ctl.h
 *
 *  Created on: Apr 22, 2013
 *      Author: amlogic
 */

#ifndef V_RES_SVR_CTL_H_
#define V_RES_SVR_CTL_H_

typedef struct v_res_svr_vf_s {
	u32 handle;	/* not for user to use. */
	u32 l_off;	/* offset of the current frame. */
	u32 u_off;	/* offset of the luma . */
	u32 v_off;	/* offset of the luma . */
	u32 bufwidth;  /* buffer width of Y. */
	u32 bufheight;  /* pixel height of Y. */

	u32 width;  /* buffer width of Y. */
	u32 height;  /* pixel height of Y. */

        u32 duration;
        u32 pts;
        u32 flag;
} v_res_svr_vf_t;

#define V_RES_SVR_IOC_MAGIC  'R'
#define V_RES_SVR_IOC_VF_GET		_IOW(V_RES_SVR_IOC_MAGIC, 0x00, struct v_res_svr_vf_s)
#define V_RES_SVR_IOC_VF_PEEK		_IOW(V_RES_SVR_IOC_MAGIC, 0x01, struct v_res_svr_vf_s)
#define V_RES_SVR_IOC_VF_PUT		_IOW(V_RES_SVR_IOC_MAGIC, 0x02, struct v_res_svr_vf_s)
#define V_RES_SVR_IOC_BUF_LEN		_IOW(V_RES_SVR_IOC_MAGIC, 0x03, unsigned int)

#endif /* V_RES_SVR_CTL_H_ */
