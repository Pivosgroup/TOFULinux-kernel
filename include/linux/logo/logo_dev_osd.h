#ifndef  LOGO_DEV_OSD_H
#define LOGO_DEV_OSD_H

#define DisableVideoLayer() \
    do { CLEAR_MPEG_REG_MASK(VPP_MISC, \
	VPP_VD1_PREBLEND | VPP_VD1_POSTBLEND);  \
    } while (0)

#define PARA_HDMI_ONLY    10

typedef  struct {
	char *name;
	int   info;	
}hdmi_only_info_t;

static int osd0_init(logo_object_t *plogo);
static int osd1_init(logo_object_t *plogo);
static  int  osd_enable_set(int  enable);
static int osd_deinit(void);
static  int  osd_transfer(logo_object_t *plogo);

#endif
