#ifndef __FT5X02_CONFIG_H__
#define __FT5X02_CONFIG_H__
/*ft5x02 config*/


#define FT5X02_KX				142
#define FT5X02_KY				160
#define FT5X02_LEMDA_X			0
#define FT5X02_LEMDA_Y			0
#define FT5X02_RESOLUTION_X	1024
#define FT5X02_RESOLUTION_Y	600
#define FT5X02_DIRECTION		0	/*0-tx is X direct. 1-rx is X direct.*/

/**/
#define FT5X02_FACE_DETECT_PRE_VALUE	20
#define FT5X02_FACE_DETECT_NUM		10
#define FT5X02_BIGAREA_PEAK_VALUE_MIN			150/*The min value to be decided as the big point*/
#define FT5X02_BIGAREA_DIFF_VALUE_OVER_NUM		30/*The min big points of the big area*/
#define FT5X02_BIGAREA_POINT_AUTO_CLEAR_TIME		3000/*3000ms*/
#define FT5X02_FACE_DETECT_LAST_TIME	233//	0xe8
#define FT5X02_MODE					0x01
#define FT5X02_PMODE					0x00
#define FT5X02_FIRMWARE_ID				0x0e
#define FT5X02_STATE					0x01
#define FT5X02_CUSTOMER_ID				0x79
#define FT5X02_PERIOD_ACTIVE			0x10
#define FT5X02_FACE_DETECT_STATISTICS_TX_NUM 3
#define FT5X02_THGROUP					120
#define FT5X02_THPEAK					60
#define FT5X02_FACE_DETECT_MODE		0x00/*close*/
#define FT5X02_MAX_TOUCH_VALUE		0x04b0

#define FT5X02_THFALSE_TOUCH_PEAK 		255
#define FT5X02_THDIFF						160
#define FT5X02_PWMODE_CTRL				1
#define FT5X02_TIME_ENTER_MONITOR 		10
#define FT5X02_PERIOD_MONITOR 				16
#define FT5X02_AUTO_CLB_MODE				0xFF
#define FT5X02_DRAW_LINE_TH				250
#define FT5X02_DIFFDATA_HADDLE_VALUE		100

#define FT5X02_ABNORMAL_DIFF_VALUE		160
#define FT5X02_ABNORMAL_DIFF_NUM			8
#define FT5X02_ABNORMAL_DIFF_LAST_FRAME	30 

#define FT5X02_POINTS_SUPPORTED		5


#define FT5X02_STATIC_TH				122	
#define FT5X02_MID_SPEED_TH			44
#define FT5X02_HIGH_SPEED_TH			116
#define FT5X02_START_RX					0
#define FT5X02_ADC_TARGET				52

#define FT5X02_FILTER_FRAME_NOISE		3
#define FT5X02_POWERNOISE_FILTER_TH	60
#define FT5X02_KX_LR					360
#define FT5X02_KY_UD					360

#define FT5X02_ESD_FILTER_FRAME 		0

#define FT5X02_MOVSTH_I				2
#define FT5X02_MOVSTH_N				3
/**/
#define TCV 	50
#define RCV 	100
#define ROV	0x55
unsigned char g_ft5x02_tx_num = 12;
unsigned char g_ft5x02_rx_num = 9;
unsigned char g_ft5x02_gain = 0x1C;

unsigned char g_ft5x02_voltage = 0x00;
unsigned char g_ft5x02_scanselect = 2;/*1-3M	2-4.5M 3-6.75M*/

unsigned char g_ft5x02_tx_order[] = {11,10,9,8,7,6,5,4,3,2,1,0};
unsigned char g_ft5x02_tx_offset = 0x05;

unsigned char g_ft5x02_tx_cap[] = {TCV,TCV,TCV,TCV,TCV,TCV,TCV,TCV,TCV,
					TCV,TCV,TCV,TCV,TCV,TCV,TCV,TCV,TCV,TCV,TCV};

unsigned char g_ft5x02_rx_order[] = {0,1,2,3,4,5,6,7,8};
unsigned char g_ft5x02_rx_offset[] = {ROV,ROV,ROV,ROV,ROV,ROV};

unsigned char g_ft5x02_rx_cap[] = {RCV,RCV,RCV,RCV,RCV,RCV,RCV,RCV,RCV,RCV,RCV,RCV};



#endif
