#ifndef __FT5X02_CONFIG_H__ 
#define __FT5X02_CONFIG_H__ 
/*FT5X02 config*/


#define FT5X02_FIRMWARE_ID			14
#define FT5X02_OTP_PARAM_ID			0
#define FT5X02_CUSTOMER_ID			121
#define FT5X02_CHIPER_ID			2
#define FT5X02_RESOLUTION_X			480
#define FT5X02_RESOLUTION_Y			800
#define FT5X02_LEMDA_X			35
#define FT5X02_LEMDA_Y			34
#define FT5X02_KX			161
#define FT5X02_KY			161
#define FT5X02_DIRECTION			1
#define FT5X02_KX_LR			306
#define FT5X02_KY_UD			306
#define FT5X02_POINTS_SUPPORTED			5
#define FT5X02_THGROUP			92
#define FT5X02_THPEAK			80
#define FT5X02_THDIFF			2560
#define FT5X02_MAX_TOUCH_VALUE			1200
#define FT5X02_DRAW_LINE_TH			244
#define FT5X02_PWMODE_CTRL			1
#define FT5X02_PERIOD_ACTIVE			16
#define FT5X02_TIME_ENTER_MONITOR			10
#define FT5X02_PERIOD_MONITOR			40
#define FT5X02_FILTER_FRAME_NOISE			2
#define FT5X02_POWERNOISE_FILTER_TH			80
#define FT5X02_DIFFDATA_HADDLE_VALUE			-60
#define FT5X02_FACE_DETECT_MODE			0
#define FT5X02_FACE_DETECT_STATISTICS_TX_NUM			3
#define FT5X02_FACE_DETECT_PRE_VALUE			20
#define FT5X02_FACE_DETECT_NUM			10
#define FT5X02_FACE_DETECT_LAST_TIME			1000
#define FT5X02_BIGAREA_PEAK_VALUE_MIN			255
#define FT5X02_BIGAREA_DIFF_VALUE_OVER_NUM			30
#define FT5X02_BIGAREA_POINT_AUTO_CLEAR_TIME			3000
#define FT5X02_ABNORMAL_DIFF_VALUE			60
#define FT5X02_ABNORMAL_DIFF_NUM			15
#define FT5X02_ABNORMAL_DIFF_LAST_FRAME			30
#define FT5X02_START_RX			0
#define FT5X02_ADC_TARGET			8500
#define FT5X02_ESD_FILTER_FRAME			0
#define FT5X02_MOVSTH_I			4
#define FT5X02_MOVSTH_N			4
#define FT5X02_MODE			1
#define FT5X02_PMODE			1
#define FT5X02_ERR			0
#define FT5X02_AUTO_CLB_MODE			255
#define FT5X02_STATE			1
#define FT5X02_HIGH_SPEED_TH			69
#define FT5X02_MID_SPEED_TH			117
#define FT5X02_STATIC_TH			100
#define FT5X02_THFALSE_TOUCH_PEAK			204


unsigned char g_ft5x02_tx_num = 20;
unsigned char g_ft5x02_rx_num = 12;
unsigned char g_ft5x02_gain = 10;
unsigned char g_ft5x02_voltage = 3;
unsigned char g_ft5x02_scanselect = 8;
unsigned char g_ft5x02_tx_order[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
unsigned char g_ft5x02_tx_offset = 1;
unsigned char g_ft5x02_tx_cap[] = {42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42};
unsigned char g_ft5x02_rx_order[] = {0,1,2,3,4,5,6,7,8,9,10,11};
unsigned char g_ft5x02_rx_offset[] = {69,85,85,69,101,85};
unsigned char g_ft5x02_rx_cap[] = {84,84,84,84,84,84,84,84,84,84,84,84};


#endif