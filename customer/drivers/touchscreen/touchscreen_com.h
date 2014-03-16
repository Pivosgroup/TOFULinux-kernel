
#ifndef _TOUCHSCREEN_COM_H_
#define _TOUCHSCREEN_COM_H_

#define SCREEN_MAX_X 		1280
#define SCREEN_MAX_Y 		800

struct fw_data
{
    u32 offset : 8;
    u32 : 0;
    u32 val;
};

#endif

