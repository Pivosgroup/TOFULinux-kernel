#ifndef __LINUX_ZFORCE_TS_H__
#define __LINUX_ZFORCE_TS_H__

//#include <mach/gpio_data.h>
//#include <mach/gpio.h>

extern	int32_t gpio_get_val(uint32_t pin);
#define GPIO_DIRECTION_INPUT(port)          gpio_set_status(port, gpio_status_in)
#define GPIO_DIRECTION_OUTPUT(port, val)    do {gpio_set_status(port, gpio_status_out); gpio_out(port, val);}while(0)
#define GPIO_SET_VALUE(port, val)           gpio_out(port, val)
#define GPIO_GET_VALUE(port)				gpio_get_val(port)
#define GPIO_FREE(port)                     //gpio_free(port)
#define GPIO_REQUEST(port, name)            0//gpio_request(port, name)
#define GPIO_PULL_UPDOWN(port, val)         //s3c_gpio_setpull(port, val)
#define GPIO_CFG_PIN(port, irq, type)        gpio_irq_set(port,GPIO_IRQ(irq-INT_GPIO_0, type))
//-------------------------GPIO REDEFINE END------------------------------//


//*************************TouchScreen Work Part Start**************************
#define TOUCH_PWR_EN		PAD_GPIOA_27//		0  
#define RESET_PORT         	PAD_GPIOC_3
#define INT_PORT            PAD_GPIOA_16
#define INT_PORT_IRQ_IDX		170
#define TS_INT          		INT_GPIO_0      //Interrupt Number,EINT18(119)
#define INT_CFG         		GPIO_IRQ_FALLING//GPIO_IRQ_LOW//GPIO_IRQ_FALLING            //IO configer as EINT
#define INT_TRIGGER         IRQ_TYPE_LEVEL_LOW//IRQ_TYPE_EDGE_RISING        //IRQ_TYPE_EDGE_FALLING       
#define POLL_TIME           10        //


#define GTP_ICS_SLOT_REPORT	1

#endif

