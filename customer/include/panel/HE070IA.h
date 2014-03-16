#ifndef PANEL_TYPICAL_TIMING_H
#define PANEL_TYPICAL_TIMING_H

//*****************************************
// Define LCD Typical Timing Parameters
//*****************************************
#define PANEL_MODEL			"HE070IA"
#define BITS_OPTION			0	//0:panel don't support bits option.	1:panel support 6/8bits option

#define ACITVE_AREA_WIDTH	94	//unit: mm
#define ACITVE_AREA_HEIGHT	151	//unit: mm
#define LCD_TYPE			LCD_DIGITAL_LVDS   //LCD_DIGITAL_TTL  //LCD_DIGITAL_LVDS  //LCD_DIGITAL_MINILVDS
#define LCD_BITS			8	//6	//8

#define H_ACTIVE			800
#define V_ACTIVE			1280
#define H_PERIOD			960
#define V_PERIOD			1320
//#define FRAME_RATE			50
#define	LCD_CLK				76000000//(H_PERIOD * V_PERIOD * FRAME_RATE)	//unit: Hz
#define CLK_AUTO_GEN		1	//1, auto generate clk parameters	//0, user set pll_ctrl, div_ctrl & clk_ctrl

//*****************************************
// modify below settings if needed
//*****************************************
#define CLK_POL				0
#define HS_WIDTH			10
#define HS_BACK_PORCH		70	//include HS_WIDTH
#define HS_POL				0	//0: negative, 1: positive
#define VS_WIDTH			2
#define VS_BACK_PORCH		20	//include VS_WIDTH
#define VS_POL				0	//0: negative, 1: positive

//*****************************************
// recommend settings, don't modify them unless there is display problem
//*****************************************
#define TTL_H_OFFSET		0	//adjust ttl display h_offset
#define H_OFFSET_SIGN		1	//0: negative value, 1: positive value
#define TTL_V_OFFSET		0	//adjust ttl display v_offset
#define V_OFFSET_SIGN		1	//0: negative value, 1: positive value
#define VIDEO_ON_PIXEL		80
#define VIDEO_ON_LINE		32
//************************************************

#endif