#ifndef AMPORTS_PRIV_HEAD_HH
#define AMPORTS_PRIV_HEAD_HH
#include "streambuf.h"
#include <linux/amports/vframe.h>

stream_buf_t *get_buf_by_type(u32  type);

/*video.c provide*/
int ext_get_cur_video_frame(vframe_t **vf,int *canvas_index);
int ext_put_video_frame(vframe_t *vf);
int ext_register_end_frame_callback(struct amvideocap_req *req);


#endif

